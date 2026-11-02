// Compile the real controller/window procedure into a test host. No desktop input,
// visible user window or external application is automated by this test.
#define CPPLIVELAB_GUI_TEST
#include "../../src/gui/gui.cpp"
#include "cpplivelab/projects.hpp"
#include "../workspace_test_support.hpp"
#include "../ui_capture.hpp"
#include <fstream>
#include <iostream>
#include <functional>
#include <atomic>
#include <psapi.h>

namespace flow_tests
{
bool baseline_worker_environment = false;

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
    if (title == L"Restart EasierCpp" && !driver.answered)
    {
        driver.answered = true;
        PostMessageW(window, WM_COMMAND, driver.approve ? IDOK : IDCANCEL, 0);
    }
    if (GetTickCount64() - driver.started > 5000)
    {
        driver.failure = "Restart did not reach the controlled task boundary";
        driver.release = true;
        cpplivelab::cancel_library_task();
        if (title == L"Restart EasierCpp")
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

void benchmark_project(const fs::path& project, unsigned samples)
{
    std::ifstream marker(project / L".easiercpp-benchmark");
    std::string ownership;
    std::getline(marker, ownership);
    require(ownership == "EasierCpp owned Phase 3 copy" && samples >= 3 && samples <= 30,
        "Benchmark requires a marked disposable copy and 3-30 samples");
    const auto source = project / L"Source/main.cpp";
    std::ifstream input(source, std::ios::binary);
    const std::string original{std::istreambuf_iterator<char>(input), {}};
    input.close();
    require(!original.empty(), "Benchmark source is missing");
    SetEnvironmentVariableW(L"EASIERCPP_PROFILE_BUILD", L"1");
    Host host(project);
    auto& app = host.app;
    // The script marks and owns this complete copy. Original source-generating
    // CMake commands are approved only inside the disposable benchmark project.
    app.configure_writes_authorized = true;
    const auto cold_started = GetTickCount64();
    app.command(RunButton);
    host.running();
    const auto find_window = [&]()
    {
        struct Search
        {
            DWORD process;
            HWND window = nullptr;
        } search{app.session.process_id()};
        EnumWindows(
            [](HWND window, LPARAM context) -> BOOL
            {
                auto& search = *reinterpret_cast<Search*>(context);
                DWORD process = 0;
                GetWindowThreadProcessId(window, &process);
                if (process == search.process && IsWindowVisible(window) &&
                    GetWindow(window, GW_OWNER) == nullptr)
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
            return find_window() != nullptr;
        },
        "FarmGame initial window missing");
    std::cout << "FARM_COLD,manual_run_to_window_ms=" << GetTickCount64() - cold_started << '\n';
    const auto dump_profile = [&](const std::string& sample)
    {
        const auto output = encode_utf8(app.build_diagnostics);
        const auto start = output.rfind("EASIERCPP_PROFILE worker_start_tick=");
        require(start != std::string::npos, "FarmGame worker profiling missing");
        std::istringstream lines(output.substr(start));
        for (std::string line; std::getline(lines, line);)
        {
            if (line.starts_with("EASIERCPP_PROFILE "))
            {
                std::cout << "FARM_PROFILE," << sample << ',' << line.substr(18) << '\n';
            }
            else if (line.find("incremental") != std::string::npos ||
                     line.find("LINK :") != std::string::npos)
            {
                std::cout << "FARM_LINK," << sample << ',' << line << '\n';
            }
        }
    };
    dump_profile("cold");
    // GLFW can expose a visible HWND before its initial resize/centering finishes.
    // Let startup settle before establishing the manual placement to restore.
    // This is outside all timed save-to-window samples.
    const auto initial_visible = GetTickCount64();
    host.await(
        [&]
        {
            return GetTickCount64() - initial_visible >= 500;
        },
        "FarmGame startup did not settle");
    SetWindowPos(find_window(), nullptr, 80, 80, 900, 650, SWP_NOZORDER | SWP_NOACTIVATE);
    RECT expected{};
    GetWindowRect(find_window(), &expected);
    if (!app.auto_build.enabled())
    {
        app.command(AutoRunButton);
    }
    std::cout << "FARM_TIMING,sample,slot,detect,debounce,build,shutdown,activation,preparation,"
                 "startup,window,total,peak_worker_bytes\n";
    for (unsigned sample = 0; sample < samples + 3; ++sample)
    {
        std::map<fs::path, fs::file_time_type> pch_times;
        if (sample >= 2)
        {
            for (const auto& file :
                fs::recursive_directory_iterator(cpplivelab::manager_state_directory()))
            {
                if (file.is_regular_file() && file.path().extension() == L".pch")
                {
                    pch_times.emplace(file.path(), file.last_write_time());
                }
            }
        }
        const auto old_process = app.session.process_id();
        std::map<std::string, ULONGLONG> trace;

        struct ResetTrace
        {
            Application& app;

            ~ResetTrace()
            {
                app.auto_run_trace = {};
            }
        } reset{app};

        app.auto_run_trace = [&](const char* stage)
        {
            trace.try_emplace(stage, GetTickCount64());
        };
        const auto saved = GetTickCount64();
        if (sample == samples + 2)
        {
            std::ofstream(project / L"Include/Config.h", std::ios::app)
                << "\n// Phase 3 header sample\n";
        }
        else
        {
            std::ofstream(source, std::ios::binary)
                << original << "\n// Phase 3 sample " << sample << '\n';
        }
        SIZE_T peak = 0;
        host.await(
            [&]
            {
                for (const auto process : app.replacement_build.active_process_ids())
                {
                    cpplivelab::windows_support::UniqueHandle handle;
                    handle.reset(OpenProcess(
                        PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, process));
                    PROCESS_MEMORY_COUNTERS memory{sizeof(memory)};
                    if (handle.value &&
                        K32GetProcessMemoryInfo(handle.value, &memory, sizeof(memory)))
                    {
                        peak = (std::max)(peak, memory.PeakWorkingSetSize);
                    }
                }
                return trace.contains("restored") && app.session.process_id() != old_process &&
                       app.program_started && !app.restore_started && find_window() != nullptr;
            },
            "FarmGame replacement did not reach a restored window",
            180000);
        app.auto_run_trace = {};
        RECT actual{};
        GetWindowRect(find_window(), &actual);
        if (!EqualRect(&expected, &actual))
        {
            std::cerr << "Placement expected " << expected.left << ',' << expected.top << ','
                      << expected.right << ',' << expected.bottom << "; actual " << actual.left
                      << ',' << actual.top << ',' << actual.right << ',' << actual.bottom << '\n';
            throw std::runtime_error("FarmGame replacement lost window placement");
        }
        for (const auto& [path, written] : pch_times)
        {
            require(fs::last_write_time(path) == written,
                "An ordinary source/project-header edit unnecessarily regenerated stable PCH");
        }
        if (sample < 2)
        {
            continue;
        }
        const auto index =
            sample == samples + 2 ? std::string("header") : std::to_string(sample - 2);
        std::cout << "FARM_TIMING," << index << ','
                  << encode_utf8(app.build_directory().filename().wstring()) << ','
                  << trace.at("detect") - saved << ',' << trace.at("build") - trace.at("detect")
                  << ',' << trace.at("shutdown") - trace.at("build") << ','
                  << trace.at("old_stopped") - trace.at("shutdown") << ','
                  << trace.at("slot_activated") - trace.at("old_stopped") << ','
                  << trace.at("startup") - trace.at("slot_activated") << ','
                  << trace.at("started") - trace.at("startup") << ','
                  << trace.at("restored") - trace.at("started") << ','
                  << trace.at("restored") - saved << ',' << peak << '\n';
        dump_profile(index);
        std::cout << "FARM_PCH_REUSE," << index << ",unchanged_files=" << pch_times.size() << '\n';
        std::cout << "FARM_WORKER_START," << index << ',' << trace.at("build") << '\n'
                  << std::flush;
    }
    app.stop();
    host.idle();
    std::uintmax_t pch = 0, ilk = 0, pdb = 0;
    for (const auto& file : fs::recursive_directory_iterator(cpplivelab::manager_state_directory()))
    {
        if (file.is_regular_file())
        {
            if (file.path().extension() == L".pch")
            {
                pch += file.file_size();
            }
            if (file.path().extension() == L".ilk")
            {
                ilk += file.file_size();
            }
            if (file.path().extension() == L".pdb")
            {
                pdb += file.file_size();
            }
        }
    }
    std::cout << "FARM_DISK,pch=" << pch << ",ilk=" << ilk << ",pdb=" << pdb << '\n';
}

HWND process_window(DWORD process)
{
    struct Search
    {
        DWORD process;
        HWND found = nullptr;
    } search{process};

    EnumWindows(
        [](HWND window, LPARAM parameter) -> BOOL
        {
            auto& search = *reinterpret_cast<Search*>(parameter);
            DWORD process = 0;
            GetWindowThreadProcessId(window, &process);
            if (process == search.process && IsWindowVisible(window) &&
                !GetWindow(window, GW_OWNER))
            {
                search.found = window;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&search));
    return search.found;
}

HWND managed_window(Application& app)
{
    return process_window(app.session.process_id());
}

void replacement_benchmark(const fs::path& fixtures)
{
    const auto project = fixtures / L"replacement benchmark";
    fs::create_directories(project);
    std::ofstream(project / L"CMakeLists.txt")
        << "cmake_minimum_required(VERSION 3.20)\nproject(ReplacementBenchmark LANGUAGES "
           "CXX)\nadd_executable(app main.cpp)\ntarget_compile_features(app PRIVATE "
           "cxx_std_20)\ntarget_link_libraries(app PRIVATE user32)\n";
    std::ifstream sample(
        fs::path(__FILE__).parent_path().parent_path() / L"consumers/native_window.cpp");
    const std::string source{std::istreambuf_iterator<char>(sample), {}};
    require(!source.empty(), "Benchmark native fixture missing");
    const auto file = project / L"main.cpp";
    std::ofstream(file) << source;
    Host host(project);
    auto& app = host.app;
    app.auto_run_preferences.placement_memory = cpplivelab::PlacementMemory::Session;
    app.command(RunButton);
    host.await(
        [&]
        {
            return app.program_started && managed_window(app);
        },
        "Benchmark initial window missing");
    SetWindowPos(managed_window(app), nullptr, 100, 120, 700, 450, SWP_NOZORDER | SWP_NOACTIVATE);
    RECT expected{};
    GetWindowRect(managed_window(app), &expected);
    app.command(AutoRunButton);
    std::cout << "REPLACEMENT_BENCH,mode,run,total_ms,save_to_window_ms,window_gap_ms,build_ms\n"
              << std::flush;
    for (unsigned index = 0; index < 22; ++index)
    {
        // Two excluded warmups configure both slots. Alternate modes, reversing
        // order in each pair, to reduce cache/temperature ordering bias.
        const unsigned measured = index >= 2 ? index - 2 : 0;
        const bool seamless =
            index >= 2 && ((measured / 2) % 2 ? measured % 2 == 0 : measured % 2 != 0);
        app.auto_run_preferences.replacement_mode =
            seamless ? cpplivelab::ReplacementMode::Seamless : cpplivelab::ReplacementMode::Safe;
        const auto old_process = app.session.process_id();
        const auto old_window = managed_window(app);
        const auto start = std::chrono::steady_clock::now();

        struct ResetTrace
        {
            Application& app;

            ~ResetTrace()
            {
                app.auto_run_trace = {};
            }
        } reset_trace{app};

        const auto elapsed = [&]
        {
            return std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start)
                .count();
        };
        double old_gone = -1, new_visible = -1, build_start = -1, build_end = -1;
        app.auto_run_trace = [&](const char* stage)
        {
            if (std::string_view(stage) == "build")
            {
                build_start = elapsed();
            }
        };
        std::ofstream(file) << source << "\n// benchmark source save " << index << '\n';
        host.await(
            [&]
            {
                const auto now = elapsed();
                if (old_gone < 0 && (!IsWindow(old_window) || !IsWindowVisible(old_window)))
                {
                    old_gone = now;
                }
                const auto candidate = app.candidate_pending
                                           ? process_window(app.candidate_session.process_id())
                                           : nullptr;
                const auto active =
                    app.session.process_id() != old_process ? managed_window(app) : nullptr;
                if (new_visible < 0 && (candidate || active))
                {
                    new_visible = now;
                }
                if (build_start >= 0 && build_end < 0 && !app.replacement_building)
                {
                    build_end = now;
                }
                return app.session.process_id() != old_process && app.program_started &&
                       !app.candidate_pending && !app.restore_started && managed_window(app) &&
                       old_gone >= 0;
            },
            "Replacement benchmark timed out",
            90000);
        const auto total = elapsed();
        app.auto_run_trace = {};
        RECT actual{};
        GetWindowRect(managed_window(app), &actual);
        require(EqualRect(&expected, &actual), "Benchmark replacement lost placement");
        require(build_start >= 0 && build_end >= build_start && new_visible >= 0,
            "Benchmark timing event missing");
        std::cout << "REPLACEMENT_BENCH,"
                  << (index < 2     ? "warmup"
                         : seamless ? "Seamless"
                                    : "Safe")
                  << ',' << (index < 2 ? index + 1 : measured / 2 + 1) << ',' << total << ','
                  << new_visible << ',' << (std::max)(0.0, new_visible - old_gone) << ','
                  << build_end - build_start << '\n'
                  << std::flush;
    }
    app.command(StopButton);
    host.idle();
}

void placement_workflow(const fs::path& fixtures)
{
    const auto project = fixtures / L"placement";
    fs::create_directories(project);
    std::ofstream(project / L"CMakeLists.txt")
        << "cmake_minimum_required(VERSION 3.20)\nproject(Placement LANGUAGES "
           "CXX)\nadd_executable(app main.cpp)\ntarget_link_libraries(app PRIVATE "
           "user32)\ntarget_compile_features(app PRIVATE cxx_std_20)\n";
    fs::copy_file(fs::path(__FILE__).parent_path().parent_path() / L"consumers/native_window.cpp",
        project / L"main.cpp");
    RECT expected{};
    {
        Host host(project);
        auto& app = host.app;
        app.auto_run_preferences.placement_memory = cpplivelab::PlacementMemory::Session;
        auto launch = [&]
        {
            app.command(RunButton);
            host.await(
                [&]
                {
                    return app.program_started && managed_window(app) && !app.restore_started;
                },
                "Manual placement launch failed");
        };
        auto check = [&]
        {
            RECT actual{};
            GetWindowRect(managed_window(app), &actual);
            require(EqualRect(&actual, &expected),
                "Manual launch lost the last valid session placement");
        };
        launch();
        SetWindowPos(
            managed_window(app), nullptr, 120, 140, 700, 440, SWP_NOZORDER | SWP_NOACTIVATE);
        GetWindowRect(managed_window(app), &expected);
        app.command(StopButton);
        host.idle();
        launch();
        check();
        // Obtain two stable polling samples before abrupt window disappearance.
        const auto stable_at = GetTickCount64();
        host.await(
            [&]
            {
                return GetTickCount64() - stable_at >= 1200;
            },
            "Placement sampling timer stalled");
        PostMessageW(managed_window(app), WM_CLOSE, 0, 0);
        host.idle();
        launch();
        check();
        const auto crash_at = GetTickCount64();
        host.await(
            [&]
            {
                return GetTickCount64() - crash_at >= 1200;
            },
            "Crash sampling timer stalled");
        app.session.stop();
        host.idle();
        launch();
        check();
        app.command(StopButton);
        host.idle();
        app.auto_run_preferences.placement_memory = cpplivelab::PlacementMemory::Off;
        app.auto_run_preferences.restore_windows = false;
        launch();
        RECT natural{};
        GetWindowRect(managed_window(app), &natural);
        require(!EqualRect(&natural, &expected), "Off applied historical placement");
        app.auto_run_preferences.placement_memory = cpplivelab::PlacementMemory::Persistent;
        app.auto_run_preferences.restore_windows = true;
        cpplivelab::save_auto_run_preferences(project, app.auto_run_preferences);
        ShowWindow(managed_window(app), SW_SHOWMAXIMIZED);
        app.command(StopButton);
        host.idle();
    }
    {
        Host reopened(project);
        reopened.app.command(RunButton);
        reopened.await(
            [&]
            {
                return reopened.app.program_started && managed_window(reopened.app) &&
                       !reopened.app.restore_started;
            },
            "Persistent relaunch failed");
        require(IsZoomed(managed_window(reopened.app)),
            "New manager instance lost persisted maximized placement");
        reopened.app.command(StopButton);
        reopened.idle();
    }
    std::cout << "Placement: Stop/Run, user close/Run, crash/Run, session, Off and persistent new "
                 "manager/maximized passed\n";
}

void window_management(const fs::path& fixtures)
{
    const auto project = fixtures / L"replacement";
    fs::create_directories(project);
    std::ofstream(project / L"CMakeLists.txt")
        << "cmake_minimum_required(VERSION 3.20)\nproject(Replacement LANGUAGES CXX)\n"
           "add_executable(app main.cpp)\ntarget_compile_features(app PRIVATE cxx_std_20)\n"
           "target_link_libraries(app PRIVATE user32)\n";
    std::ifstream sample(
        fs::path(__FILE__).parent_path().parent_path() / L"consumers/native_window.cpp");
    std::string source{std::istreambuf_iterator<char>(sample), {}};
    require(!source.empty(), "Native replacement fixture is missing");
    source.insert(source.find("const auto window = CreateWindowW"), "Sleep(600);\n    ");
    const auto file = project / L"main.cpp";
    std::ofstream(file) << source;
    Host host(project);
    auto& app = host.app;
    app.command(RunButton);
    host.running();
    auto windows = [&]
    {
        return cpplivelab::capture_application_windows(app.session.active_process_ids());
    };
    host.await(
        [&]
        {
            return !windows().empty();
        },
        "Initial native window missing");
    app.auto_run_preferences.replacement_mode = cpplivelab::ReplacementMode::Seamless;
    SetWindowPos(managed_window(app), nullptr, 100, 140, 720, 460, SWP_NOZORDER | SWP_NOACTIVATE);
    RECT expected_early{};
    GetWindowRect(managed_window(app), &expected_early);
    app.command(AutoRunButton);
    for (int cycle = 0; cycle < 2; ++cycle)
    {
        const auto old = app.session.process_id();
        std::ofstream(file) << source << "\n// replacement " << cycle << '\n';
        host.await(
            [&]
            {
                return app.candidate_pending;
            },
            "Candidate was never launched",
            90000);
        require(app.session.running() && app.session.process_id() == old,
            "Old application closed before candidate startup readiness");
        require(!IsWindowEnabled(app.find_control(ProgramRestartButton)),
            "Manual Restart remained enabled during candidate handoff");
        const auto pending_slot = app.replacement_options.recovery_slot;
        host.await(
            [&]
            {
                return !app.candidate_restored.empty() && !app.candidate_ready;
            },
            "Candidate was not positioned before readiness");
        RECT early{};
        const auto replacement_window = process_window(app.candidate_session.process_id());
        GetWindowRect(replacement_window, &early);
        require(EqualRect(&early, &expected_early) && app.session.running(),
            "Early placement did not precede old-process shutdown");
        std::cout << "EARLY_PLACEMENT,cycle=" << cycle
                  << ",visible_to_position_ms=" << GetTickCount64() - app.candidate_visible << '\n';
        SetWindowPos(
            replacement_window, nullptr, 420, 340, 500, 350, SWP_NOZORDER | SWP_NOACTIVATE);
        host.await(
            [&]
            {
                RECT corrected{};
                GetWindowRect(replacement_window, &corrected);
                return EqualRect(&corrected, &expected_early);
            },
            "Application startup recentering was not corrected");
        app.command(ProgramRestartButton);
        require(app.replacement_options.recovery_slot == pending_slot && !app.replacement_stopping,
            "Manual Restart interfered with candidate startup");
        host.await(
            [&]
            {
                return !app.candidate_pending && app.session.process_id() != old &&
                       app.session.running();
            },
            "Seamless handoff did not complete",
            30000);
        require(!app.candidate_session.running(), "Old process job survived completed handoff");
    }
    const auto healthy = app.session.process_id();
    std::ofstream(file) << "deliberately invalid C++\n";
    host.await(
        [&]
        {
            return read_control_text(app.system_console.window)
                       .find(L"Build failed — previous application is still running.") !=
                   std::wstring::npos;
        },
        "Compilation failure not reported",
        90000);
    require(app.session.running() && app.session.process_id() == healthy,
        "Failed build replaced healthy process");
    auto crash = source;
    crash.insert(crash.find("const auto instance = GetModuleHandleW"), "return 7;\n    ");
    std::ofstream(file) << crash;
    host.await(
        [&]
        {
            return read_control_text(app.system_console.window)
                       .find(L"Replacement startup exited (code 7)") != std::wstring::npos;
        },
        "Startup crash not diagnosed",
        90000);
    require(app.session.running() && app.session.process_id() == healthy &&
                !app.candidate_session.running(),
        "Startup crash killed previous application or leaked candidate");
    // A failed CreateProcess must preserve the current job as well.
    app.start_candidate(project / L"missing.exe");
    require(!app.candidate_pending && app.session.running() && app.session.process_id() == healthy,
        "Launch failure did not retain the previous process");
    std::ofstream(file) << source << "\n// recovered\n";
    host.await(
        [&]
        {
            return app.session.process_id() != healthy && app.session.running() &&
                   !app.candidate_pending;
        },
        "Replacement did not recover after startup failure",
        90000);
    auto mutex_source = source;
    mutex_source.insert(mutex_source.find("const auto instance = GetModuleHandleW"),
        "const auto mutex = CreateMutexW(nullptr, TRUE, L\"Local\\\\EasierCppReplacementTest" +
            std::to_string(GetCurrentProcessId()) +
            "\");\nif (GetLastError() == ERROR_ALREADY_EXISTS) return 9;\n");
    auto replace = [&](const std::string& contents)
    {
        const auto previous = app.session.process_id();
        std::ofstream(file) << contents;
        host.await(
            [&]
            {
                return app.session.process_id() != previous && app.session.running() &&
                       !app.candidate_pending;
            },
            "Expected replacement did not complete",
            90000);
    };
    replace(mutex_source);
    const auto exclusive = app.session.process_id();
    std::ofstream(file) << mutex_source << "\n// single-instance replacement\n";
    host.await(
        [&]
        {
            return read_control_text(app.system_console.window)
                       .find(L"Replacement startup exited (code 9)") != std::wstring::npos;
        },
        "Single-instance conflict was not diagnosed",
        90000);
    require(app.session.process_id() == exclusive && app.session.running(),
        "Single-instance conflict killed previous application");
    app.auto_run_preferences.replacement_mode = cpplivelab::ReplacementMode::Safe;
    replace(mutex_source + "\n// Safe compatibility fallback\n");
    app.auto_run_preferences.replacement_mode = cpplivelab::ReplacementMode::Seamless;
    const auto before_timeout = app.session.process_id();
    std::ofstream(file) << "#include <windows.h>\nint main(){Sleep(30000);}\n";
    host.await(
        [&]
        {
            return read_control_text(app.system_console.window)
                       .find(L"Replacement readiness timed out") != std::wstring::npos;
        },
        "Missing native window did not time out",
        90000);
    host.await(
        [&]
        {
            return !app.candidate_pending;
        },
        "Timed-out candidate cleanup did not drain");
    require(app.session.process_id() == before_timeout && app.session.running() &&
                !app.candidate_session.running(),
        "Readiness timeout killed previous application or leaked candidate");
    std::ofstream(file) << source << "\n// first rapid save\n";
    host.await(
        [&]
        {
            return app.candidate_pending;
        },
        "Rapid-save candidate not started",
        90000);
    const auto obsolete = app.candidate_session.process_id();
    std::ofstream(file) << source << "\n// latest rapid save\n";
    host.await(
        [&]
        {
            return app.session.process_id() != before_timeout &&
                   app.session.process_id() != obsolete && app.session.running() &&
                   !app.candidate_pending;
        },
        "Latest rapid save was lost or obsolete candidate promoted",
        90000);
    std::ofstream(file) << source << "\n// Stop during candidate startup\n";
    host.await(
        [&]
        {
            return app.candidate_pending;
        },
        "Stop-during-startup candidate not launched",
        90000);
    app.command(StopButton);
    host.idle();
    require(!app.candidate_session.running(), "Stop leaked replacement job");
    app.command(AutoRunButton);
    // Console readiness must not require a native window; candidate output and input
    // become the active console only after ownership is exchanged.
    const std::string console =
        "#include <iostream>\n#include <string>\nint main(){std::string s; std::cout << \"Ready:\" "
        "<< std::flush; while(std::getline(std::cin,s)) std::cout << s << std::endl;}\n";
    std::ofstream(file) << console;
    app.command(RunButton);
    host.running();
    host.prompt(L"Ready:");
    const auto old_console = app.session.process_id();
    app.command(AutoRunButton);
    std::ofstream(file) << console << "\n// console replacement\n";
    host.await(
        [&]
        {
            return app.session.process_id() != old_console && app.session.running() &&
                   !app.candidate_pending;
        },
        "Console replacement waited for a GUI window",
        90000);
    host.send(L"replacement input");
    host.prompt(L"replacement input");
    for (const auto placement :
        {cpplivelab::PlacementMemory::Session, cpplivelab::PlacementMemory::Off})
    {
        for (const auto mode :
            {cpplivelab::ReplacementMode::Safe, cpplivelab::ReplacementMode::Seamless})
        {
            app.auto_run_preferences.placement_memory = placement;
            app.auto_run_preferences.restore_windows =
                placement != cpplivelab::PlacementMemory::Off;
            app.auto_run_preferences.replacement_mode = mode;
            replace(console + "\n// preference combination " +
                    std::to_string(static_cast<int>(placement)) + " " +
                    std::to_string(static_cast<int>(mode)) + "\n");
            host.send(L"combination input");
            host.prompt(L"combination input");
        }
    }
    app.command(StopButton);
    host.idle();
    std::cout << "Seamless: slow native startup, repeated swaps, compile/launch/startup failure "
                 "retention, recovery, console I/O and Stop passed\n";
}

struct WorkerDialogDriver
{
    bool optimize = false;
    bool cancel_benchmark = false;
    bool acted = false;
    bool finished = false;
    bool snapshot_failed = false;
    bool benchmark_seen = false;
    ULONGLONG compilation_seen = 0;
    std::wstring selection = L"1";
    ULONGLONG started = GetTickCount64();
};

WorkerDialogDriver* worker_dialog_driver = nullptr;

BOOL CALLBACK drive_worker_window(HWND window, LPARAM)
{
    auto& driver = *worker_dialog_driver;
    const auto title = read_control_text(window);
    if (driver.optimize && title == L"Optimize Workers")
    {
        driver.benchmark_seen = true;
    }
    if (driver.cancel_benchmark && title == L"Optimize Workers")
    {
        const auto progress = read_control_text(GetDlgItem(window, 1001));
        if (!driver.compilation_seen && progress.find(L"jobs, trial") != std::wstring::npos)
        {
            driver.compilation_seen = GetTickCount64();
        }
        if (driver.compilation_seen && GetTickCount64() - driver.compilation_seen >= 300)
        {
            if (!driver.acted)
            {
                try
                {
                    ui_test::capture(
                        window, application_directory() / L"worker-optimizer-progress.png");
                }
                catch (...)
                {
                    driver.snapshot_failed = true;
                }
            }
            driver.acted = true;
            PostMessageW(window, WM_COMMAND, IDCANCEL, 0);
        }
    }
    if (title == L"Preferences")
    {
        if (!driver.acted)
        {
            try
            {
                ui_test::capture(window, application_directory() / L"worker-preferences.png");
            }
            catch (...)
            {
                driver.snapshot_failed = true;
            }
            driver.acted = true;
            if (driver.optimize)
            {
                PostMessageW(window, WM_COMMAND, 1800, 0);
            }
            else
            {
                SendDlgItemMessageW(window,
                    2002,
                    CB_SELECTSTRING,
                    static_cast<WPARAM>(-1),
                    reinterpret_cast<LPARAM>(driver.selection.c_str()));
                SendMessageW(window, WM_COMMAND, MAKEWPARAM(2002, CBN_SELCHANGE), 0);
                PostMessageW(window, WM_COMMAND, IDCANCEL, 0);
            }
        }
        else if (driver.optimize && driver.benchmark_seen && !cpplivelab::library_task_active())
        {
            driver.finished = cpplivelab::load_build_workers().recommended &&
                              read_control_text(GetDlgItem(window, 2002)) ==
                                  std::to_wstring(cpplivelab::load_build_workers().recommended);
            try
            {
                ui_test::capture(
                    window, application_directory() / L"worker-preferences-recommended.png");
            }
            catch (...)
            {
                driver.snapshot_failed = true;
            }
            PostMessageW(window, WM_COMMAND, IDCANCEL, 0);
        }
        if (GetTickCount64() - driver.started > 240000)
        {
            PostMessageW(window, WM_COMMAND, IDCANCEL, 0);
        }
    }
    return TRUE;
}

void CALLBACK worker_dialog_timer(HWND, UINT, UINT_PTR, DWORD)
{
    if (worker_dialog_driver)
    {
        EnumThreadWindows(GetCurrentThreadId(), drive_worker_window, 0);
    }
}

void compilation_workers(const fs::path& fixtures)
{
    Project project;
    Host host(project.root);
    auto& app = host.app;
    WorkerDialogDriver manual;
    worker_dialog_driver = &manual;
    auto timer = SetTimer(nullptr, 0, 20, worker_dialog_timer);
    app.command(WindowPreferencesMenu);
    KillTimer(nullptr, timer);
    worker_dialog_driver = nullptr;
    require(manual.acted && cpplivelab::load_build_workers().count == 1,
        "Native worker selector did not persist immediately after Cancel");
    WorkerDialogDriver inherited;
    inherited.selection = L"Build-system default";
    worker_dialog_driver = &inherited;
    timer = SetTimer(nullptr, 0, 20, worker_dialog_timer);
    app.command(WindowPreferencesMenu);
    KillTimer(nullptr, timer);
    worker_dialog_driver = nullptr;
    require(cpplivelab::load_build_workers().count == 0,
        "Native-default selector did not restore build-system behavior");
    cpplivelab::save_build_workers({1, 0});
    WorkerDialogDriver cancel;
    cancel.cancel_benchmark = true;
    worker_dialog_driver = &cancel;
    timer = SetTimer(nullptr, 0, 20, worker_dialog_timer);
    const bool cancelled_result = app.optimize_workers(app.window_handle);
    KillTimer(nullptr, timer);
    worker_dialog_driver = nullptr;
    require(cancel.acted && cancel.compilation_seen && !cancel.snapshot_failed &&
                !cancelled_result && cpplivelab::load_build_workers().count == 1 &&
                !cpplivelab::library_task_active(),
        "Benchmark cancellation changed the setting or leaked its task");
    app.worker_benchmark_cli = application_directory() / L"session_fixture.exe";
    require(!app.optimize_workers(app.window_handle) && cpplivelab::load_build_workers().count == 1,
        "Benchmark process failure changed previous compilation workers");
    app.worker_benchmark_cli.clear();
    WorkerDialogDriver optimize;
    optimize.optimize = true;
    worker_dialog_driver = &optimize;
    timer = SetTimer(nullptr, 0, 20, worker_dialog_timer);
    app.command(WindowPreferencesMenu);
    KillTimer(nullptr, timer);
    worker_dialog_driver = nullptr;
    const auto result = cpplivelab::load_build_workers();
    require(optimize.finished && !optimize.snapshot_failed && !manual.snapshot_failed &&
                result.count && result.count == result.recommended &&
                result.count <= cpplivelab::maximum_build_workers(),
        ("Optimize Workers button did not apply measured recommendation. Build & System:\n" +
            encode_utf8(read_control_text(app.system_console.window)))
            .c_str());
    const auto results = cpplivelab::manager_state_directory() / L"worker-benchmark.csv";
    require(fs::is_regular_file(results), "Worker measurements were not saved");
    fs::copy_file(results,
        application_directory() / L"worker-benchmark-test.csv",
        fs::copy_options::overwrite_existing);
    std::cout << "WORKER_RECOMMENDATION," << result.count
              << ",maximum=" << cpplivelab::maximum_build_workers() << '\n';
    std::ifstream measurements(results);
    std::cout << measurements.rdbuf();
    WorkerDialogDriver override_count;
    worker_dialog_driver = &override_count;
    timer = SetTimer(nullptr, 0, 20, worker_dialog_timer);
    app.command(WindowPreferencesMenu);
    KillTimer(nullptr, timer);
    worker_dialog_driver = nullptr;
    require(cpplivelab::load_build_workers().count == 1 &&
                cpplivelab::load_build_workers().recommended == result.recommended,
        "Manual override failed after optimization");
    require(fs::is_empty(cpplivelab::manager_state_directory() / L"worker-benchmarks"),
        "Benchmark temporary directories were not cleaned");
    std::cout << "Compilation workers: native preferences, cancellation, failure, synthetic "
                 "optimization, recommendation and override passed\n";
}

void run(const fs::path& fixtures, const fs::path& snapshots = {})
{
    cpplivelab::save_build_workers({1, 0});
    SetEnvironmentVariableW(L"CPPLIVELAB_TEST_RUNTIME", L"owned-runtime-value");
    Project project;
    const auto pch_recipe = fixtures / L"console-pch.cmake";
    const auto pch_header = project.root / L"stable_pch.hpp";
    const std::string pch_bytes = "#pragma once\n#include <iostream>\n#include <string>\n";
    std::ofstream(pch_header) << pch_bytes;
    std::ofstream(pch_recipe)
        << "easiercpp_target_pch(app HEADERS stable_pch.hpp <iostream> <string>)\n";
    cpplivelab::configure_project_pch(project.root, L"Release", pch_recipe);
    cpplivelab::configure_project_pch(project.root, L"Debug", pch_recipe);
    Host host(project.root, snapshots);
    require(
        read_control_text(host.app.window_handle).find(L"EasierCpp \u2014 Make Your Life Easier") !=
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
            require(action.second != AutoBuildButton &&
                        action.first.find(L"Auto Build") == std::wstring::npos,
                "Retired Auto Build action remains in a navigation popup");
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
    require(read_control_text(app.system_console.window).find(L"Compilation workers: 1") !=
                std::wstring::npos,
        "Manual Debug build did not use sequential compilation");
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
    const auto selected_workers = (std::min)(2u, cpplivelab::maximum_build_workers());
    cpplivelab::save_build_workers({selected_workers, 0});
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
    require(read_control_text(app.system_console.window)
                    .find(L"Compilation workers: " + std::to_wstring(selected_workers)) !=
                std::wstring::npos,
        "Release build did not use updated compilation workers");
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
    require(GetDlgItem(app.window_handle, AutoBuildButton) == nullptr,
        "Retired Auto Build control is still present");
    bool obsolete_caption = false;
    EnumChildWindows(
        app.window_handle,
        [](HWND control, LPARAM context) -> BOOL
        {
            wchar_t class_name[64]{};
            GetClassNameW(control, class_name, 64);
            // Do not search console contents: project output can contain any text.
            if (std::wstring_view(class_name) == L"Button" ||
                std::wstring_view(class_name) == L"Static")
            {
                if (read_control_text(control).find(L"Auto Build") != std::wstring::npos)
                {
                    *reinterpret_cast<bool*>(context) = true;
                }
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&obsolete_caption));
    require(!obsolete_caption, "Retired Auto Build caption remains in the workspace");
    std::ofstream(source_path) << original << "\n// Auto Run OFF\n";
    const auto off_at = GetTickCount64();
    while (GetTickCount64() - off_at < 750)
    {
        pump();
        Sleep(5);
    }
    require(app.session.process_id() == before_error && app.session.running() &&
                !app.replacement_building && !app.auto_build.pending(),
        "Auto Run OFF performed automatic work");
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
    const auto pch_failure_id = app.session.process_id();
    std::ofstream(pch_header) << pch_bytes << "\n#error EASIERCPP_EXPECTED_PCH_FAILURE\n";
    host.await(
        [&]
        {
            return !app.replacement_building &&
                   read_control_text(app.system_console.window)
                           .find(L"EASIERCPP_EXPECTED_PCH_FAILURE") != std::wstring::npos;
        },
        "Failed PCH replacement was not reported",
        90000);
    require(app.session.running() && app.session.process_id() == pch_failure_id,
        "PCH compilation failure terminated the working application");
    std::ofstream(pch_header) << pch_bytes;
    host.await(
        [&]
        {
            return app.phase == Application::Phase::Running &&
                   app.session.process_id() != pch_failure_id;
        },
        "PCH header repair did not restart automatically",
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
    app.command(AutoRunButton);
    const auto enabled_at = GetTickCount64();
    while (GetTickCount64() - enabled_at < 400)
    {
        pump();
        Sleep(5);
    }
    require(!app.operation_in_progress, "Enabling Auto Run immediately launched an idle project");
    std::ofstream(source_path) << original << "\n// Launch from idle after relevant save\n";
    host.running();
    host.prompt(L"Your name:");
    host.send(L"Idle Auto");
    host.prompt(L"First number:");
    host.send(L"4");
    host.prompt(L"Second number:");
    host.send(L"6");
    host.idle();
    require(read_control_text(app.program_console.window).find(L"Hello, Idle Auto! Sum: 10") !=
                std::wstring::npos,
        "Auto Run from idle lost interactive input/output");
    std::ofstream(source_path) << original << "\n// Cancel idle automatic build with OFF\n";
    host.await(
        [&]
        {
            return app.phase == Application::Phase::Building;
        },
        "Idle automatic build did not start",
        90000);
    app.command(AutoRunButton);
    host.idle();
    require(!app.program_started && !app.auto_build.enabled(),
        "Turning Auto Run OFF during an idle build launched a program");
    app.command(AutoRunButton);
    std::ofstream(source_path) << original
                               << "\n// Explicit Run must survive turning automation OFF\n";
    host.await(
        [&]
        {
            return app.phase == Application::Phase::Building;
        },
        "Manual override fixture did not begin",
        90000);
    app.command(RunButton);
    app.command(AutoRunButton);
    host.running();
    require(!app.auto_run_preferences.enabled, "Manual Run unexpectedly enabled automatic mode");
    app.command(StopButton);
    host.idle();
    app.command(AutoRunButton);
    app.command(StopButton);
    const auto paused_builds = app.build_sequence;
    std::ofstream(source_path) << original << "\n// Save after explicit idle Stop\n";
    host.await(
        [&]
        {
            return app.build_sequence > paused_builds && !app.operation_in_progress;
        },
        "Auto Run did not finish the save after Stop",
        90000);
    require(!app.program_started && !app.auto_launch_armed,
        "Explicit Stop did not disarm automatic launch");
    app.command(RunButton);
    host.running();
    require(app.auto_launch_armed, "Manual Run did not rearm automatic launch");
    app.command(StopButton);
    host.idle();
    app.command(AutoRunButton);
    std::cout << "Auto Run: failed build retains PID, repair restarts, manual Restart reuses "
                 "artifact, interactive I/O passed\n";
    const auto window_project = fixtures / L"Native window project";
    fs::create_directory(window_project);
    std::ofstream(window_project / L"CMakeLists.txt")
        << "cmake_minimum_required(VERSION 3.20)\nproject(WindowFixture LANGUAGES CXX)\n"
           "add_executable(app main.cpp)\ntarget_compile_features(app PRIVATE "
           "cxx_std_20)\ntarget_link_libraries(app PRIVATE user32)\n";
    std::ifstream native_sample(
        fs::path(__FILE__).parent_path().parent_path() / L"consumers/native_window.cpp");
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
    const auto native_pch = fixtures / L"native-pch.cmake";
    std::ofstream(native_pch) << "easiercpp_target_pch(app HEADERS <string> <fstream>)\n"
                                 "easiercpp_target_pch(runtime_dependency HEADERS <vector>)\n";
    cpplivelab::configure_project_pch(window_project, L"Debug", native_pch);
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
    // Both slots are warm now. Time equivalent source-only edits through the real
    // controller, retaining the delayed-window/DLL/assets fixture for both modes.
    app.reuse_worker_environment = !baseline_worker_environment;
    for (int sample = 0; sample < 3; ++sample)
    {
        const auto compile_count =
            sample % 2 ? 1u : (std::min)(2u, cpplivelab::maximum_build_workers());
        cpplivelab::save_build_workers({compile_count, 0});
        std::map<fs::path, fs::file_time_type> cached_pch;
        for (const auto& item :
            fs::recursive_directory_iterator(cpplivelab::manager_state_directory()))
        {
            if (item.is_regular_file() && item.path().extension() == L".pch")
            {
                cached_pch.emplace(item.path(), item.last_write_time());
            }
        }
        const auto previous_id = app.session.process_id();
        const auto saved = GetTickCount64();
        std::map<std::string, ULONGLONG> trace;

        struct ResetTrace
        {
            Application& application;

            ~ResetTrace()
            {
                application.auto_run_trace = {};
            }
        } reset_trace{app};

        app.auto_run_trace = [&](const char* stage)
        {
            trace.try_emplace(stage, GetTickCount64());
        };
        std::ofstream(native_file) << native_source << "\n// Warm timing sample " << sample << '\n';
        host.await(
            [&]
            {
                return trace.contains("restored") && app.session.process_id() != previous_id &&
                       app.program_started && !app.restore_started && native_window() != nullptr;
            },
            "Warm timed Auto Run failed to restore its window",
            90000);
        app.auto_run_trace = {};
        for (const auto& [path, written] : cached_pch)
        {
            require(fs::last_write_time(path) == written,
                "Changing compilation workers unnecessarily regenerated PCH");
        }
        for (const auto* stage :
            {"detect", "build", "shutdown", "old_stopped", "startup", "started", "restored"})
        {
            require(trace.contains(stage), "Warm timing lost a lifecycle event");
        }
        require(
            app.session.running() && IsZoomed(native_window()), "Warm restart lost maximization");
        std::cout << "AUTO_RUN_TIMING," << (baseline_worker_environment ? "before" : "after") << ','
                  << sample << ",detect=" << trace.at("detect") - saved
                  << ",debounce=" << trace.at("build") - trace.at("detect")
                  << ",build=" << trace.at("shutdown") - trace.at("build")
                  << ",shutdown=" << trace.at("old_stopped") - trace.at("shutdown")
                  << ",preparation=" << trace.at("startup") - trace.at("old_stopped")
                  << ",startup=" << trace.at("started") - trace.at("startup")
                  << ",window=" << trace.at("restored") - trace.at("started")
                  << ",total=" << trace.at("restored") - saved << "\n";
    }
    const auto cancelled_pid = app.session.process_id();
    std::ofstream(native_file) << native_source << "\n// Cancel replacement with OFF\n";
    host.await(
        [&]
        {
            return app.replacement_building;
        },
        "Replacement did not begin before OFF");
    app.command(AutoRunButton);
    host.await(
        [&]
        {
            return !app.replacement_building;
        },
        "OFF did not cancel automatic worker",
        30000);
    require(app.session.running() && app.session.process_id() == cancelled_pid,
        "OFF cancelled the working application instead of its replacement build");
    app.command(AutoRunButton);
    std::ofstream(native_file) << native_source << "\n// Resume after OFF\n";
    host.await(
        [&]
        {
            return app.program_started && app.session.process_id() != cancelled_pid &&
                   !app.replacement_building && !app.restore_started && native_window();
        },
        "Auto Run did not resume after OFF/ON",
        90000);
    const auto exited_pid = app.session.process_id();
    std::ofstream(native_file) << native_source << "\n// Old program exits during compilation\n";
    host.await(
        [&]
        {
            return app.replacement_building;
        },
        "Exit-during-build fixture did not begin");
    PostMessageW(native_window(), WM_CLOSE, 0, 0);
    host.await(
        [&]
        {
            return !app.program_started && app.replacement_building;
        },
        "Owned replacement lost its busy state after the old program exited");
    require(!IsWindowEnabled(app.find_control(AddLibraryButton)) &&
                !IsWindowEnabled(app.find_control(ConfigurationSelector)),
        "Project mutations were enabled while an orphaned replacement build was active");
    host.await(
        [&]
        {
            return app.program_started && app.session.process_id() != exited_pid &&
                   !app.replacement_building && native_window();
        },
        "Successful replacement was lost when the old program exited during compilation",
        90000);
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
        flow_tests::baseline_worker_environment =
            argc == 3 && std::wstring_view(argv[2]) == L"--baseline-sdk";
        if (argc == 4 && std::wstring_view(argv[1]) == L"--benchmark-project")
        {
            flow_tests::benchmark_project(argv[2], static_cast<unsigned>(std::stoul(argv[3])));
        }
        else if (argc == 2 && std::wstring_view(argv[1]) == L"--compilation-workers")
        {
            flow_tests::compilation_workers(isolated_state.root);
        }
        else if (argc == 2 && std::wstring_view(argv[1]) == L"--benchmark-replacement")
        {
            flow_tests::replacement_benchmark(isolated_state.root);
        }
        else if (argc == 2 && std::wstring_view(argv[1]) == L"--window-management")
        {
            flow_tests::window_management(isolated_state.root);
            flow_tests::placement_workflow(isolated_state.root);
        }
        else
        {
            flow_tests::run(isolated_state.root, argc >= 2 ? fs::path(argv[1]) : fs::path{});
        }
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
