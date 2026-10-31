#include "process/restart.hpp"
#include "cpplivelab/libraries.hpp"
#include "projects/project_paths.hpp"
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace cpplivelab
{
namespace
{
std::wstring instance_name()
{
    auto root = std::filesystem::weakly_canonical(manager_state_directory()).wstring();
    for (auto& character : root)
    {
        character = static_cast<wchar_t>(std::towlower(character));
    }
    return L"Local\\CppLiveLab-" +
           path_support::from_utf8(library_key(path_support::path_utf8(root))).wstring();
}

void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}
} // namespace

GuiInstance::GuiInstance(bool defer) : name(instance_name())
{
    const auto hash = name.substr(name.find_last_of(L'-') + 1);
    key = static_cast<ULONG_PTR>(std::stoull(hash, nullptr, 16));
    mutex_.reset(CreateMutexW(nullptr, FALSE, name.c_str()));
    require(mutex_.value != nullptr, "Cannot coordinate the application instance.");
    if (!defer)
    {
        windows_support::UniqueHandle handover;
        handover.reset(OpenEventW(SYNCHRONIZE, FALSE, (name + L"-handover").c_str()));
        if (!handover.value)
        {
            acquire(0);
        }
    }
}

GuiInstance::~GuiInstance()
{
    if (acquired)
    {
        ReleaseMutex(mutex_.value);
    }
}

bool GuiInstance::acquire(DWORD timeout)
{
    if (acquired)
    {
        return true;
    }
    const auto result = WaitForSingleObject(mutex_.value, timeout);
    acquired = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
    return acquired;
}

struct RestartLaunch::Impl
{
    windows_support::UniqueHandle process;
    windows_support::UniqueHandle job;
    windows_support::UniqueHandle ready;
    windows_support::UniqueHandle commit;
    windows_support::UniqueHandle handover;
    std::filesystem::path file;
    ULONGLONG started = GetTickCount64();
    DWORD process_id = 0;
    bool committed = false;

    ~Impl()
    {
        // Until commit this job owns only the hidden candidate process. A failed
        // startup must leave the original usable application alive.
        if (!committed && job.value)
        {
            TerminateJobObject(job.value, ERROR_CANCELLED);
            if (process.value)
            {
                WaitForSingleObject(process.value, 3000);
            }
        }
        if (!committed && !file.empty())
        {
            std::error_code error;
            std::filesystem::remove(file, error);
        }
    }
};

RestartLaunch::RestartLaunch(
    const std::filesystem::path& executable, const RestartState& state, const std::wstring& name)
    : impl_(std::make_unique<Impl>())
{
    const auto nonce =
        std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    const auto event_prefix = name + L"-" + nonce;
    impl_->ready.reset(CreateEventW(nullptr, TRUE, FALSE, (event_prefix + L"-ready").c_str()));
    require(impl_->ready.value && GetLastError() != ERROR_ALREADY_EXISTS,
        "Restart ready-event collision; the current application remains open.");
    impl_->commit.reset(CreateEventW(nullptr, TRUE, FALSE, (event_prefix + L"-commit").c_str()));
    require(impl_->commit.value && GetLastError() != ERROR_ALREADY_EXISTS,
        "Restart commit-event collision.");
    impl_->handover.reset(CreateEventW(nullptr, TRUE, FALSE, (name + L"-handover").c_str()));
    require(impl_->handover.value && GetLastError() != ERROR_ALREADY_EXISTS,
        "Another application handover is already active.");
    const auto directory = manager_state_directory() / L"restart";
    std::filesystem::create_directories(directory);
    impl_->file = directory / (nonce + L".state");
    std::ofstream output(impl_->file, std::ios::binary);
    const auto& rectangle = state.placement.rcNormalPosition;
    output << std::quoted(path_support::path_utf8(state.project)) << '\n'
           << std::quoted(path_support::path_utf8(state.preference_store)) << '\n'
           << state.placement.showCmd << ' ' << rectangle.left << ' ' << rectangle.top << ' '
           << rectangle.right << ' ' << rectangle.bottom << '\n';
    require(static_cast<bool>(output),
        "Cannot save the restart handover; current application retained.");
    output.close();
    impl_->job.reset(CreateJobObjectW(nullptr, nullptr));
    require(impl_->job.value != nullptr, "Cannot own replacement startup.");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    require(
        SetInformationJobObject(
            impl_->job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) != FALSE,
        "Cannot protect replacement startup.");
    auto command = windows_support::make_command_line(executable,
        {L"--restart-parent", std::to_wstring(GetCurrentProcessId()), L"--restart-token", nonce});
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    require(CreateProcessW(executable.c_str(),
                command.data(),
                nullptr,
                nullptr,
                FALSE,
                CREATE_SUSPENDED,
                nullptr,
                executable.parent_path().c_str(),
                &startup,
                &process) != FALSE,
        "Replacement could not be launched. The current application remains open.");
    impl_->process.reset(process.hProcess);
    impl_->process_id = process.dwProcessId;
    windows_support::UniqueHandle thread;
    thread.reset(process.hThread);
    if (!AssignProcessToJobObject(impl_->job.value, impl_->process.value))
    {
        TerminateProcess(impl_->process.value, ERROR_CANCELLED);
        throw std::runtime_error("Could not supervise replacement startup; original retained.");
    }
    require(
        ResumeThread(thread.value) != static_cast<DWORD>(-1), "Cannot resume replacement startup.");
}

