// Compile the real controller/window procedure into a test host. No desktop input,
// visible user window or external application is automated by this test.
#define CPPLIVELAB_GUI_TEST
#include "../src/gui.cpp"
#include "cpplivelab/projects.hpp"
#include <fstream>
#include <iostream>
#include <functional>

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
        const int width = iteration % 2 ? 940 : 1400;
        const int height = iteration % 2 ? 650 : 900;
        SetWindowPos(app.window_handle,
            nullptr,
            -30000,
            -30000,
            width,
            height,
            SWP_NOACTIVATE | SWP_NOZORDER);
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

    explicit Host(const fs::path& project)
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

void run()
{
    Project project;
    Host host(project.root);
    auto& app = host.app;
    app.command(BuildButton);
    host.idle();
    require(read_control_text(app.find_control(StatusLabel)) == L"Build Successful",
        "GUI initial build did not succeed");
    const auto builds = app.build_sequence;
    app.command(RunButton);
    host.running();
    host.prompt(L"Your name:");
    require(app.build_sequence == builds, "Up-to-date Run rebuilt instead of probing Ninja");
    require(read_control_text(app.system_console.window).find(L"Build is up to date") !=
                std::wstring::npos,
        "GUI did not report the up-to-date Run decision");
    app.command(EndInputButton);
    host.idle();
    app.command(RunButton);
    host.running();
    require(IsWindowEnabled(app.find_control(SendButton)) != 0,
        "Fast Run after End Input kept stdin disabled from the previous process");
    require(app.build_sequence == builds, "Second unchanged Run rebuilt");
    const auto program_id = app.session.process_id();
    app.command(ClearProgram);
    require(read_control_text(app.program_console.window).empty() &&
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
    const auto debug_program = project.root / L".cpplivelab/build-Debug/app.exe";
    const auto debug_written = fs::last_write_time(debug_program);
    SendMessageW(app.find_control(ConfigurationSelector), CB_SETCURSEL, 1, 0);
    app.command(BuildButton);
    host.idle();
    require(app.selected_configuration() == L"Release" &&
                read_control_text(app.find_control(StatusLabel)) == L"Build Successful" &&
                fs::is_regular_file(project.root / L".cpplivelab/build-Release/app.exe") &&
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

int main()
{
    const auto library = LoadLibraryW(L"Msftedit.dll");
    if (!library)
    {
        return 1;
    }
    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int result = 0;
    try
    {
        flow_tests::run();
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
