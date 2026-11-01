#include "build/build_workers.hpp"
#include "cpplivelab/workspace.hpp"
#include "cpplivelab/doctor.hpp"
#include "cpplivelab/process.hpp"
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace cpplivelab
{
unsigned maximum_build_workers()
{
    MEMORYSTATUSEX memory{sizeof(memory)};
    const auto processors = (std::max)(1ul, GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
    if (!GlobalMemoryStatusEx(&memory))
    {
        return 1;
    }
    return build_worker_limit(processors, memory.ullAvailPhys);
}

unsigned build_worker_limit(unsigned long processors, unsigned long long available_bytes)
{
    // Reserve a GiB for the desktop, and budget 768 MiB per compiler process.
    const auto available = available_bytes > (1ull << 30) ? available_bytes - (1ull << 30) : 0;
    return static_cast<unsigned>(
        (std::min)(static_cast<unsigned long long>((std::max)(1ul, processors)),
            (std::max)(1ull, available / (768ull << 20))));
}

BuildWorkers load_build_workers()
{
    BuildWorkers value;
    std::ifstream input(manager_state_directory() / L"build-workers.settings");
    unsigned count = 0, recommended = 0;
    if (input >> count >> recommended && count <= 65536 && recommended <= 65536)
    {
        value = {count, recommended};
    }
    return value;
}

void save_build_workers(BuildWorkers value)
{
    if (value.count > maximum_build_workers())
    {
        throw std::invalid_argument("Worker count exceeds current CPU/memory limit.");
    }
    const auto file = manager_state_directory() / L"build-workers.settings";
    std::filesystem::create_directories(file.parent_path());
    const auto temporary = std::filesystem::path(file.wstring() + L".new");
    {
        std::ofstream output(temporary);
        output << value.count << ' ' << value.recommended << '\n';
        output.close();
        if (!output)
        {
            throw std::runtime_error("Cannot save compilation workers.");
        }
    }
    if (!MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING))
    {
        throw std::runtime_error("Cannot replace compilation worker settings.");
    }
}

void initialize_build_workers()
{
    const auto state = manager_state_directory();
    // Existing managed workspaces retain their native/preset/environment defaults.
    // A fresh installation starts with a CPU/memory-bounded automatic selection.
    if (!std::filesystem::exists(state / L"build-workers.settings") &&
        !std::filesystem::exists(state / L"projects"))
    {
        save_build_workers({maximum_build_workers(), 0});
    }
}

void append_build_workers(std::vector<std::wstring>& arguments, BuildWorkers value)
{
    if (value.count)
    {
        const auto count = (std::min)(value.count, maximum_build_workers());
        arguments.insert(arguments.end(), {L"--parallel", std::to_wstring(count)});
        std::cout << "[BUILD] Compilation workers: " << count << '\n';
    }
}

unsigned worker_benchmark_units(unsigned workers)
{
    // Enough independent units for high-core candidates, with bounded probe duration.
    return (std::max)(12u, (std::min)(workers, 32u) * 2);
}

unsigned benchmark_build_workers(const std::filesystem::path& directory)
{
    const auto safe_limit = maximum_build_workers();
    const auto units = worker_benchmark_units(safe_limit);
    std::cout << "Optimize Workers: CPU/memory-safe maximum " << safe_limit << "; workload has "
              << units << " translation units; this is a bounded synthetic estimate." << std::endl;
    if (!std::filesystem::is_directory(directory) || !std::filesystem::is_empty(directory))
    {
        throw std::invalid_argument("Worker benchmark requires an empty owned directory.");
    }
    const auto tools = discover_toolchain(system_discovery_inputs());
    std::filesystem::path cmake, ninja;
    for (const auto& tool : tools.tools)
    {
        if (tool.name == "cmake")
        {
            cmake = tool.installed_path;
        }
        if (tool.name == "ninja")
        {
            ninja = tool.installed_path;
        }
    }
    if (cmake.empty() || ninja.empty() || tools.developer_command.empty())
    {
        throw std::runtime_error("Worker benchmark requires discovered CMake, Ninja and MSVC.");
    }
    const auto environment = developer_environment(tools.developer_command);
    std::ofstream cmake_file(directory / L"CMakeLists.txt");
    cmake_file << "cmake_minimum_required(VERSION 3.20)\n\nproject(\n    WorkerProbe\n    "
                  "LANGUAGES CXX\n)\n\nadd_library(\n    probe\n    STATIC\n";
    for (unsigned index = 0; index < units; ++index)
    {
        cmake_file << "    unit" << index << ".cpp\n";
        std::ofstream(directory / (L"unit" + std::to_wstring(index) + L".cpp"))
            << "#include <vector>\n#include <string>\n#include <map>\n#include "
               "<algorithm>\n#include <numeric>\n"
               "\nint function"
            << index << "()\n{\n    std::vector<int> values(1024, " << index
            << ");\n    std::sort(values.begin(), values.end());\n    return "
               "std::accumulate(values.begin(), values.end(), 0);\n}\n";
    }
    cmake_file << ")\n\ntarget_compile_features(\n    probe\n    PRIVATE\n    cxx_std_20\n)\n";
    cmake_file.close();
    const auto build = directory / L"build";
    const auto run = [&](const std::vector<std::wstring>& arguments)
    {
        if (run_process(cmake, arguments, directory, environment) != 0)
        {
            throw std::runtime_error("Worker benchmark build failed; setting unchanged.");
        }
    };
    run({L"-S",
        directory.wstring(),
        L"-B",
        build.wstring(),
        L"-G",
        L"Ninja",
        L"-DCMAKE_BUILD_TYPE=Debug",
        L"-DCMAKE_MAKE_PROGRAM=" + ninja.wstring()});
    const unsigned limit = (std::min)(maximum_build_workers(), units);
    std::vector<unsigned> candidates{1};
    for (unsigned value = 2; value <= limit; value *= 2)
    {
        candidates.push_back(value);
    }
    if (candidates.back() != limit)
    {
        candidates.push_back(limit);
    }
    unsigned best = 1;
    double best_score = 0;
    std::ofstream results(directory / L"results.csv");
    results << "workers,cold_median_ms,warm_median_ms,score_ms\n";
    for (const auto count : candidates)
    {
        if (count > maximum_build_workers())
        {
            continue;
        }
        std::vector<double> cold, warm;
        for (unsigned repeat = 0; repeat < 3; ++repeat)
        {
            if (count > maximum_build_workers())
            {
                throw std::runtime_error("Memory pressure increased; worker setting unchanged.");
            }
            std::cout << "Optimize Workers: " << count << " jobs, trial " << repeat + 1 << "/3"
                      << std::endl;
            run({L"--build", build.wstring(), L"--target", L"clean"});
            const auto measure = [&]
            {
                const auto start = std::chrono::steady_clock::now();
                run({L"--build", build.wstring(), L"--parallel", std::to_wstring(count)});
                return std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - start)
                    .count();
            };
            cold.push_back(measure());
            {
                std::ofstream(directory / L"unit0.cpp", std::ios::app)
                    << "\n// incremental trial\n";
            }
            warm.push_back(measure());
        }
        std::sort(cold.begin(), cold.end());
        std::sort(warm.begin(), warm.end());
        const auto score = cold[1] * 0.5 + warm[1] * 0.5;
        results << count << ',' << cold[1] << ',' << warm[1] << ',' << score << '\n';
        // Prefer a lower count unless the next setting is at least 3% better.
        if (!best_score || score < best_score * 0.97)
        {
            best_score = score;
            best = count;
        }
    }
    results.close();
    if (!results)
    {
        throw std::runtime_error("Cannot record worker measurements.");
    }
    std::ofstream recommendation(directory / L"recommendation.txt");
    recommendation << best << '\n';
    recommendation.close();
    if (!recommendation)
    {
        throw std::runtime_error("Cannot record worker recommendation.");
    }
    std::cout << "Synthetic recommendation: " << best
              << " workers; not a guarantee for every project." << std::endl;
    return best;
}
} // namespace cpplivelab
