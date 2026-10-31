#include "appearance.hpp"
#include "cpplivelab/libraries.hpp"
#include "workspace_test_support.hpp"
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
    if (process == result.process && std::wstring_view(title) == L"Restart Cpp")
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

void tests(const fs::path& gui)
{
    workspace_test::Fixture fixture;
    const auto project = fixture.root / L"restart project";
    create_managed_project(project, "empty");
    const auto source = project / L"src/main.cpp";
    std::ifstream source_input(source, std::ios::binary);
    const std::string original{std::istreambuf_iterator<char>(source_input), {}};
    source_input.close();
    const auto key_text = library_key((fixture.root / L"manager state").string());
    const auto preferences_key =
        L"Software\\CppLiveLab\\Tests\\" + std::wstring(key_text.begin(), key_text.end());
    Preferences preferences{ThemeChoice::Dark, false, 650};
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
    require(
        main && IsWindowVisible(GetDlgItem(main, 145)) && IsWindowVisible(GetDlgItem(main, 143)),
        "Program Restart and Settings controls are not visible");
    require(text(GetDlgItem(main, 128)).find(project.wstring()) != std::wstring::npos,
        "Initial GUI project was not opened");
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
        require(argc == 2, "Expected GUI executable");
        tests(argv[1]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "GUI restart failure: " << error.what() << '\n';
        return 1;
    }
}
