#include "cpplivelab/session.hpp"
#include "windows_support.hpp"
#include "project_paths.hpp"
#include <array>
#include <cwctype>
#include <stdexcept>

namespace cpplivelab
{
namespace
{

using windows_support::make_command_line;
using windows_support::UniqueHandle;

void create_output_pipe(UniqueHandle& read_handle, UniqueHandle& write_handle, DWORD pipe_capacity)
{
    SECURITY_ATTRIBUTES pipe_security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    if (!CreatePipe(&read_handle.value, &write_handle.value, &pipe_security, pipe_capacity))
    {
        throw std::runtime_error("Cannot create console pipe");
    }
}
} // namespace

// Implementation details stay out of the public header. Each UniqueHandle closes
// exactly one Windows resource; the GUI owns this state through its ConsoleSession.
struct ConsoleSession::Impl
{
    // The job groups the worker, compiler and program. The process handle identifies
    // only the initial worker and is used to retrieve its exit code.
    UniqueHandle job_handle;
    UniqueHandle running_process_handle;

    // Parent-side endpoints: read captured output and write the program's stdin.
    UniqueHandle output_pipe;
    UniqueHandle error_pipe;
    UniqueHandle input_pipe;

    // OVERLAPPED describes a write Windows may finish later. Its event and backing
    // text must remain alive until completion or cancellation has been acknowledged.
    UniqueHandle input_completion_event;
    OVERLAPPED pending_write{};
    std::string pending_input_text;
    bool is_input_pending = false;
    bool has_launched = false;
    int process_exit_code = 0;

    void prepare_input_pipe(UniqueHandle& child_stdin_read_handle);
};

// Prepare the parent's asynchronous writer and the inheritable child-side reader.
void ConsoleSession::Impl::prepare_input_pipe(UniqueHandle& child_stdin_read_handle)
{
    // Overlapped named-pipe writes keep Send nonblocking even if the program is
    // not reading. Peeking a synchronous stdin pipe can deadlock behind std::cin.
    static unsigned pipe_sequence = 0;
    const auto input_pipe_name = L"\\\\.\\pipe\\CppLiveLab-" +
                                 std::to_wstring(GetCurrentProcessId()) + L"-" +
                                 std::to_wstring(++pipe_sequence);
    input_pipe.reset(CreateNamedPipeW(input_pipe_name.c_str(),
        PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1,
        65536,
        65536,
        0,
        nullptr));
    if (input_pipe.value == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error("Cannot create asynchronous input pipe");
    }
    SECURITY_ATTRIBUTES pipe_security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    child_stdin_read_handle.reset(CreateFileW(input_pipe_name.c_str(),
        GENERIC_READ,
        0,
        &pipe_security,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr));
    if (child_stdin_read_handle.value == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error("Cannot connect console input");
    }
    input_completion_event.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!input_completion_event.value)
    {
        throw std::runtime_error("Cannot create console input event");
    }
    pending_write.hEvent = input_completion_event.value;
    if (!ConnectNamedPipe(input_pipe.value, &pending_write))
    {
        const auto error = GetLastError();
        if (error != ERROR_PIPE_CONNECTED)
        {
            throw std::runtime_error("Cannot connect console input server");
        }
    }
    ResetEvent(input_completion_event.value);
}

ConsoleSession::ConsoleSession() : impl_(std::make_unique<Impl>())
{
}

ConsoleSession::~ConsoleSession()
{
    // KILL_ON_JOB_CLOSE also covers unexpected GUI exit. Only owned processes die.
    stop();
}

bool ConsoleSession::running() const
{
    if (!impl_->has_launched)
    {
        return false;
    }
    // A worker may exit before one of its children. Waiting only for the original
    // process could enable Build too early and try to overwrite a running executable.
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION job_accounting{};
    if (!QueryInformationJobObject(impl_->job_handle.value,
            JobObjectBasicAccountingInformation,
            &job_accounting,
            sizeof(job_accounting),
            nullptr))
    {
        // Keep Build disabled when Windows cannot confirm that the tree is empty.
        return true;
    }
    return job_accounting.ActiveProcesses != 0;
}

bool ConsoleSession::output_pending() const
{
    const auto pending = [](HANDLE pipe)
    {
        DWORD available = 0;
        return pipe && PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) &&
               available != 0;
    };
    return pending(impl_->output_pipe.value) || pending(impl_->error_pipe.value);
}

void ConsoleSession::start(const std::filesystem::path& executable,
    const std::vector<std::wstring>& arguments,
    const std::filesystem::path& directory)
{
    start_impl(executable, arguments, directory, {}, false);
}

