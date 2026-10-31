// Compile the real controller/window procedure into a test host. No desktop input,
// visible user window or external application is automated by this test.
#define CPPLIVELAB_GUI_TEST
#include "../src/gui.cpp"
#include "cpplivelab/projects.hpp"
#include "workspace_test_support.hpp"
#include "ui_capture.hpp"
#include <fstream>
#include <iostream>
#include <functional>
#include <atomic>

namespace flow_tests
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void pump()
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
    {
        if (message.message != WM_QUIT)
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
}

struct RestartTaskDriver
{
    Application* app;
    bool approve;
    bool filesystem;
    bool requested = false;
    bool answered = false;
    bool cancelled_kept_task = false;
    std::atomic<bool> release{false};
    std::string failure;
    ULONGLONG started = GetTickCount64();
};

RestartTaskDriver* restart_task_driver = nullptr;

BOOL CALLBACK restart_task_window(HWND window, LPARAM)
{
    auto& driver = *restart_task_driver;
    const auto title = read_control_text(window);
    if ((title == L"Library verification" || title == L"Cache deletion test boundary") &&
        cpplivelab::library_task_active() && !driver.requested)
    {
        driver.requested = true;
        PostMessageW(driver.app->window_handle, WM_COMMAND, RestartButton, 0);
    }
    if (title == L"Restart Cpp" && !driver.answered)
    {
        driver.answered = true;
        PostMessageW(window, WM_COMMAND, driver.approve ? IDOK : IDCANCEL, 0);
    }
    if (GetTickCount64() - driver.started > 5000)
    {
        driver.failure = "Restart did not reach the controlled task boundary";
        driver.release = true;
        cpplivelab::cancel_library_task();
        if (title == L"Restart Cpp")
        {
            PostMessageW(window, WM_COMMAND, IDCANCEL, 0);
        }
    }
    return TRUE;
}

void CALLBACK restart_task_timer(HWND, UINT, UINT_PTR, DWORD)
{
    if (!restart_task_driver)
    {
        return;
    }
    auto& driver = *restart_task_driver;
    EnumThreadWindows(GetCurrentThreadId(), restart_task_window, 0);
    if (driver.answered && !driver.app->restart_confirmation)
    {
        if (cpplivelab::library_task_active() && IsWindowEnabled(driver.app->window_handle))
        {
            driver.failure =
                "Nested restart confirmation enabled the owner during its active modal task";
        }
        if (driver.approve && driver.app->restart_pending)
        {
            if (driver.app->restart_launch)
            {
                driver.failure = "Restart launched before the task's safe boundary";
            }
            driver.release = true;
        }
        else if (!driver.approve && !driver.app->restart_pending &&
                 cpplivelab::library_task_active())
        {
            driver.cancelled_kept_task = true;
            // End only this test's simulated download after proving that Cancel
            // left it active. This is cleanup, not the restart implementation.
            cpplivelab::cancel_library_task();
        }
    }
}

void restart_task_checks(Application& app, const fs::path& root)
{
    const auto fixture = application_directory() / L"session_fixture.exe";
    cpplivelab::ConsoleSession unrelated;
    unrelated.start(fixture, {L"silent-hold"}, root);
    const bool previous_dialog = app.project_dialog_open;
    app.project_dialog_open = true;
    for (const auto approve : {false, true})
    {
        RestartTaskDriver driver{&app, approve, false};
        restart_task_driver = &driver;
        const auto timer = SetTimer(nullptr, 0, 20, restart_task_timer);
        const bool result = cpplivelab::run_library_task(app.window_handle,
            app.preferences.theme,
            fixture,
            {L"silent-hold"},
            root,
            [](const auto&)
            {
            });
        KillTimer(nullptr, timer);
        restart_task_driver = nullptr;
        require(!result && driver.failure.empty() && driver.requested && driver.answered,
            "Restart during an owned download worker was not confirmed/cancelled safely");
        require(approve ? app.restart_pending : driver.cancelled_kept_task && !app.restart_pending,
            "Restart approval/cancellation did not preserve the correct download state");
        require(unrelated.running(), "Restart terminated an unrelated test process");
        app.restart_pending = false;
    }
    const auto cache = cpplivelab::manager_state_directory() / L"library-sources/restart-boundary";
    fs::create_directories(cache);
    std::ofstream(cache / L"payload") << "owned cache data";
    cpplivelab::register_dependency_cache(
        {"restart-boundary", "restart-fixture", "Downloaded source", cache});
    RestartTaskDriver driver{&app, true, true};
    restart_task_driver = &driver;
    const auto timer = SetTimer(nullptr, 0, 20, restart_task_timer);
    std::atomic<bool> completed{false};
    cpplivelab::run_library_work(app.window_handle,
        app.preferences.theme,
        L"Cache deletion test boundary",
        [&]
        {
            while (!driver.release && GetTickCount64() - driver.started < 6000)
            {
                Sleep(1);
            }
            cpplivelab::delete_dependency_cache("restart-boundary", true);
            completed = true;
        });
    KillTimer(nullptr, timer);
    restart_task_driver = nullptr;
    require(driver.failure.empty() && driver.answered && app.restart_pending && completed &&
                !fs::exists(cache) && !cpplivelab::library_task_active(),
        "Restart did not defer until actual owned cache deletion completed safely");
    require(unrelated.running(), "Cache-boundary restart terminated an unrelated process");
    app.restart_pending = false;
    app.project_dialog_open = previous_dialog;
    unrelated.stop();
}

