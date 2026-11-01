#include "process/application_windows.hpp"
#include "cpplivelab/workspace.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <iomanip>
#include <sstream>
#include <cstdint>

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
        entry.minimized = IsIconic(window) != FALSE;
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
    std::vector<HWND>& restored,
    bool recheck)
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
        const auto recorded = std::find(restored.begin(), restored.end(), window);
        if (recorded != restored.end() && !recheck)
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
        if (recorded != restored.end())
        {
            RECT actual{};
            const bool unchanged =
                placement.showCmd == SW_SHOWMAXIMIZED
                    ? IsZoomed(window) &&
                          MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST) == monitor
                    : !IsZoomed(window) && GetWindowRect(window, &actual) &&
                          EqualRect(&actual, &bounds);
            if (unchanged)
            {
                continue;
            }
            // Applications may finish centering after their initial ShowWindow.
            // Reapply only changed geometry during candidate startup, not each tick.
            restored.erase(recorded);
        }
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

namespace
{
std::filesystem::path placement_file(const std::filesystem::path& project,
    const std::wstring& configuration,
    const std::wstring& target)
{
    // Stable UTF-16 identity, not std::hash (which need not be stable across runs).
    std::uint64_t hash = 14695981039346656037ull;
    for (wchar_t value : configuration + L"\n" + target)
    {
        hash = (hash ^ static_cast<unsigned short>(value)) * 1099511628211ull;
    }
    return workspace_directory(project) / L"window-placement" /
           (std::to_wstring(hash) + L".settings");
}

bool valid_rectangle(const RECT& rectangle)
{
    return rectangle.left >= -1000000 && rectangle.top >= -1000000 && rectangle.right <= 1000000 &&
           rectangle.bottom <= 1000000 && rectangle.right > rectangle.left &&
           rectangle.bottom > rectangle.top;
}

void write_rectangle(std::wostream& output, const RECT& rectangle)
{
    output << rectangle.left << ' ' << rectangle.top << ' ' << rectangle.right << ' '
           << rectangle.bottom << ' ';
}

bool read_rectangle(std::wistream& input, RECT& rectangle)
{
    return static_cast<bool>(
               input >> rectangle.left >> rectangle.top >> rectangle.right >> rectangle.bottom) &&
           valid_rectangle(rectangle);
}

std::wstring placement_text(const std::wstring& configuration,
    const std::wstring& target,
    const std::vector<ApplicationWindow>& windows)
{
    std::wostringstream output;
    output << L"EasierCPP-placement-1 " << std::quoted(configuration) << ' ' << std::quoted(target)
           << ' ' << windows.size() << '\n';
    for (const auto& window : windows)
    {
        output << std::quoted(window.class_name) << ' ' << std::quoted(window.title) << ' '
               << std::quoted(window.monitor) << ' ' << window.dpi << ' '
               << window.placement.showCmd << ' ' << window.borderless_fullscreen << ' ';
        write_rectangle(output, window.bounds);
        write_rectangle(output, window.placement.rcNormalPosition);
        write_rectangle(output, window.work_area);
        write_rectangle(output, window.monitor_area);
        output << '\n';
    }
    return output.str();
}
} // namespace

std::vector<ApplicationWindow> WindowPlacementMemory::recall(const std::filesystem::path& project,
    const std::wstring& configuration,
    const std::wstring& target,
    PlacementMemory mode)
{
    if (mode == PlacementMemory::Off || target.empty())
    {
        return {};
    }
    auto& entry = entries_[placement_file(project, configuration, target)];
    if (mode == PlacementMemory::Persistent && !entry.loaded)
    {
        entry.loaded = true;
        const auto file = placement_file(project, configuration, target);
        std::error_code error;
        const auto bytes = std::filesystem::file_size(file, error);
        if (error || bytes > 65536 || bytes % sizeof(wchar_t))
        {
            return entry.windows;
        }
        std::wstring text(static_cast<size_t>(bytes / sizeof(wchar_t)), L'\0');
        std::ifstream input(file, std::ios::binary);
        if (!input.read(reinterpret_cast<char*>(text.data()), static_cast<std::streamsize>(bytes)))
        {
            return entry.windows;
        }
        std::wistringstream parsed(text);
        std::wstring version, saved_configuration, saved_target;
        size_t count = 0;
        if (!(parsed >> version >> std::quoted(saved_configuration) >> std::quoted(saved_target) >>
                count) ||
            version != L"EasierCPP-placement-1" || saved_configuration != configuration ||
            saved_target != target || count > 32)
        {
            return entry.windows;
        }
        std::vector<ApplicationWindow> loaded;
        for (size_t index = 0; index < count; ++index)
        {
            ApplicationWindow window;
            if (!(parsed >> std::quoted(window.class_name) >> std::quoted(window.title) >>
                    std::quoted(window.monitor) >> window.dpi >> window.placement.showCmd >>
                    window.borderless_fullscreen) ||
                window.dpi < 48 || window.dpi > 1000 ||
                (window.placement.showCmd != SW_SHOWNORMAL &&
                    window.placement.showCmd != SW_SHOWMAXIMIZED) ||
                !read_rectangle(parsed, window.bounds) ||
                !read_rectangle(parsed, window.placement.rcNormalPosition) ||
                !read_rectangle(parsed, window.work_area) ||
                !read_rectangle(parsed, window.monitor_area))
            {
                return entry.windows;
            }
            loaded.push_back(std::move(window));
        }
        if (entry.windows.empty())
        {
            entry.windows = std::move(loaded);
        }
        entry.serialized = placement_text(configuration, target, entry.windows);
    }
    return entry.windows;
}

