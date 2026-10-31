#pragma once

#include "process/windows_support.hpp"
#include "projects/project_paths.hpp"
#include "projects/directory_scan.hpp"

#include <stdexcept>

namespace cpplivelab
{
inline constexpr int build_deferred_exit_code = 75;

class BuildDeferredError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

// The same lock is used by CLI builds and the GUI's separately launched program.
// Holding it until the program tree exits preserves Stage 3's cross-instance safety.
class BuildLease
{
public:
    explicit BuildLease(const std::filesystem::path& directory)
    {
        std::filesystem::create_directories(directory);
        lock_.reset(CreateFileW((directory / L"cpplivelab.lock").c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr));
        if (lock_.value == INVALID_HANDLE_VALUE)
        {
            const auto error = GetLastError();
            if (error != ERROR_SHARING_VIOLATION && error != ERROR_LOCK_VIOLATION)
            {
                throw std::runtime_error(
                    "Cannot write build lock (Windows error " + std::to_string(error) + ")");
            }
            throw BuildDeferredError(
                "This configuration is already building or running in CppLiveLab. Stop it first.");
        }
    }

private:
    windows_support::UniqueHandle lock_;
};

inline void ensure_executables_stopped(const std::filesystem::path& directory)
{
    scan_directory(directory,
        [&](const std::filesystem::path& path, bool is_directory)
        {
            if (is_directory || path.extension() != L".exe")
            {
                return true;
            }

            windows_support::UniqueHandle executable_write_handle;
            executable_write_handle.reset(CreateFileW(windows_support::extended_path(path).c_str(),
                GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr));
            if (executable_write_handle.value == INVALID_HANDLE_VALUE)
            {
                const auto error = GetLastError();
                const auto message =
                    "Executable is running or not writable: " + path_support::path_utf8(path) +
                    ". Stop it before building.";
                if (error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION)
                {
                    throw BuildDeferredError(message);
                }
                throw std::runtime_error(message + " Windows error " + std::to_string(error));
            }
            return true;
        });
}

// Used at most once a second, only while an automatic build is deferred. No compiler
// is launched until the lock and executable probes permit it; no process is killed.
inline bool build_outputs_available(const std::filesystem::path& directory)
{
    std::error_code error;
    if (!std::filesystem::exists(directory, error))
    {
        if (error)
        {
            throw std::runtime_error("Cannot inspect build output directory: " + error.message());
        }
        return true;
    }

    try
    {
        const BuildLease probe(directory);
        ensure_executables_stopped(directory);
        return true;
    }
    catch (const BuildDeferredError&)
    {
        return false;
    }
}
} // namespace cpplivelab
