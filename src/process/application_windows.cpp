#include "process/application_windows.hpp"
#include "cpplivelab/workspace.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace cpplivelab
{
namespace
{
// Win11 APIs otherwise virtualize rectangles for DPI-unaware callers. Scope the
// physical-coordinate contract to these calls; never change global DPI settings.
struct PhysicalCoordinates
{
    DPI_AWARENESS_CONTEXT previous =
        SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    ~PhysicalCoordinates()
    {
        if (previous)
        {
            SetThreadDpiAwarenessContext(previous);
        }
    }
};

struct Windows
{
    const std::vector<unsigned long>& ids;
    std::vector<HWND> handles;
};

BOOL CALLBACK collect(HWND window, LPARAM parameter)
{
    auto& state = *reinterpret_cast<Windows*>(parameter);
    DWORD id = 0;
    GetWindowThreadProcessId(window, &id);
    if (std::find(state.ids.begin(), state.ids.end(), id) != state.ids.end() &&
        IsWindowVisible(window) && !GetWindow(window, GW_OWNER) &&
        !(GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW))
    {
        state.handles.push_back(window);
    }
    return TRUE;
}

std::wstring class_name(HWND window)
{
    wchar_t value[256]{};
    GetClassNameW(window, value, 256);
    return value;
}

std::wstring title(HWND window)
{
    std::wstring value(GetWindowTextLengthW(window) + 1, L'\0');
    value.resize(GetWindowTextW(window, value.data(), static_cast<int>(value.size())));
    return value;
}

struct MonitorSearch
{
    std::wstring device;
    HMONITOR found = nullptr;
};

BOOL CALLBACK find_monitor(HMONITOR monitor, HDC, LPRECT, LPARAM parameter)
{
    auto& search = *reinterpret_cast<MonitorSearch*>(parameter);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info) && search.device == info.szDevice)
    {
        search.found = monitor;
    }
    return TRUE;
}
} // namespace

std::vector<ApplicationWindow> capture_application_windows(const std::vector<unsigned long>& ids)
{
    const PhysicalCoordinates coordinates;
    Windows state{ids, {}};
    EnumWindows(collect, reinterpret_cast<LPARAM>(&state));
    std::vector<ApplicationWindow> result;
    for (const auto window : state.handles)
    {
        ApplicationWindow entry;
        MONITORINFOEXW monitor{};
        monitor.cbSize = sizeof(monitor);
        if (!GetWindowPlacement(window, &entry.placement) ||
            !GetWindowRect(window, &entry.bounds) ||
            !GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor))
        {
            continue;
        }
        entry.class_name = class_name(window);
        entry.title = title(window);
        entry.monitor = monitor.szDevice;
        entry.work_area = monitor.rcWork;
        entry.monitor_area = monitor.rcMonitor;
        entry.dpi = GetDpiForWindow(window);
        entry.borderless_fullscreen = !(GetWindowLongPtrW(window, GWL_STYLE) & WS_CAPTION) &&
                                      EqualRect(&entry.bounds, &monitor.rcMonitor);
        if (IsIconic(window))
        {
            // Minimized GetWindowRect reports icon bounds. Restore saved normal
            // workspace bounds instead of making the replacement look missing.
            entry.placement.showCmd = SW_SHOWNORMAL;
            entry.bounds = entry.placement.rcNormalPosition;
            OffsetRect(&entry.bounds,
                monitor.rcWork.left - monitor.rcMonitor.left,
                monitor.rcWork.top - monitor.rcMonitor.top);
        }
        result.push_back(entry);
    }
    return result;
}

void close_application_windows(const std::vector<unsigned long>& ids)
{
    Windows state{ids, {}};
    EnumWindows(collect, reinterpret_cast<LPARAM>(&state));
    for (const auto window : state.handles)
    {
        // Post rather than Send: a hung application must not block the manager UI.
        PostMessageW(window, WM_CLOSE, 0, 0);
    }
}

RECT visible_window_bounds(RECT bounds, const RECT& previous_work, const RECT& current_work)
{
    const LONG width =
        (std::min)(bounds.right - bounds.left, current_work.right - current_work.left);
    const LONG height =
        (std::min)(bounds.bottom - bounds.top, current_work.bottom - current_work.top);
    bounds.left += current_work.left - previous_work.left;
    bounds.top += current_work.top - previous_work.top;
    bounds.left = std::clamp(bounds.left, current_work.left, current_work.right - width);
    bounds.top = std::clamp(bounds.top, current_work.top, current_work.bottom - height);
    bounds.right = bounds.left + width;
    bounds.bottom = bounds.top + height;
    return bounds;
}

