#include "cpplivelab/workspace.hpp"
#include "cpplivelab/doctor.hpp"
#include "cpplivelab/session.hpp"
#include "projects/project_paths.hpp"
#include "build/auto_build.hpp"
#include "../workspace_test_support.hpp"
#include <fstream>
#include <iostream>

namespace
{
namespace fs = std::filesystem;
using namespace cpplivelab;

void require(bool value, const char* reason)
{
    if (!value)
    {
        throw std::runtime_error(reason);
    }
}

void write(const fs::path& path, const std::string& value)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    require(static_cast<bool>(output << value), "Could not write source-discovery fixture");
}

std::string read(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

std::pair<int, std::string> execute(const fs::path& executable,
    const std::vector<std::wstring>& arguments,
    const fs::path& directory)
{
    ConsoleSession worker;
    worker.start_worker(executable, arguments, directory);
    const auto start = GetTickCount64();
    std::string output;
    while (worker.running() || worker.output_pending())
    {
        output += worker.poll();
        if (worker.root_exited())
        {
            worker.finish_build();
        }
        require(GetTickCount64() - start < 90000, "Source-discovery worker timed out");
        Sleep(5);
    }
    output += worker.poll();
    return {worker.exit_code(), output};
}

void discovery(
    const fs::path& root, const fs::path& cmake, const fs::path& ninja, bool visual_studio)
{
    const auto project = root / (visual_studio ? L"VS project" : L"Ninja project");
    create_managed_project(project, "multi");
    const auto original = read(project / L"CMakeLists.txt");
    write(project / L"assets/probe.txt", "runtime asset");
    for (const auto* path : {L"build/poison.cpp",
             L"external/poison.cpp",
             L"include/poison.cpp",
             L"src/build/poison.cpp",
             L"src/vendor/poison.cpp",
             L"src/BUILD-old/poison.cpp",
             L"src/Debug/poison.cpp"})
    {
        write(project / path, "#error unrelated source must not compile\n");
    }
    const auto build = project / L"build";
    std::vector<std::wstring> configure{L"-S",
        project.wstring(),
        L"-B",
        build.wstring(),
        L"-G",
        visual_studio ? L"Visual Studio 17 2022" : L"Ninja"};
    if (visual_studio)
    {
        configure.insert(configure.end(), {L"-A", L"x64"});
    }
    else
    {
        configure.push_back(L"-DCMAKE_BUILD_TYPE=Debug");
        configure.push_back(L"-DCMAKE_MAKE_PROGRAM=" + ninja.wstring());
    }
    const auto configured = execute(cmake, configure, root);
    std::cout << configured.second;
    require(configured.first == 0, "Independent generated-project configure failed");
    const auto artifact = build / (visual_studio ? L"Debug/app.exe" : L"app.exe");
    const auto compile = [&]
    {
        const auto start = GetTickCount64();
        const auto result = execute(
            cmake, {L"--build", build.wstring(), L"--config", L"Debug", L"--parallel", L"2"}, root);
        std::cout << result.second;
        require(result.first == 0, "Generated project build failed");
        std::cout << (visual_studio ? "VS" : "Ninja") << " build_ms=" << GetTickCount64() - start
                  << "\n";
    };
    compile();
    require(read(artifact.parent_path() / L"assets/probe.txt") == "runtime asset",
        "Generated-project asset copying failed");
    ProjectWatcher watcher;
    watcher.start(project);
    write(project / L"include/Game.hpp", "#pragma once\nint game();\n");
    write(project / L"src/nested/Game.cpp", "#include \"Game.hpp\"\nint game() { return 42; }\n");
    write(project / L"src/nested/part.cc", "int cc_value() { return 1; }\n");
    write(project / L"src/nested/part.cxx", "int cxx_value() { return 2; }\n");
    write(project / L"src/nested/part.c", "int c_value(void) { return 3; }\n");
    write(project / L"src/main.cpp",
        "#include \"Game.hpp\"\nint cc_value();\nint cxx_value();\n"
        "extern \"C\" int c_value(void);\n"
        "int main() { return game() + cc_value() + cxx_value() + c_value() == 48 ? 0 : 1; }\n");
    const auto deadline = GetTickCount64() + 3000;
    while (!watcher.poll() && GetTickCount64() < deadline)
    {
        Sleep(10);
    }
    require(!watcher.consume_changes().empty(), "Source addition was not detected by the watcher");
    compile();
    require(execute(artifact, {}, project).first == 0,
        "New mixed-language source implementations did not link or run");
    const auto stamp = fs::last_write_time(artifact);
    std::cout << "Automatic no-op samples\n";
    for (int sample = 0; sample < 3; ++sample)
    {
        compile();
    }
    require(fs::last_write_time(artifact) == stamp, "No-op build relinked the executable");
    const auto stable_object =
        visual_studio ? fs::path{} : build / L"CMakeFiles/app.dir/src/greeting.cpp.obj";
    const auto object_stamp =
        visual_studio ? fs::file_time_type{} : fs::last_write_time(stable_object);
    write(
        project / L"src/nested/Game.cpp", "#include \"Game.hpp\"\nint game() { return 42 + 0; }\n");
    compile();
    require(visual_studio || fs::last_write_time(stable_object) == object_stamp,
        "Editing one source unnecessarily recompiled the unchanged source");
    fs::rename(project / L"src/nested/Game.cpp", project / L"src/nested/Renamed.cpp");
    compile();
    write(project / L"src/nested/transient.cpp", "int temporary() { return 1; }\n");
    compile();
    fs::remove(project / L"src/nested/transient.cpp");
    compile();
    require(read(project / L"CMakeLists.txt") == original,
        "Discovering sources rewrote the generated CMake configuration");
    configure.push_back(L"-DEASIERCPP_AUTO_SOURCES=OFF");
    const auto manual = execute(cmake, configure, root);
    require(manual.first == 0, "Manual source mode could not configure");
    const auto missing = execute(cmake, {L"--build", build.wstring(), L"--config", L"Debug"}, root);
    require(missing.first != 0, "Manual mode unexpectedly included the new sources");
    auto explicit_sources = original;
    const auto insertion = explicit_sources.find("        \"src/greeting.cpp\"");
    require(insertion != std::string::npos, "Manual source list is missing");
    explicit_sources.insert(insertion,
        "        \"src/nested/Renamed.cpp\"\n"
        "        \"src/nested/part.cc\"\n"
        "        \"src/nested/part.cxx\"\n"
        "        \"src/nested/part.c\"\n");
    write(project / L"CMakeLists.txt", explicit_sources);
    compile();
    require(execute(artifact, {}, project).first == 0, "Explicit manual source list did not run");
    const auto manual_stamp = fs::last_write_time(artifact);
    std::cout << "Manual no-op samples\n";
    for (int sample = 0; sample < 3; ++sample)
    {
        compile();
    }
    require(fs::last_write_time(artifact) == manual_stamp, "Manual no-op build relinked");
}

void managed_slots(const fs::path& root)
{
    const auto project = root / L"Managed slots";
    create_managed_project(project, "empty");
    require(
        managed_build(project, L"Debug", {}, false, false) == 0, "Initial managed build failed");
    write(project / L"src/new.cpp", "int new_value() { return 42; }\n");
    write(project / L"src/main.cpp",
        "int new_value();\nint main() { return new_value() == 42 ? 0 : 1; }\n");
    auto slot = replacement_build_options(project, L"Debug", {});
    require(managed_build(project, L"Debug", slot, true, true) == 0,
        "Alternate slot did not discover a new implementation");
    activate_build_slot(project, L"Debug", slot);
    fs::rename(project / L"src/new.cpp", project / L"src/renamed.cpp");
    slot = replacement_build_options(project, L"Debug", {});
    require(managed_build(project, L"Debug", slot, true, true) == 0,
        "Reused slot did not regenerate after a source rename");
    require(managed_build(project, L"Release", {}, true, false) == 0,
        "Release build did not discover new sources");
}
} // namespace

int wmain()
{
    try
    {
        workspace_test::Fixture fixture;
        fs::path cmake;
        fs::path ninja;
        for (const auto& tool :
            cpplivelab::discover_toolchain(cpplivelab::system_discovery_inputs()).tools)
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
        require(!cmake.empty() && !ninja.empty(),
            "CMake or Ninja unavailable for source-discovery tests");
        discovery(fixture.root, cmake, ninja, false);
        discovery(fixture.root, cmake, ninja, true);
        managed_slots(fixture.root);
        std::cout << "Source discovery, mixed languages, manual lists, Ninja/VS, incremental "
                     "objects, assets and slot reuse passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
