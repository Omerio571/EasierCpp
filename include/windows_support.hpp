#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <filesystem>
#include <string>
#include <vector>

namespace cpplivelab::windows_support
{
inline std::wstring extended_path(const std::filesystem::path& path)
{
    auto absolute = std::filesystem::absolute(path).lexically_normal().wstring();
    if (absolute.starts_with(L"\\\\?\\"))
    {
        return absolute;
    }
    if (absolute.starts_with(L"\\\\"))
    {
        return L"\\\\?\\UNC\\" + absolute.substr(2);
    }
    return L"\\\\?\\" + absolute;
}

// A HANDLE is an opaque reference to a Windows resource, not a C++ object pointer.
// nullptr means that nothing is owned; file/pipe creation can instead fail with
// INVALID_HANDLE_VALUE. Neither failure value may be passed to CloseHandle.
//
// This wrapper owns only resources released by CloseHandle (files, pipes, events,
// jobs, processes and threads). Fonts and COM interfaces have different cleanup APIs.
// Its destructor runs on normal return and while exceptions unwind the stack.
struct UniqueHandle
{
    HANDLE value = nullptr;

    UniqueHandle() = default;

    ~UniqueHandle()
    {
        reset();
    }

    // Copying would give two wrappers ownership of the same resource and close it twice.
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    // Replace ownership, or release it by passing nullptr. Callers transfer a newly
    // obtained handle here; they must not keep a second owning copy.
    void reset(HANDLE replacement = nullptr)
    {
        if (value != nullptr && value != INVALID_HANDLE_VALUE)
        {
            CloseHandle(value);
        }

        value = replacement;
    }
};

// CreateProcess receives a single mutable command-line string. The child's C/C++
// runtime reconstructs argv from it, so quoting is needed even without a shell.
// Backslashes must be doubled before a quote and before the closing quote; otherwise
// paths ending in '\\' and arguments containing '"' would be changed by that parsing.
inline std::wstring quote_argument(const std::wstring& argument)
{
    std::wstring quoted_argument = L"\"";
    size_t pending_backslashes = 0;

    for (wchar_t character : argument)
    {
        if (character == L'\\')
        {
            ++pending_backslashes;
            continue;
        }

        const size_t backslashes_to_write =
            character == L'"' ? pending_backslashes * 2 + 1 : pending_backslashes;

        quoted_argument.append(backslashes_to_write, L'\\');
        quoted_argument += character;
        pending_backslashes = 0;
    }

    quoted_argument.append(pending_backslashes * 2, L'\\');
    quoted_argument += L'"';
    return quoted_argument;
}

inline std::wstring make_command_line(
    const std::filesystem::path& executable, const std::vector<std::wstring>& arguments)
{
    std::wstring command_line = quote_argument(executable.wstring());

    for (const auto& argument : arguments)
    {
        command_line += L" " + quote_argument(argument);
    }

    return command_line;
}
} // namespace cpplivelab::windows_support
