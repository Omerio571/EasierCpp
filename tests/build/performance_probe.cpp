#include "cpplivelab/doctor.hpp"
#include "cpplivelab/process.hpp"
#include "cpplivelab/projects.hpp"
#include "cpplivelab/session.hpp"
#include "build/auto_build.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace
{
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

double milliseconds(Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

struct Report
{
    std::ofstream file;

    explicit Report(const fs::path& path) : file(path)
    {
        require(static_cast<bool>(file), "Cannot write profiling report");
        file << "metric,location,milliseconds\n" << std::fixed << std::setprecision(3);
    }

    void add(const std::string& metric, const std::string& location, double value)
    {
        file << metric << ',' << location << ',' << value << '\n';
        file.flush();
        std::cout << metric << " [" << location << "]: " << value << " ms\n";
    }
};

struct Project
{
    fs::path parent;
    fs::path root;

    Project(const fs::path& directory, const std::wstring& suffix)
        : parent(fs::weakly_canonical(directory)),
          root(parent / (L"CppLiveLab Timing " + std::to_wstring(GetCurrentProcessId()) + suffix))
    {
        require(
            fs::is_directory(parent) && !fs::exists(root), "Unsafe profiling project directory");
        cpplivelab::create_project(root, true);
    }

    ~Project()
    {
        std::error_code error;
        // Only the previously nonexistent root created by this probe is removed.
        if (fs::weakly_canonical(root, error).parent_path() == parent)
        {
            fs::remove_all(root, error);
        }
    }
};

struct Run
{
    double launch = 0;
    double root_exit = 0;
    double total = 0;
    double configure_start = -1;
    double configure_end = -1;
    double build_end = -1;
    std::string output;
};

Run execute(const fs::path& executable,
    const std::vector<std::wstring>& arguments,
    const fs::path& directory,
    const cpplivelab::Environment& environment = {})
{
    cpplivelab::ConsoleSession process;
    const auto start = Clock::now();
    process.start_program(executable, arguments, directory, environment);
    Run result;
    result.launch = milliseconds(start);
    bool root_seen = false;
    for (;;)
    {
        result.output += process.poll();
        if (result.configure_start < 0 &&
            result.output.find("Configuring Debug:") != result.output.npos)
        {
            result.configure_start = milliseconds(start);
        }
        if (result.configure_end < 0 &&
            result.output.find("Build files have been written to:") != result.output.npos)
        {
            result.configure_end = milliseconds(start);
        }
        if (result.build_end < 0 && result.output.find("Build succeeded:") != result.output.npos)
        {
            result.build_end = milliseconds(start);
        }
        if (process.root_exited())
        {
            if (!root_seen)
            {
                result.root_exit = milliseconds(start);
                root_seen = true;
            }
            process.finish_build();
        }
        require(milliseconds(start) < 120000, "Profiling command did not finish");
        if (!process.running() && !process.output_pending())
        {
            break;
        }
        else
        {
            Sleep(1);
        }
    }
    result.output += process.poll();
    result.total = milliseconds(start);
    if (!root_seen)
    {
        result.root_exit = result.total;
    }
    if (process.exit_code() != 0)
    {
        throw std::runtime_error("Profiling command failed:\n" + result.output);
    }
    return result;
}

fs::path tool(const cpplivelab::Toolchain& tools, const std::string& name)
{
    for (const auto& entry : tools.tools)
    {
        if (entry.name == name && !entry.installed_path.empty())
        {
            return entry.installed_path;
        }
    }
    throw std::runtime_error("Profiling tool missing: " + name);
}

void watcher(Report& report, const fs::path& root, const std::string& location)
{
    cpplivelab::ProjectWatcher watcher;
    auto start = Clock::now();
    watcher.start(root);
    report.add("watcher.start_snapshot", location, milliseconds(start));
    for (int iteration = 0; iteration < 5; ++iteration)
    {
        // Drain prior close/duplicate notifications before timing the next save.
        const auto quiet = Clock::now();
        Sleep(20);
        while (watcher.poll())
        {
            watcher.consume_changes();
        }
        report.add("watcher.probe_quiet_wait", location, milliseconds(quiet));
        start = Clock::now();
        std::ofstream(root / L"src/calculator.cpp", std::ios::app) << "\n// notification probe\n";
        while (!watcher.poll())
        {
            require(milliseconds(start) < 5000, "Watcher profiling notification absent");
            Sleep(1);
        }
        report.add("watcher.notification", location, milliseconds(start));
        start = Clock::now();
        require(!watcher.consume_changes().empty(), "Watcher profiling change absent");
        report.add("watcher.fingerprint", location, milliseconds(start));
    }
    watcher.stop();
    cpplivelab::AutoBuildScheduler scheduler;
    scheduler.enable(true);
    const auto tick = GetTickCount64();
    scheduler.notify(tick);
    start = Clock::now();
    while (!scheduler.settled(GetTickCount64()))
    {
        Sleep(1);
    }
    report.add("scheduler.default_debounce", location, milliseconds(start));
}

void project_timings(Report& report,
    const fs::path& parent,
    const std::string& location,
    const cpplivelab::Toolchain& tools,
    const cpplivelab::Environment& environment,
    const fs::path& cli)
{
    for (int sample = 0; sample < 2; ++sample)
    {
        Project project(
            parent, std::wstring(location.begin(), location.end()) + std::to_wstring(sample));
        const auto build = project.root / L".cpplivelab/build-Debug";
        const auto cmake = tool(tools, "cmake");
        const std::vector<std::wstring> configure{L"-S",
            project.root.wstring(),
            L"-B",
            build.wstring(),
            L"-G",
            L"Ninja",
            L"-DCMAKE_BUILD_TYPE=Debug",
            L"-DCMAKE_CXX_STANDARD=20",
            L"-DCMAKE_MAKE_PROGRAM=" + tool(tools, "ninja").wstring(),
            L"-DCMAKE_CXX_COMPILER=" + tool(tools, "cl").wstring()};
        const auto measure = [&](const char* metric, const std::vector<std::wstring>& arguments)
        {
            const auto result = execute(cmake, arguments, project.root, environment);
            report.add(metric, location, result.total);
            report.add(std::string(metric) + ".launch", location, result.launch);
            report.add(
                std::string(metric) + ".exit_drain", location, result.total - result.root_exit);
        };
        measure("cmake.configure_cold", configure);
        measure("cmake.configure_warm", configure);
        measure("cmake.compile_cold", {L"--build", build.wstring()});
        measure("cmake.build_noop", {L"--build", build.wstring()});
        std::ofstream(project.root / L"src/calculator.cpp", std::ios::app)
            << "\n// incremental compile\n";
        measure("cmake.compile_incremental", {L"--build", build.wstring()});
        watcher(report, project.root, location);
    }

    Project project(parent, std::wstring(location.begin(), location.end()) + L"CLI");
    for (int sample = 0; sample < 2; ++sample)
    {
        auto arguments = std::vector<std::wstring>{L"build", project.root.wstring()};
        if (sample)
        {
            arguments.push_back(L"--incremental");
        }
        const auto result = execute(cli, arguments, project.root);
        const std::string metric = sample ? "cli.warm_incremental" : "cli.cold";
        report.add(metric, location, result.total);
        report.add(metric + ".launch", location, result.launch);
        if (result.configure_start >= 0)
        {
            report.add(
                metric + ".preconfigure_setup", location, result.configure_start - result.launch);
        }
        if (result.configure_end >= 0)
        {
            report.add(
                metric + ".configure", location, result.configure_end - result.configure_start);
            report.add(metric + ".compile", location, result.build_end - result.configure_end);
        }
        report.add(metric + ".exit_drain", location, result.total - result.root_exit);
    }
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    try
    {
        require(
            argc == 4, "Usage: performance_probe <report.csv> <local-parent> <OneDrive-parent>");
        Report report(argv[1]);
        const auto start = Clock::now();
        const auto tools = cpplivelab::discover_toolchain(cpplivelab::system_discovery_inputs());
        report.add("tool_discovery", "shared", milliseconds(start));
        cpplivelab::Environment environment;
        for (int sample = 0; sample < 3; ++sample)
        {
            const auto setup = Clock::now();
            environment = cpplivelab::developer_environment(tools.developer_command);
            report.add("developer_environment", "shared", milliseconds(setup));
        }
        const auto binaries = fs::absolute(argv[0]).parent_path();
        Project launch_project(argv[2], L"Launch");
        const auto local_fixture = launch_project.root / L"session_fixture.exe";
        fs::copy_file(binaries / L"session_fixture.exe", local_fixture);
        // Interleave both executable locations to reduce ordering/page-cache bias.
        for (int iteration = 0; iteration < 22; ++iteration)
        {
            for (const auto& location : {std::string("local"), std::string("OneDrive")})
            {
                const auto fixture =
                    location == "local" ? local_fixture : binaries / L"session_fixture.exe";
                const auto result = execute(fixture, {L"noop"}, fixture.parent_path(), environment);
                if (iteration >= 2)
                {
                    report.add("process.launch", location, result.launch);
                    report.add("process.wait_and_exit", location, result.total - result.launch);
                }
            }
        }
        project_timings(report, argv[2], "local", tools, environment, binaries / L"cpplivelab.exe");
        project_timings(
            report, argv[3], "OneDrive", tools, environment, binaries / L"cpplivelab.exe");
        std::cout << "Timing probe completed; temporary projects removed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
