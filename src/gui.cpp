#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>
#include "cpplivelab/session.hpp"
#include "output_decoder.hpp"
#include "cpplivelab/doctor.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <stdexcept>

namespace
{
namespace fs = std::filesystem;

// Fixed IDs route Windows messages and identify controls to accessibility tools.
// Keep the numbers stable when changing labels or layout.
enum ControlId
{
    FolderInput = 101,
    BrowseButton = 102,
    OpenProjectButton = 103,
    ProjectIdentityLabel = 104,
    EditProjectButton = 105,
    ConfigurationSelector = 106,
    ExecutableInput = 107,
    BuildButton = 108,
    RunButton = 109,
    StopButton = 110,
    OutputPane = 111,
    ProgramInput = 112,
    SendButton = 113,
    EndInputButton = 114,
    StatusLabel = 115
};

std::wstring read_control_text(HWND control)
{
    std::wstring value(GetWindowTextLengthW(control) + 1, L'\0');
    value.resize(GetWindowTextW(control, value.data(), static_cast<int>(value.size())));
    return value;
}

using cpplivelab::decode_console_text;

std::string encode_utf8(const std::wstring& value)
{
    if (value.empty())
    {
        return {};
    }
    const int size = WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        size,
        nullptr,
        nullptr);
    return result;
}

fs::path application_directory()
{
    std::vector<wchar_t> buffer(32768);
    GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    return fs::path(buffer.data()).parent_path();
}

// Owns the native controls and one supervised operation. Source editing/debugging
// belongs to the separate IDE. All methods run on the GUI thread, including short,
// nonblocking output polls; no reader thread may outlive the window.
class Application
{
public:
    HWND window_handle = nullptr;
    // Fonts are GDI objects: DeleteObject releases them, not CloseHandle.
    // They outlive the child controls and are released with this Application.
    HFONT interface_font = nullptr;
    HFONT output_font = nullptr;
    cpplivelab::ConsoleSession session;
    cpplivelab::ConsoleOutputDecoder output_decoder;
    fs::path active_project_directory;
    // An operation includes final output draining. Do not re-enable Build merely
    // because the original worker has exited while descendants or output remain.
    bool operation_in_progress = false;
    bool should_run_program = false;
    bool program_started = false;
    bool input_closed = false;
    bool stop_requested = false;
    bool close_requested = false;

    // The worker's "Running:" marker may arrive in several pipe reads. Keep a
    // short tail so it can be recognized before enabling program input.
    std::string recent_output_bytes;
    wchar_t previous_output_character = L'\0';

    HWND find_control(int control_id) const
    {
        return GetDlgItem(window_handle, control_id);
    }

    ~Application()
    {
        if (interface_font)
        {
            DeleteObject(interface_font);
        }
        if (output_font)
        {
            DeleteObject(output_font);
        }
    }

