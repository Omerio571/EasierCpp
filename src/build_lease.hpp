#pragma once

#include "windows_support.hpp"
#include "project_paths.hpp"

#include <stdexcept>

namespace cpplivelab
{
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
            throw std::runtime_error(
                "This configuration is already building or running in CppLiveLab. Stop it first.");
        }
    }

private:
    windows_support::UniqueHandle lock_;
};

inline void ensure_executables_stopped(const std::filesystem::path& directory)
{
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
    {
        if (!entry.is_regular_file() || entry.path().extension() != L".exe")
        {
            continue;
        }

        windows_support::UniqueHandle executable_write_handle;
        executable_write_handle.reset(CreateFileW(entry.path().c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr));
        if (executable_write_handle.value == INVALID_HANDLE_VALUE)
        {
            throw std::runtime_error(
                "Executable is running or not writable: " + path_support::path_utf8(entry.path()) +
                ". Stop it before building.");
        }
    }
}

// Used at most once a second, only while an automatic build is deferred. No compiler
// is launched until the lock and executable probes permit it; no process is killed.
inline bool build_outputs_available(const std::filesystem::path& directory)
{
    std::error_code error;
    if (!std::filesystem::exists(directory, error))
    {
        return !error;
    }

    try
    {
        const BuildLease probe(directory);
        ensure_executables_stopped(directory);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}
} // namespace cpplivelab
