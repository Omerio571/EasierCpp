#include "process/application_windows.hpp"
#include "cpplivelab/libraries.hpp"
#include "cpplivelab/session.hpp"
#include "cpplivelab/process.hpp"
#include "build/build_workers.hpp"
#include "../workspace_test_support.hpp"
#include <fstream>
#include <iostream>

namespace
{
namespace fs = std::filesystem;
using namespace cpplivelab;

void require(bool condition, const char* reason)
{
    if (!condition)
    {
        throw std::runtime_error(reason);
    }
}

void write(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    require(static_cast<bool>(std::ofstream(path) << text), "Cannot write disposable fixture");
}

void unit(const fs::path& root)
{
    require(load_build_workers().count == 0, "Missing worker preference changed native defaults");
    initialize_build_workers();
    require(load_build_workers().count && load_build_workers().count <= maximum_build_workers(),
        "Fresh configuration did not select safe automatic workers");
    fs::remove(manager_state_directory() / L"build-workers.settings");
    fs::create_directories(manager_state_directory() / L"projects");
    initialize_build_workers();
    require(load_build_workers().count == 0,
        "Legacy workspace default was changed before manual selection");
    require(build_worker_limit(64, 64ull << 30) == 64 &&
                build_worker_limit(128, 256ull << 30) == 128 &&
                build_worker_limit(64, 2ull << 30) == 1 && build_worker_limit(0, 0) == 1,
        "Compilation worker limits hardcode 16 or ignore CPU/memory safety");
    require(worker_benchmark_units(1) == 12 && worker_benchmark_units(24) == 48 &&
                worker_benchmark_units(32) == 64 && worker_benchmark_units(128) == 64,
        "Synthetic workload does not support high-core candidates or lacks a work bound");
    std::vector<std::wstring> worker_arguments{L"--build", L"fixture"};
    append_build_workers(worker_arguments, {});
    require(worker_arguments.size() == 2, "Legacy worker preference overrides native jobs");
    save_build_workers({1, 0});
    require(load_build_workers().count == 1, "Compilation workers did not persist");
    append_build_workers(worker_arguments, load_build_workers());
    require(worker_arguments[2] == L"--parallel" && worker_arguments[3] == L"1",
        "One worker was not passed to CMake");
    const auto multiple = (std::min)(2u, maximum_build_workers());
    save_build_workers({multiple, multiple});
    worker_arguments.clear();
    append_build_workers(worker_arguments, load_build_workers());
    require(worker_arguments == std::vector<std::wstring>{L"--parallel", std::to_wstring(multiple)},
        "Multiple workers not passed to CMake");
    const RECT old_work{-1920, -1080, 0, 0};
    const RECT new_work{0, 0, 1280, 720};
    const auto moved = visible_window_bounds({-960, -1080, 0, 0}, old_work, new_work);
    require(moved.left == 320 && moved.top == 0 && moved.right == 1280 && moved.bottom == 720,
        "Disconnected upper monitor fallback did not preserve size and visible bounds");
    const auto same = visible_window_bounds({-800, -900, -200, -400}, old_work, old_work);
    require(same.left == -800 && same.top == -900 && same.right == -200,
        "Negative upper-monitor coordinates were corrupted");
    AutoRunPreferences settings{true, true, false, 1234};
    save_auto_run_preferences(root, settings);
    const auto loaded = load_auto_run_preferences(root);
    require(loaded.placement_memory == PlacementMemory::Persistent &&
                loaded.replacement_mode == ReplacementMode::Safe,
        "New preference defaults or round trip failed");
    write(workspace_directory(root / L"legacy") / L"auto-run.settings", "0 1 0 3000\n");
    require(load_auto_run_preferences(root / L"legacy").placement_memory ==
                    PlacementMemory::Persistent &&
                load_auto_run_preferences(root / L"legacy").keep_window_size,
        "Legacy window restoration preference was not migrated");
    settings.placement_memory = PlacementMemory::Off;
    settings.replacement_mode = ReplacementMode::Seamless;
    save_auto_run_preferences(root / L"modes", settings);
    const auto modes = load_auto_run_preferences(root / L"modes");
    require(!modes.restore_windows && modes.replacement_mode == ReplacementMode::Seamless,
        "Independent placement/replacement preferences did not persist");
    for (const auto placement :
        {PlacementMemory::Off, PlacementMemory::Session, PlacementMemory::Persistent})
    {
        for (const auto replacement : {ReplacementMode::Safe, ReplacementMode::Seamless})
        {
            AutoRunPreferences combination;
            combination.placement_memory = placement;
            combination.restore_windows = placement != PlacementMemory::Off;
            combination.replacement_mode = replacement;
            save_auto_run_preferences(root / L"combinations", combination);
            const auto round_trip = load_auto_run_preferences(root / L"combinations");
            require(round_trip.placement_memory == (placement == PlacementMemory::Off
                                                           ? PlacementMemory::Off
                                                           : PlacementMemory::Persistent) &&
                        round_trip.replacement_mode == replacement,
                "Placement/replacement preference combination changed during persistence");
        }
    }
    const auto defaults = load_auto_run_preferences(root / L"new-window-options");
    require(defaults.keep_window_position && !defaults.keep_window_size,
        "New projects must default to position ON and size OFF");
    for (const bool position : {false, true})
    {
        for (const bool size : {false, true})
        {
            AutoRunPreferences independent;
            independent.keep_window_position = position;
            independent.keep_window_size = size;
            save_auto_run_preferences(root / L"independent", independent);
            const auto round_trip = load_auto_run_preferences(root / L"independent");
            require(round_trip.keep_window_position == position &&
                        round_trip.keep_window_size == size &&
                        round_trip.restore_windows == (position || size),
                "Independent geometry options did not persist");
        }
    }
    write(workspace_directory(root / L"old-off") / L"auto-run.settings", "0 0 0 3000 0 1\n");
    const auto old_off = load_auto_run_preferences(root / L"old-off");
    require(!old_off.keep_window_position && !old_off.keep_window_size,
        "Legacy restoration OFF must migrate to both OFF");
    write(workspace_directory(root / L"damaged-geometry") / L"auto-run.settings",
        "0 1 0 3000 2 0 geometry-v1 1 broken\n");
    const auto damaged_geometry = load_auto_run_preferences(root / L"damaged-geometry");
    require(!damaged_geometry.keep_window_position && !damaged_geometry.keep_window_size,
        "Corrupt geometry options enabled unintended restoration");
    ApplicationWindow remembered;
    remembered.class_name = L"Memory fixture";
    remembered.title = L"Unicode \u03bb";
    remembered.monitor = L"Disconnected monitor";
    remembered.bounds = {-800, -900, -200, -400};
    remembered.placement.rcNormalPosition = remembered.bounds;
    remembered.placement.showCmd = SW_SHOWMAXIMIZED;
    remembered.work_area = old_work;
    remembered.monitor_area = old_work;
    WindowPlacementMemory memory;
    memory.remember(root, L"Debug", L"app", PlacementMemory::Session, {remembered}, true);
    require(memory.recall(root, L"Debug", L"app", PlacementMemory::Session).size() == 1 &&
                WindowPlacementMemory{}
                    .recall(root, L"Debug", L"app", PlacementMemory::Persistent)
                    .empty(),
        "Session-only placement was lost or written to disk");
    memory.remember(root, L"Debug", L"app", PlacementMemory::Persistent, {remembered}, true);
    WindowPlacementMemory reopened;
    auto saved = reopened.recall(root, L"Debug", L"app", PlacementMemory::Persistent);
    require(saved.size() == 1 && saved[0].title == remembered.title &&
                saved[0].placement.showCmd == SW_SHOWMAXIMIZED &&
                EqualRect(&saved[0].bounds, &remembered.bounds),
        "Persistent Unicode/maximized/negative-coordinate placement did not round trip");
    require(reopened.recall(root, L"Release", L"app", PlacementMemory::Persistent).empty() &&
                reopened.recall(root, L"Debug", L"other", PlacementMemory::Persistent).empty() &&
                reopened.recall(root / L"other", L"Debug", L"app", PlacementMemory::Persistent)
                    .empty() &&
                reopened.recall(root, L"Debug", L"app", PlacementMemory::Off).empty(),
        "Placement leaked across configuration, target, project or Off mode");
    auto minimized = remembered;
    minimized.minimized = true;
    minimized.bounds = {1, 1, 300, 200};
    memory.remember(root, L"Debug", L"app", PlacementMemory::Persistent, {minimized}, true);
    memory.remember(root, L"Debug", L"app", PlacementMemory::Persistent, {}, true);
    require(EqualRect(&memory.recall(root, L"Debug", L"app", PlacementMemory::Persistent)[0].bounds,
                &remembered.bounds),
        "Minimized/destroyed capture overwrote valid placement");
    const auto placement_folder = workspace_directory(root) / L"window-placement";
    for (const auto& file : fs::directory_iterator(placement_folder))
    {
        write(file.path(), std::string(65538, 'x'));
    }
    require(
        WindowPlacementMemory{}.recall(root, L"Debug", L"app", PlacementMemory::Persistent).empty(),
        "Oversized/corrupt placement record was accepted");
    require(loaded.enabled && loaded.restore_windows && !loaded.force_stop &&
                loaded.shutdown_ms == 1234,
        "Project Auto Run preferences did not round-trip");
    require(!load_auto_run_preferences(root / L"other").enabled,
        "Auto Run preference leaked across projects");
    const auto migrated = root / L"migrated";
    require(load_auto_run_preferences(migrated, true).enabled,
        "Legacy Auto Build ON was not available for first project migration");
    save_auto_run_preferences(migrated, {false, true, false, 3000});
    require(!load_auto_run_preferences(migrated, true).enabled,
        "Legacy global ON overrode an explicit project OFF");
    write(workspace_directory(migrated) / L"auto-run.settings", "damaged settings\n");
    require(!load_auto_run_preferences(migrated, true).enabled,
        "Damaged settings silently armed automatic launch through legacy fallback");
    LibraryRecord record;
    record.id = "local-test";
    record.name = "Fixture package";
    record.integration = "package";
    record.package = "Fixture";
    record.version = "1.2";
    record.targets = {"Fixture::Core", "Fixture::Extra"};
    const auto snippet = library_snippet(record, "consumer");
    require(
        snippet.find("find_package(Fixture 1.2 CONFIG REQUIRED)") != snippet.npos &&
            snippet.find("target_link_libraries(consumer PRIVATE Fixture::Extra)") != snippet.npos,
        "Package integration lost selected version/targets");
    remember_project_libraries(root, {record});
    const auto records = project_libraries(root);
    require(records.size() == 1 && records[0].integration == "package" &&
                records[0].package == "Fixture",
        "New integration schema lost project metadata");
    record.package = "Fixture)\nmessage(FATAL_ERROR injected)";
    bool rejected = false;
    try
    {
        library_snippet(record, "consumer");
    }
    catch (const std::exception&)
    {
        rejected = true;
    }
    require(rejected, "Executable CMake text accepted as package metadata");
    std::cout
        << "PASS geometry, project settings, multi-target packages, schema, injection rejection\n";
}

void libraries(const fs::path& root)
{
    const auto project = root / L"project";
    const auto headers = root / L"headers";
    const auto library = root / L"library";
    write(headers / L"value.hpp", "#pragma once\ninline int answer() { return 42; }\n");
    write(library / L"CMakeLists.txt",
        "cmake_minimum_required(VERSION 3.20)\nadd_library(LocalCore INTERFACE)\n");
    write(project / L"main.cpp", "int main() { return 0; }\n");
    write(project / L"CMakeLists.txt",
        "cmake_minimum_required(VERSION 3.20)\nproject(Fixture LANGUAGES CXX)\nadd_executable(app "
        "main.cpp)\ntarget_compile_features(app PRIVATE cxx_std_20)\n# Preserve this manual "
        "comment\n");
    LibraryRecord source;
    source.id = "local-source";
    source.name = "Local source";
    source.integration = "source";
    source.local_path = library.generic_string();
    source.targets = {"LocalCore"};
    LibraryRecord header;
    header.id = "local-headers";
    header.name = "Local headers";
    header.integration = "headers";
    header.local_path = headers.generic_string();
    const auto verify = [&]
    {
        ProjectOptions options;
        std::ifstream(workspace_directory(project) / L"library-transaction/pending") >>
            options.library_transaction;
        options.target = L"app";
        return managed_build(project, L"Debug", options, false, false) == 0;
    };
    require(apply_library_change(preview_library_change(project, {source}, "app"), true, verify),
        "First local library failed");
    // Emulate the exact older owned block, without the newly added type guard.
    // An approved edit must migrate it while still refusing arbitrary user edits.
    auto legacy = preview_library_change(project, {source}, "app").old_dependencies;
    const auto current_snippet = library_snippet(source, "app");
    const auto snippet_at = legacy.find(current_snippet);
    require(snippet_at != legacy.npos, "Cannot locate owned source-library block");
    legacy.replace(snippet_at, current_snippet.size(), library_snippet(source, "app", false));
    require(static_cast<bool>(
                std::ofstream(project / L"cmake/Dependencies.cmake", std::ios::binary) << legacy),
        "Cannot write exact legacy managed bytes");
    require(apply_library_change(preview_library_change(project, {source}, "app"), true, verify),
        "Approved edit could not migrate unchanged legacy source-library metadata");
    write(project / L"main.cpp",
        "#include <value.hpp>\n#include <iostream>\nint main() { std::cout << answer(); return "
        "answer() == 42 ? 0 : 1; }\n");
    require(apply_library_change(
                preview_library_change(project, {source, header}, "app"), true, verify),
        "Second local library failed or broke first");
    const auto targets = executable_targets(project, L"Debug", {});
    require(targets.size() == 1 && run_process(targets[0].artifact, {}, project) == 0,
        "Actual two-library consumer did not run");
    header.version = "2";
    require(apply_library_change(
                preview_library_change(project, {source, header}, "app"), true, verify),
        "Edited local integration failed");
    require(apply_library_change(preview_library_change(project, {header}, "app"), true, verify),
        "Removing source library affected header library");
    require(fs::is_regular_file(library / L"CMakeLists.txt") &&
                fs::is_regular_file(headers / L"value.hpp"),
        "Shared local library source was deleted");
    require(project_libraries(project).size() == 1, "Reopened project lost selected library");
    const auto moved_headers = root / L"moved-headers";
    require(fs::weakly_canonical(headers).parent_path() == fs::weakly_canonical(root) &&
                moved_headers.parent_path() == root,
        "Fixture move escaped its owned root");
    fs::rename(headers, moved_headers);
    header.local_path = moved_headers.generic_string();
    require(apply_library_change(preview_library_change(project, {header}, "app"), true, verify),
        "Repairing a missing local include path failed");
    LibraryRecord package;
    package.id = "local-package";
    package.name = "Local imported package";
    package.integration = "package";
    package.package = "Fixture";
    package.local_path = (root / L"package").generic_string();
    package.targets = {"Fixture::Core"};
    write(root / L"package/FixtureConfig.cmake", "add_library(Fixture::Core INTERFACE IMPORTED)\n");
    require(apply_library_change(
                preview_library_change(project, {header, package}, "app"), true, verify),
        "Actual find_package imported-target integration failed");
    LibraryRecord executable_package = package;
    executable_package.id = "application-package";
    executable_package.package = "ApplicationFixture";
    executable_package.targets = {"ApplicationFixture::App"};
    write(root / L"package/ApplicationFixtureConfig.cmake",
        "add_executable(ApplicationFixture::App IMPORTED)\n"
        "set_target_properties(ApplicationFixture::App PROPERTIES ENABLE_EXPORTS ON)\n");
    require(!apply_library_change(
                preview_library_change(project, {header, package, executable_package}, "app"),
                true,
                verify),
        "An exported executable was accepted as a package library target");
    require(project_libraries(project).size() == 2 && verify(),
        "Rejected executable package damaged the previously working libraries");
    auto missing = package;
    missing.package = "CppDeliberatelyMissingPackage";
    require(!apply_library_change(
                preview_library_change(project, {header, missing}, "app"), true, verify),
        "Missing package did not fail and roll back");
    require(project_libraries(project).size() == 2 &&
                project_libraries(project)[1].package == "Fixture",
        "Missing package rollback lost working metadata");
    require(apply_library_change(preview_library_change(project, {header}, "app"), true, verify),
        "Package removal damaged remaining local library");
    const auto change = preview_library_change(project, {header}, "app");
    require(change.new_root.find("Preserve this manual comment") != change.new_root.npos,
        "Manual CMake configuration was lost");
    std::cout
        << "PASS real local library add/add/build/run/edit/remove/reopen and source preservation\n";
}

void windows()
{
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW type{};
    type.lpfnWndProc = DefWindowProcW;
    type.hInstance = GetModuleHandleW(nullptr);
    type.lpszClassName = L"CppPlacementFixture";
    require(RegisterClassW(&type) != 0, "Cannot register native window fixture");
    MONITORINFO monitor{sizeof(monitor)};
    const POINT point{};
    require(GetMonitorInfoW(MonitorFromPoint(point, MONITOR_DEFAULTTOPRIMARY), &monitor),
        "Cannot inspect fixture monitor");
    const auto window = CreateWindowExW(0,
        type.lpszClassName,
        L"Owned placement fixture",
        WS_OVERLAPPEDWINDOW,
        monitor.rcWork.left + 24,
        monitor.rcWork.top + 24,
        480,
        320,
        nullptr,
        nullptr,
        type.hInstance,
        nullptr);
    require(window != nullptr, "Cannot create native window fixture");

    struct Cleanup
    {
        HWND window;

        ~Cleanup()
        {
            DestroyWindow(window);
            UnregisterClassW(L"CppPlacementFixture", GetModuleHandleW(nullptr));
        }
    } cleanup{window};

    ShowWindow(window, SW_SHOWNOACTIVATE);
    const std::vector<unsigned long> ids{GetCurrentProcessId()};
    auto previous = capture_application_windows(ids);
    require(previous.size() == 1, "Owned window capture failed");
    require(capture_application_windows({0}).empty(), "Captured an unrelated process window");
    const auto original = previous;
    for (const bool position : {false, true})
    {
        for (const bool size : {false, true})
        {
            SetWindowPos(window,
                nullptr,
                monitor.rcWork.left + 100,
                monitor.rcWork.top + 100,
                640,
                400,
                SWP_NOACTIVATE | SWP_NOZORDER);
            std::vector<HWND> partial;
            restore_application_windows(ids, original, partial, false, position, size);
            RECT actual{};
            GetWindowRect(window, &actual);
            require(actual.left == monitor.rcWork.left + (position ? 24 : 100) &&
                        actual.top == monitor.rcWork.top + (position ? 24 : 100) &&
                        actual.right - actual.left == (size ? 480 : 640) &&
                        actual.bottom - actual.top == (size ? 320 : 400),
                "Independent native window geometry restored a disabled component");
            if (position && !size)
            {
                SetWindowPos(window,
                    nullptr,
                    actual.left,
                    actual.top,
                    720,
                    460,
                    SWP_NOACTIVATE | SWP_NOZORDER);
                restore_application_windows(ids, original, partial, true, position, size);
                GetWindowRect(window, &actual);
                require(actual.right - actual.left == 720 && actual.bottom - actual.top == 460,
                    "Startup recheck overwrote program-controlled dimensions");
            }
        }
    }
    SetWindowPos(window,
        nullptr,
        monitor.rcWork.left + 100,
        monitor.rcWork.top + 100,
        640,
        400,
        SWP_NOACTIVATE | SWP_NOZORDER);
    std::vector<HWND> restored;
    require(restore_application_windows(ids, previous, restored) == 1,
        "Normal placement restoration failed");
    RECT actual{};
    GetWindowRect(window, &actual);
    require(
        EqualRect(&actual, &previous[0].bounds), "Native normal rectangle/size was not restored");
    SetWindowPos(window,
        nullptr,
        monitor.rcWork.left + 80,
        monitor.rcWork.top + 90,
        520,
        360,
        SWP_NOACTIVATE | SWP_NOZORDER);
    RECT user_moved{};
    GetWindowRect(window, &user_moved);
    restore_application_windows(ids, previous, restored);
    GetWindowRect(window, &actual);
    require(EqualRect(&actual, &user_moved),
        "Restoration overrode the user's move after initial success");
    ShowWindow(window, SW_SHOWMAXIMIZED);
    previous = capture_application_windows(ids);
    ShowWindow(window, SW_SHOWNORMAL);
    restored.clear();
    require(restore_application_windows(ids, previous, restored) == 1 && IsZoomed(window),
        "Native maximized state was not restored");
    ShowWindow(window, SW_MINIMIZE);
    previous = capture_application_windows(ids);
    restored.clear();
    require(restore_application_windows(ids, previous, restored) == 1 && !IsIconic(window),
        "Minimized window remained invisible after restart restoration");
    ShowWindow(window, SW_SHOWMAXIMIZED);
    restored.clear();
    restore_application_windows(ids, original, restored, false, true, false);
    require(IsZoomed(window), "Position-only restoration unmaximized the application's window");
    ShowWindow(window, SW_SHOWNORMAL);
    SetWindowPos(window,
        nullptr,
        monitor.rcWork.left + 100,
        monitor.rcWork.top + 100,
        640,
        400,
        SWP_NOACTIVATE | SWP_NOZORDER);
    auto scaled = original;
    scaled[0].dpi = GetDpiForWindow(window) * 2;
    restored.clear();
    restore_application_windows(ids, scaled, restored, false, false, true);
    GetWindowRect(window, &actual);
    require(actual.left == monitor.rcWork.left + 100 && actual.top == monitor.rcWork.top + 100 &&
                actual.right - actual.left == 240 && actual.bottom - actual.top == 160,
        "Size-only DPI conversion changed position or used unscaled remembered dimensions");
    {
        const auto second = CreateWindowW(type.lpszClassName,
            L"Second owned window",
            WS_OVERLAPPEDWINDOW,
            monitor.rcWork.left + 200,
            monitor.rcWork.top + 200,
            300,
            200,
            nullptr,
            nullptr,
            type.hInstance,
            nullptr);
        require(second != nullptr, "Cannot create second owned window");

        struct WindowCleanup
        {
            HWND window;

            ~WindowCleanup()
            {
                DestroyWindow(window);
            }
        } second_cleanup{second};

        ShowWindow(second, SW_SHOWNOACTIVATE);
        const auto multiple = capture_application_windows(ids);
        require(multiple.size() == 2, "Multiple owned windows were not captured");
        SetWindowPos(second,
            nullptr,
            monitor.rcWork.left + 300,
            monitor.rcWork.top + 300,
            340,
            240,
            SWP_NOACTIVATE | SWP_NOZORDER);
        restored.clear();
        require(restore_application_windows(ids, multiple, restored, false, true, false) == 2,
            "Multiple window position restoration failed");
        GetWindowRect(second, &actual);
        require(actual.left == monitor.rcWork.left + 200 &&
                    actual.top == monitor.rcWork.top + 200 && actual.right - actual.left == 340 &&
                    actual.bottom - actual.top == 240,
            "Multiple-window matching changed the wrong geometry component");
    }
    SetWindowLongPtrW(window, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowPos(window,
        nullptr,
        monitor.rcMonitor.left,
        monitor.rcMonitor.top,
        monitor.rcMonitor.right - monitor.rcMonitor.left,
        monitor.rcMonitor.bottom - monitor.rcMonitor.top,
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED);
    previous = capture_application_windows(ids);
    require(previous.size() == 1 && previous[0].borderless_fullscreen,
        "Borderless fullscreen window was not identified");
    SetWindowPos(window, nullptr, 100, 100, 400, 300, SWP_NOACTIVATE | SWP_NOZORDER);
    restored.clear();
    require(restore_application_windows(ids, previous, restored) == 1 &&
                GetWindowRect(window, &actual) && EqualRect(&actual, &monitor.rcMonitor),
        "Borderless fullscreen restoration incorrectly clipped to the monitor work area");
    std::cout << "PASS native normal rectangle/size, maximization, minimized fallback and "
                 "unrelated-process exclusion\n";
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    try
    {
        workspace_test::Fixture fixture;
        if (argc == 2 && std::wstring_view(argv[1]) == L"libraries")
        {
            libraries(fixture.root);
        }
        else if (argc == 2 && std::wstring_view(argv[1]) == L"windows")
        {
            windows();
        }
        else
        {
            fs::create_directory(fixture.root / L"unit-project");
            unit(fixture.root / L"unit-project");
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
