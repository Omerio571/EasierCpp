#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <oleacc.h>
#include <uxtheme.h>
#include "cpplivelab/session.hpp"
#include "process/output_decoder.hpp"
#include "build/auto_build.hpp"
#include "process/application_windows.hpp"
#include "build/build_lease.hpp"
#include "build/build_freshness.hpp"
#include "build/build_profile.hpp"
#include "gui/console_view.hpp"
#include "appearance/ui_theme.hpp"
#include "cpplivelab/workspace.hpp"
#include "gui/project_dialogs.hpp"
#include "tools/tool_workflows.hpp"
#include "gui/library_workflow.hpp"
#include "build/build_diagnostics.hpp"
#include "build/build_workers.hpp"
#include "process/restart.hpp"
#include "gui/library_manager.hpp"
#include <fstream>
#include <sstream>
#include <future>
#include <chrono>
#include <algorithm>
#include <array>
#include "cpplivelab/doctor.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <stdexcept>
#include <utility>

namespace
{
namespace fs = std::filesystem;

std::wstring application_title()
{
    return L"EasierCpp";
}

// Fixed IDs route Windows messages and identify controls to accessibility tools.
// Keep the numbers stable when changing labels or layout.
enum ControlId
{
    FolderInput = 101,
    BrowseButton = 102,
    OpenProjectButton = 103,
    ProjectIdentityLabel = 104,
    EditProjectButton = 105,
    ConfigurationSelector = 106,
    ExecutableInput = 107,
    BuildButton = 108,
    RunButton = 109,
    StopButton = 110,
    OutputPane = 111,
    ProgramInput = 112,
    SendButton = 113,
    EndInputButton = 114,
    StatusLabel = 115,
    AutoBuildButton = 116, // Retired. Keep the ID reserved for compatibility.
    DebounceInput = 117,
    ProgramTab = 118,
    SystemTab = 119,
    SystemOutputPane = 120,
    ClearProgram = 121,
    ClearSystem = 122,
    AppearanceSelector = 123,
    NewProjectMenu = 124,
    RecentProjectsMenu = 125,
    ProjectSettingsMenu = 126,
    EditorSettingsMenu = 127,
    ProjectPathLabel = 128,
    CopyProjectPath = 129,
    AppearanceMenu = 130,
    BuildPreferencesMenu = 131,
    ManageLibrariesMenu = 132,
    DoctorMenu = 133,
    DetailedDoctorMenu = 134,
    TestRunnerMenu = 135,
    CancelTestsMenu = 136,
    ReviewErrorsMenu = 137,
    RecoverBuildMenu = 138,
    NavigationProject = 140,
    NavigationBuild = 141,
    NavigationTools = 142,
    NavigationSettings = 143,
    RestartButton = 144,
    ProgramRestartButton = 145,
    AddLibraryButton = 146,
    LibrarySummary = 147,
    AutoRunButton = 148,
    WindowPreferencesMenu = 149,
    WelcomeTitle = 150,
    WelcomeNote = 151,
    WelcomeRecent = 152,
    OpenWelcomeRecent = 153,
    NavigationHome = 154,
    NavigationWorkspace,
    NavigationLibraries,
    ShellHeading,
    ShellDescription,
    ShellProject,
    ShellVersion,
    ShellTheme,
    ShellBrand,
    ShellLogo,
    WelcomeLogo,
    BuildConfigurationLabel,
    AutoRunHelp,
    TargetHelp,
    RestartHelp,
    InputHelp,
    ShellCardOne,
    ShellCardTwo,
    ShellCardThree,
    ShellCardFour,
    ShellCardNote,
    ShellCardDetail,
    ShellLibraryList,
    OptimizeWorkersButton,
    ForgetWelcomeRecent,
    ShellInlineHost,
    WorkspaceLibraries
};

constexpr int navigation_pages[] = {NavigationHome,
    NavigationWorkspace,
    NavigationLibraries,
    NavigationProject,
    NavigationTools,
    NavigationSettings};

struct ShellHelp
{
    int id;
    const wchar_t* name;
    const wchar_t* explanation;
};

constexpr ShellHelp shell_help[] = {{AutoRunHelp,
                                        L"About Auto Run",
                                        L"Watch for saved changes. A failed build keeps the "
                                        L"running program. Stop pauses automatic launch."},
    {TargetHelp,
        L"About executable target",
        L"Choose an executable for the selected build configuration. If the list is empty, "
        L"build the project to discover its targets."},
    {RestartHelp,
        L"About program restart",
        L"Restart the selected program. Restart EasierCpp in Tools restarts the manager itself."},
    {InputHelp,
        L"About program input",
        L"Send one line to the running program. End Input closes standard input until the next "
        L"launch."}};

HICON application_icon(int width, int height)
{
    return static_cast<HICON>(LoadImageW(
        GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101), IMAGE_ICON, width, height, LR_SHARED));
}

std::wstring read_control_text(HWND control)
{
    std::wstring value(GetWindowTextLengthW(control) + 1, L'\0');
    value.resize(GetWindowTextW(control, value.data(), static_cast<int>(value.size())));
    return value;
}

using cpplivelab::decode_console_text;

std::string encode_utf8(const std::wstring& value)
{
    if (value.empty())
    {
        return {};
    }
    const int size = WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        size,
        nullptr,
        nullptr);
    return result;
}

fs::path application_directory()
{
    std::vector<wchar_t> buffer(32768);
    GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    return fs::path(buffer.data()).parent_path();
}

// Owns the native controls and one supervised operation. Source editing/debugging
// belongs to the separate IDE. All methods run on the GUI thread, including short,
// nonblocking output polls; no reader thread may outlive the window.
class Application
{
public:
    HWND window_handle = nullptr;
    UINT dpi = 96;
    cpplivelab::Preferences preferences = cpplivelab::load_preferences();
    cpplivelab::UiTheme appearance;
    bool applying_appearance = false;
    // Fonts are GDI objects: DeleteObject releases them, not CloseHandle.
    // They outlive the child controls and are released with this Application.
    HFONT interface_font = nullptr;
    HFONT heading_font = nullptr;
    int shell_page = NavigationHome;
    bool inline_active = false;
    bool inline_abandoned = false;
    std::optional<MSG> inline_next_message;
    HWND shell_tooltip = nullptr;
    int tracked_help = 0;
    bool hover_help = false;
    IAccPropServices* shell_accessibility = nullptr;
    std::vector<RECT> shell_panels;
    HFONT output_font = nullptr;
    cpplivelab::ConsoleSession session;
    cpplivelab::ConsoleOutputDecoder output_decoder;
    cpplivelab::ConsoleOutputDecoder program_decoder;
    cpplivelab::ConsoleOutputDecoder error_decoder;
    cpplivelab::ConsolePane program_console;
    cpplivelab::ConsolePane system_console;
    bool program_tab_active = true;
    bool welcome_output = false;
#ifndef EASIERCPP_PUBLIC_GUI
    cpplivelab::ConsoleSession test_session;
    cpplivelab::ConsoleOutputDecoder test_decoder;
#endif
    bool tests_active = false;
    bool tests_cancelled = false;
    ULONGLONG tests_started = 0;
    std::vector<fs::path> welcome_recents;
    enum class Phase
    {
        Idle,
        PreparingBuild,
        Building,
        Checking,
        PreparingProgram,
        Running
    };
    Phase phase = Phase::Idle;
    bool automatic_operation = false;
    unsigned long long build_sequence = 0;
    std::wstring build_id;
    ULONGLONG last_build_activity = 0;
    bool stall_reported = false;
    bool build_root_observed = false;
    std::wstring pending_build_line;
    std::wstring build_diagnostics;
    std::unique_ptr<cpplivelab::BuildLease> program_lease;

    struct PreparedTools
    {
        cpplivelab::Environment environment;
        fs::path ninja;
        double discovery_ms = 0;
        double preparation_ms = 0;
    };

    std::future<PreparedTools> environment_future;
    cpplivelab::Environment program_environment;
    fs::path ninja_executable;
    cpplivelab::BuildFreshness freshness;
    cpplivelab::BuildIdentity building_identity;
    cpplivelab::SourceFingerprints building_sources;
    bool building_sources_known = false;
    bool run_requested = false;
    bool probe_pending = false;
    std::string probe_output;
    bool environment_ready = false;
    bool sdk_probe_active = false;
    bool fast_runtime = false;

    struct PendingBuild
    {
        bool run;
        bool automatic;
        bool incremental;
        bool recovery;
    };

    std::optional<PendingBuild> pending_build;
#ifdef CPPLIVELAB_GUI_TEST
    unsigned environment_preparations = 0;
    bool reuse_worker_environment = true;
    fs::path worker_benchmark_cli;
    std::function<void(const char*)> auto_run_trace;
#endif
    bool error_line_start = true;
    fs::path active_project_directory;
    bool configure_writes = false;
    bool configure_writes_authorized = false;
    bool project_dialog_open = false;
    cpplivelab::ProjectWatcher watcher;
    cpplivelab::AutoBuildScheduler auto_build;
    std::wstring auto_status;
    // An operation includes final output draining. Do not re-enable Build merely
    // because the original worker has exited while descendants or output remain.
    bool operation_in_progress = false;
    bool should_run_program = false;
    bool program_started = false;
    bool input_closed = false;
    bool stop_requested = false;
    bool close_requested = false;
    cpplivelab::AutoRunPreferences auto_run_preferences;
    bool auto_launch_armed = true;
    bool automatic_program_pending = false;
    enum class InitialAutoRun
    {
        None,
        Pending,
        Active,
        AwaitingTarget
    };
    InitialAutoRun initial_auto_run = InitialAutoRun::None;
    bool initial_targets_verified = false;
    fs::path initial_switch_project;
    cpplivelab::ConsoleSession replacement_build;
    cpplivelab::ConsoleSession candidate_session;
    std::unique_ptr<cpplivelab::BuildLease> candidate_lease;
    bool candidate_pending = false;
    bool candidate_ready = false;
    bool candidate_discarding = false;
    bool candidate_needs_window = false;
    ULONGLONG candidate_started = 0;
    ULONGLONG candidate_visible = 0;
    std::vector<cpplivelab::ApplicationWindow> candidate_placement;
    std::vector<HWND> candidate_restored;
    std::string candidate_stdout, candidate_stderr;
    cpplivelab::ProjectOptions replacement_options;
    cpplivelab::SourceFingerprints replacement_sources;
    bool replacement_building = false;
    bool replacement_ready = false;
    bool replacement_stopping = false;
    bool graceful_stop_pending = false;
    bool launch_latest = false;
    ULONGLONG shutdown_started = 0;
    ULONGLONG restore_started = 0;
    std::vector<cpplivelab::ApplicationWindow> previous_windows;
    cpplivelab::WindowPlacementMemory placement_memory;
    std::vector<cpplivelab::ApplicationWindow> placement_sample;
    std::wstring running_configuration, running_target;
    ULONGLONG placement_sampled = 0;
    bool placement_warning = false;
    std::vector<HWND> restored_windows;
    bool restore_position = true;
    bool restore_size = false;
    bool candidate_restore_position = true;
    bool candidate_restore_size = false;
    bool restart_pending = false;
    bool restart_confirmation = false;
    std::wstring restart_instance_name;
    std::unique_ptr<cpplivelab::RestartLaunch> restart_launch;

    // The worker's "Running:" marker may arrive in several pipe reads. Keep a
    // short tail so it can be recognized before enabling program input.
    std::string recent_output_bytes;
    wchar_t previous_output_character = L'\0';

    HWND find_control(int control_id) const
    {
        return GetDlgItem(window_handle, control_id);
    }

    void update_fonts()
    {
        const auto old_interface = interface_font;
        const auto old_output = output_font;
        const auto old_heading = heading_font;
        LOGFONTW heading{};
        heading.lfHeight = -cpplivelab::scale_pixels(24, dpi);
        heading.lfWeight = FW_SEMIBOLD;
        wcscpy_s(heading.lfFaceName, L"Segoe UI");
        heading_font = CreateFontIndirectW(&heading);
        interface_font = CreateFontW(-cpplivelab::scale_pixels(14, dpi),
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH,
            L"Segoe UI");
        output_font = CreateFontW(-cpplivelab::scale_pixels(15, dpi),
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            FIXED_PITCH,
            L"Consolas");
        for (HWND child = GetWindow(window_handle, GW_CHILD); child;
            child = GetWindow(child, GW_HWNDNEXT))
        {
            const auto font = child == program_console.window || child == system_console.window
                                  ? output_font
                              : GetDlgCtrlID(child) == ShellHeading ||
                                      GetDlgCtrlID(child) == ProjectIdentityLabel ||
                                      GetDlgCtrlID(child) == WelcomeTitle
                                  ? heading_font
                                  : interface_font;
            SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            wchar_t name[64]{};
            GetClassNameW(child, name, 64);
            if (std::wstring_view(name) == L"ComboBox")
            {
                SendMessageW(child,
                    CB_SETITEMHEIGHT,
                    static_cast<WPARAM>(-1),
                    cpplivelab::scale_pixels(24, dpi));
                SendMessageW(child, CB_SETITEMHEIGHT, 0, cpplivelab::scale_pixels(24, dpi));
            }
        }
        if (old_interface)
        {
            DeleteObject(old_interface);
        }
        if (old_output)
        {
            DeleteObject(old_output);
        }
        if (old_heading)
        {
            DeleteObject(old_heading);
        }
    }

    std::wstring preference_store = L"Software\\EasierCpp\\Preferences";

    void persist_preferences()
    {
        if (!cpplivelab::save_preferences(preferences, preference_store))
        {
            log(cpplivelab::MessageKind::Warning,
                L"Preferences could not be saved; the current selection still applies.");
        }
    }

    void apply_appearance()
    {
        if (applying_appearance)
        {
            return;
        }
        applying_appearance = true;
        appearance.apply(window_handle, preferences.theme, dpi, true);
        if (shell_tooltip)
        {
            SetWindowTheme(shell_tooltip, L"", L"");
            SendMessageW(shell_tooltip, TTM_SETTIPBKCOLOR, appearance.palette.panel, 0);
            SendMessageW(shell_tooltip, TTM_SETTIPTEXTCOLOR, appearance.palette.text, 0);
        }
        program_console.set_colors(appearance.palette.console);
        system_console.set_colors(appearance.palette.console);
        appearance.active_button(find_control(ProgramTab), program_tab_active);
        appearance.active_button(find_control(SystemTab), !program_tab_active);
        appearance.active_button(find_control(AutoRunButton), auto_run_preferences.enabled);
        appearance.active_button(find_control(RunButton), true);
        for (const int page : navigation_pages)
        {
            appearance.active_button(find_control(page), shell_page == page);
        }
        applying_appearance = false;
    }

    void apply_appearance_choice(cpplivelab::ThemeChoice choice)
    {
        preferences.theme = choice;
        persist_preferences();
        apply_appearance();
    }

    void apply_build_preferences(DWORD debounce)
    {
        if (debounce < 100 || debounce > 5000)
        {
            throw std::runtime_error("Debounce must be 100-5000 ms.");
        }
        preferences.debounce_ms = debounce;
        auto_build.set_debounce(debounce);
        persist_preferences();
    }

    void copy_project_path()
    {
        if (active_project_directory.empty() || !OpenClipboard(window_handle))
        {
            return;
        }
        const auto value = active_project_directory.wstring();
        const auto allocation = GlobalAlloc(GMEM_MOVEABLE, (value.size() + 1) * sizeof(wchar_t));
        if (allocation)
        {
            if (auto destination = static_cast<wchar_t*>(GlobalLock(allocation)))
            {
                std::copy(value.c_str(), value.c_str() + value.size() + 1, destination);
                GlobalUnlock(allocation);
                EmptyClipboard();
                if (!SetClipboardData(CF_UNICODETEXT, allocation))
                {
                    GlobalFree(allocation);
                }
            }
            else
            {
                GlobalFree(allocation);
            }
        }
        CloseClipboard();
    }

    void appearance_dialog()
    {
        const std::vector<std::wstring> choices{L"Dark", L"Light", L"Follow Windows"};
        std::vector<cpplivelab::FormField> fields{
            {L"Appearance", choices[static_cast<size_t>(preferences.theme)], choices}};
        cpplivelab::FormOptions options;
        options.theme = preferences.theme;
        options.appearance_field = 0;
        options.note = L"Uses your existing customized palette. Changes apply to all EasierCpp "
                       L"windows after OK.";
        if (show_project_form(L"Appearance", fields, options, shell_page == NavigationSettings))
        {
            apply_appearance_choice(static_cast<cpplivelab::ThemeChoice>(
                std::find(choices.begin(), choices.end(), fields[0].value) - choices.begin()));
        }
    }

    bool optimize_workers(HWND owner)
    {
        if (operation_in_progress || replacement_building || candidate_pending ||
            cpplivelab::library_task_active())
        {
            throw std::runtime_error(
                "Stop the current program/build before optimizing compilation workers.");
        }
        const auto parent = cpplivelab::manager_state_directory() / L"worker-benchmarks";
        fs::create_directories(parent);
        const auto directory = parent / (std::to_wstring(GetCurrentProcessId()) + L"-" +
                                            std::to_wstring(GetTickCount64()));
        if (!fs::create_directory(directory))
        {
            throw std::runtime_error("Cannot reserve worker benchmark directory.");
        }

        struct Cleanup
        {
            fs::path directory, parent;

            ~Cleanup()
            {
                std::error_code error;
                const auto resolved = fs::weakly_canonical(directory, error);
                const auto expected = fs::weakly_canonical(parent, error);
                if (!error && resolved.parent_path() == expected)
                {
                    fs::remove_all(resolved, error);
                }
            }
        } cleanup{directory, parent};

        const bool previous_dialog = project_dialog_open;
        project_dialog_open = true;

        struct RestoreDialog
        {
            bool& target;
            bool previous;

            ~RestoreDialog()
            {
                target = previous;
            }
        } restore{project_dialog_open, previous_dialog};

        auto benchmark_cli = application_directory() / L"EasierCpp.exe";
#ifdef CPPLIVELAB_GUI_TEST
        if (!worker_benchmark_cli.empty())
        {
            benchmark_cli = worker_benchmark_cli;
        }
#endif
        const bool completed = cpplivelab::run_library_task(
            owner,
            preferences.theme,
            benchmark_cli,
            {L"optimize-workers", directory.wstring()},
            directory,
            [&](const auto& text)
            {
                append(text);
            },
            600000,
            L"Optimize Workers");
        if (!completed || restart_pending || close_requested)
        {
            return false;
        }
        unsigned recommended = 0;
        if (!(std::ifstream(directory / L"recommendation.txt") >> recommended) || !recommended ||
            recommended > cpplivelab::maximum_build_workers())
        {
            throw std::runtime_error("Invalid worker recommendation; previous count retained.");
        }
        fs::copy_file(directory / L"results.csv",
            cpplivelab::manager_state_directory() / L"worker-benchmark.csv",
            fs::copy_options::overwrite_existing);
        cpplivelab::save_build_workers({recommended, recommended});
        log(cpplivelab::MessageKind::Success,
            L"Recommended compilation workers: " + std::to_wstring(recommended) +
                L". Synthetic estimate; manual override remains available.");
        return true;
    }