void WindowPlacementMemory::remember(const std::filesystem::path& project,
    const std::wstring& configuration,
    const std::wstring& target,
    PlacementMemory mode,
    const std::vector<ApplicationWindow>& windows,
    bool flush)
{
    if (mode == PlacementMemory::Off || target.empty())
    {
        return;
    }
    const auto file = placement_file(project, configuration, target);
    auto& entry = entries_[file];
    std::vector<ApplicationWindow> valid;
    for (const auto& window : windows)
    {
        if (!window.minimized && valid_rectangle(window.bounds) &&
            valid_rectangle(window.placement.rcNormalPosition) &&
            valid_rectangle(window.work_area) && valid_rectangle(window.monitor_area) &&
            window.dpi >= 48 && window.dpi <= 1000 &&
            (window.placement.showCmd == SW_SHOWNORMAL ||
                window.placement.showCmd == SW_SHOWMAXIMIZED))
        {
            valid.push_back(window);
        }
    }
    // Empty captures (destroyed, hidden, minimized) must not erase valid history.
    if (!valid.empty() && valid.size() == windows.size() && valid.size() <= 32)
    {
        const auto serialized = placement_text(configuration, target, valid);
        if (serialized != entry.serialized)
        {
            entry.windows = std::move(valid);
            entry.serialized = serialized;
            entry.dirty = true;
        }
    }
    if (mode != PlacementMemory::Persistent || !entry.dirty ||
        (!flush && GetTickCount64() - entry.written < 2000))
    {
        return;
    }
    if (entry.serialized.size() * sizeof(wchar_t) > 65536)
    {
        return;
    }
    std::filesystem::create_directories(file.parent_path());
    const auto temporary = std::filesystem::path(file.wstring() + L".new");
    {
        // Explicit UTF-16 Windows storage preserves Unicode titles without locale conversion.
        std::ofstream output(temporary, std::ios::binary);
        output.write(reinterpret_cast<const char*>(entry.serialized.data()),
            static_cast<std::streamsize>(entry.serialized.size() * sizeof(wchar_t)));
        output.close();
        if (!output)
        {
            throw std::runtime_error("Cannot save window placement.");
        }
    }
    if (!MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING))
    {
        throw std::runtime_error("Cannot replace window placement settings.");
    }
    entry.dirty = false;
    entry.written = GetTickCount64();
}

bool expects_application_window(const std::filesystem::path& executable)
{
    std::ifstream input(executable, std::ios::binary);
    IMAGE_DOS_HEADER dos{};
    input.read(reinterpret_cast<char*>(&dos), sizeof(dos));
    if (!input || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 64 ||
        dos.e_lfanew > 1024 * 1024)
    {
        return true;
    }
    input.seekg(dos.e_lfanew);
    DWORD signature = 0;
    input.read(reinterpret_cast<char*>(&signature), sizeof(signature));
    if (signature != IMAGE_NT_SIGNATURE)
    {
        return true;
    }
    // Subsystem has the same offset in PE32 and PE32+ optional headers.
    input.seekg(dos.e_lfanew + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + 68);
    WORD subsystem = 0;
    input.read(reinterpret_cast<char*>(&subsystem), sizeof(subsystem));
    return !input || subsystem != IMAGE_SUBSYSTEM_WINDOWS_CUI;
}

AutoRunPreferences load_auto_run_preferences(
    const std::filesystem::path& project, bool legacy_auto_build)
{
    AutoRunPreferences value;
    const auto settings = workspace_directory(project) / L"auto-run.settings";
    std::ifstream input(settings);
    // Do not let a legacy global ON override an explicit OFF or a damaged file.
    if (!std::filesystem::exists(settings))
    {
        value.enabled = legacy_auto_build;
    }
    AutoRunPreferences loaded;
    if (input >> loaded.enabled >> loaded.restore_windows >> loaded.force_stop >>
            loaded.shutdown_ms &&
        loaded.shutdown_ms >= 100 && loaded.shutdown_ms <= 30000)
    {
        value = loaded;
        // Legacy records remember only replacement placement during the session.
        value.placement_memory =
            loaded.restore_windows ? PlacementMemory::Session : PlacementMemory::Off;
        unsigned memory = 0, replacement = 0;
        if (input >> memory >> replacement && memory <= 2 && replacement <= 1)
        {
            value.placement_memory = static_cast<PlacementMemory>(memory);
            value.replacement_mode = static_cast<ReplacementMode>(replacement);
        }
        value.restore_windows = value.placement_memory != PlacementMemory::Off;
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
               << value.shutdown_ms << ' '
               << static_cast<unsigned>(
                      value.restore_windows ? value.placement_memory : PlacementMemory::Off)
               << ' ' << static_cast<unsigned>(value.replacement_mode) << '\n';
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
