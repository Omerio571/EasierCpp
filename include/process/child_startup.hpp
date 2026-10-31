#pragma once

#include "process/windows_support.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>

namespace cpplivelab::windows_support
{
// Discovery/setup output belongs to the root command. A background helper may
// inherit its stdout; root exit plus a drained pipe is authoritative, not EOF.
inline std::string capture_root_output(HANDLE process, HANDLE pipe)
{
    std::string output;
    std::array<char, 4096> buffer{};
    for (;;)
    {
        DWORD available = 0;
        if (PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) && available)
        {
            DWORD bytes = 0;
            if (ReadFile(pipe,
                    buffer.data(),
                    (std::min)(available, static_cast<DWORD>(buffer.size())),
                    &bytes,
                    nullptr))
            {
                output.append(buffer.data(), bytes);
            }
            continue;
        }
        if (WaitForSingleObject(process, 10) == WAIT_OBJECT_0)
        {
            // Recheck once after exit: the final SET output may have arrived
            // between PeekNamedPipe and the process becoming signaled.
            if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) || !available)
            {
                break;
            }
        }
    }
    return output;
}

// Restrict inheritance to these three duplicated standard handles. A GUI can launch
// discovery and a build concurrently; inheriting every live pipe writer can create
// an EOF dependency between otherwise unrelated processes and deadlock both workers.
class ChildStartup
{
public:
    STARTUPINFOEXW information{};

    ChildStartup(HANDLE input, HANDLE output, HANDLE error)
    {
        const std::array<HANDLE, 3> originals{input, output, error};
        for (size_t index = 0; index < originals.size(); ++index)
        {
            HANDLE source = originals[index];
            UniqueHandle fallback;
            if (!source || source == INVALID_HANDLE_VALUE)
            {
                fallback.reset(CreateFileW(L"NUL",
                    index == 0 ? GENERIC_READ : GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                    nullptr,
                    OPEN_EXISTING,
                    0,
                    nullptr));
                source = fallback.value;
            }
            HANDLE copy = nullptr;
            if (!DuplicateHandle(GetCurrentProcess(),
                    source,
                    GetCurrentProcess(),
                    &copy,
                    0,
                    TRUE,
                    DUPLICATE_SAME_ACCESS))
            {
                throw std::runtime_error("Cannot duplicate child standard handle");
            }
            handles_[index].reset(copy);
            inherited_[index] = copy;
        }
        SIZE_T bytes = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        storage_.resize(bytes);
        information.StartupInfo.cb = sizeof(information);
        information.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        information.StartupInfo.hStdInput = inherited_[0];
        information.StartupInfo.hStdOutput = inherited_[1];
        information.StartupInfo.hStdError = inherited_[2];
        information.lpAttributeList =
            reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage_.data());
        if (!InitializeProcThreadAttributeList(information.lpAttributeList, 1, 0, &bytes))
        {
            throw std::runtime_error("Cannot initialize child handle list");
        }
        if (!UpdateProcThreadAttribute(information.lpAttributeList,
                0,
                PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                inherited_.data(),
                sizeof(inherited_),
                nullptr,
                nullptr))
        {
            DeleteProcThreadAttributeList(information.lpAttributeList);
            throw std::runtime_error("Cannot restrict child handle inheritance");
        }
        initialized_ = true;
    }

    ~ChildStartup()
    {
        if (initialized_)
        {
            DeleteProcThreadAttributeList(information.lpAttributeList);
        }
    }

    ChildStartup(const ChildStartup&) = delete;
    ChildStartup& operator=(const ChildStartup&) = delete;

    // The child has its own copies after CreateProcess. Keeping any parent copy
    // of a capture writer alive while reading would itself prevent pipe EOF.
    void release_handles()
    {
        for (auto& handle : handles_)
        {
            handle.reset();
        }
    }

private:
    std::array<UniqueHandle, 3> handles_;
    std::array<HANDLE, 3> inherited_{};
    std::vector<std::byte> storage_;
    bool initialized_ = false;
};
} // namespace cpplivelab::windows_support
