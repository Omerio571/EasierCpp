#include "cpplivelab/workspace.hpp"
#include "cpplivelab/doctor.hpp"
#include "cpplivelab/process.hpp"
#include "cpplivelab/session.hpp"
#include "cpplivelab/projects.hpp"
#include "../workspace_test_support.hpp"
#include "build/auto_build.hpp"
#include "build/build_diagnostics.hpp"
#include "build/build_lease.hpp"
#include "projects/directory_scan.hpp"
#include "projects/project_templates.hpp"
#include "libraries/library_compatibility.hpp"
#include "tools/tool_workflows.hpp"
#include "process/application_windows.hpp"
#include <fstream>
#include <iostream>

namespace
{
namespace fs = std::filesystem;
using namespace cpplivelab;

void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::string read(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

void write(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    require(static_cast<bool>(output << text), "Fixture write failed");
}

template <typename Action> void refuses(Action action, const char* message)
{
    bool rejected = false;
    try
    {
        action();
    }
    catch (const std::exception&)
    {
        rejected = true;
    }
    require(rejected, message);
}

void formatting(const fs::path& project, bool use_formatter = true)
{
    static const fs::path discovered_formatter = []
    {
        const auto tools = discover_toolchain(system_discovery_inputs());
        for (const auto& tool : tools.tools)
        {
            if (tool.name == "clang-format")
            {
                return tool.installed_path;
            }
        }
        return fs::path{};
    }();
    const auto formatter = use_formatter ? discovered_formatter : fs::path{};
    if (formatter.empty())
    {
        std::cout << "Optional clang-format check unavailable; validating generated readability "
                     "and retaining the separate compilation regressions.\n";
    }
    const auto style = fs::path(CPPLIVELAB_TEST_SOURCE) / L".clang-format";
    scan_directory(project,
        [&](const fs::path& path, bool directory)
        {
            const bool cpp = path.extension() == L".cpp" || path.extension() == L".hpp";
            if (!directory && cpp && !formatter.empty())
            {
                require(run_process(formatter,
                            {L"--dry-run",
                                L"--Werror",
                                L"--style=file:" + style.wstring(),
                                path.wstring()},
                            project) == 0,
                    "Installed clang-format rejected generated C++; strict formatting was not "
                    "skipped");
            }
            if (!directory &&
                (cpp || path.filename() == L"CMakeLists.txt" || path.extension() == L".cmake"))
            {
                std::istringstream lines(read(path));
                for (std::string line; std::getline(lines, line);)
                {
                    require(line.find('\t') == line.npos, "Generated code contains tabs");
                    const auto indent = line.find_first_not_of(' ');
                    require(indent == line.npos || indent % 4 == 0,
                        "Generated code indent is not four spaces");
                    require(line.size() <= 160, "Generated code contains an unreadable long line");
                }
            }
            return true;
        });
}

void unit(const fs::path& root)
{
    const auto diagnostics = parse_build_diagnostics(
        L"[ERROR] C:\\Space Folder\\\u03a9.cpp(12,4): error C2143: missing token\n"
        L"src/example.cpp:6:3: error: expected expression\n"
        L"CMake Error at CMakeLists.txt:9 (find_package):\n  package missing\n"
        L"ninja: error: manifest 'build.ninja' still dirty after 100 tries\n"
        L"LINK : fatal error LNK1104: cannot open library\n",
        root);
    require(diagnostics.size() == 5, "Diagnostic parser lost an error");
    require(diagnostics[0].line == 12 && diagnostics[0].column == 4 &&
                diagnostics[0].code == L"C2143" && diagnostics[0].file.filename() == L"\u03a9.cpp",
        "MSVC Unicode/space path or source coordinates incorrect");
    require(diagnostics[1].file == root / L"src/example.cpp" && diagnostics[1].column == 3,
        "Clang/GCC path or column incorrect");
    require(diagnostics[2].line == 9 &&
                diagnostics[2].message.find(L"package missing") != std::wstring::npos,
        "CMake context was lost");
    require(diagnostics[3].recovery_suggested && !diagnostics[0].recovery_suggested,
        "Recovery incorrectly suggested for ordinary source error");
    EditorOptions editor{L"Custom", {}, {L"{project}", L"{file}", L"{line}", L"{column}"}};
    const auto arguments = editor_arguments(editor, root, diagnostics[0].file, 12, 4);
    require(arguments.size() == 4 && arguments[1] == diagnostics[0].file.wstring() &&
                arguments[2] == L"12" && arguments[3] == L"4",
        "Editor placeholders corrupted argument boundaries");
    const auto project = root / L"Named Project";
    create_managed_project(project, "multi");
    formatting(project);
    formatting(project, false); // The same assertions must work without this optional tool.
    create_project(root / L"Calculator", true);
    formatting(root / L"Calculator");
    create_managed_project(root / L"Empty Starter", "empty");
    formatting(root / L"Empty Starter");
    require(read(project / "CMakeLists.txt").find("project(Named_Project LANGUAGES C CXX)") !=
                std::string::npos,
        "Project name or language setup is incorrect");
    require(!fs::exists(project / "cmake/Dependencies.cmake"),
        "No-library project contains dependency boilerplate");
    create_managed_project(root / L"Two Libraries", "multi", "fmt,nlohmann_json");
    formatting(root / L"Two Libraries");
    auto deep = project;
    while (deep.wstring().size() < 300)
    {
        deep /= L"long-source-directory";
    }
    const auto native = fs::path(windows_support::extended_path(deep));
    fs::create_directories(native);
    write(native / L"deep.cpp", "int value = 1;\n");
    const auto sources = ProjectWatcher::source_fingerprints(project);
    require(sources.contains((deep / L"deep.cpp").lexically_relative(project)),
        "Long-path source was silently omitted");
    write(project / L"_deps/ignored.cpp", "not a source snapshot\n");
    require(ProjectWatcher::source_fingerprints(project) == sources,
        "Dependency tree changed source freshness");
    {
        windows_support::UniqueHandle inaccessible;
        inaccessible.reset(CreateFileW((project / L"src/main.cpp").c_str(),
            GENERIC_READ,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr));
        require(inaccessible.value != INVALID_HANDLE_VALUE, "Cannot lock owned source fixture");
        refuses(
            [&]
            {
                ProjectWatcher::source_fingerprints(project);
            },
            "Unreadable source produced a falsely complete snapshot");
    }
    const auto base = managed_build_directory(project, L"Debug", {});
    const auto pointer = fs::path(base.wstring() + L".active");
    write(pointer, "../unowned-project");
    refuses(
        [&]
        {
            managed_build_directory(project, L"Debug", {});
        },
        "Unowned recovery path was accepted");
    fs::remove(pointer);
    refuses(
        [&]
        {
            ProjectWatcher::source_fingerprints(root / L"missing");
        },
        "Missing directory produced a falsely complete snapshot");
    const auto disappearing = root / L"Disappearing";
    write(disappearing / L"child/value.cpp", "value\n");
    refuses(
        [&]
        {
            scan_directory(disappearing,
                [&](const fs::path& path, bool directory)
                {
                    if (directory)
                    {
                        fs::remove_all(path);
                    }
                    return true;
                });
        },
        "Disappearing directory produced a falsely complete scan");
    refuses(
        [&]
        {
            recover_managed_build(project, L"Debug", {}, false);
        },
        "Recovery accepted without approval");
    std::cout
        << "Unified unit: formatting, names, modular dependencies, diagnostics, editor argv, "
           "long paths, ignored dependencies, disappearing/missing scans and approval passed\n";
}

void projects(const fs::path& root)
{
    const auto project = root / L"Recovery Project";
    create_managed_project(project, "multi");
    const auto original = read(project / "CMakeLists.txt");
    require(
        managed_build(project, L"Debug", {}, true, false) == 0, "Console project build/run failed");
    const auto previous = managed_build_directory(project, L"Debug", {});
    const auto artifact = executable_targets(project, L"Debug", {}).front().artifact;
    const auto stamp = fs::last_write_time(artifact);
    const auto cache_file = previous / L"CMakeCache.txt";
    const auto valid_cache = read(cache_file);
    for (const auto* field : {"CMAKE_GENERATOR:INTERNAL=", "CMAKE_HOME_DIRECTORY:INTERNAL="})
    {
        auto corrupt = valid_cache;
        const auto start = corrupt.find(field);
        require(start != corrupt.npos, "Expected CMake identity field is absent");
        const auto value = start + std::string(field).size();
        corrupt.replace(value,
            corrupt.find('\n', value) - value,
            std::string(field).starts_with("CMAKE_GENERATOR") ? "Other Generator"
                                                              : "C:/missing-source");
        write(cache_file, corrupt);
        require(managed_build(project, L"Debug", {}, false, false) != 0 &&
                    managed_build_directory(project, L"Debug", {}) == previous,
            "Cache/source/generator mismatch triggered automatic recovery or succeeded");
        refuses(
            [&]
            {
                managed_build(project, L"Debug", {}, true, true);
            },
            "Incremental build accepted a stale source/generator cache");
        write(cache_file, valid_cache);
    }
    require(managed_build(project, L"Debug", {}, false, true) == 0 &&
                fs::last_write_time(artifact) == stamp,
        "Unchanged incremental build recompiled the executable");
    {
        BuildLease lease(previous);
        refuses(
            [&]
            {
                recover_managed_build(project, L"Debug", {}, true);
            },
            "Recovery allowed a currently owned build");
    }
    write(previous / "CMakeCache.txt", "invalid stale cache\n");
    require(recover_managed_build(project, L"Debug", {}, true) == 0, "Fresh cache recovery failed");
    require(managed_build_directory(project, L"Debug", {}) != previous && fs::exists(artifact) &&
                read(previous / "CMakeCache.txt") == "invalid stale cache\n",
        "Recovery deleted old output or failed to switch");
    const auto active = managed_build_directory(project, L"Debug", {});
    write(project / "src/main.cpp", "intentional compilation failure\n");
    require(recover_managed_build(project, L"Debug", {}, true) != 0 &&
                managed_build_directory(project, L"Debug", {}) == active,
        "Failed recovery replaced the active directory");
    require(read(project / "CMakeLists.txt") == original, "Recovery rewrote user CMake");
    fs::remove(project / "src/main.cpp");
    require(managed_build(project, L"Release", {}, false, false) != 0 &&
                !fs::exists(project / "src/main.cpp"),
        "Configuration recreated a deliberately deleted main.cpp");
    write(project / "src/main.cpp", std::string(project_templates::hello_main));
    require(
        managed_build(project, L"Release", {}, true, false) == 0, "Release console project failed");
    const auto missing = root / L"Missing Artifact";
    create_managed_project(missing, "empty");
    std::ofstream(missing / "CMakeLists.txt", std::ios::app)
        << "\nadd_custom_command(TARGET app POST_BUILD\n"
           "    COMMAND \"${CMAKE_COMMAND}\" -E rm \"$<TARGET_FILE:app>\"\n)\n";
    require(managed_build(missing, L"Debug", {}, true, false) == artifact_missing_exit_code,
        "Successful compilation with missing artifact was not distinguished/refused");
    std::cout << "Unified projects: real run, no-change build, leased/stale/failed recovery, "
                 "preserved CMake and outputs, deleted source and Release passed\n";
}

std::pair<int, std::string> execute(const fs::path& executable,
    const std::vector<std::wstring>& arguments,
    const fs::path& directory)
{
    ConsoleSession session;
    session.start_worker(executable, arguments, directory);
    std::string output;
    const auto started = GetTickCount64();
    while (session.running() || session.output_pending())
    {
        output += session.poll();
        if (session.root_exited())
        {
            session.finish_build();
        }
        require(GetTickCount64() - started < 300000, "Owned integration worker timed out");
        Sleep(5);
    }
    output += session.poll();
    return {session.exit_code(), output};
}

struct ProgramWindow
{
    DWORD process;
    HWND window = nullptr;
};

BOOL CALLBACK find_program_window(HWND window, LPARAM parameter)
{
    auto& program = *reinterpret_cast<ProgramWindow*>(parameter);
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    if (process == program.process && IsWindowVisible(window))
    {
        program.window = window;
        return FALSE;
    }
    return TRUE;
}

void raylib(const fs::path& root, const fs::path& cli)
{
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    wchar_t system[MAX_PATH]{};
    GetSystemDirectoryW(system, MAX_PATH);
    const auto powershell = fs::path(system) / L"WindowsPowerShell/v1.0/powershell.exe";
    const auto cache = root / L"download";
    const auto script = fs::path(CPPLIVELAB_TEST_SOURCE) / L"scripts/github-library.ps1";
    for (const auto* action : {L"Resolve", L"Download"})
    {
        const auto result = execute(powershell,
            {L"-NoProfile",
                L"-ExecutionPolicy",
                L"Bypass",
                L"-File",
                script.wstring(),
                L"-Action",
                action,
                L"-Repository",
                L"https://github.com/raysan5/raylib",
                L"-Version",
                L"6.2",
                L"-OutputDirectory",
                cache.wstring()},
            root);
        std::cout << result.second << std::flush;
        require(result.first == 0, "Real Raylib 6.2 resolution/download failed");
    }
    std::ifstream resolved(cache / L"resolved.txt");
    std::string repository, commit, version;
    std::getline(resolved, repository);
    std::getline(resolved, commit);
    std::getline(resolved, version);
    require(repository == "raysan5/raylib" && version == "6.2" &&
                commit == "02566453880e06bec97f5b8160fa6fe1e98397ca",
        "Raylib 6.2 did not resolve to the regression commit");
    std::ifstream downloaded(cache / L"downloaded.txt");
    std::string source, hash;
    std::getline(downloaded, source);
    std::getline(downloaded, hash);
    // Exercise the same disposable target-discovery and consumer proof used by Add
    // Library, before checking the legacy catalog compatibility path below.
    const auto probe = root / L"Raylib compatibility probe";
    write(probe / L"CMakeLists.txt",
        cmake_dependency_probe(path_support::from_utf8(source)) +
            "\ntarget_link_libraries(cpplivelab_probe PRIVATE raylib)\n");
    write(probe / L"main.cpp",
        "#include <raylib.h>\nint main() { return GetColor(0xff0000ff).r == 255 ? 0 : 1; }\n");
    const auto verified = execute(cli,
        {L"manage-run",
            probe.wstring(),
            L"--target",
            L"cpplivelab_probe",
            L"--allow-configure-writes"},
        root);
    std::cout << verified.second << std::flush;
    require(verified.first == 0, "Actual Raylib dependency consumer probe failed");
    const auto assessment =
        assess_library(path_support::from_utf8(source), discovered_library_targets(probe), true);
    if (assessment.status != LibraryCompatibility::Compatible)
    {
        throw std::runtime_error("Verified Raylib compatibility: " + assessment.reason);
    }
    const auto id = "github-raysan5-raylib-" + commit + "-raylib";
    // An isolated catalog fixture exercises the same pinned snippet generator
    // and New Project backend as the UI. The actual project build verifies it.
    const auto registry = manager_state_directory() / L"integrations";
    write(registry / (id + ".name"), "raysan5/raylib @ 6.2 (raylib)");
    write(registry / (id + ".cmake"), github_dependency_cmake(repository, commit, hash, "raylib"));
    const auto project = root / L"Raylib Generated Project";
    create_managed_project(project, "multi", id);
    auto sized_source = read(project / L"src/main.cpp");
    sized_source.replace(sized_source.find("screen_width = 800"), 18, "screen_width = 1280");
    sized_source.replace(sized_source.find("screen_height = 600"), 19, "screen_height = 720");
    write(project / L"src/main.cpp", sized_source);
    write(project / L"assets/verification.txt", "asset preserved\n");
    formatting(project);
    for (const auto* configuration : {L"Debug", L"Release"})
    {
        const auto build = [&](bool incremental)
        {
            std::vector<std::wstring> arguments{
                L"manage-build", project.wstring(), L"--config", configuration};
            if (incremental)
            {
                arguments.push_back(L"--incremental");
            }
            const auto result = execute(cli, arguments, root);
            std::cout << result.second << std::flush;
            return result;
        };
        require(build(false).first == 0, "Actual generated Raylib project failed to build");
        const auto targets = executable_targets(project, configuration, {});
        require(targets.size() == 1 && fs::is_regular_file(targets.front().artifact),
            "File API did not discover the generated Raylib artifact");
        require(read(targets.front().artifact.parent_path() / L"assets/verification.txt") ==
                    "asset preserved\n",
            "Assets were not copied beside the real artifact");
        const auto stamp = fs::last_write_time(targets.front().artifact);
        const auto unchanged = build(true);
        require(unchanged.first == 0 && fs::last_write_time(targets.front().artifact) == stamp &&
                    unchanged.second.find("no work to do") != unchanged.second.npos,
            "No-change Raylib build recompiled or regenerated unnecessarily");
        ConsoleSession program;
        program.start_program(targets.front().artifact, {}, project, {});
        ProgramWindow window{program.process_id()};
        const auto started = GetTickCount64();
        while (!window.window && program.running() && GetTickCount64() - started < 15000)
        {
            program.poll_output();
            EnumWindows(find_program_window, reinterpret_cast<LPARAM>(&window));
            Sleep(10);
        }
        require(window.window != nullptr, "Generated Raylib graphical window did not open");
        SetWindowPos(
            window.window, nullptr, 120, 140, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        const auto remembered = capture_application_windows(program.active_process_ids());
        RECT client{};
        GetClientRect(window.window, &client);
        require(client.right == 1280 && client.bottom == 720 && !remembered.empty(),
            "First Raylib window did not use its 1280x720 program-defined client size");
        PostMessageW(window.window, WM_CLOSE, 0, 0);
        while (
            (program.running() || program.output_pending()) && GetTickCount64() - started < 20000)
        {
            program.poll_output();
            Sleep(5);
        }
        require(!program.running() && program.exit_code() == 0,
            "Generated Raylib window did not close normally");
        auto resized_source = sized_source;
        resized_source.replace(
            resized_source.find("screen_width = 1280"), 19, "screen_width = 1600");
        resized_source.replace(
            resized_source.find("screen_height = 720"), 19, "screen_height = 900");
        write(project / L"src/main.cpp", resized_source);
        require(build(true).first == 0, "Changed Raylib window dimensions did not rebuild");
        program.start_program(targets.front().artifact, {}, project, {});
        ProgramWindow resized{program.process_id()};
        const auto reopened = GetTickCount64();
        while (!resized.window && program.running() && GetTickCount64() - reopened < 15000)
        {
            program.poll_output();
            EnumWindows(find_program_window, reinterpret_cast<LPARAM>(&resized));
            Sleep(10);
        }
        require(resized.window != nullptr, "Resized Raylib window did not open");
        std::vector<HWND> restored;
        require(restore_application_windows(
                    program.active_process_ids(), remembered, restored, true, true, false) ==
                    remembered.size(),
            "Raylib position-only restoration failed");
        RECT bounds{};
        GetClientRect(resized.window, &client);
        GetWindowRect(resized.window, &bounds);
        MONITORINFO monitor{sizeof(monitor)};
        require(
            GetMonitorInfoW(MonitorFromWindow(resized.window, MONITOR_DEFAULTTONEAREST), &monitor),
            "Raylib window monitor unavailable");
        auto expected = remembered[0].bounds;
        expected.right = expected.left + bounds.right - bounds.left;
        expected.bottom = expected.top + bounds.bottom - bounds.top;
        expected = visible_window_bounds(expected, remembered[0].work_area, monitor.rcWork);
        if (client.right != 1600 || client.bottom != 900 || bounds.left != expected.left ||
            bounds.top != expected.top)
        {
            throw std::runtime_error(
                "Raylib position-only actual client " + std::to_string(client.right) + "x" +
                std::to_string(client.bottom) + ", position " + std::to_string(bounds.left) + "," +
                std::to_string(bounds.top) + "; expected client 1600x900, visible position " +
                std::to_string(expected.left) + "," + std::to_string(expected.top));
        }
        PostMessageW(resized.window, WM_CLOSE, 0, 0);
        while (
            (program.running() || program.output_pending()) && GetTickCount64() - reopened < 20000)
        {
            program.poll_output();
            Sleep(5);
        }
        require(!program.running() && program.exit_code() == 0,
            "Resized Raylib window did not close normally");
        write(project / L"src/main.cpp", sized_source);
        std::cout << "Verified real Raylib window opened and closed: "
                  << path_support::path_utf8(configuration) << '\n';
        ProjectWatcher watcher;
        watcher.start(project);
        const auto original = read(project / L"src/main.cpp");
        auto edited = original;
        edited.replace(edited.find("Hello from EasierCpp!"), 21, "Updated from EasierCpp!");
        write(project / L"src/main.cpp", edited);
        Sleep(30);
        watcher.poll();
        require(!watcher.consume_changes().empty(), "Raylib source edit was not detected");
        require(build(true).first == 0, "Edited Raylib source failed to rebuild");
        write(project / L"src/main.cpp", original + "\nintentional compilation error\n");
        const auto failed = build(true);
        require(failed.first != 0 && failed.second.find("Running:") == failed.second.npos,
            "Failed Raylib compilation reported success or ran an old executable");
        const auto diagnostics =
            parse_build_diagnostics(path_support::from_utf8(failed.second).wstring(), project);
        require(!diagnostics.empty() && diagnostics.front().file.filename() == L"main.cpp" &&
                    diagnostics.front().line > 0,
            "Real Raylib compiler error lacked source location");
        write(project / L"src/main.cpp", original);
        require(build(true).first == 0, "Repaired Raylib source failed to build");
    }
    std::cout << "Raylib 6.2 generated project: Debug/Release, assets, real window, incremental "
                 "no-work, watcher edit, error location and repair passed\n";
    const auto legacy = root / L"Legacy Raylib Project";
    create_managed_project(legacy, "empty");
    auto legacy_cmake = read(legacy / L"CMakeLists.txt");
    legacy_cmake.replace(legacy_cmake.find("LANGUAGES C CXX"), 15, "LANGUAGES CXX");
    auto legacy_dependency = github_dependency_cmake(repository, commit, hash, "raylib");
    const auto short_name = "cpplivelab_" + commit.substr(0, 12);
    size_t position = 0;
    while ((position = legacy_dependency.find(short_name, position)) != legacy_dependency.npos)
    {
        legacy_dependency.replace(position, short_name.size(), "cpplivelab_" + commit);
        position += commit.size() + 11;
    }
    legacy_cmake += legacy_dependency;
    write(legacy / L"CMakeLists.txt", legacy_cmake);
    const auto result = execute(cli, {L"manage-build", legacy.wstring()}, root);
    std::cout << result.second << std::flush;
    require(result.first == 0 && read(legacy / L"CMakeLists.txt") == legacy_cmake,
        "Legacy full-SHA/CXX-only generated project failed or was rewritten");
    std::cout
        << "Legacy Raylib full-SHA/CXX-only source remained unchanged and built successfully\n";
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    try
    {
        workspace_test::Fixture fixture;
        if (argc == 3 && std::wstring(argv[1]) == L"raylib")
        {
            raylib(fixture.root, argv[2]);
        }
        else if (argc == 2 && std::wstring(argv[1]) == L"projects")
        {
            projects(fixture.root);
        }
        else
        {
            unit(fixture.root);
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
