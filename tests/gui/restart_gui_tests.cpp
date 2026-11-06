#include "appearance/appearance.hpp"
#include <shellapi.h>
#include "cpplivelab/libraries.hpp"
#include "projects/project_paths.hpp"
#include "../workspace_test_support.hpp"
#include "../ui_capture.hpp"
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

struct Windows
{
    ULONG_PTR key;
    DWORD process = 0;
    HWND main = nullptr;
    HWND confirmation = nullptr;
    unsigned count = 0;
};

BOOL CALLBACK enumerate(HWND window, LPARAM parameter)
{
    auto& result = *reinterpret_cast<Windows*>(parameter);
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    if (reinterpret_cast<ULONG_PTR>(GetPropW(window, L"CppLiveLabInstance")) == result.key &&
        IsWindowVisible(window))
    {
        result.main = window;
        result.process = process;
        ++result.count;
    }
    wchar_t title[128]{};
    GetWindowTextW(window, title, 128);
    if (process == result.process && std::wstring_view(title) == L"Restart EasierCpp")
    {
        result.confirmation = window;
    }
    return TRUE;
}

Windows windows(ULONG_PTR key, DWORD process = 0)
{
    Windows result{key, process};
    EnumWindows(enumerate, reinterpret_cast<LPARAM>(&result));
    return result;
}

template <class Predicate> void wait(Predicate predicate, const char* reason, DWORD timeout = 30000)
{
    const auto deadline = GetTickCount64() + timeout;
    while (GetTickCount64() < deadline)
    {
        if (predicate())
        {
            return;
        }
        Sleep(20);
    }
    throw std::runtime_error(reason);
}

std::wstring text(HWND control)
{
    std::wstring value(GetWindowTextLengthW(control) + 1, L'\0');
    value.resize(GetWindowTextW(control, value.data(), static_cast<int>(value.size())));
    return value;
}

