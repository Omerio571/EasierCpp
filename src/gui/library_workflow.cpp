#include "gui/library_workflow.hpp"
#include "tools/tool_workflows.hpp"
#include "cpplivelab/workspace.hpp"
#include "cpplivelab/doctor.hpp"
#include "cpplivelab/session.hpp"
#include "process/output_decoder.hpp"
#include "projects/project_paths.hpp"
#include "build/build_lease.hpp"
#include "libraries/library_compatibility.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <future>
#include <chrono>

namespace cpplivelab
{
namespace
{
namespace fs = std::filesystem;
thread_local ConsoleSession* active_library_session = nullptr;
thread_local bool cancel_requested = false;
thread_local bool restart_cancelled = false;
thread_local bool library_work_active = false;

std::vector<std::string> lines(const fs::path& file)
{
    std::ifstream input(file, std::ios::binary);
    std::vector<std::string> result;
    for (std::string line; std::getline(input, line);)
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        result.push_back(line);
    }
    return result;
}

void write(const fs::path& file, const std::string& content)
{
    if (!(std::ofstream(file, std::ios::binary) << content))
    {
        throw std::runtime_error("Could not save the manager-owned dependency probe.");
    }
}

// A modal progress window pumps messages while the existing owned process class
// drains pipes. No detached readers or unmanaged PowerShell children are introduced.
bool execute(HWND owner,
    ThemeChoice theme,
    const fs::path& executable,
    const std::vector<std::wstring>& arguments,
    const fs::path& directory,
    const LibraryOutput& output,
    ULONGLONG timeout_ms = 900000,
    const std::wstring& task_title = {})
{
    std::vector<FormField> fields;
    FormOptions options;
    options.theme = theme;
    options.progress_only = true;
    options.cancel_default = true;
    options.note = L"Working in a separate manager-owned folder. Detailed output is in Build & "
                   L"System. Cancel stops only this dependency task.";
    if (!task_title.empty())
    {
        options.note = L"Benchmarking an isolated synthetic C++ workload. Progress is shown here "
                       L"and in Build & System. Cancel preserves the current worker count.";
    }

    struct RestoreOwner
    {
        HWND owner;
        HWND previous_focus;
        BOOL enabled;

        ~RestoreOwner()
        {
            if (IsWindow(owner))
            {
                EnableWindow(owner, enabled);
                SetActiveWindow(owner);
                if (IsWindow(previous_focus) && IsWindowEnabled(previous_focus) &&
                    IsWindowVisible(previous_focus))
                {
                    SetFocus(previous_focus);
                }
            }
        }
    } restore{owner, GetFocus(), IsWindowEnabled(owner)};

    // Declared after the restorer so the modal is destroyed before focus returns.
    ProjectFormWindow form(owner,
        !task_title.empty()  ? task_title
        : timeout_ms <= 5000 ? L"Quick library metadata check"
                             : L"Library verification",
        fields,
        options);
    RECT parent{};
    GetWindowRect(owner, &parent);
    SetWindowPos(
        form.window, nullptr, parent.left + 24, parent.top + 24, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    ShowWindow(GetDlgItem(form.window, IDOK), SW_HIDE);
    EnableWindow(owner, FALSE);
    ShowWindow(form.window, SW_SHOW);
    // This is an explicit modal operation. Give keyboard users its only action,
    // and use the shared visible layout to clamp it to the monitor work area.
    form.layout();
    SetFocus(GetDlgItem(form.window, IDCANCEL));
    ConsoleSession session;

    struct ActiveTask
    {
        ActiveTask(ConsoleSession& session)
        {
            active_library_session = &session;
            cancel_requested = false;
        }

        ~ActiveTask()
        {
            active_library_session = nullptr;
            cancel_requested = false;
        }
    } active_task(session);

    ConsoleOutputDecoder decoder;
    try
    {
        session.start_worker(executable, arguments, directory);
        const auto started = GetTickCount64();
        bool cancelled = false;
        while (true)
        {
            const auto text = decoder.append(session.poll());
            if (!text.empty())
            {
                output(text);
                const auto progress = text.find(L"Optimize Workers:");
                if (!task_title.empty() && progress != std::wstring::npos)
                {
                    auto line = text.substr(progress);
                    line.resize(
                        line.find(L'\n') == std::wstring::npos ? line.size() : line.find(L'\n'));
                    SetDlgItemTextW(form.window,
                        1001,
                        (line + L"\nSynthetic estimate. Cancel preserves the current count.")
                            .c_str());
                    form.layout();
                }
            }
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            {
                if (message.message == WM_QUIT)
                {
                    form.finished = true;
                    PostQuitMessage(static_cast<int>(message.wParam));
                    break;
                }
                if (forward_restart_shortcut(form.window, message))
                {
                    continue;
                }
                if (!IsDialogMessageW(form.window, &message))
                {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
            }
            if ((form.finished || cancel_requested || GetTickCount64() - started > timeout_ms) &&
                !cancelled)
            {
                cancelled = true;
                session.stop();
                output(
                    task_title.empty()
                        ? L"Unable to Verify: dependency task cancelled or its time limit reached. "
                          L"No integration registered.\n"
                        : L"Worker benchmark cancelled or timed out; worker setting unchanged.\n");
            }
            if (session.root_exited())
            {
                session.finish_build();
            }
            if (!session.running() && !session.output_pending())
            {
                output(decoder.append({}, true));
                const auto code = session.exit_code();
                session.release_completed();
                return !cancelled && code == 0;
            }
            Sleep(10);
        }
    }
    catch (const std::exception& error)
    {
        output(decode_console_text(
            std::string(task_title.empty() ? "Unable to Verify: "
                                           : "Worker benchmark failed; count unchanged: ") +
            error.what() + "\n"));
        return false;
    }
}
} // namespace

bool library_task_active()
{
    return active_library_session != nullptr || library_work_active;
}

void cancel_library_task()
{
    restart_cancelled = true;
    cancel_requested = true;
    if (active_library_session)
    {
        active_library_session->stop();
    }
}

bool run_library_work(
    HWND owner, ThemeChoice theme, const std::wstring& title, const std::function<void()>& work)
{
    const auto previous_focus = GetFocus();
    const auto enabled = IsWindowEnabled(owner);
    std::vector<FormField> fields;
    FormOptions options;
    options.theme = theme;
    options.progress_only = true;
    options.cancel_default = true;
    options.note = L"Working in the background. Filesystem operations finish at a safe boundary; "
                   L"Cancel or Restart waits for that boundary. Only registered manager-owned data "
                   L"is affected.";
    ProjectFormWindow form(owner, title, fields, options);

    struct Restore
    {
        HWND owner;
        HWND focus;
        BOOL enabled;

        ~Restore()
        {
            library_work_active = false;
            EnableWindow(owner, enabled);
            SetActiveWindow(owner);
            if (IsWindow(focus))
            {
                SetFocus(focus);
            }
        }
    } restore{owner, previous_focus, enabled};

    library_work_active = true;
    cancel_requested = false;
    EnableWindow(owner, FALSE);
    ShowWindow(form.window, SW_SHOW);
    form.layout();
    SetFocus(GetDlgItem(form.window, IDCANCEL));
    auto future = std::async(std::launch::async, work);
    while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
    {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                PostQuitMessage(static_cast<int>(message.wParam));
                form.finished = true;
                break;
            }
            if (forward_restart_shortcut(form.window, message))
            {
                continue;
            }
            if (!IsDialogMessageW(form.window, &message))
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
        Sleep(10);
    }
    try
    {
        future.get();
    }
    catch (...)
    {
        form.close_native_window();
        throw;
    }
    form.close_native_window();
    return !form.finished && !cancel_requested;
}

bool run_library_task(HWND owner,
    ThemeChoice theme,
    const fs::path& executable,
    const std::vector<std::wstring>& arguments,
    const fs::path& directory,
    const LibraryOutput& output,
    ULONGLONG timeout_ms,
    const std::wstring& task_title)
{
    return execute(owner, theme, executable, arguments, directory, output, timeout_ms, task_title);
}

void initialize_uncertain_library_form(FormOptions& options)
{
    options.accept_label = L"Try Adding Anyway";
    options.cancel_default = true;
    options.note = L"Compatibility could not be verified. This library may not integrate or build "
                   L"correctly. Would you like to try adding it anyway?";
}

void initialize_github_repository_form(std::vector<FormField>& repository, FormOptions& options)
{
    // Fresh Add sessions carry no repository, version, result or validation state.
    options = FormOptions{};
    options.accept_label = L"Retrieve Versions";
    options.note =
        L"Public GitHub and gitlab.com repositories only. Version discovery does not execute "
        L"downloaded code. Not every C++ repository is an installable library.";
    options.disable_invalid_accept = true;
    repository = {{L"Repository URL (GitHub or GitLab)", L""}};
    repository.front().placeholder = L"https://github.com/owner/repository";
    options.validate = [](const auto& fields) -> std::wstring
    {
        return std::regex_match(path_support::path_utf8(fields[0].value),
                   std::regex("(https://github\\.com/[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+/?|"
                              "(https://)?gitlab\\.com/[A-Za-z0-9_.-]+(/[A-Za-z0-9_.-]+)+/?)"))
                   ? L""
                   : L"Enter a GitHub or gitlab.com repository URL without credentials or query "
                     L"parameters.";
    };
}

std::optional<std::string> github_library_dialog(HWND owner,
    ThemeChoice theme,
    const fs::path& manager_cli,
    const LibraryOutput& output,
    const std::optional<LibraryRecord>& existing,
    LibraryEdit operation)
{
    FormOptions options;
    std::vector<FormField> repository;
    restart_cancelled = false;
    initialize_github_repository_form(repository, options);
    options.theme = theme;
    if (existing)
    {
        repository[0].value =
            (existing->repository.starts_with("gitlab.com/") ? L"https://"
                                                             : L"https://github.com/") +
            path_support::from_utf8(existing->repository).wstring();
    }
    if (!existing && !project_form(owner, L"GitHub / GitLab Repository", repository, options))
    {
        return {};
    }
    options.disable_invalid_accept = false;
    const auto cache =
        manager_state_directory() / L"dependency-probes" /
        (std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
    fs::create_directories(cache);
    wchar_t system[MAX_PATH]{};
    GetSystemDirectoryW(system, MAX_PATH);
    const auto powershell = fs::path(system) / L"WindowsPowerShell/v1.0/powershell.exe";
    const auto script = runtime_resources() / L"scripts/github-library.ps1";
    fs::path archive_extractor;
    for (const auto& tool : discover_toolchain(system_discovery_inputs()).tools)
    {
        if (tool.name == "cmake")
        {
            archive_extractor = tool.installed_path;
        }
    }
    if (archive_extractor.empty())
    {
        throw std::runtime_error(
            "CMake is required for archive extraction and verification. Run Doctor.");
    }
    auto passive_directory = cache;
    std::wstring expected_archive_hash;
    const auto passive =
        [&](const std::wstring& action, const std::wstring& version = L"Latest Stable")
    {
        std::vector<std::wstring> arguments{L"-NoProfile",
            L"-ExecutionPolicy",
            L"Bypass",
            L"-File",
            script.wstring(),
            L"-Action",
            action,
            L"-Repository",
            repository[0].value,
            L"-Version",
            version,
            L"-OutputDirectory",
            passive_directory.wstring(),
            L"-ExpectedArchiveHash",
            expected_archive_hash,
            L"-CMakeExecutable",
            archive_extractor.wstring(),
            L"-MetadataCacheDirectory",
            (manager_state_directory() / L"github-metadata").wstring()};
        if (operation == LibraryEdit::Repair)
        {
            arguments.push_back(L"-Repair");
        }
        while (true)
        {
            const bool succeeded = execute(owner,
                theme,
                powershell,
                arguments,
                runtime_resources(),
                output,
                action == L"Discover" ? 5000 : 900000);
            if (!succeeded)
            {
                output(
                    L"Unable to Verify: repository metadata/download was unavailable or cancelled. "
                    L"Nothing was registered.\n");
            }
            if (succeeded || action != L"Download" || restart_cancelled)
            {
                return succeeded;
            }
            std::vector<FormField> retry;
            FormOptions failed;
            failed.theme = theme;
            failed.note =
                L"Unable to Verify: the pinned source download or extraction failed. See "
                L"Build & System for details. Your repository and exact version are retained. "
                L"Retry the download, or Cancel to return to Libraries. No project changed.";
            failed.accept_label = L"Retry Download";
            failed.cancel_default = true;
            if (!project_form(owner, L"Library download failed", retry, failed))
            {
                return false;
            }
        }
    };
    const bool select_version =
        !existing || operation == LibraryEdit::UpdateVersion || existing->revision.empty();
    if (select_version)
    {
        const bool discovered = passive(L"Discover");
        if (restart_cancelled)
        {
            return {};
        }
        if (!discovered || !existing)
        {
            std::vector<FormField> warning;
            FormOptions uncertain;
            uncertain.theme = theme;
            initialize_uncertain_library_form(uncertain);
            output(L"Unable to Verify: quick version metadata does not prove library "
                   L"compatibility. No source download or third-party execution performed.\n");
            if (!project_form(owner, L"Unable to Verify", warning, uncertain))
            {
                return {};
            }
        }
    }
    const auto versions =
        select_version ? lines(cache / L"versions.txt") : std::vector<std::string>{};
    std::wstring available;
    for (const auto& version : versions)
    {
        available += decode_console_text(version) + L"\r\n";
    }
    std::vector<FormField> selection{{L"Available releases and tags", available, {}, true},
        {L"Reference: tag, branch, commit, or Latest Stable",
            existing ? decode_console_text(existing->version)
                     : (versions.empty() ? L"" : decode_console_text(versions.front()))}};
    selection[0].read_only = true;
    options.validate = {};
    options.note =
        L"Latest Stable excludes prereleases. An exact commit is recorded; it will never silently "
        L"update. Listings show up to 100 releases and 100 tags; older valid tags can be typed.";
    options.accept_label = L"Validate Version";
    if (existing)
    {
        options.note += L"\nCurrent pin: " + decode_console_text(existing->version) + L" " +
                        decode_console_text(existing->revision);
    }
    while (select_version && project_form(owner, L"Library Version", selection, options))
    {
        if (passive(L"Resolve", selection[1].value))
        {
            break;
        }
        if (restart_cancelled)
        {
            return {};
        }
        options.note = L"Validation failed. Inspect Build & System for missing version, rate limit "
                       L"or network details. Available alternatives remain below.";
    }
    if (!select_version)
    {
        write(cache / L"resolved.txt",
            existing->repository + "\n" + existing->revision + "\n" + existing->version + "\n");
    }
    if (!fs::is_regular_file(cache / L"resolved.txt"))
    {
        return {};
    }
    const auto resolved = lines(cache / L"resolved.txt");
    if (resolved.size() != 3)
    {
        throw std::runtime_error("Invalid resolved dependency metadata.");
    }
    LibraryRecord record;
    record.repository = resolved[0];
    record.revision = resolved[1];
    record.version = resolved[2];
    auto repository_id = record.repository;
    std::replace(repository_id.begin(), repository_id.end(), '/', '-');
    record.id = (record.repository.starts_with("gitlab.com/") ? "gitlab-" : "github-") +
                repository_id + "-" + record.revision;
    if (existing && existing->revision == record.revision && !existing->builtin)
    {
        record.id = existing->id;
    }
    record.name = record.repository + " @ " + record.version;
    register_dependency_cache({library_key(path_support::path_utf8(cache)),
        record.id,
        "Temporary verification",
        cache,
        {},
        record.name});
    const BuildLease probe_lease(
        manager_state_directory() / L"cache-locks" / library_key(path_support::path_utf8(cache)));
    const auto source_cache = library_source_cache(record);
    fs::create_directories(source_cache);
    const BuildLease source_lease(manager_state_directory() / L"cache-locks" /
                                  library_key(path_support::path_utf8(source_cache)));
    write(source_cache / L"resolved.txt",
        resolved[0] + "\n" + resolved[1] + "\n" + resolved[2] + "\n");
    register_dependency_cache({library_key(path_support::path_utf8(source_cache)),
        record.id,
        "Downloaded source archive",
        source_cache,
        {},
        record.repository + "@" + record.revision});
    passive_directory = source_cache;
    if (existing && existing->revision == record.revision)
    {
        expected_archive_hash = decode_console_text(existing->archive_hash);
    }
    if (!passive(L"Download"))
    {
        return {};
    }
    const auto downloaded = lines(source_cache / L"downloaded.txt");
    if (downloaded.size() != 2)
    {
        throw std::runtime_error("Invalid downloaded dependency metadata.");
    }
    const auto source = path_support::from_utf8(downloaded[0]);
    LibraryAssessment assessment;
    if (!run_library_work(owner,
            theme,
            L"Inspecting library compatibility",
            [&]
            {
                assessment = assess_library(source);
            }))
    {
        output(L"Unable to Verify: compatibility inspection was cancelled.\n");
        return {};
    }
    output(decode_console_text(
        compatibility_label(assessment.status) + ": " + assessment.reason + "\n"));
    if (assessment.status == LibraryCompatibility::NotLibrary)
    {
        std::vector<FormField> blocked;
        FormOptions incompatible;
        incompatible.theme = theme;
        incompatible.note =
            decode_console_text(compatibility_label(assessment.status) + ": " + assessment.reason);
        incompatible.accept_label = L"Return to Libraries";
        project_form(owner, L"Incompatible", blocked, incompatible);
        return {};
    }
    record.archive_hash = downloaded[1];
    if (existing && existing->revision == record.revision && !existing->archive_hash.empty() &&
        existing->archive_hash != record.archive_hash)
    {
        throw std::runtime_error(
            "Pinned archive hash changed. Previous project configuration retained.");
    }
    std::vector<FormField> method{
        {L"Integration method", L"CMake library", {L"CMake library", L"Header-only include"}},
        {L"Header-only include directory (relative to repository)", L"include"},
        {L"Header to compile (relative to include directory)", L""},
        {L"CMake options (one NAME=VALUE per line)", L"", {}, true}};
    method[3].advanced = true;
    if (existing)
    {
        method[1].value = decode_console_text(existing->include_directory);
        method[2].value = decode_console_text(existing->header);
        if (!existing->include_directory.empty())
        {
            method[0].value = L"Header-only include";
        }
        for (const auto& option : existing->cmake_options)
        {
            method[3].value += decode_console_text(option) + L"\n";
        }
    }
    options.note = L"Repository: " + decode_console_text(resolved[0]) + L"\nCommit: " +
                   decode_console_text(resolved[1]) +
                   L"\nCMake can execute arbitrary third-party code under your account. Approve "
                   L"only a repository you trust. A disposable configure/link probe runs before "
                   L"registration. No existing project files are changed.";
    options.accept_label = L"Approve Verification";
    options.cancel_default = true;
    options.validate = [&](const auto& fields) -> std::wstring
    {
        if (fields[0].value == L"CMake library" && !fs::is_regular_file(source / L"CMakeLists.txt"))
        {
            return L"This source has no root CMakeLists.txt. Select Header-only include if it "
                   L"exports a public header, or Cancel. Your selected repository/version is "
                   L"retained.";
        }
        return L"";
    };
    if (!project_form(owner, L"Approve dependency verification", method, options))
    {
        return {};
    }
    std::istringstream build_options(path_support::path_utf8(method[3].value));
    std::string option_cmake;
    for (std::string option; std::getline(build_options, option);)
    {
        if (!option.empty() && option.back() == '\r')
        {
            option.pop_back();
        }
        if (option.empty())
        {
            continue;
        }
        std::smatch match;
        if (!std::regex_match(
                option, match, std::regex("([A-Za-z_][A-Za-z0-9_]*)=([A-Za-z0-9_.+-]+)")))
        {
            throw std::runtime_error(
                "CMake options must be NAME=VALUE without executable CMake text.");
        }
        record.cmake_options.push_back(option);
        option_cmake +=
            "set(" + match[1].str() + " \"" + match[2].str() + "\" CACHE STRING \"\" FORCE)\n";
    }
    const auto probe = cache / L"probe";
    fs::create_directory(probe);
    std::string setup = "# EasierCpp dependency target discovery\n"
                        "cmake_minimum_required(VERSION 3.24)\nproject(DependencyProbe LANGUAGES C "
                        "CXX)\nadd_executable(cpplivelab_probe "
                        "main.cpp)\ntarget_compile_features(cpplivelab_probe PRIVATE cxx_std_20)\n";
    const bool header_only = method[0].value == L"Header-only include";
    std::string target;
    std::string include_directory;
    if (header_only)
    {
        const std::regex safe_path("[A-Za-z0-9_./+-]+");
        include_directory = path_support::path_utf8(method[1].value);
        const auto header = path_support::path_utf8(method[2].value);
        record.header = header;
        if (!std::regex_match(include_directory, safe_path) ||
            !std::regex_match(header, safe_path) ||
            include_directory.find("..") != std::string::npos ||
            header.find("..") != std::string::npos || fs::path(include_directory).is_absolute() ||
            fs::path(header).is_absolute() ||
            !fs::is_regular_file(source / include_directory / header))
        {
            throw std::runtime_error("Choose an existing relative include directory and header "
                                     "inside the downloaded source.");
        }
        write(
            probe / L"main.cpp", "#include <" + header + ">\n\nint main()\n{\n    return 0;\n}\n");
        record.include_directory = include_directory;
        setup += "target_include_directories(cpplivelab_probe PRIVATE [==[" +
                 path_support::path_utf8(source / include_directory) + "]==])\n";
    }
    else
    {
        if (!fs::is_regular_file(source / L"CMakeLists.txt"))
        {
            throw std::runtime_error(
                "Repository has no root CMakeLists.txt. Choose suitable header-only integration or "
                "follow its manual build instructions.");
        }
        write(probe / L"main.cpp", "int main()\n{\n    return 0;\n}\n");
        setup = cmake_dependency_probe(source);
        const auto at = setup.find("add_subdirectory(");
        setup.insert(at, option_cmake);
    }
    write(probe / L"CMakeLists.txt", setup);
    const auto verify = [&]()
    {
        while (!restart_cancelled)
        {
            const bool success = execute(owner,
                theme,
                manager_cli,
                {L"manage-build",
                    probe.wstring(),
                    L"--target",
                    L"cpplivelab_probe",
                    L"--allow-configure-writes"},
                cache,
                output);
            register_probe_dependency_caches(probe, record);
            if (success)
            {
                return true;
            }
            if (restart_cancelled)
            {
                return false;
            }
            std::vector<FormField> retry;
            FormOptions failed;
            failed.theme = theme;
            failed.note =
                L"Unable to Verify: verification failed or was cancelled. See Build & System "
                L"for the exact error. Your repository, version and integration options are "
                L"retained. No library was registered or project changed. Retry the same "
                L"approved verification, or Cancel to return to Libraries.";
            failed.accept_label = L"Retry Verification";
            failed.cancel_default = true;
            if (!project_form(owner, L"Library verification failed", retry, failed))
            {
                return false;
            }
        }
        return false;
    };
    if (!verify())
    {
        output(L"Unable to Verify: dependency verification failed or was cancelled. Nothing "
               L"registered. See Build & System for the configuration, compile or link error.\n");
        return {};
    }
    if (!header_only)
    {
        auto discovered = discovered_library_targets(probe);
        if (discovered.empty())
        {
            const auto result = assess_library(source, {}, true);
            output(decode_console_text(
                compatibility_label(result.status) +
                ": no supported reusable CMake library targets were discovered. " + result.reason +
                "\n"));
            return {};
        }
        while (true)
        {
            std::vector<std::wstring> public_targets;
            std::vector<std::wstring> advanced_targets;
            std::wstring details;
            for (const auto& item : discovered)
            {
                (item.primary ? public_targets : advanced_targets)
                    .push_back(decode_console_text(item.name));
                details += decode_console_text(
                               item.name + " | " + item.type +
                               (item.alias_of.empty() ? "" : " | alias of " + item.alias_of)) +
                           L"\r\n";
            }
            std::wstring previous_public, previous_advanced;
            if (existing)
            {
                for (const auto& item : existing->targets)
                {
                    const auto name = decode_console_text(item);
                    if (std::find(public_targets.begin(), public_targets.end(), name) !=
                        public_targets.end())
                    {
                        previous_public += name + L"\n";
                    }
                    else if (std::find(advanced_targets.begin(), advanced_targets.end(), name) !=
                             advanced_targets.end())
                    {
                        previous_advanced += name + L"\n";
                    }
                }
            }
            std::vector<FormField> choose{{L"Public library targets (Ctrl/Shift selects multiple)",
                                              previous_public,
                                              public_targets},
                {L"Action", L"Verify Selection", {L"Verify Selection", L"Refresh Discovery"}},
                {L"Target details", details, {}, true},
                {L"Advanced/internal targets", previous_advanced, advanced_targets},
                {L"Manual exact targets (one per line; checked by CMake)", L"", {}, true}};
            choose[0].multiple_selection = true;
            choose[2].read_only = true;
            choose[2].advanced = true;
            choose[3].multiple_selection = true;
            choose[3].advanced = true;
            choose[4].advanced = true;
            options.note =
                L"Targets come from this configured dependency graph, including verified "
                L"public aliases. Choose the modules your consumer actually uses. No target is "
                L"selected for a fresh Add. Internal/helper targets are under Advanced Options.";
            options.accept_label = L"Continue";
            options.cancel_default = false;
            if (operation == LibraryEdit::Repair && existing)
            {
                record.targets = existing->targets;
            }
            else
            {
                if (!project_form(owner, L"Select Library Targets", choose, options))
                {
                    return {};
                }
                if (choose[1].value == L"Refresh Discovery")
                {
                    write(probe / L"CMakeLists.txt", setup);
                    if (!verify())
                    {
                        return {};
                    }
                    discovered = discovered_library_targets(probe);
                    continue;
                }
                record.targets = parse_library_targets(path_support::path_utf8(
                    choose[0].value + L"\n" + choose[3].value + L"\n" + choose[4].value));
            }
            if (record.targets.empty())
            {
                output(L"Choose at least one actual library target. No unsuitable default was "
                       L"selected.\n");
                continue;
            }
            std::vector<LibraryTarget> selected_targets;
            for (const auto& selected : record.targets)
            {
                const auto found = std::find_if(discovered.begin(),
                    discovered.end(),
                    [&](const auto& item)
                    {
                        return item.name == selected;
                    });
                if (found == discovered.end())
                {
                    output(L"Unable to Verify: select an actual discovered library target; "
                           L"executable/custom targets cannot be registered as libraries.\n");
                    return {};
                }
                selected_targets.push_back(*found);
            }
            std::string links = "\nforeach(selected IN ITEMS\n";
            for (const auto& name : record.targets)
            {
                links += "    " + name + "\n";
            }
            links +=
                ")\n    if(NOT TARGET \"${selected}\")\n"
                "        message(FATAL_ERROR \"Selected target does not exist: ${selected}\")\n"
                "    endif()\nendforeach()\ntarget_link_libraries(cpplivelab_probe PRIVATE\n";
            for (const auto& name : record.targets)
            {
                links += "    " + name + "\n";
            }
            links += ")\n";
            write(probe / L"CMakeLists.txt", setup + links);
            if (!verify())
            {
                output(L"Unable to Verify: selected targets failed configuration/link "
                       L"verification. Nothing "
                       L"registered.\n");
                return {};
            }
            assessment = assess_library(source, selected_targets, true);
            break;
        }
    }
    if (header_only)
    {
        assessment = assess_library(source, {}, true, source / include_directory / record.header);
    }
    if (assessment.status != LibraryCompatibility::Compatible)
    {
        output(decode_console_text(compatibility_label(assessment.status) + ": " +
                                   assessment.reason + " Nothing registered.\n"));
        return {};
    }
    save_known_library(record);
    // Publish only after the real consumer probe succeeds. New projects copy the
    // pinned URL/hash snippet; they never depend on an absolute manager cache path.
    output(L"Compatible: verified dependency probe; pinned integration is selectable. Project "
           L"integration "
           L"still requires its own build.\n");
    return record.id;
}
} // namespace cpplivelab