    void window_preferences_dialog()
    {
        if (active_project_directory.empty())
        {
            append(L"Open a project to configure its window and replacement preferences.");
            return;
        }
        const std::vector<std::wstring> modes{
            L"Safe Replacement", L"Seamless Replacement (Experimental)"};
        std::vector<cpplivelab::FormField> fields{
            {L"Keep Window Position (When Restarted)",
                auto_run_preferences.keep_window_position ? L"Yes" : L"No"},
            {L"Keep Window Size (When Restarted)",
                auto_run_preferences.keep_window_size ? L"Yes" : L"No"},
            {L"Auto Run Replacement Mode",
                modes[static_cast<unsigned>(auto_run_preferences.replacement_mode)],
                modes}};
        fields[0].checkbox = true;
        fields[0].help =
            L"Restore this project's application position on Run, Restart and Auto "
            L"Run, including after reopening EasierCpp. Size is controlled separately.";
        fields[1].checkbox = true;
        fields[1].help = L"Restore remembered window dimensions. Turn this off to use dimensions "
                         L"requested by your program. Choices apply to subsequent launches.";
        const auto workers = cpplivelab::load_build_workers();
        const std::wstring inherited = L"Build-system default";
        std::vector<std::wstring> counts{inherited};
        for (unsigned count = 1; count <= cpplivelab::maximum_build_workers(); ++count)
        {
            counts.push_back(std::to_wstring(count));
        }
        if (workers.count > cpplivelab::maximum_build_workers())
        {
            counts.push_back(std::to_wstring(workers.count));
        }
        fields.push_back({L"Parallel Build Workers — Current Workers",
            workers.count ? std::to_wstring(workers.count) : inherited,
            counts});
        fields.back().help =
            L"Global compilation-job limit. 1 is sequential; higher counts are maximum "
            L"concurrency. CPU/memory limits apply. Changes save immediately for future builds; "
            L"existing native defaults remain until you select a number.";
        fields.push_back({L"Recommended Workers",
            workers.recommended ? std::to_wstring(workers.recommended) + L" (synthetic estimate)"
                                : L"Not benchmarked"});
        fields.back().read_only = true;
        cpplivelab::FormOptions options;
        options.theme = preferences.theme;
        options.changed = [inherited, last = fields[3].value](auto& changed) mutable
        {
            if (changed[3].value != last)
            {
                auto selected = cpplivelab::load_build_workers();
                selected.count = changed[3].value == inherited
                                     ? 0
                                     : static_cast<unsigned>(std::stoul(changed[3].value));
                cpplivelab::save_build_workers(selected);
                last = changed[3].value;
            }
        };
        options.action_label = L"Optimize Workers";
        options.action = [&](HWND owner, auto& changed)
        {
            if (optimize_workers(owner))
            {
                const auto result = cpplivelab::load_build_workers();
                changed[3].value = std::to_wstring(result.count);
                if (std::find(changed[3].choices.begin(),
                        changed[3].choices.end(),
                        changed[3].value) == changed[3].choices.end())
                {
                    changed[3].choices.push_back(changed[3].value);
                }
                changed[4].value = std::to_wstring(result.recommended) + L" (synthetic estimate)";
            }
        };
        options.note =
            L"Window/replacement settings apply to this project; compilation workers are global "
            L"and save immediately. Seamless overlaps instances: use Safe for exclusive "
            L"resources. Mode changes apply to the next replacement.";
        if (show_project_form(L"Preferences", fields, options, shell_page == NavigationSettings))
        {
            auto_run_preferences.keep_window_position = fields[0].value == L"Yes";
            auto_run_preferences.keep_window_size = fields[1].value == L"Yes";
            auto_run_preferences.restore_windows =
                auto_run_preferences.keep_window_position || auto_run_preferences.keep_window_size;
            auto_run_preferences.placement_memory = auto_run_preferences.restore_windows
                                                        ? cpplivelab::PlacementMemory::Persistent
                                                        : cpplivelab::PlacementMemory::Off;
            // Preferences affect future launches, never reposition the active window.
            previous_windows.clear();
            restore_started = 0;
            auto_run_preferences.replacement_mode = static_cast<cpplivelab::ReplacementMode>(
                std::find(modes.begin(), modes.end(), fields[2].value) - modes.begin());
            cpplivelab::save_auto_run_preferences(active_project_directory, auto_run_preferences);
        }
    }

    void build_preferences_dialog()
    {
        std::vector<cpplivelab::FormField> fields{
            {L"Auto Run debounce (milliseconds)", std::to_wstring(preferences.debounce_ms)},
            {L"Graceful shutdown timeout (milliseconds)",
                std::to_wstring(auto_run_preferences.shutdown_ms)},
            {L"Force-stop owned program after timeout",
                auto_run_preferences.force_stop ? L"Yes" : L"No",
                {L"No", L"Yes"}}};
        fields[0].help = L"Wait after the last save before automatic compilation. Example: 400 ms "
                         L"combines rapid saves. Allowed: 100-5000 ms.";
        cpplivelab::FormOptions options;
        options.theme = preferences.theme;
        options.note =
            L"Debounce is global. Window and shutdown settings belong to the open project. "
            L"Force-stop is OFF by default. Restart sends WM_CLOSE and console EOF first.";
        options.validate = [](const auto& values) -> std::wstring
        {
            try
            {
                size_t used = 0;
                const auto value = std::stoul(values[0].value, &used);
                size_t timeout_used = 0;
                const auto timeout = std::stoul(values[1].value, &timeout_used);
                if (used == values[0].value.size() && value >= 100 && value <= 5000 &&
                    timeout_used == values[1].value.size() && timeout >= 100 && timeout <= 30000)
                {
                    return L"";
                }
            }
            catch (...)
            {
            }
            return L"Debounce must be 100–5000 ms; shutdown timeout must be 100–30000 ms.";
        };
        if (show_project_form(L"Build Preferences",
                fields,
                options,
                shell_page == NavigationSettings || shell_page == NavigationTools))
        {
            apply_build_preferences(static_cast<DWORD>(std::stoul(fields[0].value)));
            auto_run_preferences.shutdown_ms = std::stoul(fields[1].value);
            auto_run_preferences.force_stop = fields[2].value == L"Yes";
            if (!active_project_directory.empty())
            {
                cpplivelab::save_auto_run_preferences(
                    active_project_directory, auto_run_preferences);
            }
        }
    }

    ~Application()
    {
        if (interface_font)
        {
            DeleteObject(interface_font);
        }
        if (output_font)
        {
            DeleteObject(output_font);
        }
        if (heading_font)
        {
            DeleteObject(heading_font);
        }
    }

