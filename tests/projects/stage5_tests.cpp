#include "cpplivelab/workspace.hpp"
#include "cpplivelab/session.hpp"
#include "projects/project_paths.hpp"
#include "tools/tool_workflows.hpp"
#include "../workspace_test_support.hpp"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <map>
#include <set>

namespace
{
namespace fs = std::filesystem;
using namespace cpplivelab;

void require(bool condition, const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void write(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    require(static_cast<bool>(output << text), "Fixture write failed");
}

std::string read(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {(std::istreambuf_iterator<char>(input)), {}};
}

struct Snapshot
{
    std::map<fs::path, std::string> files;
    std::set<fs::path> directories;
    bool operator==(const Snapshot&) const = default;
};

Snapshot snapshot(const fs::path& root)
{
    Snapshot result;
    for (const auto& item : fs::recursive_directory_iterator(root))
    {
        const auto relative = fs::relative(item.path(), root);
        if (item.is_directory())
        {
            result.directories.insert(relative);
        }
        else if (item.is_regular_file())
        {
            // Byte-for-byte comparison is stronger than a potentially colliding hash.
            result.files.emplace(relative, read(item.path()));
        }
    }
    return result;
}

template <class Action> void refuses(Action action, const char* message)
{
    bool refused = false;
    try
    {
        action();
    }
    catch (const std::exception&)
    {
        refused = true;
    }
    require(refused, message);
}

struct Result
{
    int code;
    std::string output;
};

Result execute(
    const fs::path& cli, const std::vector<std::wstring>& arguments, const fs::path& directory)
{
    ConsoleSession session;
    session.start_worker(cli, arguments, directory);
    std::string output;
    const auto start = GetTickCount64();
    while (session.running() || session.output_pending())
    {
        output += session.poll();
        if (session.root_exited())
        {
            session.finish_build();
        }
        require(GetTickCount64() - start < 180000, "Stage 5 child timed out: " + output);
        Sleep(5);
    }
    output += session.poll();
    return {session.exit_code(), output};
}

void unit(const fs::path& parent, const fs::path& fixture)
{
    const auto metadata_project = parent / L"Target metadata project";
    fs::create_directory(metadata_project);
    const auto manifest =
        managed_build_directory(metadata_project, L"Debug", {}) / L"executables.txt";
    require(executable_targets(metadata_project, L"Debug", {}).empty(),
        "A fresh project should have no discovered targets");
    write(manifest, "");
    require(executable_targets(metadata_project, L"Debug", {}).empty(),
        "A configured library-only project should allow an empty target manifest");
    for (const auto* malformed :
        {"orphan\n", "\nmissing-name.exe\n", "app\n\n", "first\nfirst.exe\norphan\n"})
    {
        write(manifest, malformed);
        bool diagnosed = false;
        try
        {
            executable_targets(metadata_project, L"Debug", {});
        }
        catch (const std::exception& error)
        {
            const std::string message = error.what();
            diagnosed = message.find("Target metadata") != message.npos &&
                        message.find("executables.txt") != message.npos;
        }
        require(diagnosed,
            "Malformed target metadata was silently accepted or lacked its file diagnostic");
    }
    write(manifest, "first\nC:/example/first.exe\nsecond\nC:/example/second.exe\n");
    const auto parsed_targets = executable_targets(metadata_project, L"Debug", {});
    require(parsed_targets.size() == 2 && parsed_targets[1].name == L"second",
        "Valid multi-target metadata was rejected after a corrupt manifest");
    const auto transaction = parent / L"Approved transaction";
    const std::string original = "cmake_minimum_required(VERSION 3.24)\nproject(Transaction)\n";
    write(transaction / L"CMakeLists.txt", original);
    write(transaction / L"assets/preserve.txt", "untouched");
    const auto before_transaction = snapshot(transaction);
    require(!apply_approved_dependencies(transaction,
                original,
                "# controlled failure\n",
                []
                {
                    return false;
                }) &&
                snapshot(transaction) == before_transaction,
        "Failed verification changed existing project bytes");
    refuses(
        [&]
        {
            apply_approved_dependencies(transaction,
                original,
                "# controlled exception\n",
                []() -> bool
                {
                    throw std::runtime_error("fixture");
                });
        },
        "Thrown verification was ignored");
    require(snapshot(transaction) == before_transaction,
        "Thrown verification did not roll back original bytes");
    refuses(
        [&]
        {
            apply_approved_dependencies(transaction,
                "stale",
                "# addition",
                []
                {
                    return true;
                });
        },
        "Stale preview overwrote a changed CMake file");
    require(apply_approved_dependencies(transaction,
                original,
                "# approved fixture\n",
                []
                {
                    return true;
                }) &&
                read(transaction / L"assets/preserve.txt") == "untouched",
        "Approved transaction failed or changed assets");
    refuses(
        [&]
        {
            apply_approved_dependencies(transaction,
                read(transaction / L"CMakeLists.txt"),
                "# duplicate",
                []
                {
                    return true;
                });
        },
        "Duplicate dependency block accepted");
    const auto locked_build = parent / L"Locked runner build";
    fs::create_directory(locked_build);
    windows_support::UniqueHandle lease;
    lease.reset(CreateFileW((locked_build / L"cpplivelab-tests.lock").c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        nullptr));
    require(lease.value != INVALID_HANDLE_VALUE, "Could not create runner overlap fixture lock");
    wchar_t system[MAX_PATH]{};
    GetSystemDirectoryW(system, MAX_PATH);
    const auto overlap = execute(fs::path(system) / L"WindowsPowerShell/v1.0/powershell.exe",
        {L"-NoProfile",
            L"-ExecutionPolicy",
            L"Bypass",
            L"-File",
            (source_repository() / L"scripts/build.ps1").wstring(),
            L"-BuildDirectory",
            locked_build.wstring(),
            L"-TestMode",
            L"Quick",
            L"-Jobs",
            L"4"},
        parent);
    require(overlap.code != 0 && !fs::exists(locked_build / L"CMakeCache.txt"),
        "Overlapping test script ignored its lease and began configuring/building");
    const auto child_flag = GetEnvironmentVariableW(L"CPPLIVELAB_TEST_CHILD", nullptr, 0);
    if (child_flag)
    {
        refuses(
            []
            {
                test_command(L"Quick", 4);
            },
            "Recursive suite command accepted inside CTest");
    }
    refuses(
        []
        {
            test_command(L"Quick", 0);
        },
        "Zero worker count accepted");
    const auto project = parent / L"Custom \u8a08\u7b97 project";
    write(project / L"CMakeLists.txt",
        "project(Existing)\nfind_package(Eigen3)\n"
        "file(WRITE \"${CMAKE_CURRENT_SOURCE_DIR}/must-not-exist\" \"bad\")\n");
    write(project / L"deep/custom/game.cpp", "int main() {}\n");
    write(project / L"Assets/example.dat", "ASSET");
    write(project / L"CMakePresets.json", "{\"version\":3}");
    write(project / L"CMakeUserPresets.json", "{\"version\":3}");
    const auto before = snapshot(project);
    const auto inspected = inspect_project(project);
    require(inspected.sources.size() == 1 && !inspected.assets.empty() && inspected.presets &&
                inspected.user_presets && inspected.configure_writes &&
                !inspected.dependency_evidence.empty(),
        "Read-only inspection lost custom layout, dependency, preset or write-warning information");
    require(before == snapshot(project), "Inspection modified project contents or directories");
    ProjectOptions options;
    options.target = L"game";
    options.preset = L"my-preset";
    options.working_directory = project / L"Assets";
    options.allow_configure_writes = true;
    save_project_options(project, L"Debug", options);
    const auto restored = load_project_options(project, L"Debug");
    require(restored.target == options.target && restored.preset == options.preset &&
                restored.working_directory == options.working_directory &&
                !restored.allow_configure_writes,
        "Settings round trip failed or persisted configure-write authorization");
    require(load_project_options(project, L"Release").target.empty(),
        "Debug settings leaked into Release");
    auto differently_cased = options;
    differently_cased.preset = L"MY-PRESET";
    require(managed_build_directory(project, L"Debug", options) !=
                managed_build_directory(project, L"Debug", differently_cased),
        "Case-sensitive CMake preset names shared a build cache");
    require(before == snapshot(project), "Settings modified an imported project");
    remember_project(project);
    remember_project(project);
    require(recent_projects().size() == 1, "Recent list duplicated a project");
    remember_project(project, true);
    require(recent_projects().empty() && before == snapshot(project),
        "Forget recent deleted project data");
    refuses(
        [&]
        {
            managed_build(project, L"Debug", {}, false, false);
        },
        "Configure-time source writes were not gated before execution");
    require(before == snapshot(project), "Rejected configuration damaged the source");
    write(parent / L"Unsupported/game.sln", "unsupported");
    refuses(
        [&]
        {
            inspect_project(parent / L"Unsupported");
        },
        "Non-CMake project accepted");
    for (const auto* invalid : {L"CON", L"bad;name", L"bad.", L"LPT1"})
    {
        refuses(
            [&]
            {
                create_managed_project(parent / invalid, "empty");
            },
            "Invalid project name accepted");
    }
    create_managed_project(parent / L"Empty", "empty");
    create_managed_project(parent / L"Multi", "multi");
    require(fs::is_regular_file(parent / L"Multi/include/greeting.hpp") &&
                fs::is_regular_file(parent / L"Multi/src/greeting.cpp"),
        "Multi-file template omitted files");
    const auto generated = snapshot(parent / L"Empty");
    refuses(
        [&]
        {
            create_managed_project(parent / L"Empty", "empty");
        },
        "Creation overwrote existing files");
    require(generated == snapshot(parent / L"Empty"), "Rejected creation changed destination");
    ProjectOptions invalid_vcpkg;
    invalid_vcpkg.vcpkg = parent / L"missing vcpkg";
    refuses(
        [&]
        {
            managed_build(parent / L"Empty", L"Debug", invalid_vcpkg, false, false);
        },
        "Invalid vcpkg root accepted");
    refuses(
        [&]
        {
            create_managed_project(
                parent / L"Invalid library", "empty", "https://invalid.invalid/lib.git");
        },
        "Unvalidated Git dependency accepted");
    require(!fs::exists(parent / L"Invalid library"),
        "Failed library validation left partial creation");
    EditorOptions editor{L"Custom",
        fixture,
        {L"capture-editor", (parent / L"editor arguments.txt").wstring(), L"{project}"}};
    save_editor(editor);
    const auto loaded = load_editor();
    require(loaded.choice == editor.choice && loaded.arguments == editor.arguments &&
                loaded.executable == fixture,
        "Editor preference persistence lost arguments");
    launch_editor(loaded, project);
    const auto start = GetTickCount64();
    while (!fs::is_regular_file(parent / L"editor arguments.txt") ||
           read(parent / L"editor arguments.txt").empty())
    {
        require(GetTickCount64() - start < 5000, "Custom editor fixture did not launch");
        Sleep(10);
    }
    require(read(parent / L"editor arguments.txt") == path_support::path_utf8(project),
        "Editor corrupted Unicode/space arguments");
    editor.executable = parent / L"missing editor.exe";
    refuses(
        [&]
        {
            launch_editor(editor, project);
        },
        "Missing custom editor did not report failure");
    require(before == snapshot(project), "Editor launching modified project files");
}

void projects(const fs::path& parent, const fs::path& cli)
{
    const auto project = parent / L"Custom folder \u8a08\u7b97";
    write(project / L"CMakeLists.txt",
        "cmake_minimum_required(VERSION 3.24)\n"
        "project(Custom LANGUAGES CXX)\nset(CMAKE_CXX_STANDARD 20)\n"
        "add_executable(game odd/entry.cpp)\nadd_executable(tool utility.cpp)\n"
        "set_target_properties(game PROPERTIES OUTPUT_NAME actual_game RUNTIME_OUTPUT_DIRECTORY "
        "${CMAKE_BINARY_DIR}/bin)\n");
    write(project / L"odd/entry.cpp",
        "#include <fstream>\n#include <iostream>\nint main()\n{\n"
        "    std::ifstream asset(\"Assets/value.txt\");\n    std::string value;\n"
        "    asset >> value;\n    std::cout << \"GAME \" << value;\n    return value == "
        "\"original\" ? 0 : 9;\n}\n");
    write(project / L"utility.cpp", "#include <iostream>\nint main() { std::cout << \"TOOL\"; }\n");
    write(project / L"Assets/value.txt", "original");
    write(project / L"existing-build/sentinel.txt", "DO NOT OVERWRITE");
    const auto before = snapshot(project);
    auto result = execute(cli, {L"manage-build", project.wstring(), L"--target", L"game"}, parent);
    require(result.code == 0, "Custom-layout target build failed: " + result.output);
    auto targets = executable_targets(project, L"Debug", {});
    require(targets.size() == 2, "CMake File API did not expose both executable targets");
    const auto game = std::find_if(targets.begin(),
        targets.end(),
        [](const auto& target)
        {
            return target.name == L"game";
        });
    const auto tool = std::find_if(targets.begin(),
        targets.end(),
        [](const auto& target)
        {
            return target.name == L"tool";
        });
    require(game != targets.end() && tool != targets.end() && fs::is_regular_file(game->artifact) &&
                !fs::exists(tool->artifact) && game->artifact.filename() == L"actual_game.exe",
        "Wrong target/artifact was built");
    const auto debug_artifact = game->artifact;
    const auto debug_written = fs::last_write_time(debug_artifact);
    const auto debug_directory = managed_build_directory(project, L"Debug", {});
    const auto original_manifest = read(debug_directory / L"executables.txt");
    const auto original_libraries = read(debug_directory / L"libraries.txt");
    const auto original_runtime = read(debug_directory / L"runtime-paths.txt");
    const auto metadata_written = fs::last_write_time(debug_directory / L"executables.txt");
    result = execute(
        cli, {L"manage-build", project.wstring(), L"--target", L"game", L"--incremental"}, parent);
    require(result.code == 0 &&
                fs::last_write_time(debug_directory / L"executables.txt") == metadata_written,
        "Unchanged File API model unnecessarily rewrote target metadata: " + result.output);
    for (const int damage : {0, 1, 2, 3, 4, 5, 6, 7})
    {
        if (damage == 0)
        {
            fs::remove(debug_directory / L"generator.txt");
        }
        else if (damage == 1)
        {
            write(debug_directory / L"executables.txt", "incomplete target\n");
        }
        else if (damage == 2 || damage == 3)
        {
            // Repair valid-looking damaged metadata before path checks, with
            // either an old timestamp (2) or a freshly written timestamp (3).
            write(debug_directory / L"executables.txt",
                "game\n" + path_support::path_utf8(parent / L"outside.exe") + "\n");
            if (damage == 2)
            {
                fs::last_write_time(debug_directory / L"executables.txt",
                    fs::last_write_time(debug_directory / L"CMakeCache.txt") -
                        std::chrono::hours(24));
            }
        }
        else if (damage == 4)
        {
            fs::remove(debug_directory / L"libraries.txt");
        }
        else if (damage == 5)
        {
            write(debug_directory / L"runtime-paths.txt", "incorrect runtime directory");
        }
        else if (damage == 6)
        {
            fs::remove(debug_directory / L"file-api.snapshot");
        }
        else
        {
            write(debug_directory / L"file-api.snapshot", "1\nDebug\ntruncated\n");
        }
        result = execute(cli,
            {L"manage-build", project.wstring(), L"--target", L"game", L"--incremental"},
            parent);
        require(result.code == 0 &&
                    read(debug_directory / L"executables.txt") == original_manifest &&
                    read(debug_directory / L"libraries.txt") == original_libraries &&
                    read(debug_directory / L"runtime-paths.txt") == original_runtime &&
                    fs::last_write_time(debug_artifact) == debug_written &&
                    !fs::exists(parent / L"outside.exe"),
            "Incremental metadata recovery failed, rebuilt unchanged code or used a stale "
            "artifact: " +
                std::to_string(damage) + ": " + result.output);
    }
    result = execute(
        cli, {L"manage-run", project.wstring(), L"--target", L"game", L"--incremental"}, parent);
    require(result.code == 0 && result.output.find("GAME original") != result.output.npos,
        "Asset working directory or selected target Run failed: " + result.output);
    result = execute(cli,
        {L"manage-run", project.wstring(), L"--target", L"tool", L"--config", L"Release"},
        parent);
    require(result.code == 0 && result.output.find("TOOL") != result.output.npos &&
                fs::last_write_time(debug_artifact) == debug_written,
        "Release target contaminated Debug: " + result.output);
    result = execute(cli, {L"manage-run", project.wstring(), L"--target", L"not-present"}, parent);
    require(result.code != 0 && result.output.find("not present") != result.output.npos,
        "Invalid target ran a stale executable");
    result = execute(cli, {L"manage-run", project.wstring()}, parent);
    require(result.code != 0 && result.output.find("multiple programs") != result.output.npos,
        "Ambiguous target was guessed");
    require(before == snapshot(project),
        "Import/build/run changed project files, assets, presets or directory organization");

    // Regeneration during Ninja must refresh both executable and DLL lookup data.
    const auto regeneration = parent / L"Metadata regeneration";
    const std::string cmake =
        "cmake_minimum_required(VERSION 3.24)\nproject(Regeneration LANGUAGES CXX)\n"
        "add_library(helper SHARED helper.cpp)\nadd_executable(app main.cpp)\n"
        "target_link_libraries(app PRIVATE helper)\n";
    write(regeneration / L"helper.cpp", "__declspec(dllexport) int value() { return 11; }\n");
    write(regeneration / L"main.cpp",
        "__declspec(dllimport) int value();\nint main() { return value() == 11 ? 0 : 1; }\n");
    write(regeneration / L"CMakeLists.txt", cmake);
    result = execute(cli, {L"manage-run", regeneration.wstring(), L"--target", L"app"}, parent);
    require(result.code == 0, "Initial local DLL consumer failed: " + result.output);
    const auto regenerated_directory = managed_build_directory(regeneration, L"Debug", {});
    const auto previous_snapshot = read(regenerated_directory / L"file-api.snapshot");
    write(regeneration / L"CMakeLists.txt",
        cmake + "set_target_properties(app PROPERTIES OUTPUT_NAME renamed)\n"
                "set_target_properties(helper PROPERTIES RUNTIME_OUTPUT_DIRECTORY "
                "\"${CMAKE_BINARY_DIR}/dlls\")\n");
    result = execute(cli,
        {L"manage-run", regeneration.wstring(), L"--target", L"app", L"--incremental"},
        parent);
    const auto regenerated_targets = executable_targets(regeneration, L"Debug", {});
    require(
        result.code == 0 && regenerated_targets.size() == 1 &&
            regenerated_targets[0].artifact.filename() == L"renamed.exe" &&
            read(regenerated_directory / L"file-api.snapshot") != previous_snapshot &&
            read(regenerated_directory / L"runtime-paths.txt").find("dlls") != std::string::npos,
        "Build-time CMake regeneration reused stale executable/DLL metadata: " + result.output);
    const auto regenerated_manifest =
        fs::last_write_time(regenerated_directory / L"executables.txt");
    result = execute(cli,
        {L"manage-run", regeneration.wstring(), L"--target", L"app", L"--incremental"},
        parent);
    require(result.code == 0 && fs::last_write_time(regenerated_directory / L"executables.txt") ==
                                    regenerated_manifest,
        "Cached graph lost the DLL runtime path or unnecessarily refreshed: " + result.output);
    const auto broken = parent / L"Unreachable dependency";
    write(broken / L"CMakeLists.txt",
        "cmake_minimum_required(VERSION 3.24)\n"
        "project(Broken LANGUAGES CXX)\n"
        "file(DOWNLOAD https://127.0.0.1:9/missing.zip ${CMAKE_BINARY_DIR}/dependency.zip TIMEOUT "
        "2 STATUS result)\n"
        "list(GET result 0 code)\nif(NOT code EQUAL 0)\n"
        " message(FATAL_ERROR \"Dependency unavailable: ${result}\")\nendif()\n");
    const auto broken_before = snapshot(broken);
    result = execute(cli, {L"manage-run", broken.wstring()}, parent);
    require(result.code != 0 && result.output.find("Running:") == result.output.npos &&
                broken_before == snapshot(broken),
        "Failed dependency setup modified source or launched a program");
}

void presets(const fs::path& parent, const fs::path& cli)
{
    struct RestorePath
    {
        std::wstring value;

        ~RestorePath()
        {
            SetEnvironmentVariableW(L"PATH", value.c_str());
        }
    } restore;

    wchar_t previous[32768]{}, windows[32768]{};
    GetEnvironmentVariableW(L"PATH", previous, 32768);
    restore.value = previous;
    GetEnvironmentVariableW(L"SystemRoot", windows, 32768);
    SetEnvironmentVariableW(L"PATH", (fs::path(windows) / L"System32").c_str());
    // Exercise discovery from an ordinary terminal, not the SDK/Ninja PATH
    // inherited from scripts/build.ps1. Restore the test process afterwards.
    const auto project = parent / L"Preset Project";
    write(project / L"CMakeLists.txt",
        "cmake_minimum_required(VERSION 3.24)\nproject(Preset LANGUAGES CXX)\n"
        "if(NOT REQUIRED_OPTION STREQUAL \"preserved\")\n message(FATAL_ERROR \"Preset options "
        "lost\")\nendif()\n"
        "add_executable(chosen main.cpp)\n");
    write(
        project / L"main.cpp", "#include <iostream>\nint main() { std::cout << \"PRESET OK\"; }\n");
    write(project / L"CMakePresets.json", R"json({"version":3,"configurePresets":[
{"name":"base","hidden":true,"cacheVariables":{"REQUIRED_OPTION":"preserved"}},
{"name":"ninja","inherits":"base","generator":"Ninja","binaryDir":"${sourceDir}/do-not-touch"},
{"name":"multi","inherits":"base","generator":"Ninja Multi-Config","binaryDir":"${sourceDir}/multi-do-not-touch"},
{"name":"vs2022","inherits":"base","generator":"Visual Studio 17 2022","architecture":"x64","binaryDir":"${sourceDir}/also-do-not-touch"}]})json");
    const auto before = snapshot(project);
    auto result = execute(cli, {L"manage-build", project.wstring()}, parent);
    require(result.code != 0 &&
                result.output.find("Select an explicit configure preset") != result.output.npos,
        "Project presets were silently ignored");
    result = execute(cli,
        {L"manage-run", project.wstring(), L"--preset", L"ninja", L"--target", L"chosen"},
        parent);
    require(result.code == 0 && result.output.find("PRESET OK") != result.output.npos,
        "Ninja preset was not respected: " + result.output);
    result = execute(cli,
        {L"manage-run",
            project.wstring(),
            L"--preset",
            L"vs2022",
            L"--target",
            L"chosen",
            L"--config",
            L"Release"},
        parent);
    require(result.code == 0 && result.output.find("PRESET OK") != result.output.npos,
        "VS2022 multi-config preset failed: " + result.output);
    result = execute(cli,
        {L"manage-run",
            project.wstring(),
            L"--preset",
            L"multi",
            L"--target",
            L"chosen",
            L"--config",
            L"Release"},
        parent);
    require(result.code == 0 && result.output.find("PRESET OK") != result.output.npos,
        "Ninja Multi-Config preset failed without a developer terminal: " + result.output);
    require(before == snapshot(project),
        "Preset build overwrote original preset or binary directories");
}

