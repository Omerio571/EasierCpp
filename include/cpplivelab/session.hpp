#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "cpplivelab/process.hpp"

namespace cpplivelab
{
enum class OutputStream
{
    StandardOutput,
    StandardError
};

struct ProcessOutput
{
    OutputStream stream;
    std::string bytes;
};

// GUI-owned process tree. No detached reader threads or shell commands.
// Methods are called on the UI thread; poll drains output without waiting.
class ConsoleSession
{
public:
    ConsoleSession();
    ~ConsoleSession();
    // Exchange whole heap-owned jobs/pipes; pending async buffers keep their addresses.
    void swap(ConsoleSession& other) noexcept;
    ConsoleSession(const ConsoleSession&) = delete;
    ConsoleSession& operator=(const ConsoleSession&) = delete;
    // Starts one suspended child, assigns it to an owned Windows job, then resumes it.
    // That ordering prevents the child from spawning unsupervised descendants.
    void start(const std::filesystem::path& executable,
        const std::vector<std::wstring>& arguments,
        const std::filesystem::path& directory);
    // A CLI/tool worker's root exit is authoritative. Polling drains its output
    // and removes owned background helpers; interactive programs use start/start_program.
    void start_worker(const std::filesystem::path& executable,
        const std::vector<std::wstring>& arguments,
        const std::filesystem::path& directory,
        const Environment& environment = {});
    // The GUI's program phase uses independent stdout/stderr pipes. Build workers
    // and the original start/poll API retain their approved merged-stream behavior.
    void start_program(const std::filesystem::path& executable,
        const std::vector<std::wstring>& arguments,
        const std::filesystem::path& directory,
        const Environment& environment,
        bool avoid_activation = false);
    std::vector<ProcessOutput> poll_output();
    // Drain a bounded amount of available output without waiting for more bytes.
    // Call again on a later UI timer tick to keep the message loop responsive.
    std::string poll();

    // running includes descendants even if the original child has already exited.
    bool running() const;
    bool output_pending() const;
    // Build workers have an authoritative root exit, unlike interactive program
    // trees. Background tool helpers must not keep a completed build in Building.
    bool root_exited() const;
    unsigned long process_id() const;
    std::vector<unsigned long> active_process_ids() const;
    void finish_build();
    void release_completed();

    // Accept one UTF-8 line using an asynchronous write. False means input is busy,
    // unavailable or too long; the caller may retain the line and retry after polling.
    bool send_line(const std::string& line);

    // EOF closes only this session's writer. Stop also force-terminates the owned job;
    // it never searches for process names or terminates unrelated programs/Visual Studio.
    void close_input();
    void stop();
    int exit_code() const;

private:
    void start_impl(const std::filesystem::path& executable,
        const std::vector<std::wstring>& arguments,
        const std::filesystem::path& directory,
        const Environment& environment,
        bool separate_stderr,
        bool avoid_activation = false);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

std::filesystem::path validate_gui_project(const std::filesystem::path& folder);
// Open Folder is supported directly by devenv.exe; the project is not rewritten.
void open_in_visual_studio(const std::filesystem::path& ide, const std::filesystem::path& folder);
} // namespace cpplivelab
