#pragma once

#include "process/windows_support.hpp"
#include <vector>
#include <string>

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
};

// Only visible, unowned top-level windows from the supervised job are considered.
std::vector<ApplicationWindow> capture_application_windows(const std::vector<unsigned long>& ids);
void close_application_windows(const std::vector<unsigned long>& ids);

// Poll while delayed windows initialize. Matches class/title deterministically;
// ambiguous classes use stable enumeration order. Returns the restored count.
size_t restore_application_windows(const std::vector<unsigned long>& ids,
    const std::vector<ApplicationWindow>& previous,
    std::vector<HWND>& restored);
RECT visible_window_bounds(RECT bounds, const RECT& previous_work, const RECT& current_work);

struct AutoRunPreferences
{
    bool enabled = false;
    bool restore_windows = true;
    bool force_stop = false;
    unsigned long shutdown_ms = 3000;
};

AutoRunPreferences load_auto_run_preferences(
    const std::filesystem::path& project, bool legacy_auto_build = false);
void save_auto_run_preferences(
    const std::filesystem::path& project, const AutoRunPreferences& value);
} // namespace cpplivelab