void ConsoleSession::start_program(const std::filesystem::path& executable,
    const std::vector<std::wstring>& arguments,
    const std::filesystem::path& directory,
    const Environment& environment)
{
    start_impl(executable, arguments, directory, environment, true);
}

void ConsoleSession::start_impl(const std::filesystem::path& executable,
    const std::vector<std::wstring>& arguments,
    const std::filesystem::path& directory,
    const Environment& environment,
    bool separate_stderr)
{
    if (running())
    {
        throw std::runtime_error("Stop the current operation before building or running again.");
    }
    close_input();
    impl_ = std::make_unique<Impl>();
    auto& process_state = *impl_;

    // Closing this job is the last safety net if the GUI exits unexpectedly.
    process_state.job_handle.reset(CreateJobObjectW(nullptr, nullptr));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!process_state.job_handle.value || !SetInformationJobObject(process_state.job_handle.value,
                                               JobObjectExtendedLimitInformation,
                                               &limits,
                                               sizeof(limits)))
    {
        throw std::runtime_error("Cannot create process-tree safety job");
    }
    UniqueHandle child_stdout_write_handle, child_stderr_write_handle, child_stdin_read_handle;

    // stdout and stderr share one anonymous pipe, preserving their arrival order.
    // These child-side handles are local: after CreateProcess inherits them, the
    // parent must close its copies so pipe EOF can be detected correctly.
    create_output_pipe(process_state.output_pipe, child_stdout_write_handle, 65536);
    if (separate_stderr)
    {
        create_output_pipe(process_state.error_pipe, child_stderr_write_handle, 65536);
        SetHandleInformation(process_state.error_pipe.value, HANDLE_FLAG_INHERIT, 0);
    }
    process_state.prepare_input_pipe(child_stdin_read_handle);

    // Only child endpoints should be inherited. An inherited copy of the parent's
    // writer could keep stdin open forever after the GUI sends End Input.
    SetHandleInformation(process_state.output_pipe.value, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(process_state.input_pipe.value, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW startup_information{};
    startup_information.cb = sizeof(startup_information);
    startup_information.dwFlags = STARTF_USESTDHANDLES;
    startup_information.hStdInput = child_stdin_read_handle.value;
    startup_information.hStdOutput = child_stdout_write_handle.value;
    startup_information.hStdError =
        separate_stderr ? child_stderr_write_handle.value : child_stdout_write_handle.value;
    PROCESS_INFORMATION process_information{};
    auto process_command_line = make_command_line(executable, arguments);

    // CreateProcess returns separate process and primary-thread handles. Suspension
    // lets us install job ownership before any compiler or program code can execute.
    if (!CreateProcessW(executable.c_str(),
            process_command_line.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
            environment.empty() ? nullptr : const_cast<wchar_t*>(environment.data()),
            directory.c_str(),
            &startup_information,
            &process_information))
    {
        throw std::runtime_error(
            "Cannot launch operation (Windows error " + std::to_string(GetLastError()) + ")");
    }
    process_state.running_process_handle.reset(process_information.hProcess);
    UniqueHandle primary_thread_handle;
    primary_thread_handle.reset(process_information.hThread);
    if (!AssignProcessToJobObject(
            process_state.job_handle.value, process_state.running_process_handle.value))
    {
        // The child is still suspended and has never run. If job assignment fails,
        // terminate this one newly created process rather than letting it escape.
        TerminateProcess(process_state.running_process_handle.value, 1);
        WaitForSingleObject(process_state.running_process_handle.value, INFINITE);
        throw std::runtime_error(
            "Cannot safely supervise process tree; operation was not started.");
    }
    process_state.has_launched = true;
    if (ResumeThread(primary_thread_handle.value) == static_cast<DWORD>(-1))
    {
        stop();
        throw std::runtime_error("Cannot resume operation");
    }
}

std::vector<ProcessOutput> ConsoleSession::poll_output()
{
    if (impl_->is_input_pending)
    {
        DWORD bytes_written = 0;
        if (GetOverlappedResult(
                impl_->input_pipe.value, &impl_->pending_write, &bytes_written, FALSE) ||
            GetLastError() != ERROR_IO_INCOMPLETE)
        {
            impl_->is_input_pending = false;
            impl_->pending_input_text.clear();
        }
    }

    std::vector<ProcessOutput> chunks;
    std::array<char, 4096> buffer{};
    size_t total_bytes = 0;
    bool read_something = true;
    while (read_something && total_bytes < 65536)
    {
        read_something = false;
        // Fairly alternate pipes. Windows does not supply a global ordering across
        // independent stdout/stderr pipes; each stream's byte ordering is preserved.
        for (const auto stream : {OutputStream::StandardOutput, OutputStream::StandardError})
        {
            const auto pipe = stream == OutputStream::StandardOutput ? impl_->output_pipe.value
                                                                     : impl_->error_pipe.value;
            DWORD available = 0;
            if (!pipe || !PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) ||
                !available)
            {
                continue;
            }
            DWORD bytes_read = 0;
            if (ReadFile(pipe,
                    buffer.data(),
                    (std::min)(available, static_cast<DWORD>(buffer.size())),
                    &bytes_read,
                    nullptr) &&
                bytes_read)
            {
                chunks.push_back({stream, std::string(buffer.data(), bytes_read)});
                total_bytes += bytes_read;
                read_something = true;
            }
        }
    }

    if (impl_->has_launched && !running())
    {
        DWORD exit_code = 1;
        if (GetExitCodeProcess(impl_->running_process_handle.value, &exit_code))
        {
            impl_->process_exit_code = static_cast<int>(exit_code);
        }
    }
    return chunks;
}