size_t restore_application_windows(const std::vector<unsigned long>& ids,
    const std::vector<ApplicationWindow>& previous,
    std::vector<HWND>& restored)
{
    const PhysicalCoordinates coordinates;
    Windows state{ids, {}};
    EnumWindows(collect, reinterpret_cast<LPARAM>(&state));
    for (const auto& saved : previous)
    {
        auto found = std::find_if(state.handles.begin(),
            state.handles.end(),
            [&](HWND window)
            {
                return class_name(window) == saved.class_name && title(window) == saved.title;
            });
        if (found == state.handles.end())
        {
            found = std::find_if(state.handles.begin(),
                state.handles.end(),
                [&](HWND window)
                {
                    return class_name(window) == saved.class_name;
                });
        }
        if (found == state.handles.end())
        {
            continue;
        }
        const auto window = *found;
        state.handles.erase(found);
        if (std::find(restored.begin(), restored.end(), window) != restored.end())
        {
            continue;
        }
        MonitorSearch search{saved.monitor};
        EnumDisplayMonitors(nullptr, nullptr, find_monitor, reinterpret_cast<LPARAM>(&search));
        MONITORINFO current{sizeof(current)};
        const auto monitor =
            search.found ? search.found : MonitorFromRect(&saved.bounds, MONITOR_DEFAULTTONEAREST);
        if (!GetMonitorInfoW(monitor, &current))
        {
            continue;
        }
        auto placement = saved.placement;
        // WINDOWPLACEMENT normal bounds use workspace coordinates; physical
        // SetWindowPos bounds use screen coordinates. Never interchange them.
        auto bounds = saved.bounds;
        if (saved.placement.showCmd == SW_SHOWMAXIMIZED)
        {
            bounds = saved.placement.rcNormalPosition;
            OffsetRect(&bounds,
                saved.work_area.left - saved.monitor_area.left,
                saved.work_area.top - saved.monitor_area.top);
        }
        bounds = visible_window_bounds(bounds,
            saved.borderless_fullscreen ? saved.monitor_area : saved.work_area,
            saved.borderless_fullscreen ? current.rcMonitor : current.rcWork);
        placement.rcNormalPosition = bounds;
        OffsetRect(&placement.rcNormalPosition,
            current.rcMonitor.left - current.rcWork.left,
            current.rcMonitor.top - current.rcWork.top);
        placement.showCmd =
            saved.placement.showCmd == SW_SHOWMAXIMIZED ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
        // Select the intended monitor before maximizing. Otherwise an application
        // that initially opens on the primary display can maximize there instead.
        if (placement.showCmd == SW_SHOWMAXIMIZED)
        {
            SetWindowPos(window,
                nullptr,
                bounds.left,
                bounds.top,
                bounds.right - bounds.left,
                bounds.bottom - bounds.top,
                SWP_NOACTIVATE | SWP_NOZORDER);
        }
        if (SetWindowPlacement(window, &placement))
        {
            if (placement.showCmd != SW_SHOWMAXIMIZED)
            {
                SetWindowPos(window,
                    nullptr,
                    bounds.left,
                    bounds.top,
                    bounds.right - bounds.left,
                    bounds.bottom - bounds.top,
                    SWP_NOACTIVATE | SWP_NOZORDER);
            }
            RECT actual{};
            const bool verified =
                placement.showCmd == SW_SHOWMAXIMIZED
                    ? IsZoomed(window) &&
                          MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST) == monitor
                    : GetWindowRect(window, &actual) && EqualRect(&actual, &bounds);
            if (verified)
            {
                restored.push_back(window);
            }
        }
    }
    return restored.size();
}

AutoRunPreferences load_auto_run_preferences(const std::filesystem::path& project)
{
    AutoRunPreferences value;
    std::ifstream input(workspace_directory(project) / L"auto-run.settings");
    AutoRunPreferences loaded;
    if (input >> loaded.enabled >> loaded.restore_windows >> loaded.force_stop >>
            loaded.shutdown_ms &&
        loaded.shutdown_ms >= 100 && loaded.shutdown_ms <= 30000)
    {
        value = loaded;
    }
    return value;
}

void save_auto_run_preferences(
    const std::filesystem::path& project, const AutoRunPreferences& value)
{
    if (value.shutdown_ms < 100 || value.shutdown_ms > 30000)
    {
        throw std::invalid_argument("Graceful shutdown timeout must be 100–30000 milliseconds.");
    }
    const auto destination = workspace_directory(project) / L"auto-run.settings";
    std::filesystem::create_directories(destination.parent_path());
    const auto temporary = std::filesystem::path(destination.wstring() + L".new");
    {
        std::ofstream output(temporary);
        output << value.enabled << ' ' << value.restore_windows << ' ' << value.force_stop << ' '
               << value.shutdown_ms << '\n';
        if (!output)
        {
            throw std::runtime_error("Cannot save Auto Run settings.");
        }
    }
    if (!MoveFileExW(temporary.c_str(),
            destination.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        throw std::runtime_error("Cannot replace Auto Run settings.");
    }
}
} // namespace cpplivelab
