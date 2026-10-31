#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace cpplivelab
{
// CreateProcessW expects UTF-16 "NAME=value\0" entries followed by a second NUL.
// The vector owns that memory for the entire process-creation call.
using Environment = std::vector<wchar_t>;

// Capture VsDevCmd's changes into a child environment block. The caller's PATH,
// INCLUDE and LIB are never changed, and environment values are never logged.
Environment developer_environment(const std::filesystem::path& command);
Environment prepend_environment_paths(
    Environment environment, const std::vector<std::filesystem::path>& paths);
Environment runtime_environment(
    Environment environment, const std::filesystem::path& build_directory);

// Synchronous CLI launch. The child inherits the caller's stdin/stdout/stderr,
// preserving interactive console behavior. The GUI instead uses ConsoleSession.
// Returns the child's exit code; throws when process creation itself fails.
int run_process(const std::filesystem::path& executable,
    const std::vector<std::wstring>& arguments,
    const std::filesystem::path& directory,
    const Environment& environment = {});
} // namespace cpplivelab