void check_layout(Application& app)
{
    RECT before{};
    GetWindowRect(app.find_control(ProgramInput), &before);
    SendMessageW(app.window_handle, WM_SIZE, SIZE_MINIMIZED, 0);
    RECT after{};
    GetWindowRect(app.find_control(ProgramInput), &after);
    require(EqualRect(&before, &after) != 0,
        "Minimizing applied a zero-size layout and corrupted input/control geometry");
    const auto before_gdi = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int iteration = 0; iteration < 80; ++iteration)
    {
        const int width = iteration % 3 == 0 ? 780 : iteration % 2 ? 940 : 1400;
        const int height = iteration % 3 == 0 ? 600 : iteration % 2 ? 650 : 900;
        SetWindowPos(app.window_handle,
            nullptr,
            -30000,
            -30000,
            width,
            height,
            SWP_NOACTIVATE | SWP_NOZORDER);
        // The hidden host has no initial ShowWindow/WM_SIZE. Measure only after
        // a real layout pass, as the visible application receives on startup.
        RECT dropdown{};
        SendMessageW(app.find_control(ExecutableInput),
            CB_GETDROPPEDCONTROLRECT,
            0,
            reinterpret_cast<LPARAM>(&dropdown));
        if (dropdown.bottom - dropdown.top <= cpplivelab::scale_pixels(100, app.dpi))
        {
            throw std::runtime_error("Executable target dropdown height " +
                                     std::to_string(dropdown.bottom - dropdown.top) +
                                     " is unusable at DPI " + std::to_string(app.dpi));
        }
        RECT input{}, send{}, end{}, build{}, run{}, stop{};
        GetWindowRect(app.find_control(ProgramInput), &input);
        GetWindowRect(app.find_control(SendButton), &send);
        GetWindowRect(app.find_control(EndInputButton), &end);
        GetWindowRect(app.find_control(BuildButton), &build);
        GetWindowRect(app.find_control(RunButton), &run);
        GetWindowRect(app.find_control(StopButton), &stop);
        require(input.right < send.left && send.right < end.left && build.right < run.left &&
                    run.right < stop.left,
            "Controls overlapped after repeated resizing");
        const auto region = CreateRectRgn(0, 0, 0, 0);
        const auto kind = GetWindowRgn(app.find_control(BuildButton), region);
        const bool rounded =
            kind != ERROR &&
            PtInRegion(region, (build.right - build.left) / 2, (build.bottom - build.top) / 2) &&
            !PtInRegion(region, 0, 0);
        DeleteObject(region);
        require(rounded, "Resizing lost the button's usable rounded region");
    }
    require(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) <= before_gdi + 4,
        "Repeated layout updates leaked GDI objects");
}

struct Project
{
    fs::path root = fs::temp_directory_path() /
                    (L"CppLiveLab GUI regression " + std::to_wstring(GetCurrentProcessId()));

    Project()
    {
        require(!fs::exists(root), "GUI test root already exists");
        cpplivelab::create_project(root, true);
    }

    ~Project()
    {
        std::error_code error;
        fs::remove_all(root, error);
    }
};

struct Host
{
    Application app;