    HWND create_control(
        const wchar_t* window_class, const wchar_t* label, int control_id, DWORD style = 0)
    {
        HWND child_window_handle =
            CreateWindowExW(window_class == std::wstring(L"EDIT") ? WS_EX_CLIENTEDGE : 0,
                window_class,
                label,
                WS_CHILD | WS_VISIBLE | style,
                0,
                0,
                0,
                0,
                window_handle,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(control_id)),
                GetModuleHandleW(nullptr),
                nullptr);
        SendMessageW(
            child_window_handle, WM_SETFONT, reinterpret_cast<WPARAM>(interface_font), TRUE);
        return child_window_handle;
    }

    void create()
    {
        interface_font = CreateFontW(-16,
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH,
            L"Segoe UI");
        output_font = CreateFontW(-15,
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            FIXED_PITCH,
            L"Consolas");
        create_control(L"STATIC", L"Project folder", 201);
        create_control(L"EDIT", L"", FolderInput, WS_TABSTOP | ES_AUTOHSCROLL);
        create_control(L"BUTTON", L"Browse...", BrowseButton, WS_TABSTOP);
        create_control(L"BUTTON", L"Open Project", OpenProjectButton, WS_TABSTOP);
        create_control(L"STATIC",
            L"No active project. Choose a folder containing CMakeLists.txt.",
            ProjectIdentityLabel);
        create_control(L"BUTTON", L"Edit in Visual Studio 2022", EditProjectButton, WS_TABSTOP);
        create_control(L"COMBOBOX", L"", ConfigurationSelector, WS_TABSTOP | CBS_DROPDOWNLIST);
        SendMessageW(find_control(ConfigurationSelector),
            CB_ADDSTRING,
            0,
            reinterpret_cast<LPARAM>(L"Debug"));
        SendMessageW(find_control(ConfigurationSelector),
            CB_ADDSTRING,
            0,
            reinterpret_cast<LPARAM>(L"Release"));
        SendMessageW(find_control(ConfigurationSelector), CB_SETCURSEL, 0, 0);
        create_control(L"STATIC", L"Executable (relative to build folder)", 202);
        create_control(L"EDIT", L"app.exe", ExecutableInput, WS_TABSTOP | ES_AUTOHSCROLL);
        create_control(L"BUTTON", L"Build", BuildButton, WS_TABSTOP);
        create_control(L"BUTTON", L"Run", RunButton, WS_TABSTOP);
        create_control(L"BUTTON", L"Stop", StopButton, WS_TABSTOP);
        create_control(L"STATIC", L"Build and program output", 203);
        create_control(L"EDIT",
            L"",
            OutputPane,
            WS_TABSTOP | ES_READONLY | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL |
                WS_HSCROLL);
        SendMessageW(
            find_control(OutputPane), WM_SETFONT, reinterpret_cast<WPARAM>(output_font), TRUE);
        SendMessageW(find_control(OutputPane), EM_SETLIMITTEXT, 1000000, 0);
        create_control(L"STATIC", L"Program input (type a line, then Send or Enter)", 204);
        create_control(L"EDIT", L"", ProgramInput, WS_TABSTOP | ES_AUTOHSCROLL);
        SendMessageW(find_control(ProgramInput), EM_SETLIMITTEXT, 1000, 0);
        create_control(L"BUTTON", L"Send", SendButton, WS_TABSTOP);
        create_control(L"BUTTON", L"End Input", EndInputButton, WS_TABSTOP);
        create_control(L"STATIC", L"Ready", StatusLabel);
        refresh();
        SetTimer(window_handle, 1, 50, nullptr);
    }

    void layout(int width, int height)
    {
        auto position_control = [&](int control_id, int x, int y, int w, int h)
        {
            MoveWindow(find_control(control_id), x, y, w, h, TRUE);
        };
        position_control(201, 16, 12, 300, 20);
        position_control(FolderInput, 16, 36, width - 254, 28);
        position_control(BrowseButton, width - 226, 35, 96, 30);
        position_control(OpenProjectButton, width - 120, 35, 104, 30);
        position_control(ProjectIdentityLabel, 16, 76, width - 32, 48);
        position_control(EditProjectButton, 16, 154, 224, 32);
        position_control(ConfigurationSelector, 252, 154, 112, 180);
        position_control(202, 380, 130, width - 396, 20);
        position_control(ExecutableInput, 380, 156, width - 396, 28);
        position_control(BuildButton, 16, 200, 96, 32);
        position_control(RunButton, 124, 200, 96, 32);
        position_control(StopButton, 232, 200, 96, 32);
        position_control(StatusLabel, 348, 206, width - 364, 24);
        position_control(203, 16, 248, width - 32, 20);
        position_control(OutputPane, 16, 272, width - 32, height - 370);
        position_control(204, 16, height - 86, width - 32, 20);
        position_control(ProgramInput, 16, height - 60, width - 226, 30);
        position_control(SendButton, width - 198, height - 60, 76, 30);
        position_control(EndInputButton, width - 110, height - 60, 94, 30);
    }

    void append(const std::wstring& value)
    {
        // EDIT controls expect CRLF. Remember the previous character across chunks
        // so an existing CRLF sequence is not expanded twice.
        std::wstring normalized;
        for (auto ch : value)
        {
            if (ch == L'\n' && previous_output_character != L'\r')
            {
                normalized += L'\r';
            }
            normalized += ch;
            previous_output_character = ch;
        }
        auto output = find_control(OutputPane);
        // Bound history so a chatty program cannot exhaust the GUI's memory.
        if (GetWindowTextLengthW(output) > 500000)
        {
            SendMessageW(output, EM_SETSEL, 0, 200000);
            SendMessageW(output,
                EM_REPLACESEL,
                FALSE,
                reinterpret_cast<LPARAM>(L"[Older output trimmed]\r\n"));
        }
        SendMessageW(output, EM_SETSEL, -1, -1);
        SendMessageW(output, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(normalized.c_str()));
        SendMessageW(output, EM_SCROLLCARET, 0, 0);
    }

    void refresh()
    {
        // Freeze project/build choices while a job is owned. Program input is only
        // available after successful compilation and the start of execution.
        const bool selected = !active_project_directory.empty();
        for (int control_id :
            {FolderInput, BrowseButton, OpenProjectButton, ConfigurationSelector, ExecutableInput})
        {
            EnableWindow(find_control(control_id), !operation_in_progress);
        }
        for (int control_id : {EditProjectButton, BuildButton, RunButton})
        {
            EnableWindow(find_control(control_id), selected && !operation_in_progress);
        }
        EnableWindow(find_control(StopButton), operation_in_progress && !stop_requested);
        for (int control_id : {ProgramInput, SendButton, EndInputButton})
        {
            EnableWindow(find_control(control_id),
                operation_in_progress && program_started && !stop_requested && !input_closed);
        }
    }

    void open(const fs::path& folder)
    {
        if (operation_in_progress)
        {
            return;
        }
        const auto candidate = cpplivelab::validate_gui_project(folder);
        active_project_directory = candidate;
        SetWindowTextW(find_control(FolderInput), active_project_directory.c_str());
        SetWindowTextW(find_control(ProjectIdentityLabel),
            (L"Active project: " + active_project_directory.filename().wstring() + L"\r\n" +
                active_project_directory.wstring())
                .c_str());
        SetWindowTextW(window_handle,
            (L"CppLiveLab - " + active_project_directory.filename().wstring()).c_str());
        SetWindowTextW(find_control(ExecutableInput), L"app.exe");
        SetWindowTextW(find_control(StatusLabel), L"Ready");
        append(L"Opened: " + active_project_directory.wstring() + L"\n");
        refresh();
    }

    void browse()
    {
        IFileDialog* dialog = nullptr;
        if (FAILED(CoCreateInstance(
                CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
        {
            throw std::runtime_error("Could not open folder picker");
        }
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        dialog->SetTitle(L"Open a C++ CMake project folder");
        fs::path selected;
        if (SUCCEEDED(dialog->Show(window_handle)))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)))
            {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
                {
                    selected = path;
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }
        dialog->Release();
        if (!selected.empty())
        {
            open(selected);
        }
    }

    void start(bool run)
    {
        if (operation_in_progress || active_project_directory.empty())
        {
            return;
        }
        const auto cli_executable = application_directory() / L"cpplivelab.exe";
        if (!fs::is_regular_file(cli_executable))
        {
            throw std::runtime_error("Keep cpplivelab.exe beside cpplivelab-gui.exe.");
        }
        const auto selected_configuration =
            SendMessageW(find_control(ConfigurationSelector), CB_GETCURSEL, 0, 0) == 1 ? L"Release"
                                                                                       : L"Debug";
        std::vector<std::wstring> command_line_arguments{run ? L"run" : L"build",
            active_project_directory.wstring(),
            L"--config",
            selected_configuration};
        if (run && !read_control_text(find_control(ExecutableInput)).empty())
        {
            command_line_arguments.push_back(L"--exe");
            command_line_arguments.push_back(read_control_text(find_control(ExecutableInput)));
        }
        append(L"\n--- " + std::wstring(run ? L"Run " : L"Build ") + selected_configuration +
               L" ---\n");
        // Reuse the approved CLI as a hidden child. Tool discovery, developer-shell
        // setup and compilation stay there instead of blocking the GUI thread.
        session.start(cli_executable, command_line_arguments, active_project_directory);
        operation_in_progress = true;
        should_run_program = run;
        program_started = false;
        input_closed = false;
        stop_requested = false;
        recent_output_bytes.clear();
        output_decoder.reset();
        SetWindowTextW(find_control(StatusLabel), L"Building...");
        refresh();
    }

    void tick()
    {
        // WM_TIMER reads a bounded amount of output, then returns to the message loop.
        const auto output_bytes = session.poll();
        if (!output_bytes.empty())
        {
            append(output_decoder.append(output_bytes));
            recent_output_bytes += output_bytes;
            if (should_run_program && !program_started &&
                recent_output_bytes.find("Running: ") != std::string::npos)
            {
                program_started = true;
                SetWindowTextW(find_control(StatusLabel), L"Running - use input below");
                refresh();
                SetFocus(find_control(ProgramInput));
            }
            if (recent_output_bytes.size() > 1024)
            {
                recent_output_bytes.erase(0, recent_output_bytes.size() - 1024);
            }
        }
        if (operation_in_progress && !session.running())
        {
            if (session.output_pending())
            {
                return; // Drain bursts across timer ticks before re-enabling Run.
            }
            // The job is empty before another build can be enabled.
            append(output_decoder.append(session.poll(), true));
            const auto message =
                stop_requested ? L"Stopped"
                               : (L"Finished (exit " + std::to_wstring(session.exit_code()) + L")");
            append(L"\n" + std::wstring(message) + L"\n");
            SetWindowTextW(find_control(StatusLabel), message.c_str());
            operation_in_progress = false;
            refresh();
            if (close_requested)
            {
                DestroyWindow(window_handle);
            }
        }
    }

    void stop()
    {
        if (!operation_in_progress)
        {
            return;
        }
        stop_requested = true;
        session.stop();
        SetWindowTextW(find_control(StatusLabel), L"Stopping process tree...");
        refresh();
    }

    void send()
    {
        if (!operation_in_progress || !program_started || stop_requested || input_closed)
        {
            return;
        }
        const auto value = read_control_text(find_control(ProgramInput));
        if (!session.send_line(encode_utf8(value)))
        {
            append(L"Input could not be sent (buffer full or program exited).\n");
            return;
        }
        append(L"> " + value + L"\n");
        SetWindowTextW(find_control(ProgramInput), L"");
        SetFocus(find_control(ProgramInput));
    }

    void command(int control_id)
    {
        // Handle Stop/input before rejecting other commands during an operation.
        // Disabled buttons alone are insufficient: Windows may have queued a command.
        if (control_id == SendButton)
        {
            send();
            return;
        }
        if (control_id == StopButton)
        {
            stop();
            return;
        }
        if (control_id == EndInputButton && operation_in_progress && program_started)
        {
            session.close_input();
            input_closed = true;
            refresh();
            return;
        }
        if (operation_in_progress)
        {
            return;
        }
        if (control_id == OpenProjectButton)
        {
            open(read_control_text(find_control(FolderInput)));
        }
        else if (control_id == BrowseButton)
        {
            browse();
        }
        else if (control_id == BuildButton)
        {
            start(false);
        }
        else if (control_id == RunButton)
        {
            start(true);
        }
        else if (control_id == EditProjectButton && !active_project_directory.empty())
        {
            cpplivelab::open_in_visual_studio(
                cpplivelab::find_visual_studio_2022(), active_project_directory);
            append(L"Opened project in Visual Studio 2022. Save edits there before Build/Run.\n");
        }
    }
};

