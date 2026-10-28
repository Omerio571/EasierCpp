#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace cpplivelab
{
// GUI-owned process tree. No detached reader threads or shell commands.
// Methods are called on the UI thread; poll drains output without waiting.
class ConsoleSession
{
public:
    ConsoleSession();
    ~ConsoleSession();
    ConsoleSession(const ConsoleSession&) = delete;
    ConsoleSession& operator=(const ConsoleSession&) = delete;
    // Starts one suspended child, assigns it to an owned Windows job, then resumes it.
    // That ordering prevents the child from spawning unsupervised descendants.
    void start(const std::filesystem::path& executable,
        const std::vector<std::wstring>& arguments,
        const std::filesystem::path& directory);
    // Drain a bounded amount of available output without waiting for more bytes.
    // Call again on a later UI timer tick to keep the message loop responsive.
    std::string poll();

    // running includes descendants even if the original child has already exited.
    bool running() const;
    bool output_pending() const;

    // Accept one UTF-8 line using an asynchronous write. False means input is busy,
    // unavailable or too long; the caller may retain the line and retry after polling.
    bool send_line(const std::string& line);

    // EOF closes only this session's writer. Stop also force-terminates the owned job;
    // it never searches for process names or terminates unrelated programs/Visual Studio.
    void close_input();
    void stop();
    int exit_code() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

std::filesystem::path validate_gui_project(const std::filesystem::path& folder);
// Open Folder is supported directly by devenv.exe; the project is not rewritten.
void open_in_visual_studio(const std::filesystem::path& ide, const std::filesystem::path& folder);
} // namespace cpplivelab