    HWND create_control(
        const wchar_t* window_class, const wchar_t* label, int control_id, DWORD style = 0)
    {
        if (std::wstring_view(window_class) == L"COMBOBOX")
        {
            style |= CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
        }
        HWND child_window_handle = CreateWindowExW(0,
            window_class,
            label,
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | style,
            0,
            0,
            0,
            0,
            window_handle,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(control_id)),
            GetModuleHandleW(nullptr),
            nullptr);
        SendMessageW(child_window_handle,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(control_id == ShellHeading ||
                                             control_id == ProjectIdentityLabel ||
                                             control_id == WelcomeTitle
                                         ? heading_font
                                         : interface_font),
            TRUE);
        return child_window_handle;
    }

    void create()
    {
        SetWindowTextW(window_handle, application_title().c_str());
        cpplivelab::initialize_build_workers();
        dpi = GetDpiForWindow(window_handle);
        update_fonts();
        auto_build.enable(false);
        auto_build.set_debounce(preferences.debounce_ms);
        constexpr const wchar_t* page_names[] = {
            L"Home", L"Workspace", L"Libraries", L"Project", L"Tools", L"Settings"};
        for (size_t index = 0; index < std::size(navigation_pages); ++index)
        {
            create_control(
                L"BUTTON", page_names[index], navigation_pages[index], WS_TABSTOP | BS_LEFT);
        }
        create_control(L"STATIC", L"", ShellHeading, SS_NOPREFIX);
        create_control(L"STATIC", L"", ShellDescription, SS_NOPREFIX);
        for (const int id : {ShellCardOne,
                 ShellCardTwo,
                 ShellCardThree,
                 ShellCardFour,
                 ShellCardNote,
                 ShellCardDetail})
        {
            create_control(L"STATIC", L"", id, SS_NOPREFIX);
        }
        create_control(
            L"LISTBOX", L"", ShellLibraryList, WS_TABSTOP | WS_VSCROLL | LBS_NOINTEGRALHEIGHT);
        create_control(L"STATIC",
            (L"EasierCpp v" + decode_console_text(EASIERCPP_VERSION)).c_str(),
            ShellVersion,
            SS_NOPREFIX);
        const auto inline_host = create_control(L"STATIC", L"", ShellInlineHost, WS_CLIPCHILDREN);
        SetWindowLongPtrW(inline_host,
            GWL_EXSTYLE,
            GetWindowLongPtrW(inline_host, GWL_EXSTYLE) | WS_EX_CONTROLPARENT);
        for (const auto [id, size] : {std::pair{WelcomeLogo, 64}})
        {
            const auto logo = create_control(L"STATIC", L"", id, SS_ICON | SS_CENTERIMAGE);
            const int pixels = cpplivelab::scale_pixels(size, dpi);
            SendMessageW(
                logo, STM_SETICON, reinterpret_cast<WPARAM>(application_icon(pixels, pixels)), 0);
        }
        for (const auto [id, caption] : std::initializer_list<std::pair<int, const wchar_t*>>{
                 {RecentProjectsMenu, L"Recent Projects"},
                 {ProjectSettingsMenu, L"Project Settings"},
                 {CopyProjectPath, L"Copy"},
                 {DoctorMenu, L"Summary Doctor"},
                 {DetailedDoctorMenu, L"Detailed Report"},
                 {ReviewErrorsMenu, L"Build Errors"},
                 {RecoverBuildMenu, L"Clean and Rebuild"},
                 {RestartButton, L"Restart EasierCpp"},
#ifndef EASIERCPP_PUBLIC_GUI
                 {TestRunnerMenu, L"Run Tests"},
                 {CancelTestsMenu, L"Cancel Tests"},
#endif
                 {OptimizeWorkersButton, L"Optimize Workers"},
                 {AppearanceMenu, L"Appearance"},
                 {EditorSettingsMenu, L"Preferred Editor"},
                 {BuildPreferencesMenu, L"Build Preferences"},
                 {WindowPreferencesMenu, L"Window & Process"}})
        {
            create_control(L"BUTTON", caption, id, WS_TABSTOP);
        }
        create_control(L"STATIC", L"Configuration", BuildConfigurationLabel);
        create_control(L"BUTTON", L"Restart", ProgramRestartButton, WS_TABSTOP);
        create_control(L"BUTTON", L"Add Library", AddLibraryButton, WS_TABSTOP);
        create_control(L"BUTTON", L"Manage Libraries", ManageLibrariesMenu, WS_TABSTOP);
        create_control(L"BUTTON", L"Manage libraries", WorkspaceLibraries, WS_TABSTOP);
        create_control(
            L"STATIC", L"Libraries: none managed", LibrarySummary, SS_ENDELLIPSIS | SS_NOPREFIX);
        create_control(L"STATIC",
            L"No active project. Use Project > New or Open Existing.",
            ProjectIdentityLabel,
            SS_ENDELLIPSIS | SS_NOPREFIX);
        create_control(L"STATIC", L"", ProjectPathLabel, SS_PATHELLIPSIS | SS_NOPREFIX);
        create_control(L"BUTTON", L"Open Editor", EditProjectButton, WS_TABSTOP);
        create_control(L"COMBOBOX", L"", ConfigurationSelector, WS_TABSTOP | CBS_DROPDOWNLIST);
        for (const auto* label : {L"Debug", L"Release"})
        {
            SendMessageW(find_control(ConfigurationSelector),
                CB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(label));
        }
        SendMessageW(find_control(ConfigurationSelector), CB_SETCURSEL, 0, 0);
        create_control(L"STATIC", L"Executable target", 202);
        create_control(L"COMBOBOX", L"", ExecutableInput, WS_TABSTOP | CBS_DROPDOWNLIST);
        create_control(L"BUTTON", L"Build", BuildButton, WS_TABSTOP);
        create_control(L"BUTTON", L"Run", RunButton, WS_TABSTOP);
        create_control(L"BUTTON", L"Stop", StopButton, WS_TABSTOP);
        create_control(L"BUTTON", L"Program Console", ProgramTab, WS_TABSTOP);
        create_control(L"BUTTON", L"Build & System", SystemTab, WS_TABSTOP);
        create_control(L"BUTTON", L"Clear Console", ClearProgram, WS_TABSTOP);
        create_control(L"BUTTON", L"Clear Console", ClearSystem, WS_TABSTOP);
        program_console.create(window_handle, OutputPane, output_font);
        system_console.create(window_handle, SystemOutputPane, output_font);
        select_console(true);
        create_control(L"STATIC", L"Program input", 204);
        create_control(L"EDIT", L"", ProgramInput, WS_TABSTOP | ES_AUTOHSCROLL);
        SendMessageW(find_control(ProgramInput), EM_SETLIMITTEXT, 1000, 0);
        create_control(L"BUTTON", L"Send", SendButton, WS_TABSTOP);
        create_control(L"BUTTON", L"End Input", EndInputButton, WS_TABSTOP);
        create_control(L"STATIC", L"Ready", StatusLabel);
        create_control(L"BUTTON", L"Auto Run: OFF", AutoRunButton, WS_TABSTOP);
        create_control(L"STATIC", L"Open or create a CMake project", WelcomeTitle);
        create_control(L"STATIC", L"Choose a folder or start a new project.", WelcomeNote);
        create_control(L"BUTTON", L"New Project...", NewProjectMenu, WS_TABSTOP);
        create_control(L"BUTTON", L"Open Existing Project...", BrowseButton, WS_TABSTOP);
        create_control(
            L"LISTBOX", L"Recent projects", WelcomeRecent, WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY);
        create_control(L"BUTTON", L"Open", OpenWelcomeRecent, WS_TABSTOP);
        create_control(L"BUTTON", L"Remove", ForgetWelcomeRecent, WS_TABSTOP);
        reload_welcome_recents();
        refresh();
        create_shell_help();
        apply_appearance();
        SetTimer(window_handle, 1, 50, nullptr);
    }

    void reload_welcome_recents()
    {
        welcome_recents = cpplivelab::recent_projects();
        SendDlgItemMessageW(window_handle, WelcomeRecent, LB_RESETCONTENT, 0, 0);
        for (const auto& path : welcome_recents)
        {
            const auto label = path.filename().wstring() + L" - " + path.wstring();
            SendDlgItemMessageW(window_handle,
                WelcomeRecent,
                LB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(label.c_str()));
        }
    }

    void create_shell_help()
    {
        CoCreateInstance(CLSID_AccPropServices,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&shell_accessibility));
        shell_tooltip = CreateWindowExW(WS_EX_TOPMOST,
            TOOLTIPS_CLASSW,
            nullptr,
            WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            window_handle,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);
        SendMessageW(shell_tooltip, TTM_SETMAXTIPWIDTH, 0, cpplivelab::scale_pixels(340, dpi));
        for (const auto& item : shell_help)
        {
            const auto button = create_control(L"BUTTON", L"ⓘ", item.id, WS_TABSTOP);
            if (shell_accessibility)
            {
                shell_accessibility->SetHwndPropStr(
                    button, OBJID_CLIENT, CHILDID_SELF, PROPID_ACC_NAME, item.name);
            }
            // The GUI also runs with legacy Comctl32, which rejects the
            // larger structure containing the unused reserved pointer.
            TOOLINFOW tool{TTTOOLINFOW_V2_SIZE};
            tool.uFlags = TTF_TRACK | TTF_TRANSPARENT;
            tool.hwnd = window_handle;
            tool.uId = static_cast<UINT_PTR>(item.id);
            tool.lpszText = const_cast<wchar_t*>(item.explanation);
            SendMessageW(shell_tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool));
            SetWindowSubclass(button, help_focus, 2, reinterpret_cast<DWORD_PTR>(this));
        }
    }

    void destroy_shell_help()
    {
        if (shell_accessibility)
        {
            const MSAAPROPID property = PROPID_ACC_NAME;
            for (const auto& item : shell_help)
            {
                shell_accessibility->ClearHwndProps(
                    find_control(item.id), OBJID_CLIENT, CHILDID_SELF, &property, 1);
            }
            shell_accessibility->Release();
            shell_accessibility = nullptr;
        }
        if (shell_tooltip)
        {
            DestroyWindow(shell_tooltip);
            shell_tooltip = nullptr;
        }
    }

    void dismiss_shell_help()
    {
        if (tracked_help)
        {
            TOOLINFOW tool{TTTOOLINFOW_V2_SIZE};
            tool.hwnd = window_handle;
            tool.uId = static_cast<UINT_PTR>(std::exchange(tracked_help, 0));
            SendMessageW(shell_tooltip, TTM_TRACKACTIVATE, FALSE, reinterpret_cast<LPARAM>(&tool));
        }
    }

    void show_shell_help(int id, bool hover = false)
    {
        if (tracked_help == id)
        {
            hover_help = hover_help && hover;
            return;
        }
        dismiss_shell_help();
        tracked_help = id;
        hover_help = hover;
        RECT bounds{};
        GetWindowRect(find_control(id), &bounds);
        TOOLINFOW tool{TTTOOLINFOW_V2_SIZE};
        tool.hwnd = window_handle;
        tool.uId = static_cast<UINT_PTR>(id);
        // Without TTF_ABSOLUTE the native tooltip adjusts to the monitor edges.
        SendMessageW(shell_tooltip, TTM_SETMAXTIPWIDTH, 0, cpplivelab::scale_pixels(340, dpi));
        SendMessageW(shell_tooltip,
            TTM_TRACKPOSITION,
            0,
            MAKELPARAM(bounds.left, bounds.bottom + cpplivelab::scale_pixels(4, dpi)));
        SendMessageW(shell_tooltip, TTM_TRACKACTIVATE, TRUE, reinterpret_cast<LPARAM>(&tool));
        // Measure the native tooltip after activation so wrapped text and the
        // taskbar work area are taken into account, including on legacy controls.
        RECT popup{};
        MONITORINFO monitor{sizeof(monitor)};
        if (GetWindowRect(shell_tooltip, &popup) &&
            GetMonitorInfoW(MonitorFromRect(&bounds, MONITOR_DEFAULTTONEAREST), &monitor))
        {
            const auto& work = monitor.rcWork;
            const auto width = popup.right - popup.left;
            const auto height = popup.bottom - popup.top;
            const auto gap = cpplivelab::scale_pixels(4, dpi);
            const auto below = bounds.bottom + gap;
            const auto top = below + height <= work.bottom ? below : bounds.top - height - gap;
            SetWindowPos(shell_tooltip,
                nullptr,
                (std::max)(work.left, (std::min)(bounds.left, work.right - width)),
                (std::max)(work.top, (std::min)(top, work.bottom - height)),
                0,
                0,
                SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
        }
    }

    static LRESULT CALLBACK help_focus(
        HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR id, DWORD_PTR owner)
    {
        auto& app = *reinterpret_cast<Application*>(owner);
        const auto control = GetDlgCtrlID(window);
        if (message == WM_MOUSEMOVE && app.tracked_help != control)
        {
            TRACKMOUSEEVENT tracking{
                sizeof(tracking), TME_HOVER | TME_LEAVE, window, HOVER_DEFAULT};
            TrackMouseEvent(&tracking);
        }
        else if (message == WM_MOUSEHOVER && (!app.tracked_help || app.hover_help))
        {
            app.show_shell_help(control, true);
        }
        else if ((message == WM_KILLFOCUS || (message == WM_MOUSELEAVE && app.hover_help)) &&
                 app.tracked_help == control)
        {
            app.dismiss_shell_help();
        }
        else if (message == WM_SETFOCUS)
        {
            app.show_shell_help(control);
        }
        else if (message == WM_GETDLGCODE && (wparam == VK_ESCAPE || wparam == VK_RETURN))
        {
            return DLGC_WANTMESSAGE | DLGC_BUTTON;
        }
        else if (message == WM_KEYDOWN && wparam == VK_ESCAPE)
        {
            app.dismiss_shell_help();
            return 0;
        }
        else if (message == WM_KEYDOWN && wparam == VK_RETURN)
        {
            app.show_shell_help(control);
            return 0;
        }
        if (message == WM_NCDESTROY)
        {
            RemoveWindowSubclass(window, help_focus, id);
        }
        return DefSubclassProc(window, message, wparam, lparam);
    }

    void navigate_to(int page)
    {
        dismiss_shell_help();
        shell_page = page;
        update_test_actions();
        for (const int id : navigation_pages)
        {
            appearance.active_button(find_control(id), id == page);
        }
        RECT client{};
        GetClientRect(window_handle, &client);
        layout(client.right, client.bottom);
    }

    void paint_shell(HDC dc)
    {
        const auto& palette = appearance.palette;
        RECT client{};
        GetClientRect(window_handle, &client);
        const auto fill = [&](RECT bounds, COLORREF color)
        {
            SetDCBrushColor(dc, color);
            FillRect(dc, &bounds, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        };
        fill({0, 0, cpplivelab::scale_pixels(156, dpi), client.bottom},
            palette.dark ? RGB(20, 33, 39) : RGB(246, 248, 248));
        const auto pen = CreatePen(PS_SOLID, 1, palette.border);
        const auto previous_pen = SelectObject(dc, pen);
        const auto previous_brush = SelectObject(dc, GetStockObject(DC_BRUSH));
        SetDCBrushColor(dc, palette.panel);
        const auto diameter = cpplivelab::scale_pixels(16, dpi);
        for (const auto& bounds : shell_panels)
        {
            RoundRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom, diameter, diameter);
        }
        SelectObject(dc, previous_brush);
        SelectObject(dc, previous_pen);
        DeleteObject(pen);
    }

    void layout(int width, int height)
    {
        width = MulDiv(width, 96, static_cast<int>(dpi));
        height = MulDiv(height, 96, static_cast<int>(dpi));
        if (width < 620 || height < 480)
        {
            return;
        }
        const auto px = [&](int value)
        {
            return cpplivelab::scale_pixels(value, dpi);
        };
        shell_panels.clear();
        std::vector<int> visible;
        const auto move = [&](int id, int x, int y, int w, int h)
        {
            if (const auto control = find_control(id))
            {
                MoveWindow(control, px(x), px(y), px(w), px(h), FALSE);
            }
        };
        const auto place = [&](int id, int x, int y, int w, int h)
        {
            move(id, x, y, w, h);
            visible.push_back(id);
        };
        const auto panel = [&](int x, int y, int w, int h)
        {
            shell_panels.push_back({px(x), px(y), px(x + w), px(y + h)});
        };
        const auto caption = [&](int id, const wchar_t* text, int x, int y, int w, int h)
        {
            SetWindowTextW(find_control(id), text);
            place(id, x, y, w, h);
        };
        const bool selected = !active_project_directory.empty();
        const int left = 180;
        const int right = width - 24;
        const int content = right - left;
        const int half = (content - 12) / 2;
        const int second = left + half + 12;

        for (size_t index = 0; index < std::size(navigation_pages); ++index)
        {
            place(navigation_pages[index], 10, 24 + static_cast<int>(index) * 38, 136, 32);
        }
        int detail_top = 0;
        if (shell_page == NavigationWorkspace && selected)
        {
            place(ProjectIdentityLabel, left, 22, content - 140, 35);
            caption(ShellDescription, L"Active project", left, 57, content, 20);
            place(EditProjectButton, right - 111, 22, 111, 34);
            panel(left, 85, content, 113);
            place(BuildConfigurationLabel, left + 10, 93, 96, 18);
            place(ConfigurationSelector, left + 10, 114, 92, 180);
            place(202, left + 113, 93, 110, 18);
            place(TargetHelp, left + 225, 90, 22, 22);
            place(ExecutableInput, left + 113, 114, (std::min)(218, content - 324), 180);
            place(AutoRunButton, right - 181, 111, 145, 32);
            place(AutoRunHelp, right - 31, 116, 22, 22);
            place(BuildButton, left + 10, 154, 66, 34);
            place(RunButton, left + 82, 154, 62, 34);
            place(StopButton, left + 150, 154, 62, 34);
            place(ProgramRestartButton, left + 218, 154, 84, 34);
            place(RestartHelp, left + 307, 160, 22, 22);

            place(StatusLabel, left + 8, 212, content - 170, 22);
            place(WorkspaceLibraries, right - 143, 204, 135, 31);

            const int tabs_y = 245;
            panel(left, tabs_y, content, height - tabs_y - 12);
            place(ProgramTab, left + 10, tabs_y + 5, 147, 31);
            place(SystemTab, left + 164, tabs_y + 5, 155, 31);
            place(
                program_tab_active ? ClearProgram : ClearSystem, right - 122, tabs_y + 5, 112, 31);
            // The hidden Rich Edit also needs a viewport for wrapped diagnostics.
            const int console_height = (std::max)(30, height - tabs_y - 96);
            move(OutputPane, left + 9, tabs_y + 42, content - 18, console_height);
            move(SystemOutputPane, left + 9, tabs_y + 42, content - 18, console_height);
            visible.push_back(program_tab_active ? OutputPane : SystemOutputPane);
            place(204, left + 10, height - 43, 99, 24);
            place(InputHelp, left + 112, height - 43, 22, 22);
            place(ProgramInput, left + 141, height - 46, content - 317, 30);
            place(SendButton, right - 167, height - 46, 62, 30);
            place(EndInputButton, right - 98, height - 46, 88, 30);
        }
        else if (shell_page == NavigationHome || shell_page == NavigationWorkspace)
        {
            caption(ShellHeading, L"Welcome to EasierCpp", left, 59, content, 35);
            caption(ShellDescription, L"Build and run your C++ projects.", left, 97, content, 22);
            panel(left, 129, content, 126);
            place(WelcomeTitle, left + 16, 145, content - 110, 33);
            place(WelcomeNote, left + 16, 181, content - 109, 22);
            place(WelcomeLogo, right - 84, 155, 64, 64);
            place(NewProjectMenu, left + 16, 211, 119, 34);
            place(BrowseButton, left + 143, 211, 165, 34);
            if (inline_active)
            {
                detail_top = 269;
            }
            else
            {
                panel(left, 269, half, 166);
                panel(second, 269, content - half - 12, 166);
                caption(ShellCardOne, L"Recent projects", left + 12, 279, half - 24, 22);
                caption(ShellCardTwo, L"Development environment", second + 12, 279, half - 24, 22);
                place(WelcomeRecent, left + 12, 306, half - 24, 77);
                place(OpenWelcomeRecent, left + 12, 391, (half - 30) / 2, 34);
                place(ForgetWelcomeRecent, left + 18 + (half - 30) / 2, 391, (half - 30) / 2, 34);
                place(DoctorMenu, second + 12, 307, half - 24, 34);
                place(StatusLabel, second + 12, 351, half - 24, 65);
                if (!selected && welcome_output)
                {
                    place(ClearSystem, right - 113, 442, 113, 28);
                    place(SystemOutputPane, left, 476, content, (std::max)(24, height - 493));
                }
            }
        }
        else
        {
            detail_top = layout_destination(
                place, panel, caption, left, right, half, second, height, selected);
            if (shell_page == NavigationSettings)
            {
                place(ShellVersion, left, height - 30, content, 22);
            }
        }
        if (inline_active && detail_top)
        {
            const int detail_height =
                height - detail_top - (shell_page == NavigationSettings ? 40 : 12);
            panel(left, detail_top, content, detail_height);
            place(ShellInlineHost, left + 1, detail_top + 1, content - 2, detail_height - 2);
            if (const auto child = GetWindow(find_control(ShellInlineHost), GW_CHILD))
            {
                MoveWindow(child, 0, 0, px(content - 2), px(detail_height - 2), FALSE);
            }
        }
        for (auto child = GetWindow(window_handle, GW_CHILD); child;
            child = GetWindow(child, GW_HWNDNEXT))
        {
            const bool show =
                std::find(visible.begin(), visible.end(), GetDlgCtrlID(child)) != visible.end();
            if (show != ((GetWindowLongPtrW(child, GWL_STYLE) & WS_VISIBLE) != 0))
            {
                if (child == program_console.window)
                {
                    program_console.set_visible(show);
                }
                else if (child == system_console.window)
                {
                    system_console.set_visible(show);
                }
                else
                {
                    ShowWindow(child, show ? SW_SHOWNA : SW_HIDE);
                }
            }
        }
        appearance.round_controls();
        RedrawWindow(window_handle,
            nullptr,
            nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
    }

    int layout_destination(const auto& place,
        const auto& panel,
        const auto& caption,
        int left,
        int right,
        int half,
        int second,
        int height,
        bool selected)
    {
        const int width = right - left;
        const auto card =
            [&](int x, int y, int heading, const wchar_t* title, std::initializer_list<int> actions)
        {
            panel(x, y, half, 48 + 35 * static_cast<int>(actions.size()));
            caption(heading, title, x + 12, y + 10, half - 24, 22);
            int row = y + 40;
            for (const auto id : actions)
            {
                place(id, x + 12, row, half - 24, 32);
                row += 35;
            }
        };
        if (shell_page == NavigationLibraries)
        {
            caption(ShellHeading, L"Library Manager", left, 59, width, 35);
            caption(ShellDescription,
                L"Manage integrations and review existing dependencies.",
                left,
                97,
                width,
                22);
            panel(left, 129, width, inline_active ? 87 : 185);
            place(LibrarySummary, left + 12, 141, width - 24, 24);
            place(AddLibraryButton, left + 12, 170, 124, 34);
            place(ManageLibrariesMenu, left + 146, 170, 156, 34);
            if (inline_active)
            {
                return 228;
            }
            place(ShellLibraryList, left + 12, 218, width - 24, 85);
            panel(left, 326, half, 132);
            panel(second, 326, half, 132);
            caption(ShellCardOne, L"Supported ways to add", left + 12, 338, half - 24, 22);
            caption(ShellCardTwo, L"Safety and verification", second + 12, 338, half - 24, 22);
            caption(ShellCardNote,
                L"Known libraries, GitHub and GitLab, installed CMake packages, local sources and "
                L"headers.",
                left + 12,
                369,
                half - 24,
                76);
            caption(ShellCardDetail,
                L"Inspect detected dependencies and preview managed changes before approval.",
                second + 12,
                369,
                half - 24,
                76);
        }
        else if (shell_page == NavigationProject)
        {
            caption(ShellHeading, L"Project Settings", left, 59, width, 35);
            caption(ShellDescription,
                L"Project-specific configuration, independent from global preferences.",
                left,
                97,
                width,
                22);
            card(left,
                129,
                ShellCardOne,
                L"Project identity",
                {NewProjectMenu, BrowseButton, RecentProjectsMenu});
            card(second,
                129,
                ShellCardTwo,
                L"Paths and launch",
                {ProjectSettingsMenu, CopyProjectPath});
            if (selected)
            {
                place(ProjectPathLabel, second + 12, 259, half - 24, 24);
            }
            if (inline_active)
            {
                return 294;
            }
            caption(ShellCardNote,
                L"Imported CMake files remain under your control. Project changes use the existing "
                L"review workflow.",
                left,
                337,
                width,
                46);
        }

        else if (shell_page == NavigationTools)
        {
            caption(ShellHeading, L"Tools", left, 22, width - 130, 35);
            caption(ShellDescription,
                L"Diagnostics, build recovery and isolated tests.",
                left,
                58,
                width,
                22);
            place(EditProjectButton, right - 126, 22, 126, 34);
            const int cell = (width - 24) / 4;
            const auto row = [&](int y, std::initializer_list<int> actions)
            {
                int x = left;
                for (const auto id : actions)
                {
                    place(id, x, y, cell, 31);
                    x += cell + 8;
                }
            };
            row(89, {DoctorMenu, DetailedDoctorMenu, ReviewErrorsMenu, RecoverBuildMenu});
#ifndef EASIERCPP_PUBLIC_GUI
            row(128,
                {TestRunnerMenu, CancelTestsMenu, BuildPreferencesMenu, OptimizeWorkersButton});
#else
            row(128, {BuildPreferencesMenu, OptimizeWorkersButton});
#endif
            row(167, {RestartButton});
            if (inline_active)
            {
                return 212;
            }
            caption(ShellCardOne, L"Build & System", left, 214, half, 22);
            place(ClearSystem, right - 112, 204, 112, 31);
            panel(left, 241, width, height - 253);
            place(SystemOutputPane, left + 1, 242, width - 2, height - 255);
        }
        else
        {
            caption(ShellHeading, L"Settings", left, 59, width, 35);
            caption(ShellDescription,
                L"Appearance, editor, build preferences, and project-specific restart behavior.",
                left,
                97,
                width,
                22);
            card(left, 129, ShellCardOne, L"Preferences", {AppearanceMenu, EditorSettingsMenu});
            card(second,
                129,
                ShellCardTwo,
                L"Build and restart",
                {BuildPreferencesMenu, WindowPreferencesMenu});
            if (inline_active)
            {
                return 259;
            }
            caption(ShellCardNote,
                L"Keep window position and size independently, and select Safe or Seamless "
                L"Replacement. Replacement restarts the program and does not preserve application "
                L"memory.",
                left,
                263,
                width,
                62);
        }
        return 0;
    }

    void update_console_badges()
    {
        SetWindowTextW(find_control(ProgramTab),
            program_console.history.unread() ? L"Program Console (new)" : L"Program Console");
        SetWindowTextW(find_control(SystemTab),
            system_console.history.unread_error() ? L"Build & System (!)"
            : system_console.history.unread()     ? L"Build & System (new)"
                                                  : L"Build & System");
    }

    void select_console(bool program)
    {
        program_tab_active = program;
        program_console.history.set_active(program);
        system_console.history.set_active(!program);
        appearance.active_button(find_control(ProgramTab), program);
        appearance.active_button(find_control(SystemTab), !program);
        update_console_badges();
        if (shell_page == NavigationWorkspace && !active_project_directory.empty())
        {
            RECT client{};
            GetClientRect(window_handle, &client);
            layout(client.right, client.bottom);
        }
    }

    void log(cpplivelab::MessageKind kind, const std::wstring& message)
    {
        SYSTEMTIME time{};
        GetLocalTime(&time);
        wchar_t timestamp[32]{};
        swprintf_s(timestamp, L"[%02u:%02u:%02u] ", time.wHour, time.wMinute, time.wSecond);
        system_console.append(kind,
            std::wstring(timestamp) + std::wstring(cpplivelab::message_label(kind)) + message +
                L"\n");
        update_console_badges();
    }

    void append(const std::wstring& message)
    {
        log(cpplivelab::MessageKind::System, message);
    }

    void append_build_output(const std::wstring& text, bool flush = false)
    {
        if (!text.empty())
        {
            last_build_activity = GetTickCount64();
            stall_reported = false;
        }
        pending_build_line += text;
        build_diagnostics += text;
        if (build_diagnostics.size() > 32000)
        {
            build_diagnostics.erase(0, build_diagnostics.size() - 32000);
        }
        size_t end = 0;
        while ((end = pending_build_line.find(L'\n')) != std::wstring::npos ||
               (flush && !pending_build_line.empty()) || pending_build_line.size() > 32000)
        {
            if (end == std::wstring::npos)
            {
                end = pending_build_line.size();
            }
            auto line = pending_build_line.substr(0, end);
            if (!line.empty() && line.back() == L'\r')
            {
                line.pop_back();
            }
            const auto kind = cpplivelab::classify_build_line(line);
            system_console.append(
                kind, std::wstring(cpplivelab::message_label(kind)) + line + L"\n");
            pending_build_line.erase(0, end == pending_build_line.size() ? end : end + 1);
        }
        update_console_badges();
    }

    void refresh()
    {
        // Freeze project/build choices while a job is owned. Program input is only
        // available after successful compilation and the start of execution.
        const bool selected = !active_project_directory.empty();
        update_automatic_controls();
        for (int control_id : {ConfigurationSelector, ExecutableInput})
        {
            EnableWindow(find_control(control_id), !operation_in_progress && !replacement_building);
        }
        for (int control_id : {EditProjectButton, BuildButton, RunButton})
        {
            EnableWindow(find_control(control_id),
                selected && !operation_in_progress && !replacement_building);
        }
        EnableWindow(find_control(EditProjectButton), selected);
        EnableWindow(find_control(RunButton),
            selected && !stop_requested &&
                ((!operation_in_progress && !replacement_building) || phase == Phase::Building ||
                    phase == Phase::Checking));
        EnableWindow(find_control(StopButton),
            (operation_in_progress || replacement_building || automatic_launch_allowed()) &&
                selected && !stop_requested);
        EnableWindow(find_control(AutoRunButton), selected);
        EnableWindow(find_control(ProgramRestartButton),
            selected && !replacement_building && !replacement_stopping && !candidate_pending &&
                (program_started || (!operation_in_progress &&
                                        fs::is_regular_file(current_identity().executable))));
        EnableWindow(find_control(AddLibraryButton),
            selected && !operation_in_progress && !replacement_building);
        EnableWindow(find_control(ManageLibrariesMenu),
            selected && !operation_in_progress && !replacement_building);
        EnableWindow(find_control(ProjectSettingsMenu),
            selected && !operation_in_progress && !replacement_building);
        EnableWindow(find_control(CopyProjectPath), selected);
        EnableWindow(find_control(WindowPreferencesMenu), selected);
        const bool can_change_project = !operation_in_progress && !replacement_building;
        EnableWindow(find_control(NewProjectMenu), can_change_project);
        EnableWindow(find_control(RecentProjectsMenu), can_change_project);
        EnableWindow(find_control(OpenWelcomeRecent), can_change_project);
        EnableWindow(find_control(ForgetWelcomeRecent), can_change_project);
        EnableWindow(find_control(BrowseButton),
            can_change_project || (initial_auto_run == InitialAutoRun::Active && !program_started));
        EnableWindow(find_control(OptimizeWorkersButton), can_change_project && !candidate_pending);
        update_test_actions();
        for (int control_id : {ProgramInput, SendButton, EndInputButton})
        {
            EnableWindow(find_control(control_id),
                operation_in_progress && program_started && !stop_requested && !input_closed);
        }
    }

    void open(const fs::path& folder)
    {
        if (operation_in_progress || replacement_building)
        {
            if (initial_auto_run == InitialAutoRun::Active && !program_started)
            {
                const auto next = cpplivelab::inspect_project(folder).root;
                if (next != active_project_directory)
                {
                    stop();
                    initial_switch_project = next;
                    set_auto_status(L"Cancelling obsolete initial Auto Run before project switch");
                }
            }
            return;
        }
        const auto inspection = cpplivelab::inspect_project(folder);
        const auto candidate = inspection.root;
        watcher.stop();
        auto_build.cancel();
        run_requested = false;
        probe_pending = false;
        should_run_program = false;
        initial_auto_run = InitialAutoRun::None;
        initial_targets_verified = false;
        initial_switch_project.clear();
        active_project_directory = candidate;
        if (shell_page != NavigationProject)
        {
            shell_page = NavigationWorkspace;
        }
        for (const int page : navigation_pages)
        {
            appearance.active_button(find_control(page), page == shell_page);
        }
        fast_runtime = false;
        auto_run_preferences = cpplivelab::load_auto_run_preferences(
            candidate, cpplivelab::legacy_auto_build_enabled(preference_store));
        if (!fs::exists(cpplivelab::workspace_directory(candidate) / L"auto-run.settings"))
        {
            cpplivelab::save_auto_run_preferences(candidate, auto_run_preferences);
        }
        auto_launch_armed = true;
        automatic_program_pending = false;
        auto_build.enable(auto_run_preferences.enabled);
        update_automatic_controls();
        update_library_summary();
        configure_writes = inspection.configure_writes;
        configure_writes_authorized = false;
        SetWindowTextW(find_control(ProjectPathLabel), active_project_directory.c_str());
        SetWindowTextW(
            find_control(ProjectIdentityLabel), active_project_directory.filename().c_str());
        std::ifstream last_config(cpplivelab::workspace_directory(candidate) / L"last-config.txt");
        std::string configuration;
        last_config >> configuration;
        SendMessageW(find_control(ConfigurationSelector),
            CB_SETCURSEL,
            configuration == "Release" ? 1 : 0,
            0);
        reload_targets();
        RECT client{};
        GetClientRect(window_handle, &client);
        layout(client.right, client.bottom);
        cpplivelab::remember_project(candidate);
        reload_welcome_recents();
        SetWindowTextW(find_control(StatusLabel), L"Ready");
        append(L"Opened: " + active_project_directory.wstring() + L"\n");
        append(L"Read-only import; manager builds and metadata: " +
               cpplivelab::workspace_directory(candidate).wstring() + L"\n");
        for (const auto& asset : inspection.assets)
        {
            append(L"Asset directory: " + asset.wstring() + L"\n");
        }
        for (const auto& evidence : inspection.dependency_evidence)
        {
            append(L"Dependency evidence (not verified installed): " +
                   decode_console_text(evidence) + L"\n");
        }
        if (inspection.presets || inspection.user_presets)
        {
            append(L"CMake presets detected. Select a configure preset in Project Settings before "
                   L"Build.\n");
        }
        if (configure_writes)
        {
            log(cpplivelab::MessageKind::Warning,
                L"CMake file-writing commands detected. Explicit authorization is required before "
                L"configuration.");
        }
        start_watching();
        if (auto_run_preferences.enabled)
        {
            initial_auto_run = InitialAutoRun::Pending;
            log(cpplivelab::MessageKind::System,
                L"Auto Run enabled — preparing initial project build.");
        }
        refresh();
    }

    void browse()
    {
        if (const auto folder = cpplivelab::browse_path(
                window_handle, cpplivelab::BrowseKind::Folder, active_project_directory))
        {
            open(*folder);
        }
    }

    std::wstring selected_configuration() const
    {
        return SendMessageW(find_control(ConfigurationSelector), CB_GETCURSEL, 0, 0) == 1
                   ? L"Release"
                   : L"Debug";
    }

    fs::path build_directory() const
    {
        return cpplivelab::managed_build_directory(
            active_project_directory, selected_configuration(), project_options());
    }

    cpplivelab::ProjectOptions project_options() const
    {
        auto options =
            cpplivelab::load_project_options(active_project_directory, selected_configuration());
        const auto selected = read_control_text(find_control(ExecutableInput));
        for (const auto& target : cpplivelab::executable_targets(
                 active_project_directory, selected_configuration(), options))
        {
            if (target.name == selected)
            {
                options.target = selected;
            }
        }
        options.allow_configure_writes = configure_writes_authorized;
        return options;
    }

    void reload_targets()
    {
        const auto options =
            cpplivelab::load_project_options(active_project_directory, selected_configuration());
        const auto targets = cpplivelab::executable_targets(
            active_project_directory, selected_configuration(), options);
        SendMessageW(find_control(ExecutableInput), CB_RESETCONTENT, 0, 0);
        for (size_t index = 0; index < targets.size(); ++index)
        {
            SendMessageW(find_control(ExecutableInput),
                CB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(targets[index].name.c_str()));
            if (targets[index].name == options.target ||
                (options.target.empty() && targets.size() == 1))
            {
                SendMessageW(find_control(ExecutableInput), CB_SETCURSEL, index, 0);
            }
        }
    }

    void save_selection()
    {
        if (active_project_directory.empty() || operation_in_progress)
        {
            return;
        }
        cpplivelab::save_project_options(
            active_project_directory, selected_configuration(), project_options());
        if (initial_auto_run == InitialAutoRun::AwaitingTarget &&
            !read_control_text(find_control(ExecutableInput)).empty())
        {
            initial_auto_run = InitialAutoRun::Pending;
        }
    }

    void poll_initial_auto_run()
    {
        if (phase != Phase::Idle || operation_in_progress || replacement_building ||
            project_dialog_open || close_requested || restart_pending)
        {
            return;
        }
        if (!initial_switch_project.empty())
        {
            const auto next = initial_switch_project;
            initial_switch_project.clear();
            open(next);
            return;
        }
        if (initial_auto_run != InitialAutoRun::Pending)
        {
            return;
        }
        initial_auto_run = InitialAutoRun::Active; // Modal loops must not dispatch it twice.
        if (!automatic_launch_allowed())
        {
            initial_auto_run = InitialAutoRun::None;
            return;
        }
        try
        {
            if (!authorize_configuration())
            {
                initial_auto_run = InitialAutoRun::None;
                auto_build.cancel();
                set_auto_status(L"Initial Auto Run paused — configuration was not authorized");
                return;
            }
            auto options = project_options();
            const auto targets = cpplivelab::executable_targets(
                active_project_directory, selected_configuration(), options);
            const auto selected = std::find_if(targets.begin(),
                targets.end(),
                [&](const auto& target)
                {
                    return target.name == options.target;
                });
            if (initial_targets_verified && !targets.empty() && selected == targets.end())
            {
                std::wstring target;
                if (targets.size() == 1)
                {
                    target = targets.front().name;
                }
                else
                {
                    initial_auto_run = InitialAutoRun::AwaitingTarget;
                    std::vector<std::wstring> names;
                    for (const auto& item : targets)
                    {
                        names.push_back(item.name);
                    }
                    std::vector<cpplivelab::FormField> fields{{L"Executable target", L"", names}};
                    cpplivelab::FormOptions choice;
                    choice.theme = preferences.theme;
                    choice.note =
                        L"This project has multiple executables. Choose one for Auto Run. "
                        L"No executable will be guessed or launched without a selection.";
                    choice.validate = [](const auto& values)
                    {
                        return values[0].value.empty() ? L"Select an executable target." : L"";
                    };
                    if (!show_project_form(L"Choose Auto Run target", fields, choice))
                    {
                        auto_build.cancel();
                        set_auto_status(L"Initial Auto Run paused — select an executable target");
                        return;
                    }
                    target = fields[0].value;
                    initial_auto_run = InitialAutoRun::Active;
                }
                options.target = target;
                cpplivelab::save_project_options(
                    active_project_directory, selected_configuration(), options);
                reload_targets();
            }
            // Reuse the worker's configuration fingerprints and Ninja dependency checks.
            // Cold discovery stops before compilation if the graph is ambiguous.
            start(false, true, true);
        }
        catch (const std::exception& error)
        {
            initial_auto_run = InitialAutoRun::None;
            auto_build.cancel();
            log(cpplivelab::MessageKind::Error,
                L"Initial Auto Run could not start: " + decode_console_text(error.what()));
            SetWindowTextW(
                find_control(StatusLabel), L"Initial Auto Run failed — review Build & System");
        }
    }

    bool authorize_configuration()
    {
        if (!configure_writes || configure_writes_authorized)
        {
            return true;
        }
        std::vector<cpplivelab::FormField> fields;
        cpplivelab::FormOptions options;
        options.theme = preferences.theme;
        options.note = L"This project's CMake contains file-writing commands. It may create or "
                       L"change project files even with an isolated build folder. Review "
                       L"CMakeLists.txt first. Authorize configuration for this session?";
        options.accept_label = L"Authorize";
        options.cancel_default = true;
        if (!show_project_form(L"Project writes require permission", fields, options))
        {
            return false;
        }
        configure_writes_authorized = true;
        return true;
    }

    bool show_project_form(const std::wstring& title,
        std::vector<cpplivelab::FormField>& fields,
        cpplivelab::FormOptions& options,
        bool in_page = false)
    {
        // A modal dialog pumps Windows messages. Hold automatic dispatch until
        // its settings are accepted/cancelled so an operation cannot start halfway
        // through changing project identity or build configuration.
        struct DialogGuard
        {
            bool& flag;
            bool previous;

            ~DialogGuard()
            {
                flag = previous;
            }
        } guard{project_dialog_open, project_dialog_open};

        project_dialog_open = true;
        if (!in_page)
        {
            return cpplivelab::project_form(window_handle, title, fields, options);
        }
        const auto focus = GetFocus();
        inline_abandoned = false;
        inline_active = true;
        options.inline_host = find_control(ShellInlineHost);
        const auto relayout = [&]
        {
            RECT client{};
            GetClientRect(window_handle, &client);
            layout(client.right, client.bottom);
        };
        relayout();
        const auto finish = [&]
        {
            options.inline_host = nullptr;
            inline_active = false;
            relayout();
            if (IsWindow(focus) && IsWindowVisible(focus) && IsWindowEnabled(focus))
            {
                SetFocus(focus);
            }
            if (const auto next = std::exchange(inline_next_message, std::nullopt))
            {
                PostMessageW(window_handle, next->message, next->wParam, next->lParam);
            }
        };
        bool accepted;
        try
        {
            accepted = cpplivelab::project_form(window_handle, title, fields, options);
        }
        catch (...)
        {
            finish();
            throw;
        }
        finish();
        return accepted;
    }

    bool leave_inline(UINT message, WPARAM wparam = 0, LPARAM lparam = 0)
    {
        if (!inline_active)
        {
            return false;
        }
        if (const auto child = GetWindow(find_control(ShellInlineHost), GW_CHILD))
        {
            if (cpplivelab::request_inline_form_close(child))
            {
                inline_abandoned = true;
                inline_next_message = MSG{window_handle, message, wparam, lparam};
            }
            return true;
        }
        return false;
    }

    bool show_project_form(const std::wstring& title, std::vector<cpplivelab::FormField>& fields)
    {
        cpplivelab::FormOptions options;
        options.theme = preferences.theme;
        return show_project_form(title, fields, options);
    }

    void new_project()
    {
        wchar_t documents[32768]{};
        GetEnvironmentVariableW(L"USERPROFILE", documents, 32768);
        std::vector<cpplivelab::FormField> fields{
            {L"Project name (also the new folder name)", L"MyCppProject"},
            {L"Parent folder", (fs::path(documents) / L"Documents").wstring()},
            {L"Project template",
                L"Empty C++ Project",
                {L"Empty C++ Project", L"Multi-file C++ Project"}}};
        fields[1].browse = cpplivelab::BrowseKind::Folder;
        fields[1].help = L"A new project folder is created inside this directory. Choose an "
                         L"existing parent; existing projects are never overwritten.";
        fields[2].help = L"Empty: one C++20 source. Multi-file: src/ implementation and include/ "
                         L"declaration, compiled as a standard CMake project.";
        cpplivelab::FormOptions options;
        options.theme = preferences.theme;
        options.note = L"Creates a NEW folder inside the selected parent. Libraries are optional; "
                       L"installation is verified only by a successful build.";
        options.library_picker = true;
        options.add_github_library = [&](HWND owner)
        {
            select_console(false);
            return cpplivelab::github_library_dialog(owner,
                preferences.theme,
                application_directory() / L"EasierCpp.exe",
                [&](const auto& text)
                {
                    system_console.append(cpplivelab::classify_build_line(text), text);
                    update_console_badges();
                });
        };
        options.accept_label = L"Create";
        options.validate = [](const auto& values) -> std::wstring
        {
            if (values[0].value.empty() ||
                fs::path(values[0].value).filename().wstring() != values[0].value ||
                values[0].value.find_first_of(L"<>:\"/\\|?*;\r\n") != std::wstring::npos)
            {
                return L"Enter one valid new folder name, not a path.";
            }
            if (!fs::is_directory(values[1].value))
            {
                return L"Choose an existing parent folder.";
            }
            if (fs::exists(fs::path(values[1].value) / values[0].value))
            {
                return L"That project folder already exists. Choose a new name.";
            }
            return L"";
        };
        if (show_project_form(L"New Project",
                fields,
                options,
                shell_page == NavigationHome || shell_page == NavigationProject))
        {
            std::string libraries;
            for (const auto& id : options.libraries)
            {
                if (!libraries.empty())
                {
                    libraries += ',';
                }
                libraries += id;
            }
            const auto destination = fs::path(fields[1].value) / fields[0].value;
            cpplivelab::create_managed_project(destination,
                fields[2].value == L"Multi-file C++ Project" ? "multi" : "empty",
                libraries);
            open(destination);
        }
    }

    void recent_dialog()
    {
        std::vector<cpplivelab::FormField> fields;
        cpplivelab::FormOptions options;
        options.theme = preferences.theme;
        options.note =
            L"Select a project to open. Removing an entry only forgets it; no files are deleted.";
        options.recents = cpplivelab::recent_projects();
        options.recent_list = true;
        options.accept_label = L"Open";
        options.forget_recent = [](const auto& path)
        {
            cpplivelab::remember_project(path, true);
        };
        if (show_project_form(L"Recent Projects", fields, options, shell_page == NavigationProject))
        {
            open(options.selected_recent);
        }
        reload_welcome_recents();
    }

    void project_settings()
    {
        if (active_project_directory.empty())
        {
            throw std::runtime_error("Open a project first.");
        }
        const auto original = project_options();
        std::vector<cpplivelab::FormField> fields{
            {L"Program working directory", original.working_directory.wstring()},
            {L"CMake configure preset", original.preset},
            {L"vcpkg root", original.vcpkg.wstring()},
            {L"Program arguments (one argument per line)", L"", {}, true}};
        fields[3].advanced = true;
        for (const auto& argument : original.arguments)
        {
            fields[3].value += argument + L"\n";
        }
        fields[0].browse = cpplivelab::BrowseKind::Folder;
        fields[0].help = L"Relative asset paths are resolved here. Blank uses the project root. "
                         L"Example: Assets/Textures/player.png.";
        fields[1].advanced = true;
        fields[1].help = L"Name from your existing CMakePresets.json. Blank means no preset; '-' "
                         L"explicitly bypasses presets. No preset files are edited.";
        fields[2].advanced = true;
        fields[2].browse = cpplivelab::BrowseKind::Folder;
        fields[2].help =
            L"An existing vcpkg installation containing scripts/buildsystems/vcpkg.cmake. Leave "
            L"blank when your preset already chooses a toolchain.";
        cpplivelab::FormOptions options;
        options.theme = preferences.theme;
        options.note = L"Applies only to this project and the selected " +
                       selected_configuration() +
                       L" configuration. Saved outside the source folder.";
        options.validate = [](const auto& values) -> std::wstring
        {
            if (!values[0].value.empty() && !fs::is_directory(values[0].value))
            {
                return L"The program working directory does not exist.";
            }
            return L"";
        };
        if (show_project_form(
                L"Project Settings", fields, options, shell_page == NavigationProject))
        {
            auto changed = original;
            changed.working_directory = fields[0].value;
            changed.preset = fields[1].value;
            changed.vcpkg = fields[2].value;
            changed.arguments.clear();
            std::wistringstream argument_lines(fields[3].value);
            for (std::wstring argument; std::getline(argument_lines, argument);)
            {
                if (!argument.empty() && argument.back() == L'\r')
                {
                    argument.pop_back();
                }
                changed.arguments.push_back(argument);
            }
            if (changed.preset != original.preset || changed.vcpkg != original.vcpkg)
            {
                changed.target.clear();
            }
            cpplivelab::save_project_options(
                active_project_directory, selected_configuration(), changed);
            reload_targets();
        }
    }

    void editor_settings()
    {
        auto editor = cpplivelab::load_editor();
        std::wstring arguments;
        for (const auto& argument : editor.arguments)
        {
            arguments += argument + L"\r\n";
        }
        std::vector<cpplivelab::FormField> fields{
            {L"Preferred editor",
                editor.choice,
                {L"Visual Studio 2022", L"Visual Studio Code", L"CLion", L"Custom"}},
            {L"Executable override", editor.executable.wstring()},
            {L"Custom arguments (one per line)", arguments, {}, true}};
        fields[1].advanced = true;
        fields[1].browse = cpplivelab::BrowseKind::Executable;
        fields[1].help = L"Normally detected automatically. Override only for a nonstandard "
                         L"installation or a custom editor; select its .exe file.";
        fields[2].advanced = true;
        fields[2].help = L"Each line is one argument. {project} becomes the full folder as one "
                         L"argument. Without it, the project folder is appended.";
        cpplivelab::FormOptions options;
        options.theme = preferences.theme;
        options.note = L"Editor and compiler are independent. Ordinary editor choices use "
                       L"automatic detection.";
        options.custom_editor_field = 0;
        options.expanded = !editor.executable.empty() || !editor.arguments.empty();
        if (show_project_form(
                L"Preferred Editor", fields, options, shell_page == NavigationSettings))
        {
            editor.choice = fields[0].value;
            editor.executable = fields[1].value;
            editor.arguments.clear();
            std::wistringstream lines(fields[2].value);
            for (std::wstring line; std::getline(lines, line);)
            {
                if (!line.empty() && line.back() == L'\r')
                {
                    line.pop_back();
                }
                if (!line.empty())
                {
                    editor.arguments.push_back(line);
                }
            }
            cpplivelab::save_editor(editor);
            const auto executable = editor.executable.empty()
                                        ? cpplivelab::discover_editor(editor.choice)
                                        : editor.executable;
            append(fs::is_regular_file(executable)
                       ? L"Preferred editor located.\n"
                       : L"Editor not detected. Use Advanced Options to choose its executable.\n");
        }
    }

    void update_library_summary()
    {
        std::wstring text = L"Libraries: ";
        const auto records = cpplivelab::project_libraries(active_project_directory);
        SendDlgItemMessageW(window_handle, ShellLibraryList, LB_RESETCONTENT, 0, 0);
        for (const auto& record : records)
        {
            if (text != L"Libraries: ")
            {
                text += L", ";
            }
            text += decode_console_text(record.name);
            const auto row =
                decode_console_text(record.name + "  |  " + record.version + "  |  Managed");
            SendDlgItemMessageW(window_handle,
                ShellLibraryList,
                LB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(row.c_str()));
        }
        if (records.empty())
        {
            text += L"none managed (manual dependencies are preserved)";
        }
        SetWindowTextW(find_control(LibrarySummary), text.c_str());
    }

    void manage_libraries(bool adding = false)
    {
        struct Guard
        {
            bool& flag;
            bool previous;

            ~Guard()
            {
                flag = previous;
            }
        } guard{project_dialog_open, project_dialog_open};

        project_dialog_open = true;
        cpplivelab::LibraryManagerContext context;
        bool left_library_page = false;
        context.owner = window_handle;
        context.theme = preferences.theme;
        context.project = active_project_directory;
        context.cli = application_directory() / L"EasierCpp.exe";
        context.configuration = selected_configuration();
        context.options = project_options();
        context.output = [&](const auto& text)
        {
            if (shell_page != NavigationLibraries)
            {
                reveal_system_output();
            }
            system_console.append(cpplivelab::classify_build_line(text), text);
            update_console_badges();
        };
        context.changed = [&]
        {
            if (!active_project_directory.empty())
            {
                freshness.invalidate(current_identity());
                reload_targets();
                update_library_summary();
            }
        };
        context.busy = [&]
        {
            return operation_in_progress || replacement_building || tests_active;
        };
        context.restarting = [&]
        {
            return restart_pending || close_requested || left_library_page;
        };
        if (shell_page == NavigationLibraries)
        {
            context.present_editor = [&](const auto& title, auto& fields, auto& options)
            {
                const bool accepted = show_project_form(title, fields, options, true);
                left_library_page = inline_abandoned;
                return accepted;
            };
        }
        if (adding)
        {
            cpplivelab::add_project_libraries_dialog(context);
        }
        else
        {
            cpplivelab::manage_libraries_dialog(context);
        }
    }

    cpplivelab::BuildIdentity current_identity() const
    {
        auto executable = build_directory() / L"app.exe";
        const auto options = project_options();
        for (const auto& target : cpplivelab::executable_targets(
                 active_project_directory, selected_configuration(), options))
        {
            if (target.name == options.target ||
                (options.target.empty() &&
                    cpplivelab::executable_targets(
                        active_project_directory, selected_configuration(), options)
                            .size() == 1))
            {
                executable = target.artifact;
            }
        }
        return {active_project_directory,
            selected_configuration(),
            fs::weakly_canonical(executable),
            build_directory()};
    }

    bool collect_environment()
    {
        if (environment_future.valid() &&
            environment_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
        {
            auto prepared = environment_future.get();
            if (cpplivelab::BuildProfile::enabled())
            {
                log(cpplivelab::MessageKind::System,
                    L"EASIERCPP_PROFILE gui_tool_discovery_ms=" +
                        std::to_wstring(prepared.discovery_ms));
                log(cpplivelab::MessageKind::System,
                    L"EASIERCPP_PROFILE gui_sdk_environment_ms=" +
                        std::to_wstring(prepared.preparation_ms));
            }
            program_environment = std::move(prepared.environment);
            ninja_executable = std::move(prepared.ninja);
            environment_ready = true;
            fast_runtime = false;
        }
        return environment_ready;
    }

    void request_environment()
    {
        if (!environment_ready && !environment_future.valid())
        {
#ifdef CPPLIVELAB_GUI_TEST
            ++environment_preparations;
#endif
            // Developer setup can invoke utilities. Run it once in a worker and
            // retain its result only in memory, never in a file or diagnostic log.
            environment_future = std::async(std::launch::async,
                []
                {
                    const auto started = std::chrono::steady_clock::now();
                    const auto tools =
                        cpplivelab::discover_toolchain(cpplivelab::system_discovery_inputs());
                    PreparedTools prepared;
                    const auto discovered = std::chrono::steady_clock::now();
                    prepared.discovery_ms =
                        std::chrono::duration<double, std::milli>(discovered - started).count();
                    for (const auto& tool : tools.tools)
                    {
                        if (tool.name == "ninja")
                        {
                            prepared.ninja = tool.installed_path;
                        }
                    }
                    if (!tools.developer_command.empty())
                    {
                        prepared.environment =
                            cpplivelab::developer_environment(tools.developer_command);
                    }
                    prepared.preparation_ms = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - discovered)
                                                  .count();
                    return prepared;
                });
        }
    }

    void start(bool run,
        bool automatic = false,
        bool incremental = false,
        bool recovery = false,
        bool checked = false)
    {
        if (operation_in_progress || replacement_building || active_project_directory.empty())
        {
            return;
        }
        if (!automatic)
        {
            initial_auto_run = InitialAutoRun::None;
        }
        const bool check_cached = !checked && !recovery && !environment_ready &&
                                  (run || initial_auto_run == InitialAutoRun::Active) &&
                                  fs::is_regular_file(build_directory() / L"CMakeCache.txt");
        if (!check_cached && !collect_environment())
        {
            // Prepare once in the GUI; the existing validated marker lets the
            // owned worker reuse this block instead of running VsDevCmd again.
            pending_build = PendingBuild{run, automatic, incremental, recovery};
            request_environment();
            operation_in_progress = true;
            phase = Phase::PreparingBuild;
            stop_requested = false;
            run_requested = run_requested || run;
            SetWindowTextW(find_control(StatusLabel), L"Preparing compiler environment");
            refresh();
            return;
        }
        if (run)
        {
            auto_launch_armed = true;
            run_requested = true;
            if (!check_cached)
            {
                request_environment();
            }
            select_console(true);
            bool can_reuse = false;
            try
            {
                can_reuse = !check_cached && freshness.can_probe(current_identity()) &&
                            fs::is_regular_file(build_directory() / L"build.ninja");
            }
            catch (const std::exception&)
            {
                freshness.invalidate(current_identity());
            }
            if (can_reuse)
            {
                operation_in_progress = true;
                should_run_program = true;
                program_started = false;
                stop_requested = false;
                probe_pending = true;
                phase = Phase::PreparingProgram;
                SetWindowTextW(find_control(StatusLabel), L"Checking build dependencies");
                refresh();
                return;
            }
        }
        else if (!automatic)
        {
            run_requested = false;
        }
        const auto cli = application_directory() / L"EasierCpp.exe";
        if (!fs::is_regular_file(cli))
        {
            throw std::runtime_error("Keep EasierCpp.exe beside EasierCpp-Gui.exe.");
        }
        if (!automatic && auto_build.enabled())
        {
            watcher.poll();
            watcher.consume_changes();
            auto_build.cancel();
        }
        std::vector<std::wstring> arguments{check_cached ? L"manage-probe"
                                            : recovery   ? L"manage-rebuild"
                                                         : L"manage-build",
            active_project_directory.wstring(),
            L"--config",
            selected_configuration()};
        if (recovery)
        {
            arguments.push_back(L"--approve-recovery");
        }
        auto options = project_options();
        if (initial_auto_run == InitialAutoRun::Active && !initial_targets_verified)
        {
            // A cached sole target can be stale after an external CMake edit.
            // Only a saved user selection or the freshly configured graph may choose it.
            options.target =
                cpplivelab::load_project_options(active_project_directory, selected_configuration())
                    .target;
        }
        if (!options.target.empty())
        {
            arguments.insert(arguments.end(), {L"--target", options.target});
        }
        if (configure_writes_authorized)
        {
            arguments.push_back(L"--allow-configure-writes");
        }
        if (incremental)
        {
            arguments.push_back(L"--incremental");
        }
        if (initial_auto_run == InitialAutoRun::Active)
        {
            arguments.push_back(L"--require-target");
        }
        building_identity = current_identity();
        building_sources_known = false;
        try
        {
            building_sources = freshness.begin(building_identity);
            building_sources_known = true;
        }
        catch (const std::exception& error)
        {
            freshness.invalidate(building_identity);
            log(cpplivelab::MessageKind::Warning,
                L"Freshness cannot be recorded: " + decode_console_text(error.what()));
        }
        if (run && !check_cached)
        {
            request_environment();
            select_console(true);
        }
        log(cpplivelab::MessageKind::Build,
            (build_id = L"Build #" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                        std::to_wstring(++build_sequence)) +
                L": " + std::wstring(automatic ? L"Automatic " : L"Manual ") +
                selected_configuration() + L" build started");
        session.start_worker(cli,
            arguments,
            active_project_directory,
            environment_ready ? program_environment : cpplivelab::Environment{});
        log(cpplivelab::MessageKind::System,
            build_id + L": worker PID " + std::to_wstring(session.process_id()));
        last_build_activity = GetTickCount64();
        stall_reported = false;
        build_root_observed = false;
        auto_build.begin_build();
        automatic_operation = automatic;
        operation_in_progress = true;
        should_run_program = run_requested || automatic_launch_allowed();
        automatic_program_pending = false;
        if (should_run_program && !check_cached)
        {
            request_environment();
        }
        probe_pending = false;
        program_started = false;
        input_closed = false;
        stop_requested = false;
        phase = Phase::Building;
        sdk_probe_active = check_cached;
        if (!check_cached)
        {
            fast_runtime = false;
        }
        output_decoder.reset();
        pending_build_line.clear();
        build_diagnostics.clear();
        SetWindowTextW(find_control(StatusLabel), L"Building");
        refresh();
    }

    void finish_operation(const std::wstring& status)
    {
        track_placement(true);
        if (!session.running() && !session.output_pending())
        {
            session.release_completed();
        }
        program_lease.reset();
        graceful_stop_pending = false;
        if (!replacement_stopping)
        {
            previous_windows.clear();
            restore_started = 0;
        }
        phase = Phase::Idle;
        program_started = false;
        operation_in_progress = candidate_pending;
        stop_requested = false;
        automatic_program_pending = false;
        SetWindowTextW(find_control(StatusLabel), status.c_str());
        refresh();
    }

    void begin_program()
    {
        if (!fast_runtime && !collect_environment())
        {
            if (!environment_future.valid() ||
                environment_future.wait_for(std::chrono::milliseconds(0)) !=
                    std::future_status::ready)
            {
                return;
            }
            collect_environment();
        }
        if (!launch_latest && !freshness.can_probe(current_identity()))
        {
            finish_operation(L"Build Pending - inputs changed");
            start(true, false, true);
            return;
        }
        if (probe_pending)
        {
            if (!fs::is_regular_file(ninja_executable))
            {
                freshness.invalidate(current_identity());
                finish_operation(L"Build Pending - dependency check unavailable");
                start(true, false, true);
                return;
            }
            auto arguments = std::vector<std::wstring>{L"-n"};
            const auto configuration_file = L"build-" + selected_configuration() + L".ninja";
            if (fs::is_regular_file(build_directory() / configuration_file))
            {
                arguments.insert(arguments.end(), {L"-f", configuration_file});
            }
            const auto options = project_options();
            if (!options.target.empty())
            {
                arguments.push_back(options.target);
            }
            session.start_program(
                ninja_executable, arguments, build_directory(), program_environment);
            phase = Phase::Checking;
            probe_output.clear();
            SetWindowTextW(find_control(StatusLabel), L"Checking build dependencies");
            refresh();
            return;
        }
        const auto executable = current_identity().executable;
        if (read_control_text(find_control(ExecutableInput)).empty())
        {
            throw std::runtime_error("Choose an executable target before Run.");
        }
        if (!fs::is_regular_file(executable))
        {
            throw std::runtime_error(
                "Build completed but the selected File API artifact is missing. "
                "Review Build Errors and the selected target; Run was refused.");
        }
        program_lease = std::make_unique<cpplivelab::BuildLease>(build_directory());
        const auto options = project_options();
        const auto working = options.working_directory.empty() ? active_project_directory
                                                               : options.working_directory;
#ifdef CPPLIVELAB_GUI_TEST
        if (auto_run_trace)
        {
            auto_run_trace("startup");
        }
#endif
        session.start_program(executable,
            options.arguments,
            working,
            cpplivelab::runtime_environment(program_environment, build_directory()));
#ifdef CPPLIVELAB_GUI_TEST
        if (auto_run_trace)
        {
            auto_run_trace("started");
        }
#endif
        launch_latest = false;
        running_configuration = selected_configuration();
        running_target = options.target;
        placement_sample.clear();
        if (!auto_run_preferences.restore_windows)
        {
            previous_windows.clear();
        }
        else if (previous_windows.empty())
        {
            previous_windows = placement_memory.recall(active_project_directory,
                running_configuration,
                running_target,
                auto_run_preferences.placement_memory);
        }
        restore_started = previous_windows.empty() ? 0 : GetTickCount64();
        restore_position = auto_run_preferences.keep_window_position;
        restore_size = auto_run_preferences.keep_window_size;
        restored_windows.clear();
        program_decoder.reset();
        error_decoder.reset();
        error_line_start = true;
        program_started = true;
        if (initial_auto_run == InitialAutoRun::Active)
        {
            initial_auto_run = InitialAutoRun::None;
            log(cpplivelab::MessageKind::System, L"Initial Auto Run completed.");
        }
        input_closed = false;
        run_requested = false;
        phase = Phase::Running;
        log(cpplivelab::MessageKind::System, L"Program started: " + executable.wstring());
        SetWindowTextW(find_control(StatusLabel), L"Running - input below");
        // Do not SetFocus: asynchronous program startup must not steal focus.
        refresh();
    }

    void append_program_output(const std::wstring& text, bool error)
    {
        std::wstring rendered;
        for (wchar_t character : text)
        {
            if (error && error_line_start && character != L'\r' && character != L'\n')
            {
                rendered += L"[stderr] ";
            }
            rendered += character;
            if (error)
            {
                error_line_start = character == L'\r' || character == L'\n';
            }
        }
        program_console.append(
            error ? cpplivelab::MessageKind::ProgramError : cpplivelab::MessageKind::ProgramOutput,
            rendered);
    }

    void reveal_system_output()
    {
        welcome_output = true;
        select_console(false);
        navigate_to(shell_page == NavigationTools      ? NavigationTools
                    : active_project_directory.empty() ? NavigationHome
                                                       : NavigationWorkspace);
    }

#ifndef EASIERCPP_PUBLIC_GUI
    void run_tests_dialog()
    {
        reveal_system_output();
        if (tests_active)
        {
            append(L"A test session is already running. Tools > Cancel Running Tests stops only "
                   L"that session.");
            return;
        }
        std::vector<cpplivelab::FormField> fields{
            {L"Test suite", L"Quick", {L"Quick", L"Full"}}, {L"Parallel workers (1-16)", L"4"}};
        fields[1].help = L"Workers run independent CTest cases in parallel. Four is a useful "
                         L"default. More workers use more memory.";
        cpplivelab::FormOptions options;
        options.theme = preferences.theme;
        options.accept_label = L"Run Tests";
        options.note = L"Tests use the manager source checkout and a separate build directory. "
                       L"Program Console and the running user program are preserved.";
        options.validate = [](const auto& values) -> std::wstring
        {
            try
            {
                size_t used = 0;
                const auto count = std::stoi(values[1].value, &used);
                if (used == values[1].value.size() && count >= 1 && count <= 16)
                {
                    return {};
                }
            }
            catch (...)
            {
            }
            return L"Worker count must be a whole number from 1 to 16.";
        };
        if (show_project_form(
                L"Run EasierCpp Tests", fields, options, shell_page == NavigationTools))
        {
            const auto workers = std::stoi(fields[1].value);
            const auto command = cpplivelab::test_command(fields[0].value, workers);
            test_decoder.reset();
            tests_cancelled = false;
            test_session.start_worker(command.executable, command.arguments, command.repository);
            tests_active = true;
            update_test_actions();
            tests_started = GetTickCount64();
            append(L"Starting " + fields[0].value + L" tests with " + std::to_wstring(workers) +
                   L" workers.");
        }
    }

    void poll_tests()
    {
        if (!tests_active)
        {
            return;
        }
        const auto bytes = test_session.poll();
        const auto output = test_decoder.append(bytes);
        if (!output.empty())
        {
            system_console.append(cpplivelab::classify_build_line(output), output);
            update_console_badges();
        }
        if (test_session.root_exited())
        {
            test_session.finish_build();
        }
        if (!test_session.running() && !test_session.output_pending())
        {
            const auto tail = test_decoder.append({}, true);
            if (!tail.empty())
            {
                system_console.append(cpplivelab::MessageKind::System, tail);
            }
            const auto code = test_session.exit_code();
            log(tests_cancelled ? cpplivelab::MessageKind::Warning
                : code == 0     ? cpplivelab::MessageKind::Success
                                : cpplivelab::MessageKind::Error,
                (tests_cancelled ? L"Tests cancelled" : L"Tests completed") +
                    std::wstring(L"; exit ") + std::to_wstring(code) + L"; elapsed " +
                    std::to_wstring((GetTickCount64() - tests_started) / 1000.0) + L" seconds.");
            test_session.release_completed();
            tests_active = false;
            update_test_actions();
        }
    }

#endif

    void update_test_actions()
    {
#ifndef EASIERCPP_PUBLIC_GUI
        EnableWindow(find_control(TestRunnerMenu), !tests_active);
        EnableWindow(find_control(CancelTestsMenu), tests_active);
#endif
    }

    void request_restart()
    {
        if (restart_pending || restart_launch || restart_confirmation)
        {
            return;
        }
        const bool busy = operation_in_progress || replacement_building || tests_active ||
                          project_dialog_open || cpplivelab::library_task_active();
        if (busy)
        {
            restart_confirmation = true;
            cpplivelab::FormOptions options;
            options.theme = preferences.theme;
            options.cancel_default = true;
            options.accept_label = L"Restart Safely";
            options.note =
                L"Restart stops only EasierCpp-owned builds, tests and programs. "
                L"Downloads are cancelled safely. Cache deletion and metadata writes finish at "
                L"a safe boundary. Finish or cancel any open settings dialog. Your editor stays "
                L"open.";
            std::vector<cpplivelab::FormField> fields;
            bool confirmed = false;
            try
            {
                confirmed = show_project_form(L"Restart EasierCpp", fields, options);
            }
            catch (...)
            {
                restart_confirmation = false;
                throw;
            }
            restart_confirmation = false;
            if (!confirmed)
            {
                return;
            }
        }
        restart_pending = true;
        run_requested = false;
        auto_build.cancel();
        if (cpplivelab::library_task_active())
        {
            cpplivelab::cancel_library_task();
        }
#ifndef EASIERCPP_PUBLIC_GUI
        if (tests_active)
        {
            tests_cancelled = true;
            test_session.stop();
        }
#endif
        if (operation_in_progress || replacement_building)
        {
            stop();
        }
        SetWindowTextW(find_control(StatusLabel), L"Restart pending - waiting for a safe boundary");
    }

    void poll_restart()
    {
        if (restart_launch)
        {
            const auto progress = restart_launch->poll();
            if (progress == cpplivelab::RestartProgress::Ready)
            {
                try
                {
                    restart_launch->commit();
                    DestroyWindow(window_handle);
                }
                catch (const std::exception& error)
                {
                    restart_launch.reset();
                    EnableWindow(find_control(RestartButton), TRUE);
                    log(cpplivelab::MessageKind::Error, decode_console_text(error.what()));
                    if (auto_build.enabled() && !active_project_directory.empty())
                    {
                        watcher.start(active_project_directory);
                    }
                }
            }
            else if (progress == cpplivelab::RestartProgress::Failed)
            {
                restart_launch.reset();
                restart_pending = false;
                EnableWindow(find_control(RestartButton), TRUE);
                if (auto_build.enabled() && !active_project_directory.empty())
                {
                    watcher.start(active_project_directory);
                }
                log(cpplivelab::MessageKind::Error,
                    L"Replacement startup failed. The current application remains open and "
                    L"usable.");
                SetWindowTextW(
                    find_control(StatusLabel), L"Restart failed - current application retained");
            }
            return;
        }
        if (!restart_pending || operation_in_progress || replacement_building || tests_active ||
            project_dialog_open || cpplivelab::library_task_active() ||
            (environment_future.valid() && environment_future.wait_for(std::chrono::milliseconds(
                                               0)) != std::future_status::ready))
        {
            return;
        }
        restart_pending = false;
        cpplivelab::RestartState state;
        state.project = active_project_directory;
        state.preference_store = preference_store;
        GetWindowPlacement(window_handle, &state.placement);
        if (!cpplivelab::save_preferences(preferences, preference_store))
        {
            throw std::runtime_error("Preferences could not be saved; restart was not attempted.");
        }
        if (!active_project_directory.empty())
        {
            save_selection();
        }
        try
        {
            restart_launch = std::make_unique<cpplivelab::RestartLaunch>(
                application_directory() / L"EasierCpp-Gui.exe", state, restart_instance_name);
            watcher.stop();
            auto_build.cancel();
            EnableWindow(find_control(RestartButton), FALSE);
            SetWindowTextW(find_control(StatusLabel),
                L"Preparing replacement - current app retained until ready");
        }
        catch (const std::exception& error)
        {
            log(cpplivelab::MessageKind::Error, decode_console_text(error.what()));
            SetWindowTextW(
                find_control(StatusLabel), L"Restart failed - current application retained");
        }
    }

    void tick()
    {
#ifndef EASIERCPP_PUBLIC_GUI
        poll_tests();
#endif
        poll_restart();
        if (!IsWindow(window_handle) || restart_launch)
        {
            return;
        }
        if (project_dialog_open && !operation_in_progress)
        {
            return;
        }
        track_placement();
        poll_initial_auto_run();
        poll_watcher();
        poll_auto_run();
        if (phase == Phase::PreparingBuild)
        {
            if (stop_requested || close_requested ||
                (pending_build && pending_build->automatic && !auto_build.enabled() &&
                    !pending_build->run))
            {
                pending_build.reset();
                run_requested = false;
                finish_operation(L"Compiler preparation cancelled");
            }
            else
            {
                try
                {
                    if (collect_environment() && pending_build)
                    {
                        const auto action = *pending_build;
                        pending_build.reset();
                        operation_in_progress = false;
                        phase = Phase::Idle;
                        start(action.run,
                            action.automatic,
                            action.incremental,
                            action.recovery,
                            true);
                    }
                }
                catch (const std::exception& error)
                {
                    pending_build.reset();
                    run_requested = false;
                    initial_auto_run = InitialAutoRun::None;
                    auto_build.cancel();
                    log(cpplivelab::MessageKind::Error, decode_console_text(error.what()));
                    finish_operation(L"Compiler environment preparation failed");
                }
            }
        }
        if (phase == Phase::Idle && run_requested && !auto_build.pending() && !close_requested)
        {
            start(true);
        }
        if (phase == Phase::Checking)
        {
            const auto bytes = session.poll();
            if (probe_output.size() < 131072)
            {
                probe_output += bytes;
            }
            if (session.root_exited())
            {
                session.finish_build();
            }
            if (!session.running() && !session.output_pending())
            {
                const auto result = session.exit_code();
                session.release_completed();
                if (stop_requested)
                {
                    run_requested = false;
                    auto_build.cancel();
                    log(cpplivelab::MessageKind::Warning,
                        L"Run dependency check cancelled by user.");
                    finish_operation(L"Build Cancelled");
                }
                else if (cpplivelab::ninja_reports_current(probe_output, result) &&
                         freshness.can_probe(current_identity()))
                {
                    log(cpplivelab::MessageKind::System,
                        L"Build is up to date - starting application.");
                    probe_pending = false;
                    phase = Phase::PreparingProgram;
                }
                else
                {
                    freshness.invalidate(current_identity());
                    finish_operation(L"Build Pending - dependency changes");
                    start(true, false, true);
                }
            }
        }
        if (phase == Phase::Building)
        {
            append_build_output(output_decoder.append(session.poll()));
            if (session.root_exited() && !build_root_observed)
            {
                build_root_observed = true;
                std::wstring helpers;
                for (const auto id : session.active_process_ids())
                {
                    helpers += L" " + std::to_wstring(id);
                }
                log(cpplivelab::MessageKind::System,
                    build_id + L": worker exited " + std::to_wstring(session.exit_code()) +
                        (helpers.empty() ? L"; draining output"
                                         : L"; closing owned helper PIDs:" + helpers));
                session.finish_build();
            }
            if (!build_root_observed && !stall_reported &&
                GetTickCount64() - last_build_activity > 30000)
            {
                stall_reported = true;
                std::wstring processes;
                for (const auto id : session.active_process_ids())
                {
                    processes += L" " + std::to_wstring(id);
                }
                log(cpplivelab::MessageKind::Warning,
                    build_id + L": no output for 30 seconds; owned PIDs:" + processes +
                        L". Compilation may still be working. Stop is available; no automatic "
                        L"timeout kill.");
            }
            if (!session.running() && !session.output_pending())
            {
                append_build_output(output_decoder.append(session.poll(), true), true);
                const int result = session.exit_code();
                const bool was_sdk_probe = std::exchange(sdk_probe_active, false);
                const bool blocked = result == cpplivelab::build_deferred_exit_code;
                if (stop_requested)
                {
                    run_requested = false;
                    auto_build.cancel();
                    if (auto_build.enabled())
                    {
                        try
                        {
                            watcher.start(active_project_directory);
                        }
                        catch (const std::exception& error)
                        {
                            watcher.stop();
                            disable_automatic_work();
                            log(cpplivelab::MessageKind::Warning,
                                L"Watcher could not restart after cancellation: " +
                                    decode_console_text(error.what()));
                        }
                    }
                    log(cpplivelab::MessageKind::Warning,
                        build_id + L": Build cancelled by user. Pending automatic work cancelled; "
                                   L"save again or Build to resume.");
                    finish_operation(L"Build Cancelled");
                }
                else if (was_sdk_probe && result != 0 && !blocked)
                {
                    const bool automatic = automatic_operation;
                    const bool run = run_requested;
                    finish_operation(L"Build required - preparing compiler environment");
                    start(run, automatic, true, false, true);
                }
                else if (blocked)
                {
                    if (auto_build.enabled())
                    {
                        auto_build.defer(GetTickCount64());
                    }
                    else
                    {
                        run_requested = false;
                    }
                    log(cpplivelab::MessageKind::Warning,
                        build_id + L": Build Deferred - active executable/configuration preserved");
                    finish_operation(L"Build Deferred");
                }
                else if (result == cpplivelab::target_selection_exit_code &&
                         initial_auto_run == InitialAutoRun::Active)
                {
                    run_requested = false; // A queued Run must not bypass the target chooser.
                    reload_targets();
                    initial_targets_verified = true;
                    auto_build.cancel();
                    finish_operation(L"Initial Auto Run — choose an executable target");
                    const auto targets = cpplivelab::executable_targets(
                        active_project_directory, selected_configuration(), project_options());
                    initial_auto_run =
                        targets.empty() ? InitialAutoRun::None : InitialAutoRun::Pending;
                    if (targets.empty())
                    {
                        log(cpplivelab::MessageKind::Warning,
                            L"Initial Auto Run needs an executable target; this configuration "
                            L"exports none.");
                    }
                }
                else if (result == cpplivelab::artifact_missing_exit_code)
                {
                    initial_auto_run = InitialAutoRun::None;
                    run_requested = false;
                    log(cpplivelab::MessageKind::Error,
                        build_id +
                            L": Compilation completed, executable missing. "
                            L"Tools > Review Build Errors provides target/configuration actions.");
                    finish_operation(L"Executable Missing");
                }
                else if (result != 0)
                {
                    initial_auto_run = InitialAutoRun::None;
                    run_requested = false;
                    try
                    {
                        reload_targets();
                    }
                    catch (const std::exception& error)
                    {
                        log(cpplivelab::MessageKind::Warning, decode_console_text(error.what()));
                    }
                    log(cpplivelab::MessageKind::Error,
                        build_id + L": Build Failed (exit " + std::to_wstring(result) +
                            L"). Tools > Review Build Errors provides details and next actions; "
                            L"double-click a file diagnostic to open it.");
                    finish_operation(L"Build Failed");
                }
                else
                {
                    if (was_sdk_probe)
                    {
                        try
                        {
                            program_environment =
                                cpplivelab::cached_runtime_environment(build_directory());
                            fast_runtime = true;
                        }
                        catch (const std::exception& error)
                        {
                            run_requested = false;
                            log(cpplivelab::MessageKind::Error, decode_console_text(error.what()));
                            finish_operation(L"Cached runtime environment unavailable");
                            return;
                        }
                    }
                    log(cpplivelab::MessageKind::Success,
                        build_id + L": Build Successful (exit 0)");
                    std::ifstream libraries(build_directory() / L"libraries.txt");
                    for (std::string library; std::getline(libraries, library);)
                    {
                        log(cpplivelab::MessageKind::System,
                            L"Configured library target: " + decode_console_text(library));
                    }
                    bool recorded = false;
                    try
                    {
                        reload_targets();
                        building_identity = current_identity();
                        cpplivelab::save_project_options(
                            active_project_directory, selected_configuration(), project_options());
                        recorded = building_sources_known &&
                                   freshness.succeeded(building_identity, building_sources);
                    }
                    catch (const std::exception& error)
                    {
                        log(cpplivelab::MessageKind::Warning,
                            L"Build completed; freshness not verified: " +
                                decode_console_text(error.what()));
                    }
                    if (should_run_program && (run_requested || automatic_launch_allowed()))
                    {
                        if (read_control_text(find_control(ExecutableInput)).empty())
                        {
                            run_requested = false;
                            log(cpplivelab::MessageKind::Warning,
                                L"Choose an executable target, then Run.");
                            finish_operation(L"Choose executable target");
                        }
                        else if (!fs::is_regular_file(current_identity().executable))
                        {
                            run_requested = false;
                            log(cpplivelab::MessageKind::Error,
                                L"Build completed but selected executable is missing. Check "
                                L"Executable and configuration.");
                            finish_operation(L"Executable missing after build");
                        }
                        else if (auto_build.pending())
                        {
                            finish_operation(
                                L"Build Pending - Run waits for latest source changes");
                        }
                        else if (!recorded && building_sources_known)
                        {
                            finish_operation(L"Build Pending - sources changed during build");
                            start(run_requested, !run_requested, true);
                        }
                        else if (!recorded)
                        {
                            run_requested = false;
                            log(cpplivelab::MessageKind::Error,
                                L"Run blocked because source freshness could not be verified. Fix "
                                L"file access and Run again.");
                            finish_operation(L"Run blocked - source access");
                        }
                        else
                        {
                            automatic_program_pending = !run_requested;
                            phase = Phase::PreparingProgram;
                            SetWindowTextW(find_control(StatusLabel), L"Preparing program");
                        }
                    }
                    else
                    {
                        finish_operation(L"Build Successful");
                    }
                }
            }
        }
        else if (phase == Phase::Running)
        {
            for (const auto& chunk : session.poll_output())
            {
                const bool error = chunk.stream == cpplivelab::OutputStream::StandardError;
                auto& decoder = error ? error_decoder : program_decoder;
                append_program_output(decoder.append(chunk.bytes), error);
            }
            update_console_badges();
            if (!session.running() && !session.output_pending())
            {
                append_program_output(program_decoder.append({}, true), false);
                append_program_output(error_decoder.append({}, true), true);
                const auto result = session.exit_code();
                log(stop_requested ? cpplivelab::MessageKind::Warning
                    : result == 0  ? cpplivelab::MessageKind::Success
                                   : cpplivelab::MessageKind::Error,
                    stop_requested ? L"Program stopped by user"
                                   : L"Program finished (exit " + std::to_wstring(result) + L")");
                const bool replacing = replacement_stopping;
                finish_operation(stop_requested
                                     ? L"Stopped"
                                     : L"Program finished (exit " + std::to_wstring(result) + L")");
                if (replacing)
                {
                    complete_program_restart();
                }
            }
        }
        if (phase == Phase::PreparingProgram)
        {
            if (project_dialog_open && initial_auto_run == InitialAutoRun::Active)
            {
                return;
            }
            if (stop_requested || (automatic_program_pending && !automatic_launch_allowed()))
            {
                run_requested = false;
                finish_operation(L"Stopped");
            }
            else
            {
                try
                {
                    begin_program();
                }
                catch (const std::exception& error)
                {
                    run_requested = false;
                    log(cpplivelab::MessageKind::Error, decode_console_text(error.what()));
                    finish_operation(L"Program launch failed");
                }
            }
        }
        const bool environment_worker_done =
            !environment_future.valid() ||
            environment_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
        if (close_requested && !operation_in_progress && !replacement_building &&
            environment_worker_done && !project_dialog_open && !cpplivelab::library_task_active())
        {
            DestroyWindow(window_handle);
        }
    }

    void stop()
    {
        initial_auto_run = InitialAutoRun::None;
        initial_switch_project.clear();
        track_placement(true);
        if (candidate_pending)
        {
            discard_candidate(L"Replacement cancelled by Stop.");
        }
        auto_launch_armed = false;
        automatic_program_pending = false;
        update_automatic_controls();
        run_requested = false;
        should_run_program = false;
        auto_build.cancel();
        if (!operation_in_progress)
        {
            if (replacement_building)
            {
                replacement_build.stop();
            }
            set_auto_status(L"Auto Run launch paused after Stop; Run or toggle Auto Run to resume");
            return;
        }
        replacement_ready = false;
        replacement_stopping = false;
        previous_windows.clear();
        if (replacement_building)
        {
            replacement_build.stop();
        }
        auto_build.cancel();
        stop_requested = true;
        run_requested = false;
        if (phase != Phase::Running)
        {
            auto_build.cancel();
        }
        if (phase == Phase::Running)
        {
            cpplivelab::close_application_windows(session.active_process_ids());
            session.close_input();
            input_closed = true;
            graceful_stop_pending = true;
            shutdown_started = GetTickCount64();
        }
        else
        {
            session.stop();
        }
        SetWindowTextW(find_control(StatusLabel), L"Stopping process tree...");
        refresh();
    }

    void send()
    {
        if (!operation_in_progress || !program_started || stop_requested || input_closed)
        {
            return;
        }
        const auto value = read_control_text(find_control(ProgramInput));
        if (!session.send_line(encode_utf8(value)))
        {
            log(cpplivelab::MessageKind::Warning,
                L"Input could not be sent (buffer full or program exited).");
            return;
        }
        program_console.append(cpplivelab::MessageKind::Input, L"> " + value + L"\n");
        update_console_badges();
        SetWindowTextW(find_control(ProgramInput), L"");
        SetFocus(find_control(ProgramInput));
    }

    void set_auto_status(const std::wstring& status)
    {
        SetWindowTextW(find_control(StatusLabel), status.c_str());
        if (auto_status != status)
        {
            log(status.starts_with(L"Build Deferred") ? cpplivelab::MessageKind::Warning
                                                      : cpplivelab::MessageKind::System,
                status);
            auto_status = status;
        }
    }

    void update_automatic_controls()
    {
        SetWindowTextW(find_control(AutoRunButton),
            !auto_run_preferences.enabled ? L"Auto Run: OFF"
            : auto_launch_armed           ? L"Auto Run: On"
                                          : L"Auto Run: Paused");
        appearance.active_button(find_control(AutoRunButton), auto_run_preferences.enabled);
    }

    bool automatic_launch_allowed() const
    {
        return auto_run_preferences.enabled && auto_launch_armed && !close_requested &&
               !restart_pending;
    }

    void disable_automatic_work()
    {
        auto_build.enable(false);
        auto_run_preferences.enabled = false;
        auto_launch_armed = false;
        if (!active_project_directory.empty())
        {
            cpplivelab::save_auto_run_preferences(active_project_directory, auto_run_preferences);
        }
        update_automatic_controls();
    }

    void start_watching()
    {
        auto_status.clear();
        if (auto_build.enabled() && !active_project_directory.empty())
        {
            auto_build.set_debounce(preferences.debounce_ms);
            try
            {
                watcher.start(active_project_directory);
            }
            catch (...)
            {
                watcher.stop();
                disable_automatic_work();
                throw;
            }
            set_auto_status(L"Watching");
        }
        else
        {
            set_auto_status(L"Automatic building and running OFF");
        }
    }

    void start_replacement_build()
    {
        if (replacement_building || replacement_stopping || candidate_pending || stop_requested ||
            project_dialog_open)
        {
            return;
        }
        if (!collect_environment())
        {
            request_environment();
            set_auto_status(L"Preparing compiler environment; current application retained");
            return;
        }
        replacement_options = project_options();
        if (replacement_options.target.empty())
        {
            auto_build.cancel();
            log(cpplivelab::MessageKind::Warning, L"Auto Run needs a selected executable target.");
            return;
        }
        replacement_options = cpplivelab::replacement_build_options(
            active_project_directory, selected_configuration(), replacement_options);
        replacement_sources =
            cpplivelab::ProjectWatcher::source_fingerprints(active_project_directory);
        std::vector<std::wstring> arguments{L"manage-build",
            active_project_directory.wstring(),
            L"--config",
            selected_configuration(),
            L"--target",
            replacement_options.target,
            L"--build-slot",
            replacement_options.recovery_slot};
        if (!auto_build.needs_reconfigure() &&
            fs::is_regular_file(
                cpplivelab::managed_build_directory(
                    active_project_directory, selected_configuration(), replacement_options) /
                L"CMakeCache.txt"))
        {
            arguments.push_back(L"--incremental");
        }
        if (configure_writes_authorized)
        {
            arguments.push_back(L"--allow-configure-writes");
        }
        // The running program already required successful SDK preparation. Pass
        // that in-memory block only to this owned build child; never change PATH.
        const auto* worker_environment = &program_environment;
#ifdef CPPLIVELAB_GUI_TEST
        if (!reuse_worker_environment)
        {
            worker_environment = nullptr;
        }
#endif
#ifdef CPPLIVELAB_GUI_TEST
        if (auto_run_trace)
        {
            auto_run_trace("build");
        }
#endif
        replacement_build.start_worker(application_directory() / L"EasierCpp.exe",
            arguments,
            active_project_directory,
            worker_environment ? *worker_environment : cpplivelab::Environment{});
        replacement_building = true;
        replacement_ready = false;
        auto_build.begin_build();
        log(cpplivelab::MessageKind::Build,
            L"Auto Run: building replacement; previous application is still running.");
        set_auto_status(L"Auto Run: building replacement");
        refresh();
    }

    void request_program_restart()
    {
#ifdef CPPLIVELAB_GUI_TEST
        if (auto_run_trace)
        {
            auto_run_trace("shutdown");
        }
#endif
        track_placement(true);
        previous_windows = placement_memory.recall(active_project_directory,
            selected_configuration(),
            replacement_options.target,
            auto_run_preferences.placement_memory);
        cpplivelab::close_application_windows(session.active_process_ids());
        session.close_input();
        input_closed = true;
        replacement_stopping = true;
        shutdown_started = GetTickCount64();
        set_auto_status(L"Restarting: waiting for graceful shutdown");
        refresh();
    }

    void complete_program_restart()
    {
        if (candidate_pending && candidate_ready)
        {
            promote_candidate();
            return;
        }
#ifdef CPPLIVELAB_GUI_TEST
        if (auto_run_trace)
        {
            auto_run_trace("old_stopped");
        }
#endif
        replacement_stopping = false;
        automatic_program_pending = replacement_ready;
        if (replacement_ready)
        {
            cpplivelab::activate_build_slot(
                active_project_directory, selected_configuration(), replacement_options);
#ifdef CPPLIVELAB_GUI_TEST
            if (auto_run_trace)
            {
                auto_run_trace("slot_activated");
            }
#endif
            reload_targets();
            const bool current = freshness.succeeded(current_identity(), replacement_sources);
            replacement_ready = false;
            if (!current)
            {
                // A save can arrive during graceful shutdown. Do not launch an
                // obsolete replacement after the old process has finished.
                run_requested = false;
                start(false, true, true);
                return;
            }
        }
        // Restart intentionally uses the last successful artifact, including when
        // the editor currently contains errors. It never starts another build.
        launch_latest = true;
        probe_pending = false;
        operation_in_progress = true;
        phase = Phase::PreparingProgram;
        if (!fast_runtime)
        {
            request_environment();
        }
    }

    void track_placement(bool flush = false)
    {
        if (!program_started || running_target.empty() || restore_started ||
            !auto_run_preferences.restore_windows)
        {
            return;
        }
        const auto now = GetTickCount64();
        if (!flush && now - placement_sampled < 500)
        {
            return;
        }
        placement_sampled = now;
        const auto windows = cpplivelab::capture_application_windows(session.active_process_ids());
        // Require two consecutive samples before accepting transient startup/resize
        // geometry. Explicit Stop captures the latest valid geometry before WM_CLOSE.
        const bool stable = windows.size() == placement_sample.size() &&
                            std::equal(windows.begin(),
                                windows.end(),
                                placement_sample.begin(),
                                [](const auto& first, const auto& second)
                                {
                                    return first.class_name == second.class_name &&
                                           EqualRect(&first.bounds, &second.bounds) &&
                                           first.placement.showCmd == second.placement.showCmd &&
                                           first.minimized == second.minimized;
                                });
        placement_sample = windows;
        try
        {
            placement_memory.remember(active_project_directory,
                running_configuration,
                running_target,
                auto_run_preferences.placement_memory,
                stable || flush ? windows : std::vector<cpplivelab::ApplicationWindow>{},
                flush);
        }
        catch (const std::exception& error)
        {
            if (!placement_warning)
            {
                log(cpplivelab::MessageKind::Warning, decode_console_text(error.what()));
                placement_warning = true;
            }
        }
    }

    void finish_candidate_discard()
    {
        if (candidate_session.running() || candidate_session.output_pending())
        {
            return;
        }
        candidate_session.release_completed();
        candidate_lease.reset();
        candidate_pending = candidate_discarding = false;
        if (phase == Phase::Idle)
        {
            operation_in_progress = false;
        }
        refresh();
    }

    void discard_candidate(const std::wstring& reason)
    {
        candidate_session.stop();
        // TerminateJobObject is asynchronous. Keep both the slot lease and async
        // pipe buffers until the job is gone and output has drained on later ticks.
        candidate_ready = false;
        candidate_discarding = true;
        replacement_ready = replacement_stopping = false;
        candidate_restored.clear();
        log(cpplivelab::MessageKind::Warning,
            reason + L" Use Safe Replacement for incompatible applications.");
        if (!candidate_stderr.empty())
        {
            log(cpplivelab::MessageKind::Error, decode_console_text(candidate_stderr));
        }
        if (!candidate_stdout.empty())
        {
            log(cpplivelab::MessageKind::System, decode_console_text(candidate_stdout));
        }
        finish_candidate_discard();
        refresh();
    }

    void start_candidate(const fs::path& executable)
    {
        candidate_stdout.clear();
        candidate_stderr.clear();
        candidate_restored.clear();
        candidate_ready = false;
        candidate_discarding = false;
        candidate_pending = true;
        candidate_started = GetTickCount64();
        candidate_restore_position = auto_run_preferences.keep_window_position;
        candidate_restore_size = auto_run_preferences.keep_window_size;
        candidate_visible = 0;
        try
        {
            const auto directory = cpplivelab::managed_build_directory(
                active_project_directory, selected_configuration(), replacement_options);
            candidate_lease = std::make_unique<cpplivelab::BuildLease>(directory);
            candidate_placement =
                running_configuration == selected_configuration() &&
                        running_target == replacement_options.target
                    ? cpplivelab::capture_application_windows(session.active_process_ids())
                    : std::vector<cpplivelab::ApplicationWindow>{};
            // Historical GUI placement must not turn a console-only process into
            // a GUI readiness requirement after a project changes executable type.
            candidate_needs_window =
                !candidate_placement.empty() || cpplivelab::expects_application_window(executable);
            track_placement(true);
            if (auto_run_preferences.restore_windows)
            {
                candidate_placement = placement_memory.recall(active_project_directory,
                    selected_configuration(),
                    replacement_options.target,
                    auto_run_preferences.placement_memory);
            }
            candidate_session.start_program(executable,
                replacement_options.arguments,
                replacement_options.working_directory.empty()
                    ? active_project_directory
                    : replacement_options.working_directory,
                cpplivelab::runtime_environment(program_environment, directory),
                true);
            log(cpplivelab::MessageKind::System,
                L"Seamless: checking replacement startup; previous application retained.");
        }
        catch (const std::exception& error)
        {
            discard_candidate(L"Replacement launch failed: " + decode_console_text(error.what()));
        }
    }

    void promote_candidate()
    {
        if (!candidate_session.running() || candidate_session.root_exited())
        {
            discard_candidate(L"Replacement exited before handoff.");
            return;
        }
        // The candidate was already launched fresh. A later edit must not promote
        // an obsolete result after the old application's graceful-close interval.
        if (replacement_sources !=
            cpplivelab::ProjectWatcher::source_fingerprints(active_project_directory))
        {
            discard_candidate(L"Newer source changes arrived before handoff.");
            auto_build.notify(GetTickCount64());
            return;
        }
        // Do not exchange ownership until the old job and its pipes are drained.
        if (session.running() || session.output_pending())
        {
            return;
        }
        try
        {
            cpplivelab::activate_build_slot(
                active_project_directory, selected_configuration(), replacement_options);
            freshness.succeeded(current_identity(), replacement_sources);
        }
        catch (const std::exception& error)
        {
            discard_candidate(
                L"Replacement activation failed: " + decode_console_text(error.what()));
            return;
        }
        session.swap(candidate_session);
        program_lease.swap(candidate_lease);
        candidate_session.release_completed();
        candidate_lease.reset();
        candidate_pending = candidate_ready = replacement_ready = replacement_stopping = false;
        program_started = operation_in_progress = true;
        stop_requested = input_closed = false;
        phase = Phase::Running;
        running_configuration = selected_configuration();
        running_target = replacement_options.target;
        restore_started = 0;
        previous_windows.clear();
        restored_windows.clear();
        placement_sample.clear();
        program_decoder.reset();
        error_decoder.reset();
        error_line_start = true;
        append_program_output(program_decoder.append(candidate_stdout), false);
        append_program_output(error_decoder.append(candidate_stderr), true);
        candidate_stdout.clear();
        candidate_stderr.clear();
        log(cpplivelab::MessageKind::Success,
            L"Seamless replacement active; previous process closed.");
        SetWindowTextW(find_control(StatusLabel), L"Running - input below");
        refresh();
    }

    void poll_candidate()
    {
        if (!candidate_pending)
        {
            return;
        }
        for (const auto& chunk : candidate_session.poll_output())
        {
            auto& text = chunk.stream == cpplivelab::OutputStream::StandardError ? candidate_stderr
                                                                                 : candidate_stdout;
            text += chunk.bytes;
            if (text.size() > 65536)
            {
                text.erase(0, text.size() - 65536);
            }
        }
        if (candidate_discarding)
        {
            finish_candidate_discard();
            return;
        }
        if (candidate_session.root_exited() || !candidate_session.running())
        {
            discard_candidate(L"Replacement startup exited (code " +
                              std::to_wstring(candidate_session.exit_code()) +
                              L"); previous application retained where available.");
            return;
        }
        if (candidate_ready)
        {
            if (!session.running() && !session.output_pending())
            {
                promote_candidate();
            }
            return;
        }
        if (!automatic_launch_allowed())
        {
            discard_candidate(L"Automatic replacement disarmed.");
            return;
        }
        const auto now = GetTickCount64();
        const auto windows =
            cpplivelab::capture_application_windows(candidate_session.active_process_ids());
        if (!windows.empty() && !candidate_visible)
        {
            candidate_visible = now;
        }
        if (now - candidate_started >= 15000)
        {
            discard_candidate(L"Replacement readiness timed out; previous application retained.");
            return;
        }
        // Placement is independent of startup readiness: use the first visible
        // window, while retaining the old application through the safety interval.
        if (auto_run_preferences.restore_windows && (candidate_needs_window || !windows.empty()))
        {
            track_placement();
            const auto latest = placement_memory.recall(active_project_directory,
                selected_configuration(),
                replacement_options.target,
                auto_run_preferences.placement_memory);
            if (!latest.empty())
            {
                candidate_placement = latest;
            }
            if (cpplivelab::restore_application_windows(candidate_session.active_process_ids(),
                    candidate_placement,
                    candidate_restored,
                    true,
                    candidate_restore_position,
                    candidate_restore_size) < candidate_placement.size())
            {
                return;
            }
        }
        if (now - candidate_started < 1000 ||
            (candidate_needs_window && (!candidate_visible || now - candidate_visible < 250)))
        {
            return;
        }
        if (replacement_sources !=
            cpplivelab::ProjectWatcher::source_fingerprints(active_project_directory))
        {
            discard_candidate(L"Replacement superseded by a newer save.");
            auto_build.notify(now);
            return;
        }
        candidate_ready = true;
        log(cpplivelab::MessageKind::Success,
            L"Seamless replacement ready; closing previous process gracefully.");
        if (session.running())
        {
            cpplivelab::close_application_windows(session.active_process_ids());
            session.close_input();
            input_closed = true;
            replacement_stopping = true;
            shutdown_started = now;
            refresh();
        }
        else
        {
            promote_candidate();
        }
    }

    void poll_auto_run()
    {
        poll_candidate();
        if (graceful_stop_pending && session.running() &&
            GetTickCount64() - shutdown_started >= auto_run_preferences.shutdown_ms)
        {
            graceful_stop_pending = false;
            if (auto_run_preferences.force_stop)
            {
                session.stop();
            }
            else
            {
                stop_requested = false;
                log(cpplivelab::MessageKind::Warning,
                    L"Stop timed out; application retained. Enable force-stop in Build Preferences "
                    L"to terminate the owned tree after the timeout.");
                SetWindowTextW(
                    find_control(StatusLabel), L"Stop timed out — application still running");
                refresh();
            }
        }
        if (replacement_building)
        {
            append_build_output(output_decoder.append(replacement_build.poll()));
            if (replacement_build.root_exited())
            {
                replacement_build.finish_build();
            }
            if (!replacement_build.running() && !replacement_build.output_pending())
            {
                const auto result = replacement_build.exit_code();
                replacement_build.release_completed();
                replacement_building = false;
                append_build_output(output_decoder.append({}, true), true);
                if (!automatic_launch_allowed() || stop_requested)
                {
                    log(cpplivelab::MessageKind::System,
                        L"Automatic replacement cancelled or launch paused; no application "
                        L"replaced.");
                }
                else if (result != 0)
                {
                    const auto message =
                        session.running() ? L"Build failed — previous application is still running."
                                          : L"Build failed — previous application has exited.";
                    log(cpplivelab::MessageKind::Error, message);
                    set_auto_status(message);
                }
                else if (const auto latest_sources =
                             cpplivelab::ProjectWatcher::source_fingerprints(
                                 active_project_directory);
                    auto_build.pending() || replacement_sources != latest_sources)
                {
                    auto_build.notify(GetTickCount64());
                    // Source-only saves need another build, not another configure.
                    // Preserve configuration changes, including events not yet debounced.
                    auto_build.confirm(true,
                        cpplivelab::ProjectWatcher::configuration_changed(
                            replacement_sources, latest_sources));
                    set_auto_status(
                        L"Newer changes pending; obsolete replacement was not launched");
                }
                else
                {
                    const auto targets = cpplivelab::executable_targets(
                        active_project_directory, selected_configuration(), replacement_options);
                    const auto selected = std::find_if(targets.begin(),
                        targets.end(),
                        [&](const auto& target)
                        {
                            return target.name == replacement_options.target &&
                                   fs::is_regular_file(target.artifact);
                        });
                    if (selected == targets.end())
                    {
                        log(cpplivelab::MessageKind::Error,
                            L"Replacement executable missing; previous application retained.");
                    }
                    else
                    {
                        replacement_ready = true;
                        if (program_started && session.running())
                        {
                            if (auto_run_preferences.replacement_mode ==
                                cpplivelab::ReplacementMode::Seamless)
                            {
                                start_candidate(selected->artifact);
                            }
                            else
                            {
                                request_program_restart();
                            }
                        }
                        else
                        {
                            previous_windows.clear();
                            complete_program_restart();
                        }
                    }
                }
                refresh();
            }
        }
        if (replacement_stopping && session.running() &&
            GetTickCount64() - shutdown_started >= auto_run_preferences.shutdown_ms)
        {
            if (auto_run_preferences.force_stop)
            {
                session.stop();
            }
            else
            {
                if (candidate_pending)
                {
                    discard_candidate(
                        L"Previous application refused graceful shutdown; replacement cancelled.");
                }
                replacement_stopping = false;
                replacement_ready = false;
                previous_windows.clear();
                log(cpplivelab::MessageKind::Warning,
                    L"Graceful shutdown timed out. Application retained; input is closed. Stop "
                    L"manually or enable force-stop in Build Preferences.");
                set_auto_status(L"Restart cancelled — application did not close gracefully");
                refresh();
            }
        }
        if (restore_started && program_started && !previous_windows.empty())
        {
            const auto restored =
                cpplivelab::restore_application_windows(session.active_process_ids(),
                    previous_windows,
                    restored_windows,
                    false,
                    restore_position,
                    restore_size);
            if (restored >= previous_windows.size() || GetTickCount64() - restore_started > 15000)
            {
#ifdef CPPLIVELAB_GUI_TEST
                if (auto_run_trace && restored >= previous_windows.size())
                {
                    auto_run_trace("restored");
                }
#endif
                log(restored >= previous_windows.size() ? cpplivelab::MessageKind::Success
                                                        : cpplivelab::MessageKind::Warning,
                    restored >= previous_windows.size()
                        ? L"Window restoration complete."
                        : L"Window restoration partially supported: some matching windows did not "
                          L"appear within 15 seconds.");
                restore_started = 0;
                previous_windows.clear();
            }
        }
    }

    void poll_watcher()
    {
        if (!auto_build.enabled() || active_project_directory.empty() || close_requested ||
            restart_pending)
        {
            return;
        }
        if (stop_requested && phase == Phase::Building)
        {
            return;
        }
        const auto now = GetTickCount64();
        bool notified = false;
        try
        {
            notified = watcher.poll();
        }
        catch (const std::exception& error)
        {
            watcher.stop();
            disable_automatic_work();
            log(cpplivelab::MessageKind::Error, decode_console_text(error.what()));
            set_auto_status(L"Watcher stopped - turn Auto Run on to retry");
            return;
        }
        if (notified)
        {
#ifdef CPPLIVELAB_GUI_TEST
            if (auto_run_trace)
            {
                auto_run_trace("detect");
            }
#endif
            auto_build.notify(now);
        }
        if (auto_build.settled(now))
        {
            std::vector<cpplivelab::SourceChange> changes;
            try
            {
                changes = watcher.consume_changes();
            }
            catch (const std::exception&)
            {
                // Editors may briefly lock a file during an atomic save. Keep the
                // notification pending and retry after another debounce interval.
                auto_build.notify(now);
                set_auto_status(L"Changes Detected - waiting for source file access");
                return;
            }
            const bool configuration_changed = std::any_of(changes.begin(),
                changes.end(),
                [](const auto& change)
                {
                    return change.affects_cmake;
                });
            auto_build.confirm(!changes.empty(), configuration_changed);
            if (!changes.empty())
            {
                log(cpplivelab::MessageKind::System, L"Source changes confirmed after debounce");
                if (!operation_in_progress)
                {
                    set_auto_status(L"Changes Detected");
                }
            }
        }
        if (initial_auto_run == InitialAutoRun::AwaitingTarget ||
            initial_auto_run == InitialAutoRun::Pending)
        {
            return;
        }
        if (auto_build.pending() && operation_in_progress)
        {
            set_auto_status(program_started
                                ? L"Build Deferred - running program preserved"
                                : build_id + L": Building; one follow-up Build Pending");
        }
        if (auto_run_preferences.enabled && program_started && phase == Phase::Running &&
            auto_build.ready(
                now, replacement_building || replacement_stopping || candidate_pending, true))
        {
            try
            {
                start_replacement_build();
            }
            catch (const std::exception& error)
            {
                auto_build.cancel();
                log(cpplivelab::MessageKind::Error, decode_console_text(error.what()));
                set_auto_status(
                    L"Replacement build could not start; previous application retained");
            }
            return;
        }
        if (auto_build.ready(
                now, operation_in_progress || replacement_building || replacement_stopping, true))
        {
            const auto configuration =
                SendMessageW(find_control(ConfigurationSelector), CB_GETCURSEL, 0, 0) == 1
                    ? L"Release"
                    : L"Debug";
            const auto directory = build_directory();
            bool available = false;
            try
            {
                available = cpplivelab::build_outputs_available(directory);
            }
            catch (const std::exception& error)
            {
                auto_build.cancel();
                log(cpplivelab::MessageKind::Error, decode_console_text(error.what()));
                SetWindowTextW(find_control(StatusLabel), L"Build Failed - output access error");
                return;
            }
            if (!available)
            {
                auto_build.defer(now);
                set_auto_status(L"Build Deferred - configuration is busy or not writable");
            }
            else
            {
                const bool incremental = !auto_build.needs_reconfigure();
                try
                {
                    start(false, true, incremental);
                    set_auto_status(L"Building (Auto)");
                }
                catch (const std::exception& error)
                {
                    auto_build.cancel();
                    run_requested = false;
                    log(cpplivelab::MessageKind::Error, decode_console_text(error.what()));
                    SetWindowTextW(
                        find_control(StatusLabel), L"Build Failed - worker could not start");
                }
            }
        }
    }

    void recover_build()
    {
        if (operation_in_progress || replacement_building || active_project_directory.empty())
        {
            append(L"Stop the active operation and select a project before recovery.\n");
            return;
        }
        cpplivelab::FormOptions options;
        options.accept_label = L"Build Fresh Directory";
        options.cancel_default = true;
        options.note = L"Keep the old cache and diagnostics. Build in a fresh manager-owned "
                       L"directory and activate it only if successful. No existing outputs, "
                       L"source, assets or project configuration will be deleted.";
        std::vector<cpplivelab::FormField> fields;
        if (show_project_form(L"Clean & Rebuild", fields, options) && authorize_configuration())
        {
            start(false, false, false, true);
        }
    }

    void open_diagnostic(const cpplivelab::BuildDiagnostic& diagnostic)
    {
        std::error_code error;
        if (!diagnostic.file.empty() && fs::is_regular_file(diagnostic.file, error))
        {
            cpplivelab::launch_editor(cpplivelab::load_editor(),
                active_project_directory,
                diagnostic.file,
                diagnostic.line,
                diagnostic.column);
        }
        else
        {
            append(L"No existing source location. Tools > Review Build Errors provides "
                   L"configuration, library and recovery actions.\n");
        }
    }

    void review_errors()
    {
        const auto diagnostics =
            cpplivelab::parse_build_diagnostics(build_diagnostics, active_project_directory);
        std::vector<std::wstring> entries;
        for (const auto& diagnostic : diagnostics)
        {
            entries.push_back(std::to_wstring(entries.size() + 1) + L". " + diagnostic.raw);
        }
        if (entries.empty())
        {
            entries.push_back(L"No recognized diagnostic; inspect the detailed output below.");
        }
        cpplivelab::FormOptions options;
        options.note = L"Open source errors in your editor. Review Libraries for missing packages. "
                       L"Use fresh build recovery for cache/generator mismatch. Ordinary compiler "
                       L"errors should be repaired in source. Original output is retained.";
        options.accept_label = L"Perform Action";
        std::vector<cpplivelab::FormField> fields{{L"Diagnostic", entries.front(), entries},
            {L"Action",
                L"Show Details",
                {L"Show Details",
                    L"Open in Editor",
                    L"Review Libraries",
                    L"Project Settings",
                    L"Clean & Rebuild"}},
            {L"Detailed Output",
                build_diagnostics,
                {},
                true,
                L"",
                cpplivelab::BrowseKind::None,
                false,
                true}};
        if (!show_project_form(L"Build Errors", fields, options, shell_page == NavigationTools))
        {
            return;
        }
        if (fields[1].value == L"Open in Editor" && !diagnostics.empty())
        {
            const auto selected = std::find(entries.begin(), entries.end(), fields[0].value);
            if (selected != entries.end())
            {
                open_diagnostic(diagnostics[static_cast<size_t>(selected - entries.begin())]);
            }
        }
        else if (fields[1].value == L"Clean & Rebuild")
        {
            recover_build();
        }
        else if (fields[1].value == L"Review Libraries")
        {
            manage_libraries();
        }
        else if (fields[1].value == L"Project Settings")
        {
            command(ProjectSettingsMenu);
        }
    }

    void activate_console_diagnostic()
    {
        const HWND console = find_control(SystemOutputPane);
        CHARRANGE selection{};
        SendMessageW(console, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
        // Visual rows wrap as the shell narrows. Read Rich Edit's logical text
        // without CRLF expansion so its selection offset still identifies a line.
        GETTEXTLENGTHEX length{GTL_NUMCHARS | GTL_PRECISE, 1200};
        const auto characters =
            SendMessageW(console, EM_GETTEXTLENGTHEX, reinterpret_cast<WPARAM>(&length), 0);
        std::wstring text(static_cast<size_t>((std::max)(LRESULT{0}, characters)) + 1, L'\0');
        GETTEXTEX options{static_cast<DWORD>(text.size() * sizeof(wchar_t)), GT_DEFAULT, 1200};
        const auto copied = SendMessageW(console,
            EM_GETTEXTEX,
            reinterpret_cast<WPARAM>(&options),
            reinterpret_cast<LPARAM>(text.data()));
        text.resize(static_cast<size_t>((std::max)(LRESULT{0}, copied)));
        const auto offset =
            (std::min)(text.size(), static_cast<size_t>((std::max)(0L, selection.cpMin)));
        const auto previous = offset == 0 ? text.npos : text.find_last_of(L"\r\n", offset - 1);
        const auto first = previous == text.npos ? 0 : previous + 1;
        const auto last = text.find_first_of(L"\r\n", offset);
        const auto line = text.substr(first, last == text.npos ? text.npos : last - first);
        const auto diagnostics =
            cpplivelab::parse_build_diagnostics(line, active_project_directory);
        if (!diagnostics.empty())
        {
            open_diagnostic(diagnostics.front());
        }
        else
        {
            review_errors();
        }
    }

    void command(int control_id)
    {
        if (restart_launch)
        {
            return;
        }
        if (leave_inline(WM_COMMAND, MAKEWPARAM(control_id, BN_CLICKED)))
        {
            return;
        }
        if (control_id == RestartButton)
        {
            request_restart();
            return;
        }
        if (control_id == RecoverBuildMenu)
        {
            recover_build();
            return;
        }
        if (control_id == ReviewErrorsMenu)
        {
            review_errors();
            return;
        }
        if (control_id == OpenWelcomeRecent || control_id == ForgetWelcomeRecent)
        {
            const auto selected =
                SendDlgItemMessageW(window_handle, WelcomeRecent, LB_GETCURSEL, 0, 0);
            if (selected >= 0 && static_cast<size_t>(selected) < welcome_recents.size())
            {
                const auto path = welcome_recents[selected];
                if (control_id == ForgetWelcomeRecent)
                {
                    cpplivelab::remember_project(path, true);
                    reload_welcome_recents();
                }
                else if (fs::is_directory(path))
                {
                    open(path);
                }
                else
                {
                    SetWindowTextW(find_control(StatusLabel),
                        L"Recent folder is missing. Use Project > Recent to remove it.");
                }
            }
            return;
        }
        if (active_project_directory.empty() &&
            (control_id == ManageLibrariesMenu || control_id == ProjectSettingsMenu ||
                control_id == BuildButton || control_id == RunButton ||
                control_id == EditProjectButton))
        {
            SetWindowTextW(
                find_control(StatusLabel), L"Select or create a project before using this action.");
            return;
        }
        if (std::find(std::begin(navigation_pages), std::end(navigation_pages), control_id) !=
            std::end(navigation_pages))
        {
            navigate_to(control_id);
            return;
        }
        if (control_id == WorkspaceLibraries)
        {
            navigate_to(NavigationLibraries);
            return;
        }
        if (control_id == OptimizeWorkersButton)
        {
            optimize_workers(window_handle);
            return;
        }
        if (std::any_of(std::begin(shell_help),
                std::end(shell_help),
                [&](const auto& help)
                {
                    return help.id == control_id;
                }))
        {
            show_shell_help(control_id);
            return;
        }
        if (control_id == AppearanceMenu)
        {
            appearance_dialog();
            return;
        }
        if (control_id == WindowPreferencesMenu)
        {
            window_preferences_dialog();
            return;
        }
        if (control_id == BuildPreferencesMenu)
        {
            build_preferences_dialog();
            return;
        }
        if (control_id == CopyProjectPath)
        {
            copy_project_path();
            return;
        }
#ifndef EASIERCPP_PUBLIC_GUI
        if (control_id == TestRunnerMenu)
        {
            run_tests_dialog();
            return;
        }
        if (control_id == CancelTestsMenu)
        {
            if (tests_active)
            {
                tests_cancelled = true;
                test_session.stop();
            }
            return;
        }
#endif
        if (control_id == DoctorMenu || control_id == DetailedDoctorMenu)
        {
            std::ostringstream report;
            const auto tools =
                cpplivelab::discover_toolchain(cpplivelab::system_discovery_inputs());
            if (control_id == DoctorMenu)
            {
                report << cpplivelab::diagnostics_summary(tools);
            }
            else
            {
                cpplivelab::run_doctor(report, tools);
            }
            reveal_system_output();
            append(decode_console_text(report.str()));
            select_console(false);
            return;
        }
        // Handle Stop/input before rejecting other commands during an operation.
        // Disabled buttons alone are insufficient: Windows may have queued a command.
        if (control_id == ProgramTab)
        {
            select_console(true);
            return;
        }
        if (control_id == SystemTab)
        {
            select_console(false);
            return;
        }
        if (control_id == ClearProgram)
        {
            if (program_started && phase == Phase::Running && !stop_requested && session.running())
            {
                program_console.clear_preserving_unfinished_stdout();
            }
            else
            {
                program_console.clear();
            }
            if (program_started && phase == Phase::Running && !input_closed)
            {
                SetWindowTextW(find_control(StatusLabel),
                    L"Program running - input remains available (earlier output cleared)");
            }
            update_console_badges();
            return;
        }
        if (control_id == ClearSystem)
        {
            system_console.clear();
            update_console_badges();
            return;
        }
        if (control_id == AutoRunButton && !active_project_directory.empty())
        {
            auto_run_preferences.enabled = !auto_run_preferences.enabled;
            auto_launch_armed = auto_run_preferences.enabled;
            cpplivelab::save_auto_run_preferences(active_project_directory, auto_run_preferences);
            auto_build.enable(auto_run_preferences.enabled);
            watcher.stop();
            start_watching();
            if (auto_run_preferences.enabled && !operation_in_progress && !program_started)
            {
                initial_targets_verified = false;
                initial_auto_run = InitialAutoRun::Pending;
                log(cpplivelab::MessageKind::System,
                    L"Auto Run enabled — preparing initial project build.");
            }
            if (!auto_run_preferences.enabled)
            {
                initial_auto_run = InitialAutoRun::None;
                if (replacement_building)
                {
                    replacement_build.stop();
                }
                if (automatic_operation && phase == Phase::Building && !run_requested)
                {
                    stop_requested = true;
                    run_requested = false;
                    should_run_program = false;
                    session.stop();
                }
            }
            if (!auto_run_preferences.enabled && replacement_ready && replacement_stopping)
            {
                replacement_ready = false;
                replacement_stopping = false;
                previous_windows.clear();
                log(cpplivelab::MessageKind::Warning,
                    L"Automatic restart cancelled; Auto Run is OFF.");
            }
            update_automatic_controls();
            return;
        }
        if (control_id == ProgramRestartButton)
        {
            if (candidate_pending)
            {
                return;
            }
            auto_launch_armed = true;
            automatic_program_pending = false;
            if (program_started && !replacement_building && !replacement_stopping)
            {
                replacement_options = project_options();
                replacement_ready = false;
                request_program_restart();
            }
            else if (!operation_in_progress && !replacement_building &&
                     !active_project_directory.empty() &&
                     fs::is_regular_file(current_identity().executable))
            {
                launch_latest = true;
                probe_pending = false;
                operation_in_progress = true;
                phase = Phase::PreparingProgram;
                if (!fast_runtime)
                {
                    request_environment();
                }
            }
            return;
        }
        if (control_id == AddLibraryButton)
        {
            if (!operation_in_progress)
            {
                manage_libraries(true);
            }
            return;
        }
        if (control_id == SendButton)
        {
            send();
            return;
        }
        if (control_id == StopButton)
        {
            stop();
            return;
        }
        if (control_id == EndInputButton && operation_in_progress && program_started)
        {
            session.close_input();
            input_closed = true;
            refresh();
            return;
        }
        if (control_id == RunButton && operation_in_progress &&
            (phase == Phase::Building || phase == Phase::Checking) && !stop_requested)
        {
            auto_launch_armed = true;
            run_requested = true;
            should_run_program = true;
            request_environment();
            select_console(true);
            log(cpplivelab::MessageKind::System,
                L"Run Pending - waiting for the current build and latest source changes.");
            return;
        }
        if (control_id == EditProjectButton && !active_project_directory.empty())
        {
            cpplivelab::launch_editor(cpplivelab::load_editor(), active_project_directory);
            append(L"Opened project in preferred editor. Save edits before Build/Run.\n");
            return;
        }
        if (initial_auto_run == InitialAutoRun::Active && operation_in_progress &&
            (control_id == OpenProjectButton || control_id == BrowseButton))
        {
            struct PickerGuard
            {
                bool& flag;
                bool before;

                ~PickerGuard()
                {
                    flag = before;
                }
            } guard{project_dialog_open, project_dialog_open};

            project_dialog_open = true;
            browse();
            return;
        }
        if (operation_in_progress)
        {
            return;
        }
        if (control_id == NewProjectMenu)
        {
            new_project();
        }
        else if (control_id == RecentProjectsMenu)
        {
            recent_dialog();
        }
        else if (control_id == ProjectSettingsMenu)
        {
            project_settings();
        }
        else if (control_id == EditorSettingsMenu)
        {
            editor_settings();
        }
        else if (control_id == ManageLibrariesMenu)
        {
            manage_libraries();
        }
        else if (control_id == OpenProjectButton)
        {
            browse();
        }
        else if (control_id == BrowseButton)
        {
            browse();
        }
        else if (control_id == BuildButton)
        {
            if (!authorize_configuration())
            {
                return;
            }
            start(false);
        }
        else if (control_id == RunButton)
        {
            if (!authorize_configuration())
            {
                return;
            }
            start(true);
        }
    }
};

LRESULT CALLBACK window_procedure(HWND window_handle, UINT message, WPARAM wparam, LPARAM lparam)
{
    // Windows calls a free function for messages. This non-owning window property
    // connects the callback to the stack-owned Application created by wWinMain.
    auto application =
        reinterpret_cast<Application*>(GetWindowLongPtrW(window_handle, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        application =
            static_cast<Application*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        application->window_handle = window_handle;
        SetWindowLongPtrW(window_handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(application));
    }
    if (!application)
    {
        return DefWindowProcW(window_handle, message, wparam, lparam);
    }
    try
    {
        switch (message)
        {
            case WM_CREATE:
                application->create();
                return 0;
            case WM_SIZE:
                // Minimize supplies a zero client size, not a usable layout. Keep
                // the last geometry/regions until Windows sends the restored size.
                if (wparam != SIZE_MINIMIZED && LOWORD(lparam) != 0 && HIWORD(lparam) != 0)
                {
                    application->layout(LOWORD(lparam), HIWORD(lparam));
                }
                return 0;
            case WM_GETMINMAXINFO:
                reinterpret_cast<MINMAXINFO*>(lparam)->ptMinTrackSize = {
                    cpplivelab::scale_pixels(780, application->dpi),
                    cpplivelab::scale_pixels(600, application->dpi)};
                return 0;
            case WM_MEASUREITEM:
                if (reinterpret_cast<MEASUREITEMSTRUCT*>(lparam)->CtlType == ODT_MENU)
                {
                    auto& measure = *reinterpret_cast<MEASUREITEMSTRUCT*>(lparam);
                    measure.itemWidth = cpplivelab::scale_pixels(280, application->dpi);
                    measure.itemHeight = cpplivelab::scale_pixels(34, application->dpi);
                    return TRUE;
                }
                if (reinterpret_cast<MEASUREITEMSTRUCT*>(lparam)->CtlType == ODT_COMBOBOX)
                {
                    reinterpret_cast<MEASUREITEMSTRUCT*>(lparam)->itemHeight =
                        cpplivelab::scale_pixels(24, application->dpi);
                    return TRUE;
                }
                break;
            case WM_DRAWITEM:
                if (application->appearance.draw_item(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam)))
                {
                    return TRUE;
                }
                break;
            case WM_ERASEBKGND:
                if (application->appearance.background_brush())
                {
                    RECT client{};
                    GetClientRect(window_handle, &client);
                    FillRect(reinterpret_cast<HDC>(wparam),
                        &client,
                        application->appearance.background_brush());
                    return 1;
                }
                break;
            case WM_PAINT:
            {
                PAINTSTRUCT paint{};
                const auto dc = BeginPaint(window_handle, &paint);
                application->paint_shell(dc);
                application->appearance.paint_panels(dc);
                EndPaint(window_handle, &paint);
                return 0;
            }
            case WM_LBUTTONDOWN:
            case WM_NCLBUTTONDOWN:
                application->dismiss_shell_help();
                break;
            case WM_ACTIVATE:
                if (LOWORD(wparam) == WA_INACTIVE)
                {
                    application->dismiss_shell_help();
                }
                break;
            case WM_PARENTNOTIFY:
                if (LOWORD(wparam) == WM_LBUTTONDOWN && application->tracked_help)
                {
                    const POINT point{
                        static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam))};
                    if (ChildWindowFromPointEx(window_handle, point, CWP_SKIPINVISIBLE) !=
                        application->find_control(application->tracked_help))
                    {
                        application->dismiss_shell_help();
                    }
                }
                break;
            case WM_CTLCOLORSTATIC:
            case WM_CTLCOLOREDIT:
            case WM_CTLCOLORLISTBOX:
            {
                if (!application->appearance.background_brush())
                {
                    break;
                }
                const auto child = reinterpret_cast<HWND>(lparam);
                if (GetDlgCtrlID(child) == ShellBrand || GetDlgCtrlID(child) == ShellLogo)
                {
                    const auto dc = reinterpret_cast<HDC>(wparam);
                    SetTextColor(dc, RGB(238, 246, 245));
                    SetBkMode(dc, TRANSPARENT);
                    SetDCBrushColor(dc, RGB(13, 30, 37));
                    return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));
                }
                wchar_t class_name[64]{};
                GetClassNameW(child, class_name, 64);
                const bool input =
                    message != WM_CTLCOLORSTATIC || std::wstring_view(class_name) == L"Edit";
                RECT bounds{};
                GetWindowRect(child, &bounds);
                POINT center{(bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2};
                ScreenToClient(window_handle, &center);
                const bool panel_label = message == WM_CTLCOLORSTATIC &&
                                         std::any_of(application->shell_panels.begin(),
                                             application->shell_panels.end(),
                                             [&](const RECT& panel)
                                             {
                                                 return PtInRect(&panel, center) != FALSE;
                                             });
                const auto brush =
                    application->appearance.control_colors(reinterpret_cast<HDC>(wparam),
                        input || panel_label,
                        IsWindowEnabled(child) != FALSE);
                if (child == application->find_control(StatusLabel))
                {
                    const auto status = read_control_text(child);
                    const auto colors =
                        cpplivelab::theme_palette(cpplivelab::ThemeChoice::FollowWindows,
                            application->appearance.palette.dark)
                            .console;
                    if (status.find(L"Failed") != status.npos || status.starts_with(L"Error"))
                    {
                        SetTextColor(reinterpret_cast<HDC>(wparam), colors.error);
                    }
                    else if (status.find(L"Successful") != status.npos)
                    {
                        SetTextColor(reinterpret_cast<HDC>(wparam), colors.success);
                    }
                    else if (status.find(L"Deferred") != status.npos)
                    {
                        SetTextColor(reinterpret_cast<HDC>(wparam), colors.warning);
                    }
                }
                return reinterpret_cast<LRESULT>(brush);
            }
            case WM_THEMECHANGED:
            case WM_SETTINGCHANGE:
                if (application->preferences.theme == cpplivelab::ThemeChoice::FollowWindows)
                {
                    application->apply_appearance();
                }
                break;
            case WM_DPICHANGED:
            {
                application->dpi = HIWORD(wparam);
                application->update_fonts();
                const auto* suggested = reinterpret_cast<RECT*>(lparam);
                SetWindowPos(window_handle,
                    nullptr,
                    suggested->left,
                    suggested->top,
                    suggested->right - suggested->left,
                    suggested->bottom - suggested->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
                application->apply_appearance();
                return 0;
            }
            case WM_NOTIFY:
            {
                const auto* notification = reinterpret_cast<NMHDR*>(lparam);
                if (notification->idFrom == SystemOutputPane && notification->code == EN_MSGFILTER)
                {
                    const auto* filter = reinterpret_cast<MSGFILTER*>(lparam);
                    if (filter->msg == WM_LBUTTONDBLCLK ||
                        (filter->msg == WM_KEYDOWN && filter->wParam == VK_RETURN))
                    {
                        if (filter->msg == WM_LBUTTONDBLCLK)
                        {
                            POINTL point{static_cast<short>(LOWORD(filter->lParam)),
                                static_cast<short>(HIWORD(filter->lParam))};
                            const auto console = application->find_control(SystemOutputPane);
                            const auto character = SendMessageW(
                                console, EM_CHARFROMPOS, 0, reinterpret_cast<LPARAM>(&point));
                            SendMessageW(console, EM_SETSEL, character, character);
                        }
                        application->activate_console_diagnostic();
                        return 1;
                    }
                }
                break;
            }
            case WM_COMMAND:
                if ((LOWORD(wparam) == ConfigurationSelector ||
                        LOWORD(wparam) == ExecutableInput) &&
                    HIWORD(wparam) == CBN_SELCHANGE && !application->operation_in_progress &&
                    !application->active_project_directory.empty())
                {
                    if (LOWORD(wparam) == ConfigurationSelector)
                    {
                        application->reload_targets();
                    }
                    application->save_selection();
                    return 0;
                }
                if (HIWORD(wparam) == BN_CLICKED)
                {
                    application->command(LOWORD(wparam));
                }
                return 0;
            case WM_TIMER:
                application->tick();
                return 0;
            case WM_SYSKEYDOWN:
                if (wparam == 'P' || wparam == 'T' || wparam == 'S' || wparam == 'R')
                {
                    application->command(wparam == 'R'   ? RestartButton
                                         : wparam == 'P' ? NavigationProject
                                         : wparam == 'T' ? NavigationTools
                                                         : NavigationSettings);
                    return 0;
                }
                break;
            case WM_CLOSE:
                if (application->leave_inline(WM_CLOSE))
                {
                    return 0;
                }
                application->restart_pending = false;
                application->restart_launch.reset();
#ifndef EASIERCPP_PUBLIC_GUI
                if (application->tests_active)
                {
                    application->test_session.stop();
                }
#endif
                application->watcher.stop();
                application->auto_build.enable(false);
                if (application->operation_in_progress || application->replacement_building ||
                    application->environment_future.valid() || application->project_dialog_open ||
                    cpplivelab::library_task_active())
                {
                    application->close_requested = true;
                    application->stop();
                }
                else
                {
                    DestroyWindow(window_handle);
                }
                return 0;
            case WM_DESTROY:
                KillTimer(window_handle, 1);
                application->destroy_shell_help();
                PostQuitMessage(0);
                return 0;
        }
    }
    catch (const std::exception& error)
    {
        application->log(cpplivelab::MessageKind::Error, decode_console_text(error.what()));
        SetWindowTextW(application->find_control(StatusLabel), L"Error - see output");
    }
    return DefWindowProcW(window_handle, message, wparam, lparam);
}
} // namespace