LRESULT CALLBACK window_procedure(HWND window_handle, UINT message, WPARAM wparam, LPARAM lparam)
{
    // Windows calls a free function for messages. This non-owning window property
    // connects the callback to the stack-owned Application created by wWinMain.
    auto application =
        reinterpret_cast<Application*>(GetWindowLongPtrW(window_handle, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        application =
            static_cast<Application*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        application->window_handle = window_handle;
        SetWindowLongPtrW(window_handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(application));
    }
    if (!application)
    {
        return DefWindowProcW(window_handle, message, wparam, lparam);
    }
    try
    {
        switch (message)
        {
            case WM_CREATE:
                application->create();
                return 0;
            case WM_SIZE:
                application->layout(LOWORD(lparam), HIWORD(lparam));
                return 0;
            case WM_GETMINMAXINFO:
                reinterpret_cast<MINMAXINFO*>(lparam)->ptMinTrackSize = {800, 560};
                return 0;
            case WM_COMMAND:
                if (HIWORD(wparam) == BN_CLICKED)
                {
                    application->command(LOWORD(wparam));
                }
                return 0;
            case WM_TIMER:
                application->tick();
                return 0;
            case WM_CLOSE:
                if (application->operation_in_progress)
                {
                    application->close_requested = true;
                    application->stop();
                }
                else
                {
                    DestroyWindow(window_handle);
                }
                return 0;
            case WM_DESTROY:
                KillTimer(window_handle, 1);
                PostQuitMessage(0);
                return 0;
        }
    }
    catch (const std::exception& error)
    {
        application->append(L"Error: " + decode_console_text(error.what()) + L"\n");
        SetWindowTextW(application->find_control(StatusLabel), L"Error - see output");
    }
    return DefWindowProcW(window_handle, message, wparam, lparam);
}
} // namespace

