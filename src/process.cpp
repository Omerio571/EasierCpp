#include "cpplivelab/process.hpp"
#include "windows_support.hpp"
#include "child_startup.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <fstream>
#include "project_paths.hpp"

namespace cpplivelab
{
using windows_support::make_command_line;
using windows_support::quote_argument;

Environment prepend_environment_paths(
    Environment environment, const std::vector<std::filesystem::path>& paths)
{
    if (paths.empty())
    {
        return environment;
    }
    if (environment.empty())
    {
        const auto inherited = GetEnvironmentStringsW();
        if (!inherited)
        {
            throw std::runtime_error("Cannot read the child environment.");
        }
        const auto* end = inherited;
        while (*end)
        {
            end += std::wcslen(end) + 1;
        }
        environment.assign(static_cast<const wchar_t*>(inherited), end + 1);
        FreeEnvironmentStringsW(inherited);
    }
    std::wstring prefix;
    for (const auto& path : paths)
    {
        prefix += path.wstring() + L";";
    }
    Environment result;
    bool found_path = false;
    for (const wchar_t* entry = environment.data(); *entry; entry += std::wcslen(entry) + 1)
    {
        std::wstring value(entry);
        if (_wcsnicmp(entry, L"PATH=", 5) == 0)
        {
            value = L"PATH=" + prefix + value.substr(5);
            found_path = true;
        }
        result.insert(result.end(), value.begin(), value.end());
        result.push_back(L'\0');
    }
    if (!found_path)
    {
        const auto value = L"PATH=" + prefix;
        result.insert(result.end(), value.begin(), value.end());
        result.push_back(L'\0');
    }
    result.push_back(L'\0');
    return result;
}

Environment runtime_environment(
    Environment environment, const std::filesystem::path& build_directory)
{
    std::ifstream input(build_directory / L"runtime-paths.txt");
    std::vector<std::filesystem::path> paths;
    for (std::string line; std::getline(input, line);)
    {
        if (!line.empty())
        {
            paths.push_back(path_support::from_utf8(line));
        }
    }
    return prepend_environment_paths(std::move(environment), paths);
}

int run_process(const std::filesystem::path& executable,
    const std::vector<std::wstring>& arguments,
    const std::filesystem::path& directory,
    const Environment& environment)
{
    auto process_command_line = make_command_line(executable, arguments);
    // Use the existing console (or the GUI worker's redirected pipes). Replacing
    // these handles would break std::cin/std::cout in the user's program.
    windows_support::ChildStartup startup(GetStdHandle(STD_INPUT_HANDLE),
        GetStdHandle(STD_OUTPUT_HANDLE),
        GetStdHandle(STD_ERROR_HANDLE));
    PROCESS_INFORMATION process_information{};
    if (!CreateProcessW(executable.c_str(),
            process_command_line.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT,
            environment.empty() ? nullptr : const_cast<wchar_t*>(environment.data()),
            directory.c_str(),
            &startup.information.StartupInfo,
            &process_information))
    {
        throw std::runtime_error(
            "Cannot launch process (Windows error " + std::to_string(GetLastError()) + ")");
    }
    startup.release_handles();
    // Waiting belongs in this CLI worker, never in the GUI's message loop.
    // Closing a process/thread HANDLE releases our reference; it does not kill it.
    WaitForSingleObject(process_information.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(process_information.hProcess, &exit_code);
    CloseHandle(process_information.hThread);
    CloseHandle(process_information.hProcess);
    return static_cast<int>(exit_code);
}

Environment developer_environment(const std::filesystem::path& developer_setup_script)
{
    // Only the discovered VS setup script is run through cmd. User paths and build
    // arguments are passed directly to CreateProcess, never interpolated into cmd.
    if (developer_setup_script.wstring().find_first_of(L"\"%\r\n") != std::wstring::npos)
    {
        throw std::runtime_error("Unsupported Visual Studio setup path");
    }
    wchar_t system_directory[MAX_PATH]{};
    GetSystemDirectoryW(system_directory, MAX_PATH);
    const auto command_interpreter = std::filesystem::path(system_directory) / L"cmd.exe";
    std::wstring process_command_line =
        quote_argument(command_interpreter.wstring()) + L" /u /d /s /c \"set \"PATH=" +
        std::wstring(system_directory) + L";%PATH%\" && call \"" +
        developer_setup_script.wstring() + L"\" -arch=x64 -host_arch=x64 >nul && set\"";
    SECURITY_ATTRIBUTES pipe_security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE environment_read_handle = nullptr, environment_write_handle = nullptr;
    if (!CreatePipe(&environment_read_handle, &environment_write_handle, &pipe_security, 0))
    {
        throw std::runtime_error("Cannot capture developer environment");
    }
    SetHandleInformation(environment_read_handle, HANDLE_FLAG_INHERIT, 0);
    windows_support::ChildStartup startup(
        GetStdHandle(STD_INPUT_HANDLE), environment_write_handle, GetStdHandle(STD_ERROR_HANDLE));
    PROCESS_INFORMATION process_information{};
    // /u makes cmd's built-in SET output UTF-16LE. System32 above supplies
    // registry tools required by VsDevCmd even when the caller has empty PATH.
    const auto process_started = CreateProcessW(command_interpreter.c_str(),
        process_command_line.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT,
        nullptr,
        nullptr,
        &startup.information.StartupInfo,
        &process_information);
    startup.release_handles();
    CloseHandle(environment_write_handle);
    if (!process_started)
    {
        CloseHandle(environment_read_handle);
        throw std::runtime_error("Cannot initialize Visual Studio");
    }
    const auto captured_bytes =
        windows_support::capture_root_output(process_information.hProcess, environment_read_handle);
    CloseHandle(environment_read_handle);
    WaitForSingleObject(process_information.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(process_information.hProcess, &exit_code);
    CloseHandle(process_information.hThread);
    CloseHandle(process_information.hProcess);
    if (exit_code != 0 || captured_bytes.size() % 2)
    {
        throw std::runtime_error("Visual Studio environment setup failed");
    }
    std::wstring captured_environment;
    for (size_t i = 0; i < captured_bytes.size(); i += 2)
    {
        captured_environment +=
            static_cast<wchar_t>(static_cast<unsigned char>(captured_bytes[i]) |
                                 (static_cast<unsigned char>(captured_bytes[i + 1]) << 8));
    }
    // Convert SET's UTF-16LE lines into the double-NUL-terminated environment block
    // required by CreateProcessW. Do not write these values to diagnostic logs:
    // inherited environments may contain credentials belonging to the user.
    Environment child_environment;
    size_t line_begin = 0;
    while (line_begin < captured_environment.size())
    {
        auto line_end = captured_environment.find(L'\n', line_begin);
        if (line_end == captured_environment.npos)
        {
            line_end = captured_environment.size();
        }
        auto environment_entry = captured_environment.substr(line_begin, line_end - line_begin);
        if (!environment_entry.empty() && environment_entry.back() == L'\r')
        {
            environment_entry.pop_back();
        }
        if (environment_entry.find(L'=') != environment_entry.npos)
        {
            child_environment.insert(
                child_environment.end(), environment_entry.begin(), environment_entry.end());
            child_environment.push_back(L'\0');
        }
        line_begin = line_end + 1;
    }
    child_environment.push_back(L'\0');
    if (child_environment.size() < 2)
    {
        throw std::runtime_error("Empty developer environment");
    }
    return child_environment;
}
} // namespace cpplivelab