std::string ConsoleSession::poll()
{
    std::string merged_output;
    for (const auto& chunk : poll_output())
    {
        merged_output += chunk.bytes;
    }
    return merged_output;
}

bool ConsoleSession::send_line(const std::string& input_line)
{
    if (!running() || !impl_->input_pipe.value || impl_->is_input_pending ||
        input_line.size() > 4096)
    {
        return false;
    }
    impl_->pending_input_text = input_line + "\n";
    ResetEvent(impl_->input_completion_event.value);
    DWORD bytes_written = 0;
    if (WriteFile(impl_->input_pipe.value,
            impl_->pending_input_text.data(),
            static_cast<DWORD>(impl_->pending_input_text.size()),
            &bytes_written,
            &impl_->pending_write))
    {
        impl_->pending_input_text.clear();
        return true;
    }
    if (GetLastError() == ERROR_IO_PENDING)
    {
        impl_->is_input_pending = true;
        return true;
    }
    impl_->pending_input_text.clear();
    return false;
}

void ConsoleSession::close_input()
{
    if (impl_->is_input_pending)
    {
        // Cancellation is asynchronous too. Wait for acknowledgement before clearing
        // the string; Windows may still be reading its memory until then.
        CancelIoEx(impl_->input_pipe.value, &impl_->pending_write);
        DWORD bytes_written = 0;
        GetOverlappedResult(impl_->input_pipe.value, &impl_->pending_write, &bytes_written, TRUE);
        impl_->is_input_pending = false;
        impl_->pending_input_text.clear();
    }
    impl_->input_pipe.reset();
}

void ConsoleSession::stop()
{
    close_input();
    if (impl_->job_handle.value && impl_->has_launched && running())
    {
        // This is a forced stop, not a user-program cleanup callback. The job limits
        // termination to our owned tree and leaves the separately opened IDE alone.
        TerminateJobObject(impl_->job_handle.value, 130);
    }
}

int ConsoleSession::exit_code() const
{
    return impl_->process_exit_code;
}

std::filesystem::path validate_gui_project(const std::filesystem::path& folder)
{
    namespace fs = std::filesystem;
    if (folder.empty())
    {
        throw std::runtime_error("Choose a CMake project folder first.");
    }
    const auto selected = fs::weakly_canonical(fs::absolute(folder));
    const auto manager_directory = fs::weakly_canonical(fs::path(CPPLIVELAB_SOURCE_DIR));

    if (path_support::is_within_directory(selected, manager_directory))
    {
        throw std::runtime_error("Choose a user project outside the CppLiveLab source tree.");
    }
    if (!fs::is_directory(selected) || !fs::is_regular_file(selected / L"CMakeLists.txt"))
    {
        throw std::runtime_error("This folder does not contain CMakeLists.txt.");
    }
    return selected;
}

void open_in_visual_studio(
    const std::filesystem::path& ide_executable, const std::filesystem::path& folder)
{
    if (!std::filesystem::is_regular_file(ide_executable))
    {
        throw std::runtime_error(
            "Visual Studio 2022 IDE not found. Install its Desktop development with C++ workload.");
    }
    auto process_command_line =
        make_command_line(ide_executable, {validate_gui_project(folder).wstring()});
    STARTUPINFOW startup_information{};
    startup_information.cb = sizeof(startup_information);
    PROCESS_INFORMATION process_information{};
    if (!CreateProcessW(ide_executable.c_str(),
            process_command_line.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            folder.c_str(),
            &startup_information,
            &process_information))
    {
        throw std::runtime_error("Could not open Visual Studio 2022");
    }
    CloseHandle(process_information.hThread);
    CloseHandle(process_information.hProcess);
}
} // namespace cpplivelab
