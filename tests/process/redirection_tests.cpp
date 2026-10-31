#include "process/application_windows.hpp"
#include "cpplivelab/libraries.hpp"
#include "cpplivelab/session.hpp"
#include "cpplivelab/process.hpp"
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
    ShowWindow(window, SW_SHOWNORMAL);
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