void check_workspace_help(HWND main)
{
    HWND tooltip = nullptr;
    const auto visible_help = [&]
    {
        for (auto popup = GetTopWindow(nullptr); popup; popup = GetWindow(popup, GW_HWNDNEXT))
        {
            wchar_t name[64]{};
            GetClassNameW(popup, name, static_cast<int>(std::size(name)));
            if (GetWindow(popup, GW_OWNER) == main && IsWindowVisible(popup) &&
                std::wstring_view(name) == L"tooltips_class32")
            {
                tooltip = popup;
                return true;
            }
        }
        return false;
    };
    for (const int id : {166, 167, 168, 169})
    {
        const auto button = GetDlgItem(main, id);
        RECT bounds{};
        wait(
            [&]
            {
                return IsWindowVisible(button) && GetWindowRect(button, &bounds);
            },
            "Workspace information button did not become visible",
            1500);
        POINT center{(bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2};
        ScreenToClient(main, &center);
        require(ChildWindowFromPointEx(main, center, CWP_SKIPINVISIBLE) == button,
            "Another control overlaps the Workspace information button");

        SendMessageW(main, WM_LBUTTONDOWN, 0, 0);
        SendMessageW(button, WM_MOUSEHOVER, 0, 0);
        require(visible_help(), "Workspace information button did not show hover help");
        MONITORINFO monitor{sizeof(monitor)};
        RECT popup{};
        RECT intersection{};
        GetMonitorInfoW(MonitorFromWindow(button, MONITOR_DEFAULTTONEAREST), &monitor);
        require(GetWindowRect(tooltip, &popup) &&
                    IntersectRect(&intersection, &popup, &monitor.rcWork) &&
                    EqualRect(&intersection, &popup),
            "Workspace help opened outside the monitor work area");
        SendMessageW(button, WM_MOUSELEAVE, 0, 0);
        require(!visible_help(), "Hover help survived mouse leave");

        SendMessageW(button, BM_CLICK, 0, 0);
        require(visible_help(), "Workspace information button did not open help when clicked");
        SendMessageW(main, WM_PARENTNOTIFY, WM_LBUTTONDOWN, MAKELPARAM(5, 5));
        require(!visible_help(), "Workspace help survived a click elsewhere");

        SendMessageW(button, WM_SETFOCUS, 0, 0);
        require(visible_help(), "Workspace information button did not show help on focus");
        SendMessageW(button, WM_KEYDOWN, VK_ESCAPE, 0);
        require(!visible_help(), "Escape did not dismiss Workspace help");
        SendMessageW(button, WM_KEYDOWN, VK_RETURN, 0);
        require(visible_help(), "Enter did not open Workspace help");
        SendMessageW(button, WM_KEYDOWN, VK_ESCAPE, 0);
        SendMessageW(button, WM_KEYDOWN, VK_SPACE, 0);
        SendMessageW(button, WM_KEYUP, VK_SPACE, 0);
        require(visible_help(), "Space did not open Workspace help");
        SendMessageW(button, WM_KEYDOWN, VK_ESCAPE, 0);
    }
}

void check_public_build_and_run(HWND main, const fs::path& source)
{
    const auto write_program = [&](const char* marker)
    {
        std::ofstream output(source, std::ios::trunc);
        output << "#include <iostream>\n#include <string>\nint main() {\n"
                  "    std::cout << \""
               << marker
               << "\" << std::endl;\n"
                  "    for (std::string line; std::getline(std::cin, line);) {\n"
                  "        std::cout << \"echo: \" << line << std::endl;\n"
                  "    }\n}\n";
        require(output.good(), "Could not write the portable smoke-test program");
    };
    const auto sees_output = [&](std::wstring_view expected)
    {
        wchar_t output[1024]{};
        SendDlgItemMessageW(
            main, 111, WM_GETTEXT, std::size(output), reinterpret_cast<LPARAM>(output));
        return std::wstring_view(output).find(expected) != std::wstring_view::npos;
    };
    write_program("portable initial");
    PostMessageW(main, WM_COMMAND, 109, 0); // Run
    wait(
        [&]
        {
            return sees_output(L"portable initial");
        },
        "Packaged GUI did not build and run the disposable project",
        90000);
    require(
        SendDlgItemMessageW(main, 112, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(L"package input")),
        "Could not enter packaged program input");
    PostMessageW(main, WM_COMMAND, 113, 0); // Send
    wait(
        [&]
        {
            return sees_output(L"echo: package input");
        },
        "Packaged GUI did not deliver interactive program input");
    PostMessageW(main, WM_COMMAND, 148, 0); // Auto Run
    wait(
        [&]
        {
            return text(GetDlgItem(main, 148)) == L"Auto Run: On";
        },
        "Packaged GUI did not enable Auto Run");
    write_program("portable replacement");
    wait(
        [&]
        {
            return sees_output(L"portable replacement");
        },
        "Packaged Auto Run did not launch the edited program",
        90000);
    PostMessageW(main, WM_COMMAND, 110, 0); // Stop
    wait(
        [&]
        {
            return !IsWindowEnabled(GetDlgItem(main, 110));
        },
        "Packaged GUI did not stop its program");
    std::cout << "Packaged GUI: Build/Run, interactive input, Auto Run and Stop passed\n";
}

void tests(const fs::path& gui, const fs::path& screenshots, bool public_build)
{
    require(SetEnvironmentVariableW(L"CPPLIVELAB_TEST_CHILD", L"1"),
        "Could not isolate the GUI test registry profile");
    workspace_test::Fixture fixture;
    const auto project = fixture.root / L"restart project";
    const auto headers = fixture.root / L"test headers";
    fs::create_directories(headers);
    LibraryRecord local;
    local.id = "detail-test-headers";
    local.name = "Test Headers";
    local.version = "1";
    local.integration = "headers";
    local.local_path = path_support::path_utf8(headers);
    save_known_library(local);
    create_managed_project(project, "empty", local.id);
    const auto source = project / L"src/main.cpp";
    std::ifstream source_input(source, std::ios::binary);
    const std::string original{std::istreambuf_iterator<char>(source_input), {}};
    source_input.close();
    const auto key_text = library_key((fixture.root / L"manager state").string());
    const auto preferences_key =
        L"Software\\CppLiveLab\\Tests\\" + std::wstring(key_text.begin(), key_text.end());
    Preferences preferences{ThemeChoice::Dark, 650};
    require(
        save_preferences(preferences, preferences_key), "Cannot seed isolated restart preferences");

    struct RegistryCleanup
    {
        std::wstring key;

        ~RegistryCleanup()
        {
            RegDeleteTreeW(HKEY_CURRENT_USER, key.c_str());
        }
    } cleanup{preferences_key};

    EditorOptions editor;
    editor.choice = L"Custom";
    editor.executable = gui;
    editor.arguments = {L"{project}"};
    save_editor(editor);
    windows_support::UniqueHandle job;
    job.reset(CreateJobObjectW(nullptr, nullptr));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    require(SetInformationJobObject(
                job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) != FALSE,
        "Cannot supervise GUI test processes");
    const auto launch = [&]
    {
        auto command = windows_support::make_command_line(gui, {project.wstring()});
        STARTUPINFOW startup{sizeof(startup)};
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_SHOWNOACTIVATE;
        PROCESS_INFORMATION process{};
        require(CreateProcessW(gui.c_str(),
                    command.data(),
                    nullptr,
                    nullptr,
                    FALSE,
                    CREATE_SUSPENDED,
                    nullptr,
                    fixture.root.c_str(),
                    &startup,
                    &process) != FALSE,
            "Cannot launch actual GUI");
        windows_support::UniqueHandle process_handle;
        process_handle.reset(process.hProcess);
        windows_support::UniqueHandle thread;
        thread.reset(process.hThread);
        if (!AssignProcessToJobObject(job.value, process_handle.value))
        {
            TerminateProcess(process_handle.value, ERROR_CANCELLED);
            throw std::runtime_error("Cannot own actual GUI test process");
        }
        ResumeThread(thread.value);
        const auto result = process_handle.value;
        process_handle.value = nullptr;
        return result;
    };
    windows_support::UniqueHandle first;
    first.reset(launch());
    const auto original_pid = GetProcessId(first.value);
    ULONG_PTR key = 0;
    wait(
        [&]
        {
            struct Search
            {
                DWORD pid;
                ULONG_PTR key = 0;
            } search{original_pid};
            EnumWindows(
                [](HWND window, LPARAM parameter) -> BOOL
                {
                    auto& found = *reinterpret_cast<Search*>(parameter);
                    DWORD pid = 0;
                    GetWindowThreadProcessId(window, &pid);
                    if (pid == found.pid)
                    {
                        found.key =
                            reinterpret_cast<ULONG_PTR>(GetPropW(window, L"CppLiveLabInstance"));
                        if (found.key)
                        {
                            return FALSE;
                        }
                    }
                    return TRUE;
                },
                reinterpret_cast<LPARAM>(&search));
            key = search.key;
            return key != 0 && windows(key).count == 1;
        },
        "Actual GUI did not start");
    auto main = windows(key).main;
    require(ExtractIconExW(gui.c_str(), -1, nullptr, nullptr, 0) != 0 &&
                SendMessageW(main, WM_GETICON, ICON_SMALL, 0) != 0 &&
                SendMessageW(main, WM_GETICON, ICON_BIG, 0) != 0,
        "Actual EasierCpp executable or window is missing its branded icon");
    require(text(main) == L"EasierCpp", "Actual GUI title contains redundant branding");
    PostMessageW(main, WM_COMMAND, 142, 0); // Tools
    wait(
        [&]
        {
            return IsWindowVisible(GetDlgItem(main, 133));
        },
        "Toolchain diagnostics are missing from Tools");
    require((GetDlgItem(main, 135) != nullptr) == !public_build &&
                (GetDlgItem(main, 136) != nullptr) == !public_build &&
                GetDlgItem(main, 176) != nullptr,
        "Tools did not expose the expected public or development capabilities");
    PostMessageW(main, WM_COMMAND, 155, 0); // Workspace
    wait(
        [&]
        {
            return IsWindowVisible(GetDlgItem(main, 145));
        },
        "Workspace did not return");
    require(text(GetDlgItem(main, 160)) ==
                L"EasierCpp v" + path_support::from_utf8(EASIERCPP_VERSION).wstring(),
        "Application information does not display the authoritative version");
    require(
        main && IsWindowVisible(GetDlgItem(main, 145)) && IsWindowVisible(GetDlgItem(main, 143)),
        "Program Restart and Settings controls are not visible");
    require(text(GetDlgItem(main, 128)).find(project.wstring()) != std::wstring::npos,
        "Initial GUI project was not opened");
    if (!screenshots.empty())
    {
        ui_test::capture(main, screenshots / L"workspace-dark.png");
    }
    const auto open_section = [&](int id, std::wstring_view title)
    {
        PostMessageW(main, WM_COMMAND, id, 0);
        HWND form = nullptr;
        wait(
            [&]
            {
                form = GetWindow(GetDlgItem(main, 179), GW_CHILD);
                return form && IsWindowVisible(form) && text(form) == title;
            },
            "The requested section did not open in the inline detail panel");
        require(IsWindowEnabled(main) && IsWindowVisible(GetDlgItem(main, 157)) &&
                    IsWindowVisible(GetDlgItem(main, id)),
            "An inline form hid or disabled its parent page");
        return form;
    };
    const auto close_section = [&](HWND form)
    {
        PostMessageW(form, WM_COMMAND, IDCANCEL, 0);
        wait(
            [&]
            {
                return !IsWindowVisible(GetDlgItem(main, 179));
            },
            "Cancel did not dismiss the detail panel");
    };
    const auto workspace = [&]
    {
        PostMessageW(main, WM_COMMAND, 155, 0);
        wait(
            [&]
            {
                return IsWindowVisible(GetDlgItem(main, 145));
            },
            "Workspace did not return after navigating away from the detail panel");
    };
    const auto change_theme = [&](ThemeChoice theme)
    {
        PostMessageW(main, WM_COMMAND, 143, 0); // Settings
        const auto form = open_section(130, L"Appearance");
        SendDlgItemMessageW(form, 2000, CB_SETCURSEL, static_cast<WPARAM>(theme), 0);
        PostMessageW(form, WM_COMMAND, IDOK, 0);
        wait(
            [&]
            {
                return load_preferences(preferences_key).theme == theme;
            },
            "Inline theme selection was not persisted");
        workspace();
    };
    for (const auto theme : {ThemeChoice::Dark, ThemeChoice::Light})
    {
        if (theme == ThemeChoice::Light)
        {
            change_theme(theme);
        }
        check_workspace_help(main);
        if (theme == ThemeChoice::Light && !screenshots.empty())
        {
            ui_test::capture(main, screenshots / L"workspace-light.png");
        }
        PostMessageW(main, WM_COMMAND, 143, 0);
        const auto form = open_section(130, L"Appearance");
        if (!screenshots.empty())
        {
            ui_test::capture(main,
                screenshots /
                    (theme == ThemeChoice::Dark ? L"settings-dark.png" : L"settings-light.png"));
        }
        const auto other_theme =
            theme == ThemeChoice::Light ? ThemeChoice::Dark : ThemeChoice::Light;
        SendDlgItemMessageW(form, 2000, CB_SETCURSEL, static_cast<WPARAM>(other_theme), 0);
        PostMessageW(main, WM_COMMAND, 127, 0);
        HWND confirmation = nullptr;
        wait(
            [&]
            {
                confirmation = FindWindowW(L"#32770", L"Unsaved changes");
                return confirmation && GetWindow(confirmation, GW_OWNER) == main;
            },
            "Switching sections did not protect unsaved changes");
        PostMessageW(confirmation, WM_COMMAND, IDNO, 0);
        wait(
            [&]
            {
                return !IsWindow(confirmation);
            },
            "Discard confirmation did not close");
        require(IsWindowVisible(form) && SendDlgItemMessageW(form, 2000, CB_GETCURSEL, 0, 0) ==
                                             static_cast<LRESULT>(other_theme),
            "Keeping edits did not retain the original section and its pending theme");
        close_section(form);
        require(load_preferences(preferences_key).theme == theme,
            "Cancelling the section persisted its unsaved theme");

        const auto unsaved = open_section(130, L"Appearance");
        SendDlgItemMessageW(unsaved, 2000, CB_SETCURSEL, static_cast<WPARAM>(other_theme), 0);
        PostMessageW(main, WM_COMMAND, 127, 0);
        wait(
            [&]
            {
                confirmation = FindWindowW(L"#32770", L"Unsaved changes");
                return confirmation && GetWindow(confirmation, GW_OWNER) == main;
            },
            "Discard confirmation did not reopen");
        PostMessageW(confirmation, WM_COMMAND, IDYES, 0);
        wait(
            [&]
            {
                const auto editor = GetWindow(GetDlgItem(main, 179), GW_CHILD);
                return IsWindowVisible(editor) && text(editor) == L"Preferred Editor";
            },
            "Confirming discard did not open the requested Settings section");
        require(load_preferences(preferences_key).theme == theme,
            "Discarding a section persisted its unsaved theme");
        open_section(131, L"Build Preferences");
        close_section(open_section(149, L"Preferences"));
        PostMessageW(main, WM_COMMAND, 140, 0);
        open_section(125, L"Recent Projects");
        const auto project_form = open_section(126, L"Project Settings");
        SendMessageW(project_form, WM_COMMAND, 1500, 0);
        require(IsWindowVisible(GetDlgItem(project_form, 2001)) &&
                    IsWindowVisible(GetDlgItem(main, 125)),
            "Advanced Options hid the Project page or did not reveal its fields");
        const auto working_directory = theme == ThemeChoice::Dark ? headers : project;
        require(SendDlgItemMessageW(project_form,
                    2000,
                    WM_SETTEXT,
                    0,
                    reinterpret_cast<LPARAM>(working_directory.c_str())),
            "Could not enter the test working directory");
        PostMessageW(project_form, WM_COMMAND, IDOK, 0);
        wait(
            [&]
            {
                return !IsWindowVisible(GetDlgItem(main, 179)) &&
                       load_project_options(project, L"Debug").working_directory ==
                           working_directory;
            },
            "Project Settings did not persist the accepted working directory");
        PostMessageW(main, WM_COMMAND, 156, 0);
        const auto add_library = open_section(146, L"Add Library");
        SendDlgItemMessageW(add_library, 2000, CB_SETCURSEL, 1, 0);
        PostMessageW(add_library, WM_COMMAND, IDOK, 0);
        HWND repository = nullptr;
        wait(
            [&]
            {
                repository = GetWindow(GetDlgItem(main, 179), GW_CHILD);
                return IsWindowVisible(repository) &&
                       text(repository) == L"GitHub / GitLab Repository";
            },
            "Repository editing opened outside the Libraries page");
        require(IsWindowVisible(GetDlgItem(main, 146)) && IsWindowVisible(GetDlgItem(main, 132)),
            "Repository editing hid the Libraries section controls");
        close_section(repository);
        const auto manager = open_section(132, L"Manage Libraries");
        wchar_t action[128]{};
        SendDlgItemMessageW(manager, 2003, CB_GETLBTEXT, 3, reinterpret_cast<LPARAM>(action));
        require(std::wstring_view(action) == L"Edit Integration",
            "Managed test library did not expose an edit action");
        SendDlgItemMessageW(manager, 2003, CB_SETCURSEL, 3, 0);
        PostMessageW(manager, WM_COMMAND, IDOK, 0);
        HWND integration = nullptr;
        wait(
            [&]
            {
                integration = GetWindow(GetDlgItem(main, 179), GW_CHILD);
                return IsWindowVisible(integration) &&
                       text(integration) == L"Edit Library Integration";
            },
            "Managed library integration did not open in the Libraries detail panel");
        require(IsWindowVisible(GetDlgItem(main, 146)) && IsWindowVisible(GetDlgItem(main, 132)),
            "Editing an integration hid the Libraries section controls");
        require(
            SendDlgItemMessageW(integration, 2000, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(L"2")),
            "Could not edit the test library version");
        PostMessageW(integration, WM_COMMAND, IDCANCEL, 0);
        wait(
            [&]
            {
                const auto reopened = GetWindow(GetDlgItem(main, 179), GW_CHILD);
                return IsWindowVisible(reopened) && text(reopened) == L"Manage Libraries";
            },
            "Cancelling integration edits did not return to Library Manager");
        const auto pins = project_libraries(project);
        require(pins.size() == 1 && pins.front().version == "1",
            "Cancelling integration edits changed the fixture's library pin");
        workspace();
    }
    change_theme(ThemeChoice::Dark);
    PostMessageW(main, WM_COMMAND, 156, 0); // Libraries
    PostMessageW(main, WM_COMMAND, 132, 0); // Manage Libraries
    HWND library_form = nullptr;
    wait(
        [&]
        {
            library_form = GetWindow(GetDlgItem(main, 179), GW_CHILD);
            return library_form && IsWindowVisible(library_form) && GetDlgItem(library_form, 2004);
        },
        "Library management did not open in the main window");
    require(SendDlgItemMessageW(library_form, 2000, CB_GETCOUNT, 0, 0) == 3,
        "Managed, external and catalog categories are missing");
    const auto search = GetDlgItem(library_form, 2001);
    for (const auto character : std::wstring_view(L"owned-test-no-match"))
    {
        SendMessageW(search, WM_CHAR, character, 1);
    }
    wchar_t details[256]{};
    SendDlgItemMessageW(
        library_form, 2004, WM_GETTEXT, std::size(details), reinterpret_cast<LPARAM>(details));
    require(std::wstring_view(details).find(L"No entries match") != std::wstring_view::npos,
        "Library search did not update the in-page results");
    PostMessageW(main, WM_COMMAND, 155, 0);
    wait(
        [&]
        {
            return IsWindowVisible(GetDlgItem(main, 145)) != FALSE;
        },
        "Sidebar did not dismiss the inline library editor");
    windows_support::UniqueHandle duplicate;
    duplicate.reset(launch());
    require(WaitForSingleObject(duplicate.value, 10000) == WAIT_OBJECT_0 && windows(key).count == 1,
        "Second normal launch created a duplicate GUI");
    SendMessageW(main, WM_COMMAND, 108, 0);
    wait(
        [&]
        {
            return IsWindowEnabled(GetDlgItem(main, 110)) != FALSE;
        },
        "GUI build did not start");
    PostMessageW(main, WM_COMMAND, 144, 0);
    wait(
        [&]
        {
            return windows(key, original_pid).confirmation != nullptr;
        },
        "Restart during build did not request confirmation");
    PostMessageW(windows(key, original_pid).confirmation, WM_COMMAND, IDCANCEL, 0);
    wait(
        [&]
        {
            return windows(key, original_pid).confirmation == nullptr;
        },
        "Cancelled restart confirmation did not close");
    require(WaitForSingleObject(first.value, 0) == WAIT_TIMEOUT,
        "Cancelling restart closed the working application");
    PostMessageW(main, WM_COMMAND, 144, 0);
    wait(
        [&]
        {
            return windows(key, original_pid).confirmation != nullptr;
        },
        "Second restart request did not confirm active operation");
    PostMessageW(windows(key, original_pid).confirmation, WM_COMMAND, IDOK, 0);
    if (WaitForSingleObject(first.value, 30000) != WAIT_OBJECT_0)
    {
        std::wcout << L"Restart status: " << text(GetDlgItem(main, 115)) << L"\n"
                   << text(GetDlgItem(main, 120)) << std::flush;
        throw std::runtime_error("Original GUI did not exit");
    }
    wait(
        [&]
        {
            const auto replacement = windows(key);
            return replacement.count == 1 && replacement.process != original_pid;
        },
        "Replacement GUI did not become the sole visible instance");
    main = windows(key).main;
    require(text(GetDlgItem(main, 128)).find(project.wstring()) != std::wstring::npos,
        "Restart did not restore active project");
    require(load_preferences(preferences_key).theme == ThemeChoice::Dark &&
                load_preferences(preferences_key).debounce_ms == 650 &&
                load_editor().choice == L"Custom",
        "Restart lost persistent appearance/build/editor preferences");
    require(!IsWindowEnabled(GetDlgItem(main, 110)), "Restart retained a stale active build flag");
    const auto replacement_pid = windows(key).process;
    PostMessageW(main, WM_COMMAND, 144, 0);
    wait(
        [&]
        {
            const auto next = windows(key);
            return next.count == 1 && next.process != replacement_pid;
        },
        "Idle restart did not create a fresh GUI process");
    std::ifstream after(source, std::ios::binary);
    require(std::string(std::istreambuf_iterator<char>(after), {}) == original,
        "Restart altered project source");
    main = windows(key).main;
    if (public_build)
    {
        check_public_build_and_run(main, source);
    }
    PostMessageW(main, WM_CLOSE, 0, 0);
    wait(
        [&]
        {
            return windows(key).count == 0;
        },
        "Replacement did not close safely");
    std::cout
        << "Actual GUI restart: header, single instance, build confirmation/cancel, owned stop, "
           "new processes, project/settings/editor restoration and idle restart passed\n";
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    try
    {
        require(argc == 2 || argc == 3,
            "Expected GUI executable and optional screenshot folder or --public");
        const bool public_build = argc == 3 && std::wstring_view(argv[2]) == L"--public";
        tests(argv[1], argc == 3 && !public_build ? fs::path(argv[2]) : fs::path{}, public_build);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "GUI restart failure: " << error.what() << '\n';
        return 1;
    }
}