#ifndef CPPLIVELAB_GUI_TEST
int WINAPI wWinMain(HINSTANCE application_instance, HINSTANCE, PWSTR, int initial_show_state)
try
{
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    const bool restarting = argv && argc > 1 && std::wstring_view(argv[1]) == L"--restart-parent";
    cpplivelab::GuiInstance instance(restarting);
    std::unique_ptr<cpplivelab::RestartChild> restart;
    try
    {
        if (restarting)
        {
            restart = cpplivelab::restart_child(argc, argv, instance.name);
        }
        else if (!instance.acquired)
        {
            struct ExistingWindow
            {
                ULONG_PTR key;
                HWND window = nullptr;
            } existing{instance.key};

            EnumWindows(
                [](HWND window, LPARAM parameter) -> BOOL
                {
                    auto& value = *reinterpret_cast<ExistingWindow*>(parameter);
                    if (reinterpret_cast<ULONG_PTR>(GetPropW(window, L"CppLiveLabInstance")) ==
                            value.key &&
                        IsWindowVisible(window))
                    {
                        value.window = window;
                        return FALSE;
                    }
                    return TRUE;
                },
                reinterpret_cast<LPARAM>(&existing));
            if (existing.window)
            {
                ShowWindow(existing.window, SW_RESTORE);
                SetForegroundWindow(existing.window);
            }
            if (argv)
            {
                LocalFree(argv);
            }
            return 0;
        }
    }
    catch (...)
    {
        if (argv)
        {
            LocalFree(argv);
        }
        return 2;
    }
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const auto rich_edit_library = LoadLibraryW(L"Msftedit.dll");
    if (!rich_edit_library)
    {
        return 1;
    }
    // The folder picker uses COM. Pair successful initialization with CoUninitialize
    // on this same thread once the message loop has ended.
    const auto com_initialization_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = window_procedure;
    window_class.hInstance = application_instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hIcon = application_icon(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
    window_class.hbrBackground = nullptr;
    window_class.lpszClassName = L"CppLiveLabWindow";
    RegisterClassW(&window_class);
    Application application;
    application.restart_instance_name = instance.name;
    if (restart)
    {
        application.preference_store = restart->state.preference_store;
        application.preferences = cpplivelab::load_preferences(application.preference_store);
    }
    else
    {
        wchar_t test_child[8]{}, state_override[32768]{};
        if (GetEnvironmentVariableW(L"CPPLIVELAB_TEST_CHILD", test_child, 8))
        {
            const bool has_override =
                GetEnvironmentVariableW(L"CPPLIVELAB_STATE_HOME", state_override, 32768) ||
                GetEnvironmentVariableW(L"EASIERCPP_STATE_HOME", state_override, 32768);
            // Default-location tests substitute LOCALAPPDATA rather than bypassing
            // migration with STATE_HOME; their registry preferences stay isolated.
            const auto identity = has_override ? std::wstring(state_override)
                                               : cpplivelab::manager_state_directory().wstring();
            application.preference_store =
                L"Software\\CppLiveLab\\Tests\\" +
                decode_console_text(cpplivelab::library_key(encode_utf8(identity)));
            application.preferences = cpplivelab::load_preferences(application.preference_store);
        }
    }
    const auto initial_dpi = GetDpiForSystem();
    RECT desktop{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &desktop, 0);
    HWND window_handle = CreateWindowExW(WS_EX_CONTROLPARENT,
        window_class.lpszClassName,
        application_title().c_str(),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        (std::min)(cpplivelab::scale_pixels(1080, initial_dpi),
            static_cast<int>(desktop.right - desktop.left)),
        (std::min)(cpplivelab::scale_pixels(800, initial_dpi),
            static_cast<int>(desktop.bottom - desktop.top)),
        nullptr,
        nullptr,
        application_instance,
        &application);
    if (!window_handle)
    {
        if (SUCCEEDED(com_initialization_result))
        {
            CoUninitialize();
        }
        return 1;
    }
    SetPropW(window_handle, L"CppLiveLabInstance", reinterpret_cast<HANDLE>(instance.key));
    const auto migration_report = cpplivelab::manager_state_directory() / L"migration-report.txt";
    if (fs::is_regular_file(migration_report))
    {
        application.log(cpplivelab::MessageKind::System,
            L"Settings migration report: " + migration_report.wstring() +
                L". Originals retained; existing EasierCpp data takes precedence. Review any "
                L"conflicts.");
    }
    if (restart && !restart->state.project.empty())
    {
        try
        {
            application.open(restart->state.project);
        }
        catch (...)
        {
            DestroyWindow(window_handle);
            return 3;
        }
    }
    else if (!restart && argv && argc == 2)
    {
        try
        {
            application.open(argv[1]);
        }
        catch (const std::exception& error)
        {
            application.append(L"Error: " + decode_console_text(error.what()) + L"\n");
        }
    }
    if (argv)
    {
        LocalFree(argv);
    }
    if (restart)
    {
        if (!restart->acknowledge_and_wait(instance))
        {
            DestroyWindow(window_handle);
            return 4;
        }
        const auto& rectangle = restart->state.placement.rcNormalPosition;
        if (rectangle.right > rectangle.left + 200 && rectangle.bottom > rectangle.top + 200)
        {
            auto placement = restart->state.placement;
            placement.showCmd = SW_HIDE;
            SetWindowPlacement(window_handle, &placement);
        }
        initial_show_state =
            restart->state.placement.showCmd == SW_SHOWMAXIMIZED ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
    }
    const auto icon_dpi = GetDpiForWindow(window_handle);
    for (const auto [kind, x_metric, y_metric] : {std::array{ICON_SMALL, SM_CXSMICON, SM_CYSMICON},
             std::array{ICON_BIG, SM_CXICON, SM_CYICON}})
    {
        SendMessageW(window_handle,
            WM_SETICON,
            kind,
            reinterpret_cast<LPARAM>(application_icon(GetSystemMetricsForDpi(x_metric, icon_dpi),
                GetSystemMetricsForDpi(y_metric, icon_dpi))));
    }
    ShowWindow(window_handle, initial_show_state);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE &&
            application.tracked_help)
        {
            application.dismiss_shell_help();
            continue;
        }
        if (message.message == WM_SYSKEYDOWN &&
            (message.wParam == 'P' || message.wParam == 'B' || message.wParam == 'T' ||
                message.wParam == 'S' || message.wParam == 'R'))
        {
            SendMessageW(window_handle, WM_SYSKEYDOWN, message.wParam, message.lParam);
            continue;
        }
        if (message.message == WM_KEYDOWN && message.wParam == VK_F10)
        {
            SetFocus(application.find_control(NavigationProject));
            continue;
        }
        if (message.message == WM_KEYDOWN && message.wParam == VK_RETURN &&
            GetFocus() == application.find_control(ProgramInput))
        {
            application.send();
            continue;
        }
        if (!IsDialogMessageW(window_handle, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (SUCCEEDED(com_initialization_result))
    {
        CoUninitialize();
    }
    return static_cast<int>(message.wParam);
}
catch (const std::exception& error)
{
    MessageBoxW(nullptr,
        decode_console_text(error.what()).c_str(),
        L"EasierCpp startup",
        MB_OK | MB_ICONERROR);
    return 1;
}
#endif
