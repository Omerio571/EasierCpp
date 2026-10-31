#define CPPLIVELAB_GUI_TEST
#include "../../src/gui/gui.cpp"
#include "cpplivelab/projects.hpp"
#include "../workspace_test_support.hpp"
#include "../ui_capture.hpp"
#include <iostream>

namespace audit
{
std::string failure;
fs::path screenshots;
std::string case_name;
int observed_forms = 0;

struct UncertainFlow
{
    bool try_anyway;
    int step = 0;
    bool version_retained = false;
    ULONGLONG deadline = GetTickCount64() + 15000;
};

UncertainFlow* uncertain_flow = nullptr;

void CALLBACK drive_uncertain_flow(HWND, UINT, UINT_PTR, DWORD)
{
    EnumThreadWindows(
        GetCurrentThreadId(),
        [](HWND window, LPARAM) -> BOOL
        {
            auto& flow = *uncertain_flow;
            wchar_t title[128]{};
            GetWindowTextW(window, title, 128);
            if (GetTickCount64() > flow.deadline)
            {
                PostMessageW(window, WM_COMMAND, IDCANCEL, 0);
            }
            else if (flow.step == 0 && std::wstring_view(title) == L"GitHub Repository")
            {
                SetDlgItemTextW(window, 2000, L"https://github.com/example/library");
                ++flow.step;
                PostMessageW(window, WM_COMMAND, IDOK, 0);
            }
            else if (flow.step == 1 && std::wstring_view(title) == L"Unable to Verify")
            {
                ++flow.step;
                PostMessageW(window, WM_COMMAND, flow.try_anyway ? IDOK : IDCANCEL, 0);
            }
            else if (flow.step == 2 && std::wstring_view(title) == L"Library Version")
            {
                wchar_t version[128]{};
                GetDlgItemTextW(window, 2001, version, 128);
                flow.version_retained = std::wstring_view(version) == L"v1.0";
                ++flow.step;
                PostMessageW(window, WM_COMMAND, IDCANCEL, 0);
            }
            return TRUE;
        },
        0);
}

void require(bool value, const std::string& reason)
{
    if (!value)
    {
        throw std::runtime_error(reason);
    }
}

RECT bounds(HWND window, HWND parent)
{
    RECT result{};
    GetWindowRect(window, &result);
    MapWindowPoints(nullptr, parent, reinterpret_cast<POINT*>(&result), 2);
    return result;
}

// Only visible direct children count. Native dropdown lists and deliberately
// scrolled-off form fields are checked separately, not mistaken for overlaps.
void check_controls(HWND parent, bool scrollable)
{
    RECT client{};
    GetClientRect(parent, &client);
    std::vector<std::pair<int, RECT>> visible;
    for (auto child = GetWindow(parent, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
    {
        if (!(GetWindowLongPtrW(child, GWL_STYLE) & WS_VISIBLE))
        {
            continue;
        }
        const auto rectangle = bounds(child, parent);
        const int id = GetDlgCtrlID(child);
        require(rectangle.right > rectangle.left && rectangle.bottom > rectangle.top,
            case_name + ": zero-sized visible control " + std::to_string(id));
        require(rectangle.left >= 0 && rectangle.right <= client.right,
            case_name + ": horizontal clipping at control " + std::to_string(id));
        if (!scrollable)
        {
            require(rectangle.top >= 0 && rectangle.bottom <= client.bottom,
                case_name + ": vertical clipping at control " + std::to_string(id));
        }
        RECT clipped{};
        if (!IntersectRect(&clipped, &rectangle, &client))
        {
            continue;
        }
        for (const auto& [other_id, other] : visible)
        {
            RECT intersection{};
            if (IntersectRect(&intersection, &clipped, &other))
            {
                throw std::runtime_error(case_name + ": visible controls overlap: " +
                                         std::to_string(other_id) + "/" + std::to_string(id));
            }
        }
        visible.push_back({id, clipped});
    }
}

void capture(HWND window, const std::string& name)
{
    if (screenshots.empty())
    {
        return;
    }
    RedrawWindow(window,
        nullptr,
        nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
    ui_test::capture(window, screenshots / (name + ".png"));
}

// Runs inside the actual modal loop, inspecting the real production form built
// by its command handler. It never accepts changes or touches user-owned HWNDs.
void CALLBACK inspect_modal(HWND, UINT, UINT_PTR timer, DWORD)
{
    HWND form_window = nullptr;
    EnumThreadWindows(
        GetCurrentThreadId(),
        [](HWND window, LPARAM result) -> BOOL
        {
            wchar_t type[80]{};
            GetClassNameW(window, type, 80);
            if (std::wstring_view(type) == L"CppLiveLabProjectForm" && IsWindowVisible(window) &&
                IsWindowEnabled(window))
            {
                *reinterpret_cast<HWND*>(result) = window;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&form_window));
    if (!form_window)
    {
        return;
    }
    KillTimer(nullptr, timer);
    ++observed_forms;
    auto& form = *reinterpret_cast<cpplivelab::ProjectFormWindow*>(
        GetWindowLongPtrW(form_window, GWLP_USERDATA));
    try
    {
        const bool progress =
            !(GetWindowLongPtrW(GetDlgItem(form_window, IDOK), GWL_STYLE) & WS_VISIBLE);
        if (progress)
        {
            require(GetFocus() == GetDlgItem(form_window, IDCANCEL),
                case_name + ": progress Cancel lacks keyboard focus");
            require(LOWORD(SendMessageW(form_window, DM_GETDEFID, 0, 0)) == IDCANCEL,
                case_name + ": progress default action is hidden");
        }
        capture(form_window, case_name + "-normal");
        if (GetDlgItem(form_window, 1500))
        {
            form.command(1500);
            capture(form_window, case_name + "-expanded");
            form.command(1500);
        }
        const auto resources = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
        for (UINT dpi : {96U, 120U, 144U})
        {
            form.dpi = dpi;
            form.apply_theme();
            form.layout();
            for (const auto size : {SIZE{550, 320}, SIZE{760, 720}, SIZE{624, 540}})
            {
                SetWindowPos(form_window,
                    nullptr,
                    -30000,
                    -30000,
                    cpplivelab::scale_pixels(size.cx, dpi),
                    cpplivelab::scale_pixels(size.cy, dpi),
                    SWP_NOACTIVATE | SWP_NOZORDER);
                check_controls(form_window, true);
                for (int id : {IDOK, IDCANCEL})
                {
                    const auto action = GetDlgItem(form_window, id);
                    if (!(GetWindowLongPtrW(action, GWL_STYLE) & WS_VISIBLE))
                    {
                        continue;
                    }
                    form.ensure_visible(action);
                    RECT client{};
                    GetClientRect(form_window, &client);
                    const auto rectangle = bounds(action, form_window);
                    require(rectangle.top >= 0 && rectangle.bottom <= client.bottom,
                        case_name +
                            ": confirmation action cannot be reached by keyboard scrolling");
                }
                if (GetDlgItem(form_window, 1500))
                {
                    for (int repeat = 0; repeat < 4; ++repeat)
                    {
                        form.command(1500);
                        check_controls(form_window, true);
                    }
                }
            }
        }
        require(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) <= resources + 4,
            case_name + ": GDI leak during form resize/theme/toggle audit");
    }
    catch (const std::exception& error)
    {
        failure = error.what();
    }
    form.command(IDCANCEL);
}

void modal(Application& application, int command, const std::string& name)
{
    case_name = name;
    const int before = observed_forms;
    const auto timer = SetTimer(nullptr, 0, 30, inspect_modal);
    application.command(command);
    KillTimer(nullptr, timer);
    require(failure.empty(), failure);
    require(observed_forms == before + 1, name + ": expected one actual modal form");
}
} // namespace audit

int wmain(int argc, wchar_t** argv)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const auto rich_edit = LoadLibraryW(L"Msftedit.dll");
    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int result = 0;
    try
    {
        workspace_test::Fixture fixture;
        const auto metadata = cpplivelab::manager_state_directory() / L"github-metadata";
        fs::create_directories(metadata);
        std::ofstream(metadata / L"example--library.json")
            << R"({"repository":"example/library","versions":["v1.0"]})";
        for (const bool try_anyway : {false, true})
        {
            audit::UncertainFlow flow{try_anyway};
            audit::uncertain_flow = &flow;
            const auto timer = SetTimer(nullptr, 0, 20, audit::drive_uncertain_flow);
            const auto result = cpplivelab::github_library_dialog(nullptr,
                cpplivelab::ThemeChoice::Dark,
                application_directory() / L"cpplivelab.exe",
                [](const auto&)
                {
                });
            KillTimer(nullptr, timer);
            audit::uncertain_flow = nullptr;
            audit::require(!result && flow.step == (try_anyway ? 3 : 2) &&
                               (!try_anyway || flow.version_retained) &&
                               !fs::exists(cpplivelab::manager_state_directory() / L"integrations"),
                "Uncertain Add/Cancel flow lost version state, registered a library or failed to "
                "return");
        }
        for (const bool accept : {false, true})
        {
            cpplivelab::FormOptions uncertain;
            cpplivelab::initialize_uncertain_library_form(uncertain);
            audit::require(uncertain.accept_label == L"Try Adding Anyway" &&
                               uncertain.cancel_default &&
                               uncertain.note.find(L"Compatibility could not be verified.") == 0,
                "Uncertain compatibility lost its explicit warning or safe Cancel default");
            std::vector<cpplivelab::FormField> fields;
            cpplivelab::ProjectFormWindow warning(nullptr, L"Unable to Verify", fields, uncertain);
            warning.command(accept ? IDOK : IDCANCEL);
            audit::require(warning.finished && warning.accepted == accept,
                "Try Adding Anyway/Cancel did not preserve the native dialog decision");
        }
        wchar_t system[MAX_PATH]{};
        GetSystemDirectoryW(system, MAX_PATH);
        std::wstring timeout_output;
        const auto timeout_started = GetTickCount64();
        const bool timed_task = cpplivelab::run_library_task(
            nullptr,
            cpplivelab::ThemeChoice::Dark,
            fs::path(system) / L"WindowsPowerShell/v1.0/powershell.exe",
            {L"-NoProfile", L"-Command", L"Start-Sleep -Seconds 30"},
            fixture.root,
            [&](const auto& text)
            {
                timeout_output += text;
            },
            100);
        audit::require(!timed_task && GetTickCount64() - timeout_started < 5000 &&
                           timeout_output.find(L"Unable to Verify") != std::wstring::npos,
            "Slow metadata worker did not time out responsively with an uncertain result");
        for (int session = 0; session < 2; ++session)
        {
            std::vector<cpplivelab::FormField> repository{
                {L"Stale repository", L"https://github.com/raysan5/raylib"}};
            cpplivelab::FormOptions options;
            cpplivelab::initialize_github_repository_form(repository, options);
            audit::require(repository.size() == 1 && repository.front().value.empty(),
                "Fresh GitHub Add retained repository/version state");
            cpplivelab::ProjectFormWindow form(nullptr, L"GitHub Repository", repository, options);
            audit::require(GetWindowTextLengthW(GetDlgItem(form.window, 2000)) == 0 &&
                               !IsWindowEnabled(GetDlgItem(form.window, IDOK)),
                "GitHub placeholder counted as input or empty URL enabled retrieval");
            SetDlgItemTextW(form.window, 2000, L"https://github.com/raysan5/raylib");
            audit::require(IsWindowEnabled(GetDlgItem(form.window, IDOK)),
                "Valid GitHub URL did not enable retrieval");
            SetDlgItemTextW(form.window, 2000, L"https://github.com/user/repo?token=invalid");
            audit::require(!IsWindowEnabled(GetDlgItem(form.window, IDOK)),
                "Invalid GitHub URL enabled retrieval");
            form.command(IDCANCEL);
        }
        const auto project = fixture.root / L"Audit calculator";
        cpplivelab::create_project(project, true);
        cpplivelab::remember_project(project);
        Application app;
        app.preferences = {};
        app.preference_store =
            L"Software\\CppLiveLab\\UiAudit\\" + std::to_wstring(GetCurrentProcessId());
        WNDCLASSW type{};
        type.lpfnWndProc = window_procedure;
        type.hInstance = GetModuleHandleW(nullptr);
        type.lpszClassName = L"CppLiveLabAuditHost";
        RegisterClassW(&type);
        const bool quick = argc > 1 && std::wstring(argv[1]) == L"--quick";
        const bool interactive = argc > 1 && std::wstring(argv[1]) == L"--interactive";
        if (argc > 1 && !interactive && !quick)
        {
            audit::screenshots = fs::absolute(argv[1]);
        }
        const auto window = CreateWindowExW(WS_EX_CONTROLPARENT,
            type.lpszClassName,
            L"CppLiveLab UI Audit (isolated settings)",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            interactive ? CW_USEDEFAULT : -30000,
            interactive ? CW_USEDEFAULT : -30000,
            1080,
            800,
            nullptr,
            nullptr,
            type.hInstance,
            &app);
        audit::require(window != nullptr, "Audit host creation failed");
        ShowWindow(window, interactive ? SW_SHOW : SW_SHOWNOACTIVATE);
        if (interactive)
        {
            MSG message{};
            while (GetMessageW(&message, nullptr, 0, 0) > 0)
            {
                if (message.message == WM_KEYDOWN && message.wParam == VK_RETURN &&
                    GetFocus() == app.find_control(ProgramInput))
                {
                    app.send();
                    continue;
                }
                if (!IsDialogMessageW(window, &message))
                {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
            }
        }
        else
        {
            for (const auto theme : {cpplivelab::ThemeChoice::Dark,
                     cpplivelab::ThemeChoice::Light,
                     cpplivelab::ThemeChoice::FollowWindows})
            {
                const auto prefix = theme == cpplivelab::ThemeChoice::Dark    ? "dark"
                                    : theme == cpplivelab::ThemeChoice::Light ? "light"
                                                                              : "windows";
                app.apply_appearance_choice(theme);
                audit::case_name = std::string(prefix) + "-welcome-diagnostics";
                app.active_project_directory.clear();
                app.command(DoctorMenu);
                audit::check_controls(window, false);
                audit::capture(window, audit::case_name);
                app.open(project);
                for (const auto [id, name] :
                    std::vector<std::pair<int, std::string>>{{NewProjectMenu, "new-project"},
                        {RecentProjectsMenu, "recents"},
                        {ProjectSettingsMenu, "project-settings"},
                        {EditorSettingsMenu, "editor"},
                        {ManageLibrariesMenu, "library-manager"},
                        {BuildPreferencesMenu, "build-preferences"},
                        {AppearanceMenu, "appearance"},
                        {ReviewErrorsMenu, "build-errors"},
                        {RecoverBuildMenu, "fresh-build-recovery"},
                        {TestRunnerMenu, "test-runner"}})
                {
                    if (!quick)
                    {
                        audit::modal(app, id, std::string(prefix) + "-" + name);
                    }
                }
                app.select_console(false);
                for (const auto size :
                    {SIZE{780, 600}, SIZE{1080, 800}, SIZE{1600, 950}, SIZE{780, 600}})
                {
                    audit::case_name = std::string(prefix) + "-workspace";
                    SetWindowPos(window,
                        nullptr,
                        -30000,
                        -30000,
                        size.cx,
                        size.cy,
                        SWP_NOACTIVATE | SWP_NOZORDER);
                    audit::check_controls(window, false);
                }
                audit::capture(window, audit::case_name);
            }
            if (!quick)
            {
                audit::case_name = "progress-cancellation";
                const auto script = fixture.root / L"controlled-progress.ps1";
                std::ofstream(script)
                    << "Write-Output 'Owned progress fixture'\nStart-Sleep -Seconds 30\n";
                wchar_t system[MAX_PATH]{};
                GetSystemDirectoryW(system, MAX_PATH);
                SetFocus(app.find_control(RunButton));
                const auto timer = SetTimer(nullptr, 0, 30, audit::inspect_modal);
                const bool completed = cpplivelab::run_library_task(window,
                    app.preferences.theme,
                    fs::path(system) / L"WindowsPowerShell/v1.0/powershell.exe",
                    {L"-NoProfile", L"-ExecutionPolicy", L"Bypass", L"-File", script.wstring()},
                    fixture.root,
                    [](const std::wstring&)
                    {
                    });
                KillTimer(nullptr, timer);
                audit::require(!completed && audit::failure.empty(),
                    "Progress cancellation failed: " + audit::failure);
                audit::require(GetFocus() == app.find_control(RunButton) && IsWindowEnabled(window),
                    "Progress modal did not restore owner focus/state");
            }
            DestroyWindow(window);
            std::cout << (quick ? "UI layout: welcome/system console and workspace bounds passed\n"
                                : "UI audit: 30 production modal forms, themes, 100/125/150% "
                                  "logical DPI, resize, Advanced, action accessibility and "
                                  "workspace bounds passed\n");
        }
        RegDeleteTreeW(HKEY_CURRENT_USER, app.preference_store.c_str());
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
    if (rich_edit)
    {
        FreeLibrary(rich_edit);
    }
    return result;
}
