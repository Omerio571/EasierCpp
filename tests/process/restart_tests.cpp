#include "process/restart.hpp"
#include "cpplivelab/workspace.hpp"
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

fs::path executable()
{
    std::wstring path(32768, L'\0');
    path.resize(GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size())));
    return path;
}

int parent()
{
    GuiInstance instance;
    require(instance.acquired, "Restart parent could not become primary");
    RestartState state;
    state.project = manager_state_directory() / L"project with spaces";
    state.preference_store = L"Software\\CppLiveLab\\Tests\\Restart";
    state.placement.showCmd = SW_SHOWMAXIMIZED;
    state.placement.rcNormalPosition = {10, 20, 1010, 720};
    RestartLaunch replacement(executable(), state, instance.name);
    const auto deadline = GetTickCount64() + 25000;
    while (GetTickCount64() < deadline)
    {
        const auto progress = replacement.poll();
        require(progress != RestartProgress::Failed, "Replacement failed before acknowledgment");
        if (progress == RestartProgress::Ready)
        {
            std::ofstream(manager_state_directory() / L"parent.txt")
                << GetCurrentProcessId() << ' ' << replacement.process_id();
            replacement.commit();
            return 0;
        }
        Sleep(10);
    }
    throw std::runtime_error("Replacement never acknowledged startup");
}

void tests()
{
    workspace_test::Fixture fixture;
    fs::create_directories(manager_state_directory());
    {
        GuiInstance instance;
        require(instance.acquired, "Primary instance acquisition failed");
        bool refused = false;
        try
        {
            RestartLaunch missing(fixture.root / L"missing.exe", {}, instance.name);
        }
        catch (const std::exception&)
        {
            refused = true;
        }
        require(
            refused && instance.acquired, "Failed relaunch did not retain the original instance");
    }
    // The driver owns the complete helper tree even after the parent hands its
    // child off. A test failure cannot leave a replacement helper behind.
    windows_support::UniqueHandle job;
    job.reset(CreateJobObjectW(nullptr, nullptr));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    require(SetInformationJobObject(
                job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) != FALSE,
        "Cannot supervise restart test helpers");
    auto command = windows_support::make_command_line(executable(), {L"--parent"});
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    require(CreateProcessW(executable().c_str(),
                command.data(),
                nullptr,
                nullptr,
                FALSE,
                CREATE_SUSPENDED | CREATE_NO_WINDOW,
                nullptr,
                fixture.root.c_str(),
                &startup,
                &process) != FALSE,
        "Cannot launch restart test parent");
    windows_support::UniqueHandle parent_process;
    parent_process.reset(process.hProcess);
    windows_support::UniqueHandle thread;
    thread.reset(process.hThread);
    if (!AssignProcessToJobObject(job.value, parent_process.value))
    {
        TerminateProcess(parent_process.value, ERROR_CANCELLED);
        throw std::runtime_error("Cannot assign restart test parent to owned job");
    }
    ResumeThread(thread.value);
    require(WaitForSingleObject(parent_process.value, 35000) == WAIT_OBJECT_0,
        "Restart parent did not exit");
    DWORD exit_code = 0;
    GetExitCodeProcess(parent_process.value, &exit_code);
    require(exit_code == 0, "Restart parent failed");
    const auto result = manager_state_directory() / L"child.txt";
    const auto deadline = GetTickCount64() + 20000;
    while (!fs::is_regular_file(result) && GetTickCount64() < deadline)
    {
        Sleep(10);
    }
    DWORD original = 0, replacement = 0, actual = 0;
    std::ifstream(manager_state_directory() / L"parent.txt") >> original >> replacement;
    std::ifstream(result) >> actual;
    require(original == process.dwProcessId && replacement != original && actual == replacement,
        "Restart did not transfer to a distinct acknowledged process");
    GuiInstance final_instance;
    require(final_instance.acquire(5000), "Replacement left single-instance mutex locked");
    std::cout << "Restart: real parent/child handover, project/preferences/window state, failed "
                 "relaunch passed\n";
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    try
    {
        if (argc == 2 && std::wstring_view(argv[1]) == L"--parent")
        {
            return parent();
        }
        if (argc > 1)
        {
            GuiInstance instance(true);
            const auto child = restart_child(argc, argv, instance.name);
            require(child->state.project == manager_state_directory() / L"project with spaces" &&
                        child->state.preference_store == L"Software\\CppLiveLab\\Tests\\Restart" &&
                        child->state.placement.showCmd == SW_SHOWMAXIMIZED &&
                        child->state.placement.rcNormalPosition.right == 1010,
                "Replacement did not restore durable restart state");
            require(child->acknowledge_and_wait(instance), "Replacement handover failed");
            std::ofstream(manager_state_directory() / L"child.txt") << GetCurrentProcessId();
            return 0;
        }
        tests();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Restart failure: " << error.what() << '\n';
        return 1;
    }
}
