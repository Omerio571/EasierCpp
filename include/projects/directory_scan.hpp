#pragma once

#include "projects/project_paths.hpp"
#include "process/windows_support.hpp"
#include <filesystem>
#include <stdexcept>

namespace cpplivelab
{
// An incomplete scan must never be accepted as a fresh source snapshot. Error
// code traversal prevents iterator exceptions when an editor removes a directory;
// the caller receives a controlled, actionable failure and may retry later.
template <typename Visitor> void scan_directory(const std::filesystem::path& root, Visitor visitor)
{
    namespace fs = std::filesystem;
    std::error_code error;
    fs::recursive_directory_iterator entry(
        windows_support::extended_path(root), fs::directory_options::none, error),
        end;
    fs::path current = root;
    while (!error && entry != end)
    {
        auto native = entry->path().wstring();
        if (native.starts_with(L"\\\\?\\UNC\\"))
        {
            native = L"\\\\" + native.substr(8);
        }
        else if (native.starts_with(L"\\\\?\\"))
        {
            native.erase(0, 4);
        }
        current = native;
        const auto attributes = GetFileAttributesW(windows_support::extended_path(current).c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES)
        {
            error = std::error_code(GetLastError(), std::system_category());
            break;
        }
        if (attributes & FILE_ATTRIBUTE_REPARSE_POINT)
        {
            entry.disable_recursion_pending();
        }
        else if (!visitor(current, (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0))
        {
            entry.disable_recursion_pending();
        }
        entry.increment(error);
    }
    if (error)
    {
        throw std::runtime_error(
            "Directory scan incomplete at " + path_support::path_utf8(current) + ": " +
            error.message() + ". Retry after the directory is available; freshness is unverified.");
    }
}
} // namespace cpplivelab
