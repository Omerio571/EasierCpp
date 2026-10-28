#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>
#include "cpplivelab/session.hpp"
#include "output_decoder.hpp"
#include "auto_build.hpp"
#include "build_lease.hpp"
#include "console_view.hpp"
#include "ui_theme.hpp"
#include <future>
#include <chrono>
#include <algorithm>
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
    StatusLabel = 115,
    AutoBuildButton = 116,
    DebounceInput = 117,
    ProgramTab = 118,
    SystemTab = 119,
    SystemOutputPane = 120,
    ClearProgram = 121,
    ClearSystem = 122,
    AppearanceSelector = 123
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
    UINT dpi = 96;
    cpplivelab::Preferences preferences = cpplivelab::load_preferences();
    cpplivelab::UiTheme appearance;
    bool applying_appearance = false;
    // Fonts are GDI objects: DeleteObject releases them, not CloseHandle.
    // They outlive the child controls and are released with this Application.
    HFONT interface_font = nullptr;
    HFONT output_font = nullptr;
    cpplivelab::ConsoleSession session;
    cpplivelab::ConsoleOutputDecoder output_decoder;
    cpplivelab::ConsoleOutputDecoder program_decoder;
    cpplivelab::ConsoleOutputDecoder error_decoder;
    cpplivelab::ConsolePane program_console;
    cpplivelab::ConsolePane system_console;
    bool program_tab_active = true;
    enum class Phase
    {
        Idle,
        Building,
        PreparingProgram,
        Running
    };
    Phase phase = Phase::Idle;
    bool automatic_operation = false;
    std::wstring pending_build_line;
    std::wstring build_diagnostics;
    std::unique_ptr<cpplivelab::BuildLease> program_lease;
    std::future<cpplivelab::Environment> environment_future;
    cpplivelab::Environment program_environment;
    bool environment_ready = false;
    bool error_line_start = true;
    fs::path active_project_directory;
    cpplivelab::ProjectWatcher watcher;
    cpplivelab::AutoBuildScheduler auto_build;
    std::wstring auto_status;
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

    void update_fonts()
    {
        const auto old_interface = interface_font;
        const auto old_output = output_font;
        interface_font = CreateFontW(-cpplivelab::scale_pixels(16, dpi),
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
        output_font = CreateFontW(-cpplivelab::scale_pixels(15, dpi),
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
        for (HWND child = GetWindow(window_handle, GW_CHILD); child;
            child = GetWindow(child, GW_HWNDNEXT))
        {
            const auto font = child == program_console.window || child == system_console.window
                                  ? output_font
                                  : interface_font;
            SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            wchar_t name[64]{};
            GetClassNameW(child, name, 64);
            if (std::wstring_view(name) == L"ComboBox")
            {
                SendMessageW(child,
                    CB_SETITEMHEIGHT,
                    static_cast<WPARAM>(-1),
                    cpplivelab::scale_pixels(24, dpi));
                SendMessageW(child, CB_SETITEMHEIGHT, 0, cpplivelab::scale_pixels(24, dpi));
            }
        }
        if (old_interface)
        {
            DeleteObject(old_interface);
        }
        if (old_output)
        {
            DeleteObject(old_output);
        }
    }

    void persist_preferences()
    {
        if (!cpplivelab::save_preferences(preferences))
        {
            log(cpplivelab::MessageKind::Warning,
                L"Preferences could not be saved; the current selection still applies.");
        }
    }

    void apply_appearance()
    {
        if (applying_appearance || !find_control(AppearanceSelector))
        {
            return;
        }
        applying_appearance = true;
        appearance.apply(window_handle, preferences.theme, dpi);
        program_console.set_colors(appearance.palette.console);
        system_console.set_colors(appearance.palette.console);
        appearance.active_button(find_control(ProgramTab), program_tab_active);
        appearance.active_button(find_control(SystemTab), !program_tab_active);
        appearance.active_button(find_control(AutoBuildButton), auto_build.enabled());
        applying_appearance = false;
    }

    void change_appearance()
    {
        const auto choice = SendMessageW(find_control(AppearanceSelector), CB_GETCURSEL, 0, 0);
        if (choice < 0 || choice > 2)
        {
            return;
        }
        preferences.theme = static_cast<cpplivelab::ThemeChoice>(choice);
        persist_preferences();
        apply_appearance();
    }

    void update_debounce()
    {
        try
        {
            const auto value = std::stoul(read_control_text(find_control(DebounceInput)));
            preferences.debounce_ms = static_cast<DWORD>((std::clamp)(value, 100UL, 5000UL));
        }
        catch (const std::exception&)
        {
            log(cpplivelab::MessageKind::Warning,
                L"Debounce must be 100-5000 ms; the previous value is retained.");
        }
        SetWindowTextW(
            find_control(DebounceInput), std::to_wstring(preferences.debounce_ms).c_str());
        auto_build.set_debounce(preferences.debounce_ms);
        persist_preferences();
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
        if (std::wstring_view(window_class) == L"COMBOBOX")
        {
            style |= CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
        }
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
        dpi = GetDpiForWindow(window_handle);
        update_fonts();
        auto_build.enable(preferences.auto_build);
        auto_build.set_debounce(preferences.debounce_ms);
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
        create_control(L"BUTTON", L"Program Console", ProgramTab, WS_TABSTOP);
        create_control(L"BUTTON", L"Build & System", SystemTab, WS_TABSTOP);
        create_control(L"BUTTON", L"Clear Program", ClearProgram, WS_TABSTOP);
        create_control(L"BUTTON", L"Clear Build & System", ClearSystem, WS_TABSTOP);
        program_console.create(window_handle, OutputPane, output_font);
        system_console.create(window_handle, SystemOutputPane, output_font);
        select_console(true);
        create_control(L"STATIC", L"Program input (type a line, then Send or Enter)", 204);
        create_control(L"EDIT", L"", ProgramInput, WS_TABSTOP | ES_AUTOHSCROLL);
        SendMessageW(find_control(ProgramInput), EM_SETLIMITTEXT, 1000, 0);
        create_control(L"BUTTON", L"Send", SendButton, WS_TABSTOP);
        create_control(L"BUTTON", L"End Input", EndInputButton, WS_TABSTOP);
        create_control(L"STATIC", L"Ready", StatusLabel);
        create_control(L"STATIC", L"Appearance", 206);
        create_control(L"COMBOBOX", L"", AppearanceSelector, WS_TABSTOP | CBS_DROPDOWNLIST);
        for (const auto* choice : {L"Dark", L"Light", L"Follow Windows"})
        {
            SendMessageW(find_control(AppearanceSelector),
                CB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(choice));
        }
        SendMessageW(find_control(AppearanceSelector),
            CB_SETCURSEL,
            static_cast<WPARAM>(preferences.theme),
            0);
        create_control(L"BUTTON", L"Auto Build: OFF", AutoBuildButton, WS_TABSTOP);
        create_control(L"STATIC", L"Debounce (ms)", 205);
        create_control(L"EDIT", L"400", DebounceInput, WS_TABSTOP | ES_NUMBER);
        SetWindowTextW(
            find_control(DebounceInput), std::to_wstring(preferences.debounce_ms).c_str());
        SetWindowTextW(find_control(AutoBuildButton),
            preferences.auto_build ? L"Auto Build: ON" : L"Auto Build: OFF");
        refresh();
        apply_appearance();
        SetTimer(window_handle, 1, 50, nullptr);
    }

    void layout(int width, int height)
    {
        width = MulDiv(width, 96, static_cast<int>(dpi));
        height = MulDiv(height, 96, static_cast<int>(dpi));
        auto position_control = [&](int control_id, int x, int y, int w, int h)
        {
            MoveWindow(find_control(control_id),
                cpplivelab::scale_pixels(x, dpi),
                cpplivelab::scale_pixels(y, dpi),
                cpplivelab::scale_pixels(w, dpi),
                cpplivelab::scale_pixels(h, dpi),
                TRUE);
        };
        position_control(201, 16, 12, 300, 20);
        position_control(FolderInput, 16, 36, width - 270, 28);
        position_control(BrowseButton, width - 242, 35, 96, 30);
        position_control(OpenProjectButton, width - 136, 35, 120, 30);
        position_control(ProjectIdentityLabel, 16, 76, width - 32, 48);
        position_control(EditProjectButton, 16, 154, 224, 32);
        position_control(ConfigurationSelector, 252, 154, 112, 180);
        position_control(202, 380, 130, width - 600, 20);
        position_control(ExecutableInput, 380, 156, width - 600, 28);
        position_control(206, width - 200, 130, 184, 20);
        position_control(AppearanceSelector, width - 200, 154, 184, 180);
        position_control(BuildButton, 16, 200, 96, 32);
        position_control(RunButton, 124, 200, 96, 32);
        position_control(StopButton, 232, 200, 96, 32);
        position_control(AutoBuildButton, 348, 200, 160, 32);
        position_control(205, 524, 206, 130, 20);
        position_control(DebounceInput, 662, 200, 80, 32);
        position_control(StatusLabel, 16, 244, width - 32, 24);
        position_control(ProgramTab, 16, 276, 178, 30);
        position_control(SystemTab, 202, 276, 230, 30);
        position_control(ClearProgram, width - 180, 276, 164, 30);
        position_control(ClearSystem, width - 202, 276, 186, 30);
        position_control(OutputPane, 16, 312, width - 32, height - 410);
        position_control(SystemOutputPane, 16, 312, width - 32, height - 410);
        position_control(204, 16, height - 86, width - 32, 20);
        position_control(ProgramInput, 16, height - 60, width - 226, 30);
        position_control(SendButton, width - 198, height - 60, 76, 30);
        position_control(EndInputButton, width - 110, height - 60, 94, 30);
        appearance.round_controls();
        InvalidateRect(window_handle, nullptr, TRUE);
    }

    void update_console_badges()
    {
        SetWindowTextW(find_control(ProgramTab),
            program_console.history.unread() ? L"Program Console (new)" : L"Program Console");
        SetWindowTextW(find_control(SystemTab),
            system_console.history.unread_error() ? L"Build & System (!)"
            : system_console.history.unread()     ? L"Build & System (new)"
                                                  : L"Build & System");
    }

    void select_console(bool program)
    {
        program_tab_active = program;
        program_console.history.set_active(program);
        system_console.history.set_active(!program);
        program_console.set_visible(program);
        system_console.set_visible(!program);
        ShowWindow(find_control(ClearProgram), program ? SW_SHOW : SW_HIDE);
        ShowWindow(find_control(ClearSystem), program ? SW_HIDE : SW_SHOW);
        appearance.active_button(find_control(ProgramTab), program);
        appearance.active_button(find_control(SystemTab), !program);
        update_console_badges();
    }

    void log(cpplivelab::MessageKind kind, const std::wstring& message)
    {
        SYSTEMTIME time{};
        GetLocalTime(&time);
        wchar_t timestamp[32]{};
        swprintf_s(timestamp, L"[%02u:%02u:%02u] ", time.wHour, time.wMinute, time.wSecond);
        system_console.append(kind,
            std::wstring(timestamp) + std::wstring(cpplivelab::message_label(kind)) + message +
                L"\n");
        update_console_badges();
    }

    void append(const std::wstring& message)
    {
        log(cpplivelab::MessageKind::System, message);
    }

    void append_build_output(const std::wstring& text, bool flush = false)
    {
        pending_build_line += text;
        build_diagnostics += text;
        if (build_diagnostics.size() > 32000)
        {
            build_diagnostics.erase(0, build_diagnostics.size() - 32000);
        }
        size_t end = 0;
        while ((end = pending_build_line.find(L'\n')) != std::wstring::npos ||
               (flush && !pending_build_line.empty()) || pending_build_line.size() > 32000)
        {
            if (end == std::wstring::npos)
            {
                end = pending_build_line.size();
            }
            auto line = pending_build_line.substr(0, end);
            if (!line.empty() && line.back() == L'\r')
            {
                line.pop_back();
            }
            const auto kind = cpplivelab::classify_build_line(line);
            system_console.append(
                kind, std::wstring(cpplivelab::message_label(kind)) + line + L"\n");
            pending_build_line.erase(0, end == pending_build_line.size() ? end : end + 1);
        }
        update_console_badges();
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
        watcher.stop();
        auto_build.cancel();
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
        start_watching();
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

    std::wstring selected_configuration() const
    {
        return SendMessageW(find_control(ConfigurationSelector), CB_GETCURSEL, 0, 0) == 1
                   ? L"Release"
                   : L"Debug";
    }

    fs::path build_directory() const
    {
        return active_project_directory / L".cpplivelab" / (L"build-" + selected_configuration());
    }

    void request_environment()
    {
        if (!environment_ready && !environment_future.valid())
        {
            // Developer setup can invoke utilities. Run it once in a worker and
            // retain its result only in memory, never in a file or diagnostic log.
            environment_future = std::async(std::launch::async,
                []
                {
                    const auto tools =
                        cpplivelab::discover_toolchain(cpplivelab::system_discovery_inputs());
                    return tools.developer_command.empty()
                               ? cpplivelab::Environment{}
                               : cpplivelab::developer_environment(tools.developer_command);
                });
        }
    }

    void start(bool run, bool automatic = false, bool incremental = false)
    {
        if (operation_in_progress || active_project_directory.empty())
        {
            return;
        }
        const auto cli = application_directory() / L"cpplivelab.exe";
        if (!fs::is_regular_file(cli))
        {
            throw std::runtime_error("Keep cpplivelab.exe beside cpplivelab-gui.exe.");
        }
        if (!automatic && auto_build.enabled())
        {
            watcher.poll();
            watcher.consume_changes();
            auto_build.cancel();
        }
        std::vector<std::wstring> arguments{
            L"build", active_project_directory.wstring(), L"--config", selected_configuration()};
        if (incremental)
        {
            arguments.push_back(L"--incremental");
        }
        if (run)
        {
            request_environment();
            select_console(true);
        }
        log(cpplivelab::MessageKind::Build,
            std::wstring(automatic ? L"Automatic " : L"Manual ") + selected_configuration() +
                L" build started");
        session.start(cli, arguments, active_project_directory);
        auto_build.begin_build();
        automatic_operation = automatic;
        operation_in_progress = true;
        should_run_program = run;
        program_started = false;
        input_closed = false;
        stop_requested = false;
        phase = Phase::Building;
        output_decoder.reset();
        pending_build_line.clear();
        build_diagnostics.clear();
        SetWindowTextW(find_control(StatusLabel), L"Building");
        refresh();
    }

    void finish_operation(const std::wstring& status)
    {
        program_lease.reset();
        phase = Phase::Idle;
        program_started = false;
        operation_in_progress = false;
        SetWindowTextW(find_control(StatusLabel), status.c_str());
        refresh();
    }

    void begin_program()
    {
        if (!environment_ready)
        {
            if (!environment_future.valid() ||
                environment_future.wait_for(std::chrono::milliseconds(0)) !=
                    std::future_status::ready)
            {
                return;
            }
            program_environment = environment_future.get();
            environment_ready = true;
        }
        const auto configured = read_control_text(find_control(ExecutableInput));
        const fs::path selected = configured.empty() ? L"app.exe" : configured;
        const auto executable = selected.is_absolute() ? selected : build_directory() / selected;
        if (!fs::is_regular_file(executable))
        {
            throw std::runtime_error(
                "Executable not found. Set Executable relative to the selected build folder.");
        }
        program_lease = std::make_unique<cpplivelab::BuildLease>(build_directory());
        session.start_program(executable, {}, active_project_directory, program_environment);
        program_decoder.reset();
        error_decoder.reset();
        error_line_start = true;
        program_started = true;
        phase = Phase::Running;
        log(cpplivelab::MessageKind::System, L"Program started: " + executable.wstring());
        SetWindowTextW(find_control(StatusLabel), L"Running - input below");
        // Do not SetFocus: asynchronous program startup must not steal focus.
        refresh();
    }

    void append_program_output(const std::wstring& text, bool error)
    {
        std::wstring rendered;
        for (wchar_t character : text)
        {
            if (error && error_line_start && character != L'\r' && character != L'\n')
            {
                rendered += L"[stderr] ";
            }
            rendered += character;
            if (error)
            {
                error_line_start = character == L'\r' || character == L'\n';
            }
        }
        program_console.append(
            error ? cpplivelab::MessageKind::ProgramError : cpplivelab::MessageKind::ProgramOutput,
            rendered);
    }

    void tick()
    {
        poll_watcher();
        if (phase == Phase::Building)
        {
            append_build_output(output_decoder.append(session.poll()));
            if (!session.running() && !session.output_pending())
            {
                append_build_output(output_decoder.append(session.poll(), true), true);
                const int result = session.exit_code();
                const bool blocked =
                    build_diagnostics.find(L"already building or running") != std::wstring::npos ||
                    build_diagnostics.find(L"Executable is running or not writable") !=
                        std::wstring::npos;
                if (stop_requested)
                {
                    log(cpplivelab::MessageKind::Warning, L"Build stopped by user");
                    finish_operation(L"Stopped");
                }
                else if (blocked)
                {
                    if (auto_build.enabled())
                    {
                        auto_build.defer(GetTickCount64());
                    }
                    log(cpplivelab::MessageKind::Warning,
                        L"Build Deferred - active executable/configuration preserved");
                    finish_operation(L"Build Deferred");
                }
                else if (result != 0)
                {
                    log(cpplivelab::MessageKind::Error,
                        L"Build Failed (exit " + std::to_wstring(result) + L")");
                    finish_operation(L"Build Failed");
                }
                else
                {
                    log(cpplivelab::MessageKind::Success, L"Build Successful (exit 0)");
                    if (should_run_program)
                    {
                        phase = Phase::PreparingProgram;
                        SetWindowTextW(find_control(StatusLabel), L"Preparing program");
                    }
                    else
                    {
                        finish_operation(L"Build Successful");
                    }
                }
            }
        }
        else if (phase == Phase::Running)
        {
            for (const auto& chunk : session.poll_output())
            {
                const bool error = chunk.stream == cpplivelab::OutputStream::StandardError;
                auto& decoder = error ? error_decoder : program_decoder;
                append_program_output(decoder.append(chunk.bytes), error);
            }
            update_console_badges();
            if (!session.running() && !session.output_pending())
            {
                append_program_output(program_decoder.append({}, true), false);
                append_program_output(error_decoder.append({}, true), true);
                const auto result = session.exit_code();
                log(stop_requested ? cpplivelab::MessageKind::Warning
                    : result == 0  ? cpplivelab::MessageKind::Success
                                   : cpplivelab::MessageKind::Error,
                    stop_requested ? L"Program stopped by user"
                                   : L"Program finished (exit " + std::to_wstring(result) + L")");
                finish_operation(stop_requested
                                     ? L"Stopped"
                                     : L"Program finished (exit " + std::to_wstring(result) + L")");
            }
        }
        if (phase == Phase::PreparingProgram)
        {
            if (stop_requested)
            {
                finish_operation(L"Stopped");
            }
            else
            {
                try
                {
                    begin_program();
                }
                catch (const std::exception& error)
                {
                    log(cpplivelab::MessageKind::Error, decode_console_text(error.what()));
                    finish_operation(L"Program launch failed");
                }
            }
        }
        const bool environment_worker_done =
            !environment_future.valid() ||
            environment_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
        if (close_requested && !operation_in_progress && environment_worker_done)
        {
            DestroyWindow(window_handle);
        }
    }

    void stop()
    {
        if (!operation_in_progress)
        {
            return;
        }
        stop_requested = true;
        if (phase != Phase::Running)
        {
            auto_build.cancel();
        }
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
            log(cpplivelab::MessageKind::Warning,
                L"Input could not be sent (buffer full or program exited).");
            return;
        }
        program_console.append(cpplivelab::MessageKind::Input, L"> " + value + L"\n");
        update_console_badges();
        SetWindowTextW(find_control(ProgramInput), L"");
        SetFocus(find_control(ProgramInput));
    }

    void set_auto_status(const std::wstring& status)
    {
        SetWindowTextW(find_control(StatusLabel), status.c_str());
        if (auto_status != status)
        {
            log(status.starts_with(L"Build Deferred") ? cpplivelab::MessageKind::Warning
                                                      : cpplivelab::MessageKind::System,
                status);
            auto_status = status;
        }
    }

    void start_watching()
    {
        auto_status.clear();
        if (auto_build.enabled() && !active_project_directory.empty())
        {
            const auto value = read_control_text(find_control(DebounceInput));
            const auto milliseconds = value.empty() ? 400UL : std::stoul(value);
            auto_build.set_debounce((std::clamp)(milliseconds, 100UL, 5000UL));
            try
            {
                watcher.start(active_project_directory);
            }
            catch (...)
            {
                watcher.stop();
                auto_build.enable(false);
                SetWindowTextW(find_control(AutoBuildButton), L"Auto Build: OFF");
                preferences.auto_build = false;
                persist_preferences();
                throw;
            }
            set_auto_status(L"Watching");
        }
        else
        {
            set_auto_status(L"Auto Build OFF");
        }
    }

    void poll_watcher()
    {
        if (!auto_build.enabled() || active_project_directory.empty() || close_requested)
        {
            return;
        }
        const auto now = GetTickCount64();
        bool notified = false;
        try
        {
            notified = watcher.poll();
        }
        catch (const std::exception& error)
        {
            watcher.stop();
            auto_build.enable(false);
            preferences.auto_build = false;
            SetWindowTextW(find_control(AutoBuildButton), L"Auto Build: OFF");
            persist_preferences();
            log(cpplivelab::MessageKind::Error, decode_console_text(error.what()));
            set_auto_status(L"Watcher stopped - turn Auto Build on to retry");
            return;
        }
        if (notified)
        {
            auto_build.notify(now);
            set_auto_status(L"Changes Detected");
        }
        if (auto_build.settled(now))
        {
            std::vector<cpplivelab::SourceChange> changes;
            try
            {
                changes = watcher.consume_changes();
            }
            catch (const std::exception&)
            {
                // Editors may briefly lock a file during an atomic save. Keep the
                // notification pending and retry after another debounce interval.
                auto_build.notify(now);
                set_auto_status(L"Changes Detected - waiting for source file access");
                return;
            }
            const bool configuration_changed = std::any_of(changes.begin(),
                changes.end(),
                [](const auto& change)
                {
                    return change.affects_cmake;
                });
            auto_build.confirm(!changes.empty(), configuration_changed);
        }
        if (auto_build.pending() && operation_in_progress)
        {
            set_auto_status(
                program_started ? L"Build Deferred - running program preserved" : L"Build Queued");
        }
        if (auto_build.ready(now, operation_in_progress, true))
        {
            const auto configuration =
                SendMessageW(find_control(ConfigurationSelector), CB_GETCURSEL, 0, 0) == 1
                    ? L"Release"
                    : L"Debug";
            const auto directory = active_project_directory / L".cpplivelab" /
                                   (L"build-" + std::wstring(configuration));
            if (!cpplivelab::build_outputs_available(directory))
            {
                auto_build.defer(now);
                set_auto_status(L"Build Deferred - configuration is busy or not writable");
            }
            else
            {
                const bool incremental = !auto_build.needs_reconfigure();
                start(false, true, incremental);
                set_auto_status(L"Building (Auto)");
            }
        }
    }

    void command(int control_id)
    {
        // Handle Stop/input before rejecting other commands during an operation.
        // Disabled buttons alone are insufficient: Windows may have queued a command.
        if (control_id == ProgramTab)
        {
            select_console(true);
            return;
        }
        if (control_id == SystemTab)
        {
            select_console(false);
            return;
        }
        if (control_id == ClearProgram)
        {
            program_console.clear();
            update_console_badges();
            return;
        }
        if (control_id == ClearSystem)
        {
            system_console.clear();
            update_console_badges();
            return;
        }
        if (control_id == AutoBuildButton)
        {
            auto_build.enable(!auto_build.enabled());
            preferences.auto_build = auto_build.enabled();
            persist_preferences();
            appearance.active_button(find_control(AutoBuildButton), preferences.auto_build);
            watcher.stop();
            SetWindowTextW(find_control(AutoBuildButton),
                auto_build.enabled() ? L"Auto Build: ON" : L"Auto Build: OFF");
            start_watching();
            return;
        }
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
                reinterpret_cast<MINMAXINFO*>(lparam)->ptMinTrackSize = {
                    cpplivelab::scale_pixels(940, application->dpi),
                    cpplivelab::scale_pixels(650, application->dpi)};
                return 0;
            case WM_MEASUREITEM:
                if (reinterpret_cast<MEASUREITEMSTRUCT*>(lparam)->CtlType == ODT_COMBOBOX)
                {
                    reinterpret_cast<MEASUREITEMSTRUCT*>(lparam)->itemHeight =
                        cpplivelab::scale_pixels(24, application->dpi);
                    return TRUE;
                }
                break;
            case WM_DRAWITEM:
                if (application->appearance.draw_item(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam)))
                {
                    return TRUE;
                }
                break;
            case WM_ERASEBKGND:
                if (application->appearance.background_brush())
                {
                    RECT client{};
                    GetClientRect(window_handle, &client);
                    FillRect(reinterpret_cast<HDC>(wparam),
                        &client,
                        application->appearance.background_brush());
                    return 1;
                }
                break;
            case WM_PAINT:
            {
                PAINTSTRUCT paint{};
                const auto dc = BeginPaint(window_handle, &paint);
                application->appearance.paint_panels(dc);
                EndPaint(window_handle, &paint);
                return 0;
            }
            case WM_CTLCOLORSTATIC:
            case WM_CTLCOLOREDIT:
            case WM_CTLCOLORLISTBOX:
            {
                if (!application->appearance.background_brush())
                {
                    break;
                }
                const auto child = reinterpret_cast<HWND>(lparam);
                wchar_t class_name[64]{};
                GetClassNameW(child, class_name, 64);
                const bool input =
                    message != WM_CTLCOLORSTATIC || std::wstring_view(class_name) == L"Edit";
                const auto brush = application->appearance.control_colors(
                    reinterpret_cast<HDC>(wparam), input, IsWindowEnabled(child) != FALSE);
                if (child == application->find_control(StatusLabel))
                {
                    const auto status = read_control_text(child);
                    const auto& colors = application->appearance.palette.console;
                    if (status.find(L"Failed") != status.npos || status.starts_with(L"Error"))
                    {
                        SetTextColor(reinterpret_cast<HDC>(wparam), colors.error);
                    }
                    else if (status.find(L"Successful") != status.npos)
                    {
                        SetTextColor(reinterpret_cast<HDC>(wparam), colors.success);
                    }
                    else if (status.find(L"Deferred") != status.npos)
                    {
                        SetTextColor(reinterpret_cast<HDC>(wparam), colors.warning);
                    }
                }
                return reinterpret_cast<LRESULT>(brush);
            }
            case WM_THEMECHANGED:
            case WM_SETTINGCHANGE:
                if (application->preferences.theme == cpplivelab::ThemeChoice::FollowWindows)
                {
                    application->apply_appearance();
                }
                break;
            case WM_DPICHANGED:
            {
                application->dpi = HIWORD(wparam);
                application->update_fonts();
                const auto* suggested = reinterpret_cast<RECT*>(lparam);
                SetWindowPos(window_handle,
                    nullptr,
                    suggested->left,
                    suggested->top,
                    suggested->right - suggested->left,
                    suggested->bottom - suggested->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
                application->apply_appearance();
                return 0;
            }
            case WM_COMMAND:
                if (LOWORD(wparam) == AppearanceSelector && HIWORD(wparam) == CBN_SELCHANGE)
                {
                    application->change_appearance();
                    return 0;
                }
                if (LOWORD(wparam) == DebounceInput && HIWORD(wparam) == EN_KILLFOCUS)
                {
                    application->update_debounce();
                    return 0;
                }
                if (HIWORD(wparam) == BN_CLICKED)
                {
                    application->command(LOWORD(wparam));
                }
                return 0;
            case WM_TIMER:
                application->tick();
                return 0;
            case WM_CLOSE:
                application->watcher.stop();
                application->auto_build.enable(false);
                if (application->operation_in_progress || application->environment_future.valid())
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
        application->log(cpplivelab::MessageKind::Error, decode_console_text(error.what()));
        SetWindowTextW(application->find_control(StatusLabel), L"Error - see output");
    }
    return DefWindowProcW(window_handle, message, wparam, lparam);
}
} // namespace

int WINAPI wWinMain(HINSTANCE application_instance, HINSTANCE, PWSTR, int initial_show_state)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const auto rich_edit_library = LoadLibraryW(L"Msftedit.dll");
    if (!rich_edit_library)
    {
        return 1;
    }
    // The folder picker uses COM. Pair successful initialization with CoUninitialize
    // on this same thread once the message loop has ended.
    const auto com_initialization_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = window_procedure;
    window_class.hInstance = application_instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    window_class.hbrBackground = nullptr;
    window_class.lpszClassName = L"CppLiveLabWindow";
    RegisterClassW(&window_class);
    Application application;
    const auto initial_dpi = GetDpiForSystem();
    RECT desktop{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &desktop, 0);
    HWND window_handle = CreateWindowExW(WS_EX_CONTROLPARENT,
        window_class.lpszClassName,
        L"CppLiveLab",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        (std::min)(cpplivelab::scale_pixels(1080, initial_dpi),
            static_cast<int>(desktop.right - desktop.left)),
        (std::min)(cpplivelab::scale_pixels(800, initial_dpi),
            static_cast<int>(desktop.bottom - desktop.top)),
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
