#pragma once

#include "process/windows_support.hpp"
#include <memory>
#include <optional>

namespace cpplivelab
{
struct RestartState
{
    std::filesystem::path project;
    std::wstring preference_store = L"Software\\CppLiveLab";
    WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};
};

// Exactly one visible GUI per manager-state root. Restart children prepare
// hidden while the parent retains ownership, then acquire after parent exit.
class GuiInstance
{
public:
    explicit GuiInstance(bool defer = false);
    ~GuiInstance();
    bool acquired = false;
    bool acquire(DWORD timeout);
    std::wstring name;
    ULONG_PTR key = 0;

private:
    windows_support::UniqueHandle mutex_;
};

enum class RestartProgress
{
    Waiting,
    Ready,
    Failed
};

class RestartLaunch
{
public:
    RestartLaunch(const std::filesystem::path& executable,
        const RestartState& state,
        const std::wstring& instance_name);
    ~RestartLaunch();
    RestartProgress poll() const;
    void commit();
    unsigned long process_id() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

struct RestartChild
{
    RestartState state;
    windows_support::UniqueHandle parent;
    windows_support::UniqueHandle ready;
    windows_support::UniqueHandle commit;
    windows_support::UniqueHandle handover;
    std::filesystem::path state_file;
    // Called only after the hidden replacement window and project are usable.
    bool acknowledge_and_wait(GuiInstance& instance);
};

// Strict one-shot protocol. Replayed/malformed handovers cannot spawn a loop.
std::unique_ptr<RestartChild> restart_child(
    int argc, wchar_t** argv, const std::wstring& instance_name);
} // namespace cpplivelab