RestartLaunch::~RestartLaunch() = default;

RestartProgress RestartLaunch::poll() const
{
    if (WaitForSingleObject(impl_->process.value, 0) == WAIT_OBJECT_0 ||
        GetTickCount64() - impl_->started > 20000)
    {
        return RestartProgress::Failed;
    }
    return WaitForSingleObject(impl_->ready.value, 0) == WAIT_OBJECT_0 ? RestartProgress::Ready
                                                                       : RestartProgress::Waiting;
}

void RestartLaunch::commit()
{
    require(poll() == RestartProgress::Ready, "Replacement startup was not acknowledged.");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    require(
        SetInformationJobObject(
            impl_->job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) != FALSE,
        "Cannot release replacement ownership; original retained.");
    require(SetEvent(impl_->commit.value) != FALSE, "Cannot commit the restart handover.");
    impl_->committed = true;
}

unsigned long RestartLaunch::process_id() const
{
    return impl_->process_id;
}

std::unique_ptr<RestartChild> restart_child(int argc, wchar_t** argv, const std::wstring& name)
{
    if (argc != 5 || std::wstring_view(argv[1]) != L"--restart-parent" ||
        std::wstring_view(argv[3]) != L"--restart-token")
    {
        throw std::runtime_error("Malformed restart handover.");
    }
    const std::wstring nonce = argv[4];
    require(!nonce.empty() && nonce.find_first_not_of(L"0123456789-") == nonce.npos,
        "Invalid restart nonce.");
    auto child = std::make_unique<RestartChild>();
    child->parent.reset(OpenProcess(SYNCHRONIZE, FALSE, std::stoul(argv[2])));
    child->ready.reset(
        OpenEventW(EVENT_MODIFY_STATE, FALSE, (name + L"-" + nonce + L"-ready").c_str()));
    child->commit.reset(OpenEventW(SYNCHRONIZE, FALSE, (name + L"-" + nonce + L"-commit").c_str()));
    child->handover.reset(OpenEventW(SYNCHRONIZE, FALSE, (name + L"-handover").c_str()));
    require(
        child->parent.value && child->ready.value && child->commit.value && child->handover.value,
        "Restart is expired or its parent is unavailable.");
    child->state_file = manager_state_directory() / L"restart" / (nonce + L".state");
    std::ifstream input(child->state_file, std::ios::binary);
    std::string project, preferences;
    auto& rectangle = child->state.placement.rcNormalPosition;
    input >> std::quoted(project) >> std::quoted(preferences) >> child->state.placement.showCmd >>
        rectangle.left >> rectangle.top >> rectangle.right >> rectangle.bottom;
    require(static_cast<bool>(input), "Restart state is incomplete.");
    child->state.project = path_support::from_utf8(project);
    child->state.preference_store = path_support::from_utf8(preferences).wstring();
    require(child->state.preference_store == L"Software\\CppLiveLab" ||
                child->state.preference_store.starts_with(L"Software\\CppLiveLab\\Tests\\"),
        "Restart preference store is outside CppLiveLab.");
    return child;
}

bool RestartChild::acknowledge_and_wait(GuiInstance& instance)
{
    if (!SetEvent(ready.value))
    {
        return false;
    }
    HANDLE events[]{commit.value, parent.value};
    if (WaitForMultipleObjects(2, events, FALSE, 25000) != WAIT_OBJECT_0 ||
        WaitForSingleObject(parent.value, 15000) != WAIT_OBJECT_0 || !instance.acquire(5000))
    {
        return false;
    }
    std::error_code error;
    std::filesystem::remove(state_file, error);
    handover.reset();
    return true;
}
} // namespace cpplivelab
