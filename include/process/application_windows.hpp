#pragma once

#include "process/windows_support.hpp"
#include <vector>
#include <string>
#include <map>

namespace cpplivelab
{
struct ApplicationWindow
{
    std::wstring class_name;
    std::wstring title;
    std::wstring monitor;
    WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};
    RECT bounds{};
    RECT work_area{};
    RECT monitor_area{};
    UINT dpi = 96;
    bool borderless_fullscreen = false;
    bool minimized = false;
};

// Only visible, unowned top-level windows from the supervised job are considered.
std::vector<ApplicationWindow> capture_application_windows(const std::vector<unsigned long>& ids);
void close_application_windows(const std::vector<unsigned long>& ids);
bool expects_application_window(const std::filesystem::path& executable);

// Poll while delayed windows initialize. Matches class/title deterministically;
// ambiguous classes use stable enumeration order. Returns the restored count.
size_t restore_application_windows(const std::vector<unsigned long>& ids,
    const std::vector<ApplicationWindow>& previous,
    std::vector<HWND>& restored,
    bool recheck = false);
RECT visible_window_bounds(RECT bounds, const RECT& previous_work, const RECT& current_work);

enum class PlacementMemory
{
    Off,
    Session,
    Persistent
};
enum class ReplacementMode
{
    Safe,
    Seamless
};

// Stored outside user sources, independently of the A/B executable slot. A session
// instance owns volatile history; only Persistent mode reads or writes disk.
class WindowPlacementMemory
{
public:
    std::vector<ApplicationWindow> recall(const std::filesystem::path& project,
        const std::wstring& configuration,
        const std::wstring& target,
        PlacementMemory mode);
    void remember(const std::filesystem::path& project,
        const std::wstring& configuration,
        const std::wstring& target,
        PlacementMemory mode,
        const std::vector<ApplicationWindow>& windows,
        bool flush = false);

private:
    struct Entry
    {
        std::vector<ApplicationWindow> windows;
        std::wstring serialized;
        bool loaded = false;
        bool dirty = false;
        ULONGLONG written = 0;
    };

    std::map<std::filesystem::path, Entry> entries_;
};

struct AutoRunPreferences
{
    bool enabled = false;
    bool restore_windows = true;
    bool force_stop = false;
    unsigned long shutdown_ms = 3000;
    PlacementMemory placement_memory = PlacementMemory::Persistent;
    ReplacementMode replacement_mode = ReplacementMode::Safe;
};

AutoRunPreferences load_auto_run_preferences(
    const std::filesystem::path& project, bool legacy_auto_build = false);
void save_auto_run_preferences(
    const std::filesystem::path& project, const AutoRunPreferences& value);
} // namespace cpplivelab