void libraries(const fs::path& parent, const fs::path& cli)
{
    const auto project = parent / L"fmt project";
    create_managed_project(project, "multi", "fmt");
    const auto before = snapshot(project);
    const auto result =
        execute(cli, {L"manage-run", project.wstring(), L"--target", L"app"}, parent);
    require(
        result.code == 0 && result.output.find("fmt dependency verified: 42") != result.output.npos,
        "Pinned real fmt integration failed: " + result.output);
    require(!inspect_project(project).dependency_evidence.empty(),
        "Configured dependency was not discoverable");
    require(read(managed_build_directory(project, L"Debug", {}) / L"libraries.txt").find("fmt") !=
                std::string::npos,
        "CMake File API did not report the built fmt library target");
    require(before == snapshot(project), "Library setup wrote back into generated source");
    const auto combined = parent / L"Two optional libraries";
    create_managed_project(combined, "multi", "fmt,nlohmann_json");
    const auto combined_before = snapshot(combined);
    const auto combined_run =
        execute(cli, {L"manage-run", combined.wstring(), L"--target", L"app"}, parent);
    require(combined_run.code == 0 &&
                combined_run.output.find("fmt dependency verified:") != combined_run.output.npos &&
                combined_run.output.find("answer") != combined_run.output.npos &&
                combined_run.output.find("42") != combined_run.output.npos,
        "Two selected libraries did not compile/link/run together: " + combined_run.output);
    require(combined_before == snapshot(combined),
        "Multi-library build changed project-controlled files");
    refuses(
        [&]
        {
            create_managed_project(parent / L"Duplicate libraries", "empty", "fmt,fmt");
        },
        "Duplicate library selection was accepted");
    const auto empty = parent / L"empty build";
    create_managed_project(empty, "empty");
    auto run = execute(cli, {L"manage-run", empty.wstring()}, parent);
    require(run.code == 0 && run.output.find("Hello") != run.output.npos,
        "Empty template did not build/run: " + run.output);
    const auto multi = parent / L"multi build";
    create_managed_project(multi, "multi");
    run = execute(cli, {L"manage-run", multi.wstring()}, parent);
    require(run.code == 0 && run.output.find("two C++ files") != run.output.npos,
        "Multi-file template did not build/run: " + run.output);
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    try
    {
        require(argc == 4, "Usage: stage5_tests <unit|projects|presets|libraries> <cli> <fixture>");
        workspace_test::Fixture fixture;
        const std::wstring mode = argv[1];
        if (mode == L"unit")
        {
            unit(fixture.root, argv[3]);
        }
        else if (mode == L"projects")
        {
            projects(fixture.root, argv[2]);
        }
        else if (mode == L"presets")
        {
            presets(fixture.root, argv[2]);
        }
        else if (mode == L"libraries")
        {
            libraries(fixture.root, argv[2]);
        }
        else
        {
            throw std::runtime_error("Unknown Stage 5 test mode");
        }
        std::cout << "Stage 5 fixture checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Stage 5 failure: " << error.what() << '\n';
        return 1;
    }
}