int WINAPI wWinMain(HINSTANCE application_instance, HINSTANCE, PWSTR, int initial_show_state)
{
    SetProcessDPIAware();
    // The folder picker uses COM. Pair successful initialization with CoUninitialize
    // on this same thread once the message loop has ended.
    const auto com_initialization_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = window_procedure;
    window_class.hInstance = application_instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    window_class.lpszClassName = L"CppLiveLabWindow";
    RegisterClassW(&window_class);
    Application application;
    HWND window_handle = CreateWindowExW(WS_EX_CONTROLPARENT,
        window_class.lpszClassName,
        L"CppLiveLab",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1080,
        740,
        nullptr,
        nullptr,
        application_instance,
        &application);
    if (!window_handle)
    {
        if (SUCCEEDED(com_initialization_result))
        {
            CoUninitialize();
        }
        return 1;
    }
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc == 2)
    {
        try
        {
            application.open(argv[1]);
        }
        catch (const std::exception& error)
        {
            application.append(L"Error: " + decode_console_text(error.what()) + L"\n");
        }
    }
    if (argv)
    {
        LocalFree(argv);
    }
    ShowWindow(window_handle, initial_show_state);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (message.message == WM_KEYDOWN && message.wParam == VK_RETURN &&
            GetFocus() == application.find_control(ProgramInput))
        {
            application.send();
            continue;
        }
        if (!IsDialogMessageW(window_handle, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (SUCCEEDED(com_initialization_result))
    {
        CoUninitialize();
    }
    return static_cast<int>(message.wParam);
}