    explicit Host(const fs::path& project, const fs::path& snapshots = {})
    {
        app.preferences = {};
        app.preference_store =
            L"Software\\CppLiveLab\\GuiFlowTests\\" + std::to_wstring(GetCurrentProcessId());
        WNDCLASSW window_class{};
        window_class.lpfnWndProc = window_procedure;
        window_class.hInstance = GetModuleHandleW(nullptr);
        window_class.lpszClassName = L"CppLiveLabGuiFlowTest";
        require(RegisterClassW(&window_class) != 0, "Could not register GUI test host");
        const auto window = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_CONTROLPARENT,
            window_class.lpszClassName,
            L"CppLiveLab automated controller test",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            -30000,
            -30000,
            1080,
            800,
            nullptr,
            nullptr,
            window_class.hInstance,
            &app);
        require(window != nullptr, "Could not create GUI test host");
        RECT client{};
        GetClientRect(window, &client);
        app.layout(client.right, client.bottom);
        require(app.active_project_directory.empty() &&
                    (GetWindowLongPtrW(app.find_control(WelcomeTitle), GWL_STYLE) & WS_VISIBLE) &&
                    !(GetWindowLongPtrW(app.find_control(BuildButton), GWL_STYLE) & WS_VISIBLE),
            "Startup did not show welcome without automatically reopening a project");
        if (!snapshots.empty())
        {
            ShowWindow(window, SW_SHOWNOACTIVATE);
            for (const auto theme : {cpplivelab::ThemeChoice::Dark, cpplivelab::ThemeChoice::Light})
            {
                app.apply_appearance_choice(theme);
                RedrawWindow(window,
                    nullptr,
                    nullptr,
                    RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
                pump();
                UpdateWindow(window);
                ui_test::capture(window,
                    snapshots / (theme == cpplivelab::ThemeChoice::Dark ? L"welcome-dark.png"
                                                                        : L"welcome-light.png"));
            }
            ShowWindow(window, SW_HIDE);
        }
        app.command(ManageLibrariesMenu);
        require(read_control_text(app.find_control(StatusLabel)).find(L"Select or create") !=
                    std::wstring::npos,
            "A normal no-project prerequisite was treated as an internal error");
        app.command(DoctorMenu);
        require(app.welcome_output && app.active_project_directory.empty(),
            "Diagnostics requires a project or replaced project identity");
        app.open(project);
    }

    ~Host()
    {
        if (IsWindow(app.window_handle))
        {
            SendMessageW(app.window_handle, WM_CLOSE, 0, 0);
            const auto started = GetTickCount64();
            while (IsWindow(app.window_handle) && GetTickCount64() - started < 10000)
            {
                pump();
                Sleep(5);
            }
            if (IsWindow(app.window_handle))
            {
                DestroyWindow(app.window_handle);
            }
        }
        RegDeleteTreeW(HKEY_CURRENT_USER, app.preference_store.c_str());
        UnregisterClassW(L"CppLiveLabGuiFlowTest", GetModuleHandleW(nullptr));
    }

    void await(
        const std::function<bool()>& predicate, const char* description, ULONGLONG timeout = 60000)
    {
        const auto started = GetTickCount64();
        while (!predicate())
        {
            pump();
            if (GetTickCount64() - started >= timeout)
            {
                const auto log = read_control_text(app.system_console.window);
                throw std::runtime_error(
                    std::string(description) + "\nStatus: " +
                    encode_utf8(read_control_text(app.find_control(StatusLabel))) + "\nLog:\n" +
                    encode_utf8(log.substr(log.size() > 6000 ? log.size() - 6000 : 0)));
            }
            Sleep(5);
        }
    }

    void idle()
    {
        await(
            [&]
            {
                return !app.operation_in_progress;
            },
            "GUI operation did not complete");
    }

    void running()
    {
        await(
            [&]
            {
                return app.phase == Application::Phase::Running;
            },
            "GUI did not start the selected program");
    }

    void prompt(std::wstring_view text)
    {
        await(
            [&]
            {
                return read_control_text(app.program_console.window).find(text) !=
                       std::wstring::npos;
            },
            "Interactive prompt was not delivered",
            10000);
    }

    void send(const wchar_t* line)
    {
        SetWindowTextW(app.find_control(ProgramInput), line);
        app.command(SendButton);
    }
};

void run(const fs::path& fixtures, const fs::path& snapshots = {})
{
    SetEnvironmentVariableW(L"CPPLIVELAB_TEST_RUNTIME", L"owned-runtime-value");
    Project project;
    Host host(project.root, snapshots);
    require(read_control_text(host.app.window_handle).find(L"Cpp \u2014 Make Your Life Easier") !=
                std::wstring::npos,
        "Displayed identity/tagline lost its Unicode spelling in the active project");
    {
        const auto capture = fixtures / L"diagnostic location.txt";
        cpplivelab::save_editor({L"Custom",
            application_directory() / L"session_fixture.exe",
            {L"capture-editor-location",
                capture.wstring(),
                L"{project}",
                L"{file}",
                L"{line}",
                L"{column}"}});
        auto& controller = host.app;
        controller.append_build_output((project.root / L"src/main.cpp").wstring() +
                                           L"(12,4): error C2143: intentional navigation fixture\n",
            true);
        SendMessageW(controller.find_control(SystemOutputPane), EM_SETSEL, -1, -1);
        // The last paragraph is empty after the newline. Select the diagnostic's
        // first character explicitly, then send the real Rich Edit keyboard route.
        const auto text = read_control_text(controller.find_control(SystemOutputPane));
        const auto position = text.find(L"[ERROR] ");
        require(position != text.npos, "Build console lost the diagnostic severity label");
        SendMessageW(controller.find_control(SystemOutputPane), EM_SETSEL, position, position);
        MSGFILTER notification{};
        notification.nmhdr.hwndFrom = controller.find_control(SystemOutputPane);
        notification.nmhdr.idFrom = SystemOutputPane;
        notification.nmhdr.code = EN_MSGFILTER;
        notification.msg = WM_KEYDOWN;
        notification.wParam = VK_RETURN;
        SendMessageW(controller.window_handle,
            WM_NOTIFY,
            SystemOutputPane,
            reinterpret_cast<LPARAM>(&notification));
        const auto started = GetTickCount64();
        std::string actual;
        while (GetTickCount64() - started < 5000)
        {
            std::ifstream input(capture, std::ios::binary);
            actual.assign(std::istreambuf_iterator<char>(input), {});
            if (!actual.empty())
            {
                break;
            }
            Sleep(10);
        }
        require(actual == cpplivelab::path_support::path_utf8(project.root) + "\n" +
                              cpplivelab::path_support::path_utf8(project.root / L"src/main.cpp") +
                              "\n12\n4\n",
            "Rich Edit keyboard diagnostic navigation did not pass exact source/line/column");
        cpplivelab::save_editor({});
    }
    auto& app = host.app;
    const auto initial_source = project.root / L"src/main.cpp";
    std::ifstream initial_file(initial_source);
    const std::string initial_bytes{std::istreambuf_iterator<char>(initial_file), {}};
    initial_file.close();
    std::ofstream(initial_source) << "intentional first compilation failure\n";
    app.start(false);
    host.idle();
    require(read_control_text(app.find_control(StatusLabel)) == L"Build Failed" &&
                read_control_text(app.find_control(ExecutableInput)) == L"app",
        "A configured target remained hidden after initial compilation failure");
    std::ofstream(initial_source) << initial_bytes;
    for (const auto id : {FolderInput, OpenProjectButton, AppearanceSelector, DebounceInput})
    {
        require(app.find_control(id) == nullptr,
            "Rare configuration remains duplicated in the main workspace");
    }
    require(!(GetWindowLongPtrW(app.find_control(BrowseButton), GWL_STYLE) & WS_VISIBLE),
        "Welcome-only Open Existing button remains visible inside the active workspace");
    require(GetMenu(app.window_handle) == nullptr, "Old menu bar competes with themed navigation");
    int settings_entries = 0;
    for (const auto section : {NavigationProject, NavigationTools, NavigationSettings})
    {
        require(app.find_control(section) != nullptr, "Themed navigation section missing");
        for (const auto& action : app.navigation_actions(section))
        {
            if (action.second == AppearanceMenu)
            {
                ++settings_entries;
            }
        }
    }
    require(settings_entries == 1, "Appearance has more than one settings entry");
    require(app.find_control(NavigationBuild) == nullptr &&
                app.navigation_actions(NavigationTools)[0].second == EditProjectButton,
        "Navigation duplicates implementation instead of routing to existing actions");
    const auto before_manual_build = app.build_sequence;
    SendMessageW(app.window_handle, WM_COMMAND, MAKEWPARAM(BuildButton, BN_CLICKED), 0);
    require(app.build_sequence == before_manual_build + 1,
        "One routed Build command executed more than once");
    host.idle();
    require(read_control_text(app.find_control(StatusLabel)) == L"Build Successful",
        "GUI initial build did not succeed");
    const auto builds = app.build_sequence;
    app.command(RunButton);
    host.running();
    host.prompt(L"Your name:");
    if (!snapshots.empty())
    {
        for (const auto theme : {cpplivelab::ThemeChoice::Dark, cpplivelab::ThemeChoice::Light})
        {
            app.apply_appearance_choice(theme);
            ShowWindow(app.window_handle, SW_SHOWNOACTIVATE);
            UpdateWindow(app.window_handle);
            ui_test::capture(app.window_handle,
                snapshots / (theme == cpplivelab::ThemeChoice::Dark ? L"workspace-dark.png"
                                                                    : L"workspace-light.png"));
        }
    }
    require(app.build_sequence == builds, "Up-to-date Run rebuilt instead of probing Ninja");
    require(read_control_text(app.system_console.window).find(L"Build is up to date") !=
                std::wstring::npos,
        "GUI did not report the up-to-date Run decision");
    app.command(EndInputButton);
    host.idle();
    app.command(ClearProgram);
    app.command(RunButton);
    host.running();
    host.prompt(L"Your name: ");
    require(IsWindowEnabled(app.find_control(SendButton)) != 0,
        "Fast Run after End Input kept stdin disabled from the previous process");
    require(app.build_sequence == builds, "Second unchanged Run rebuilt");
    const auto program_id = app.session.process_id();
    const auto editor_capture = fixtures / L"editor-while-running.txt";
    cpplivelab::save_editor({L"Custom",
        application_directory() / L"session_fixture.exe",
        {L"capture-editor-location",
            editor_capture.wstring(),
            L"{project}",
            L"{file}",
            L"{line}",
            L"{column}"}});
    require(IsWindowEnabled(app.find_control(EditProjectButton)),
        "Open Editor is disabled during execution");
    app.command(EditProjectButton);
    host.await(
        [&]
        {
            return fs::is_regular_file(editor_capture) && fs::file_size(editor_capture) > 0;
        },
        "Open Editor was not routed while the program was running",
        10000);
    require(app.session.process_id() == program_id && app.session.running(),
        "Open Editor stopped or replaced the running application");
    cpplivelab::save_editor({});
    const auto test_script = project.root / L"controlled-test-worker.ps1";
    std::ofstream(test_script)
        << "Write-Output 'controlled test started'\nStart-Sleep -Seconds 30\n";
    wchar_t system[MAX_PATH]{};
    GetSystemDirectoryW(system, MAX_PATH);
    app.test_session.start_worker(fs::path(system) / L"WindowsPowerShell/v1.0/powershell.exe",
        {L"-NoProfile", L"-ExecutionPolicy", L"Bypass", L"-File", test_script.wstring()},
        project.root);
    app.tests_active = true;
    app.tests_started = GetTickCount64();
    host.await(
        [&]
        {
            return read_control_text(app.system_console.window).find(L"controlled test started") !=
                   std::wstring::npos;
        },
        "Integrated runner failed to capture actual background output");
    const auto test_pid = app.test_session.process_id();
    app.command(TestRunnerMenu);
    require(app.test_session.process_id() == test_pid,
        "Overlapping runner request replaced a running test process");
    app.command(CancelTestsMenu);
    host.await(
        [&]
        {
            return !app.tests_active;
        },
        "Integrated runner cancellation did not finish");
    require(app.session.running() && app.session.process_id() == program_id &&
                app.phase == Application::Phase::Running,
        "Cancelling tests terminated or changed the user program");
    require(read_control_text(app.system_console.window).find(L"Tests cancelled; exit") !=
                std::wstring::npos,
        "Integrated runner did not report actual cancellation/exit status");
    fs::remove(test_script);

    app.command(ClearProgram);
    require(read_control_text(app.program_console.window) == L"Your name: " &&
                app.session.process_id() == program_id &&
                app.phase == Application::Phase::Running &&
                IsWindowEnabled(app.find_control(SendButton)) &&
                read_control_text(app.find_control(StatusLabel)).find(L"input remains available") !=
                    std::wstring::npos,
        "Clearing program history changed process state or hid input availability");
    host.send(L"Ada");
    host.prompt(L"First number:");
    host.send(L"2.5");
    host.prompt(L"Second number:");
    app.command(ClearProgram);
    require(read_control_text(app.program_console.window) == L"Second number: " &&
                app.session.process_id() == program_id &&
                IsWindowEnabled(app.find_control(SendButton)),
        "Clear lost the current prompt or reset a partially entered calculation");
    host.send(L"3.75");
    host.idle();
    require(read_control_text(app.program_console.window).find(L"Sum: 6.25") != std::wstring::npos,
        "Fresh Run did not preserve interactive input/output");
    const auto retained_program = read_control_text(app.program_console.window);
    app.command(ClearSystem);
    require(read_control_text(app.system_console.window).empty() &&
                read_control_text(app.program_console.window) == retained_program,
        "Clearing build history also changed Program Console");
    app.command(RunButton);
    host.running();
    app.command(StopButton);
    host.idle();
    require(IsWindowEnabled(app.find_control(RunButton)) != 0,
        "Completed Stop left Run disabled until another Build");
    require(!app.run_requested && !app.auto_build.pending(), "Stop retained queued Run work");
    check_layout(app);
    const auto debug_program = app.current_identity().executable;
    const auto debug_written = fs::last_write_time(debug_program);
    SendMessageW(app.find_control(ConfigurationSelector), CB_SETCURSEL, 1, 0);
    app.command(BuildButton);
    host.idle();
    require(app.selected_configuration() == L"Release" &&
                read_control_text(app.find_control(StatusLabel)) == L"Build Successful" &&
                fs::is_regular_file(app.current_identity().executable) &&
                app.current_identity().executable != debug_program &&
                fs::last_write_time(debug_program) == debug_written,
        "GUI Release selection did not build independently of Debug");
    const auto release_builds = app.build_sequence;
    app.command(ClearProgram);
    app.command(RunButton);
    host.running();
    host.prompt(L"Your name:");
    host.send(L"Release");
    host.prompt(L"First number:");
    host.send(L"1.25");
    host.prompt(L"Second number:");
    host.send(L"2.75");
    host.idle();
    require(app.build_sequence == release_builds &&
                read_control_text(app.program_console.window).find(L"Hello, Release! Sum: 4") !=
                    std::wstring::npos,
        "GUI did not reuse and run the selected Release executable correctly");
    // Drive the real controller, watcher, CMake workers and interactive program.
    const auto source_path = project.root / L"src/main.cpp";
    std::ifstream original_file(source_path, std::ios::binary);
    const std::string original{std::istreambuf_iterator<char>(original_file), {}};
    original_file.close();
    app.command(RunButton);
    host.running();
    const auto before_error = app.session.process_id();
    app.command(AutoBuildButton);
    require(app.preferences.auto_build && !app.auto_run_preferences.enabled,
        "Auto Build toggle changed the independent Auto Run setting");
    std::ofstream(source_path) << original << "\n// Auto Build without restart\n";
    host.await(
        [&]
        {
            return app.auto_build.pending();
        },
        "Auto Build did not observe the save",
        10000);
    require(app.session.process_id() == before_error && app.session.running() &&
                !app.replacement_building,
        "Auto Build unexpectedly restarted the running program");
    app.command(AutoBuildButton);
    app.command(AutoRunButton);
    require(app.auto_build.enabled(), "Auto Run toggle did not enable scheduling");
    std::ofstream(source_path) << "this is a deliberate syntax error\n";
    host.await(
        [&]
        {
            return read_control_text(app.system_console.window)
                       .find(L"Build failed — previous application is still running.") !=
                   std::wstring::npos;
        },
        "Auto Run compilation error was not reported",
        90000);
    require(app.session.running() && app.session.process_id() == before_error,
        "Failed replacement terminated the existing application");
    std::ofstream(source_path) << original << "\n// Fixed source saved\n";
    host.await(
        [&]
        {
            return app.phase == Application::Phase::Running &&
                   app.session.process_id() != before_error;
        },
        "Fixed source did not build and restart automatically",
        90000);
    const auto replacement_id = app.session.process_id();
    const auto replacement_time = fs::last_write_time(app.current_identity().executable);
    app.command(ProgramRestartButton);
    host.await(
        [&]
        {
            return app.phase == Application::Phase::Running &&
                   app.session.process_id() != replacement_id;
        },
        "Manual program Restart did not finish",
        15000);
    require(fs::last_write_time(app.current_identity().executable) == replacement_time,
        "Manual Restart unnecessarily rebuilt its successful artifact");
    host.send(L"Auto Run");
    host.prompt(L"First number:");
    host.send(L"2");
    host.prompt(L"Second number:");
    host.send(L"5");
    host.idle();
    require(read_control_text(app.program_console.window).find(L"Sum: 7") != std::wstring::npos,
        "Interactive console input failed after automatic/manual restart");
    app.command(AutoRunButton);
    require(!app.auto_build.enabled(), "Auto Run toggle did not disable scheduling");
    std::cout << "Auto Run: failed build retains PID, repair restarts, manual Restart reuses "
                 "artifact, interactive I/O passed\n";
    const auto window_project = fixtures / L"Native window project";
    fs::create_directory(window_project);
    std::ofstream(window_project / L"CMakeLists.txt")
        << "cmake_minimum_required(VERSION 3.20)\nproject(WindowFixture LANGUAGES CXX)\n"
           "add_executable(app main.cpp)\ntarget_compile_features(app PRIVATE "
           "cxx_std_20)\ntarget_link_libraries(app PRIVATE user32)\n";
    std::ifstream native_sample(fs::path(__FILE__).parent_path() / L"consumers/native_window.cpp");
    std::string native_source{std::istreambuf_iterator<char>(native_sample), {}};
    require(!native_source.empty(), "Native manual acceptance sample is missing");
    native_source.insert(0,
        "#include <fstream>\n#include <string>\n#include <cstdlib>\nextern \"C\" "
        "__declspec(dllimport) int runtime_value();\n");
    const auto entry = native_source.find("int main()\n{");
    require(entry != native_source.npos, "Native fixture entry is missing");
    native_source.replace(entry,
        std::string("int main()\n{").size(),
        "int main(int argc, char** argv)\n{\n"
        "    if (argc != 3 || std::string(argv[1]) != \"two words\" || std::string(argv[2]) != "
        "\"second\" ||\n"
        "        !std::ifstream(\"asset.txt\") || !std::getenv(\"CPPLIVELAB_TEST_RUNTIME\") || "
        "runtime_value() != 42)\n"
        "    {\n        return 9;\n    }\n");
    native_source.insert(native_source.find("    const auto window ="), "    Sleep(600);\n");
    std::ofstream(window_project / L"dependency.cpp")
        << "extern \"C\" __declspec(dllexport) int runtime_value() { return 42; }\n";
    std::ofstream(window_project / L"CMakeLists.txt", std::ios::app)
        << "add_library(runtime_dependency SHARED dependency.cpp)\ntarget_link_libraries(app "
           "PRIVATE runtime_dependency)\n";
    const auto working = window_project / L"asset working directory";
    fs::create_directory(working);
    std::ofstream(working / L"asset.txt") << "Owned asset\n";
    cpplivelab::ProjectOptions runtime_options;
    runtime_options.arguments = {L"two words", L"second"};
    runtime_options.working_directory = working;
    cpplivelab::save_project_options(window_project, L"Debug", runtime_options);
    const auto native_file = window_project / L"main.cpp";
    std::ofstream(native_file) << native_source;
    app.open(window_project);
    app.command(RunButton);
    host.running();
    auto native_window = [&]()
    {
        struct Search
        {
            DWORD process;
            HWND window = nullptr;
        } search{app.session.process_id()};
        EnumWindows(
            [](HWND window, LPARAM parameter) -> BOOL
            {
                auto& search = *reinterpret_cast<Search*>(parameter);
                DWORD process = 0;
                GetWindowThreadProcessId(window, &process);
                if (process == search.process && IsWindowVisible(window))
                {
                    search.window = window;
                }
                return TRUE;
            },
            reinterpret_cast<LPARAM>(&search));
        return search.window;
    };
    host.await(
        [&]
        {
            return native_window() != nullptr;
        },
        "Native GUI window did not appear");
    RECT placement{};
    GetWindowRect(native_window(), &placement);
    SetWindowPos(native_window(),
        nullptr,
        placement.left + 25,
        placement.top + 30,
        540,
        350,
        SWP_NOACTIVATE | SWP_NOZORDER);
    GetWindowRect(native_window(), &placement);
    const auto before_native = app.session.process_id();
    std::ofstream(native_file) << native_source << "\n// Saved while Auto Run is OFF\n";
    const auto off_saved = GetTickCount64();
    while (GetTickCount64() - off_saved < 1000)
    {
        pump();
        Sleep(5);
    }
    require(app.session.process_id() == before_native && !app.replacement_building,
        "Auto Run OFF rebuilt or restarted after a save");
    app.command(AutoRunButton);
    std::ofstream(native_file) << native_source << "\n// Native replacement\n";
    host.await(
        [&]
        {
            return app.session.process_id() != before_native && app.program_started &&
                   !app.restore_started && native_window() != nullptr;
        },
        "Native Auto Run did not restore its replacement window",
        90000);
    RECT after_native{};
    GetWindowRect(native_window(), &after_native);
    require(EqualRect(&placement, &after_native),
        "Actual restarted application lost its window rectangle");
    const auto before_identical_save = app.session.process_id();
    std::ofstream(native_file) << native_source << "\n// Native replacement\n";
    const auto identical_saved = GetTickCount64();
    while (GetTickCount64() - identical_saved < 1000)
    {
        pump();
        Sleep(5);
    }
    require(app.session.process_id() == before_identical_save && !app.replacement_building,
        "An identical-content save unnecessarily rebuilt or restarted");
    ShowWindow(native_window(), SW_SHOWMAXIMIZED);
    const auto before_maximized = app.session.process_id();
    std::ofstream(native_file) << native_source << "\n// First rapid save\n";
    host.await(
        [&]
        {
            return app.replacement_building;
        },
        "Native replacement build did not start");
    std::ofstream(native_file) << native_source << "\n// Latest rapid save\n";
    host.await(
        [&]
        {
            return app.session.process_id() != before_maximized && app.program_started &&
                   !app.restore_started && native_window() != nullptr &&
                   !app.replacement_building && !app.auto_build.pending();
        },
        "Rapid native saves did not coalesce into a maximized replacement",
        90000);
    require(IsZoomed(native_window()), "Actual restarted application lost maximization");
    app.command(StopButton);
    host.idle();
    const auto stopped_at = GetTickCount64();
    while (GetTickCount64() - stopped_at < 1000)
    {
        pump();
        Sleep(5);
    }
    require(!app.operation_in_progress && !app.run_requested,
        "Manual Stop immediately relaunched Auto Run");
    app.command(ProgramRestartButton);
    host.running();
    host.await(
        [&]
        {
            return native_window() != nullptr;
        },
        "Native Restart after Stop failed");
    auto crashing_source = native_source;
    const auto show = crashing_source.find("ShowWindow(window, SW_SHOWNOACTIVATE);");
    require(show != crashing_source.npos, "Native crash fixture launch marker missing");
    crashing_source.replace(
        show, std::string("ShowWindow(window, SW_SHOWNOACTIVATE);").size(), "ExitProcess(7);");
    std::ofstream(native_file) << crashing_source;
    host.await(
        [&]
        {
            return !app.operation_in_progress &&
                   read_control_text(app.system_console.window)
                           .find(L"Program finished (exit 7)") != std::wstring::npos;
        },
        "Crashing replacement was not reported",
        90000);
    const auto crashed_at = GetTickCount64();
    while (GetTickCount64() - crashed_at < 1000)
    {
        pump();
        Sleep(5);
    }
    require(!app.operation_in_progress && !app.run_requested && !app.auto_build.pending(),
        "Crashing application entered an uncontrolled restart loop");
    app.command(AutoRunButton);
    app.open(project.root);
    std::cout << "Native Auto Run: actual restart rectangle, maximization, rapid saves and manual "
                 "Stop passed\n";
    const auto second_project = fixtures / L"Second project";
    cpplivelab::create_project(second_project, false);
    app.apply_build_preferences(675);
    require(cpplivelab::load_preferences(app.preference_store).debounce_ms == 675,
        "Build Preferences did not persist through the shared preference source");
    app.auto_build.enable(true);
    app.auto_build.notify(GetTickCount64());
    app.run_requested = true;
    require(app.auto_build.pending(), "Project-switch fixture did not queue a build");
    cpplivelab::save_auto_run_preferences(second_project, {true, true, false, 3000});
    app.open(second_project);
    app.auto_build.notify(1000);
    require(!app.auto_build.settled(1674) && app.auto_build.settled(1675),
        "Watcher restart replaced the configured debounce with a removed control's default");
    app.auto_build.cancel();
    require(app.active_project_directory == fs::weakly_canonical(second_project) &&
                !app.auto_build.pending() && !app.run_requested &&
                app.selected_configuration() == L"Debug",
        "Project switch retained old pending build/Run or configuration");
    app.open(project.root);
    require(app.selected_configuration() == L"Release" &&
                read_control_text(app.find_control(ExecutableInput)) == L"app",
        "Reopened project did not restore its own configuration/target");
    app.start(false);
    app.open(second_project);
    require(app.active_project_directory == fs::weakly_canonical(project.root) &&
                app.operation_in_progress,
        "Project switched while a build was owned");
    app.stop();
    host.idle();
    app.open(second_project);
    require(!app.auto_build.pending() && !app.operation_in_progress,
        "Switch after Stop retained a build or pending queue");
    restart_task_checks(app, fixtures);
    SendMessageW(app.window_handle, WM_CLOSE, 0, 0);
    host.await(
        [&]
        {
            return !IsWindow(app.window_handle);
        },
        "GUI shutdown did not complete",
        15000);
    std::cout << "GUI controller: fresh Run, EOF/restart, interactive I/O, Stop/restart and "
                 "shutdown passed\n";
}
} // namespace flow_tests

int wmain(int argc, wchar_t** argv)
{
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const auto library = LoadLibraryW(L"Msftedit.dll");
    if (!library)
    {
        return 1;
    }
    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int result = 0;
    try
    {
        workspace_test::Fixture isolated_state;
        flow_tests::run(isolated_state.root, argc == 2 ? fs::path(argv[1]) : fs::path{});
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    if (SUCCEEDED(com))
    {
        CoUninitialize();
    }
    FreeLibrary(library);
    return result;
}
