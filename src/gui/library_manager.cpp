#include "gui/library_manager.hpp"
#include "projects/project_paths.hpp"
#include "libraries/library_compatibility.hpp"
#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>

namespace cpplivelab
{
namespace
{
namespace fs = std::filesystem;

std::wstring wide(const std::string& text)
{
    return path_support::from_utf8(text).wstring();
}

struct Snapshot
{
    std::vector<LibraryRecord> libraries;
    std::vector<LibraryRecord> project_records;
    std::vector<DependencyCache> caches;
    std::map<std::string, std::vector<fs::path>> references;
};

Snapshot snapshot(const fs::path& project)
{
    Snapshot result;
    for (const auto& item : supported_libraries())
    {
        if (const auto record = known_library(item.id))
        {
            result.libraries.push_back(*record);
        }
    }
    for (const auto* builtin : {"fmt", "nlohmann_json"})
    {
        if (library_hidden(builtin))
        {
            result.libraries.push_back(*known_library(builtin));
        }
    }
    if (!project.empty())
    {
        result.project_records = project_libraries(project);
        for (const auto& record : result.project_records)
        {
            const auto entry = std::find_if(result.libraries.begin(),
                result.libraries.end(),
                [&](const auto& item)
                {
                    return item.id == record.id;
                });
            if (entry == result.libraries.end())
            {
                result.libraries.push_back(record);
            }
            else
            {
                *entry = record;
            }
        }
    }
    result.caches = dependency_caches();
    for (const auto& cache : result.caches)
    {
        if (const auto record = known_library(cache.library_id))
        {
            result.references[record->id] = library_references(*record);
        }
    }
    for (const auto& record : result.libraries)
    {
        result.references[record.id] = library_references(record);
    }
    return result;
}

std::wstring details(const Snapshot& state, const LibraryRecord& record)
{
    std::wstring text =
        wide(record.name) + L"\r\nRepository: " + wide(record.repository) + L"\r\nVersion: " +
        wide(record.version) + L"\r\nPinned commit: " +
        wide(record.integration != "fetch" ? "Not applicable (local/package integration)"
             : record.revision.empty()     ? "Built-in release archive with SHA256"
                                           : record.revision) +
        L"\r\nTargets:";
    for (const auto& target : record.targets)
    {
        text += L" " + wide(target);
    }
    text += record.builtin ? L"\r\nProtected built-in; catalog visibility may be hidden."
                           : L"\r\nUser-added integration; project pins are independent.";
    text += L"\r\nIntegration: " + wide(record.integration) + L"\r\nPackage: " +
            wide(record.package) + L"\r\nLocal path: " + wide(record.local_path) +
            L"\r\nStatus: recorded integration; Build verifies current availability."
            L"\r\nUpdate availability: not checked (select a version explicitly).";
    const bool attached = std::any_of(state.project_records.begin(),
        state.project_records.end(),
        [&](const auto& item)
        {
            return item.id == record.id;
        });
    text += attached ? L"\r\nOwnership: managed by Cpp for the selected project."
                     : L"\r\nOwnership: catalog entry, not attached to the selected project.";
    text += L"\r\nKnown project references (not an exhaustive disk scan):";
    if (const auto references = state.references.find(record.id);
        references != state.references.end())
    {
        for (const auto& project : references->second)
        {
            text += L"\r\n" + project.wstring();
        }
    }
    std::uintmax_t total = 0;
    size_t present = 0, missing = 0;
    for (const auto& cache : state.caches)
    {
        if (cache.library_id == record.id)
        {
            total += cache.bytes;
            cache.present ? ++present : ++missing;
        }
    }
    text += L"\r\nRegistered cache bytes: " + std::to_wstring(total) + L"; present: " +
            std::to_wstring(present) + L"; missing: " + std::to_wstring(missing);
    if (missing)
    {
        text += L"\r\nMissing dependency data: use Reinstall / Repair to retain this exact pin.";
    }
    return text;
}

bool confirm(const LibraryManagerContext& context,
    const std::wstring& title,
    const std::wstring& explanation,
    const std::wstring& action,
    std::vector<FormField>& fields)
{
    FormOptions options;
    options.theme = context.theme;
    options.note = explanation;
    options.accept_label = action;
    options.cancel_default = true;
    return project_form(context.owner, title, fields, options);
}

std::string executable_target(const LibraryManagerContext& context)
{
    if (!context.options.target.empty())
    {
        return path_support::path_utf8(context.options.target);
    }
    std::ifstream input(context.project / L"CMakeLists.txt");
    const std::string content{std::istreambuf_iterator<char>(input), {}};
    if (content.find("# CppLiveLab starter: explicit sources;") != content.npos)
    {
        return "app";
    }
    throw std::runtime_error(
        "Build/configure this imported project and select its consumer executable target first.");
}

bool apply_library_change(const LibraryManagerContext& context,
    const std::vector<LibraryRecord>& records,
    bool build_consumer = true)
{
    if (context.project.empty())
    {
        throw std::runtime_error("Select a project before changing project dependencies.");
    }
    if (context.busy())
    {
        throw std::runtime_error(
            "Stop the active build/program/test before changing project libraries.");
    }
    const auto target = executable_target(context);
    const auto change = preview_library_change(context.project, records, target);
    std::vector<FormField> preview{
        {L"Current root CMakeLists.txt", wide(change.old_root), {}, true},
        {L"Proposed root CMakeLists.txt", wide(change.new_root), {}, true},
        {L"Current dependency file", wide(change.old_dependencies), {}, true},
        {L"Proposed dependency file", wide(change.new_dependencies), {}, true}};
    for (auto& field : preview)
    {
        field.read_only = true;
    }
    if (!confirm(context,
            L"Approve Library Changes",
            build_consumer
                ? L"Review the exact managed-file changes. A fresh isolated consumer build "
                  L"validates them before success is reported. Previous pins/configuration are "
                  L"restored "
                  L"on failure; old build caches remain. Source/assets/presets are never "
                  L"rewritten. "
                  L"Version API changes may require your own source edits."
                : L"Remove only the selected managed declarations and links. Keep caches, other "
                  L"libraries "
                  L"and all source files. Existing source that still uses this library may fail on "
                  L"the next build.",
            build_consumer ? L"Approve & Verify" : L"Approve Removal",
            preview))
    {
        return false;
    }
    const bool success = apply_library_change(change,
        true,
        [&]
        {
            if (!build_consumer)
            {
                return true;
            }
            return run_library_task(context.owner,
                context.theme,
                context.cli,
                {L"manage-rebuild",
                    context.project.wstring(),
                    L"--approve-recovery",
                    L"--config",
                    context.configuration,
                    L"--target",
                    wide(target),
                    L"--allow-configure-writes",
                    L"--transaction",
                    wide(change.token)},
                context.project,
                context.output);
        });
    context.changed();
    context.output(success ? L"Library change completed and its metadata was saved.\n"
                           : L"Library verification failed/cancelled. Previous configuration "
                             L"restored; old cache retained.\n");
    return success;
}

void add(const LibraryManagerContext& context, const Snapshot& state)
{
    FormOptions method_options;
    method_options.theme = context.theme;
    method_options.note =
        L"Choose metadata you already have. Packages use the configured toolchain/package manager. "
        L"Precompiled libraries need a CMake package with imported targets. Local CMake code "
        L"executes only after the separate preview approval.";
    std::vector<FormField> method{{L"Integration method",
        L"Known / GitHub library",
        {L"Known / GitHub library",
            L"Installed CMake package",
            L"Local CMake source",
            L"Local header-only directory"}}};
    if (!project_form(context.owner, L"Add Library", method, method_options))
    {
        return;
    }
    if (method[0].value != L"Known / GitHub library")
    {
        LibraryRecord record;
        record.integration = method[0].value == L"Installed CMake package" ? "package"
                             : method[0].value == L"Local CMake source"    ? "source"
                                                                           : "headers";
        std::vector<FormField> fields{{L"Library name", L""},
            {L"CMake package name (packages only)", L""},
            {L"Version (optional numeric package version)", L""},
            {L"Local source / include / package prefix", L""},
            {L"Actual CMake targets (one per line; not needed for headers)", L"", {}, true},
            {L"Package components (optional, one per line)", L"", {}, true}};
        fields[3].browse = BrowseKind::Folder;
        method_options.accept_label = L"Preview Integration";
        method_options.validate = [](const auto& values)
        {
            return values[0].value.empty() ? L"Enter a library name." : L"";
        };
        if (!project_form(context.owner, L"Configure Library Integration", fields, method_options))
        {
            return;
        }
        record.name = path_support::path_utf8(fields[0].value);
        record.id = "local-" + library_key(record.name);
        record.package = path_support::path_utf8(fields[1].value);
        record.version = path_support::path_utf8(fields[2].value);
        record.local_path = path_support::path_utf8(fields[3].value);
        record.targets = parse_library_targets(path_support::path_utf8(fields[4].value));
        record.components = path_support::path_utf8(fields[5].value);
        if (record.integration != "package")
        {
            LibraryAssessment assessment;
            if (!run_library_work(context.owner,
                    context.theme,
                    L"Inspecting library compatibility",
                    [&]
                    {
                        assessment = assess_library(path_support::from_utf8(record.local_path));
                    }) ||
                context.restarting())
            {
                return;
            }
            context.output(
                wide(compatibility_label(assessment.status) + ": " + assessment.reason + "\n"));
            if (!assessment.native_candidate)
            {
                throw std::runtime_error(
                    compatibility_label(assessment.status) + ": " + assessment.reason);
            }
        }
        auto records = state.project_records;
        if (std::any_of(records.begin(),
                records.end(),
                [&](const auto& item)
                {
                    return item.id == record.id ||
                           (!record.package.empty() && item.package == record.package);
                }))
        {
            throw std::runtime_error(
                "Library already managed by this project. Use Edit Integration.");
        }
        records.push_back(record);
        apply_library_change(context, records);
        return;
    }
    FormOptions options;
    options.theme = context.theme;
    options.library_picker = true;
    options.note = L"Add one or more libraries. Existing project dependencies are retained. "
                   L"The next step previews the exact configuration changes.";
    options.accept_label = L"Preview Addition";
    options.add_github_library = [&](HWND owner)
    {
        return github_library_dialog(owner, context.theme, context.cli, context.output);
    };
    std::vector<FormField> fields;
    if (!project_form(context.owner, L"Add Project Libraries", fields, options) ||
        options.libraries.empty())
    {
        return;
    }
    auto records = state.project_records;
    for (const auto& id : options.libraries)
    {
        const auto record = known_library(id);
        if (!record)
        {
            throw std::runtime_error("Selected library metadata is missing.");
        }
        if (std::any_of(records.begin(),
                records.end(),
                [&](const auto& current)
                {
                    return current.repository == record->repository;
                }))
        {
            throw std::runtime_error(
                "That repository is already attached. Use Update Version or Change Target.");
        }
        records.push_back(*record);
    }
    apply_library_change(context, records);
}

void caches(const LibraryManagerContext& context,
    const Snapshot& state,
    const std::optional<std::string>& library)
{
    std::vector<DependencyCache> entries;
    std::vector<std::wstring> labels;
    std::uintmax_t total = 0;
    for (const auto& cache : state.caches)
    {
        if (library && cache.library_id != *library)
        {
            continue;
        }
        entries.push_back(cache);
        const auto record = known_library(cache.library_id);
        const auto name =
            record ? wide(record->name) : L"Unregistered/failed dependency verification";
        labels.push_back(std::to_wstring(labels.size() + 1) + L". " + name + L" - " +
                         wide(cache.kind) +
                         (!cache.issue.empty() ? L" (unavailable)"
                             : cache.present   ? L" (" + std::to_wstring(cache.bytes) + L" bytes)"
                                               : L" (missing)"));
        total += cache.bytes;
    }
    if (entries.empty())
    {
        context.output(L"No registered cache entries for this selection. Legacy unregistered "
                       L"folders are preserved.\n");
        return;
    }
    FormOptions options;
    options.theme = context.theme;
    options.note =
        L"Measured registered cache bytes: " + std::to_wstring(total) +
        L" (entries marked unavailable are excluded). Sizes were measured in the background. "
        L"Source, compiled and temporary data are separate. "
        L"Deletion affects only the selected registered entry, never external package "
        L"installations.";
    options.accept_label = L"Continue";
    std::vector<FormField> fields{{L"Managed cache entry", labels.front(), labels},
        {L"Action", L"Inspect", {L"Inspect", L"Delete Selected Cache"}},
        {L"Details", L"", {}, true}};
    fields[2].read_only = true;
    fields[2].multiline_height = 150;
    options.changed = [&](auto& values)
    {
        const auto index = static_cast<size_t>(
            std::find(labels.begin(), labels.end(), values[0].value) - labels.begin());
        if (index < entries.size())
        {
            const auto& cache = entries[index];
            values[2].value = L"Location: " + cache.path.wstring() + L"\r\nKind: " +
                              wide(cache.kind) + L"\r\nIdentity: " + wide(cache.identity) +
                              L"\r\nBytes: " +
                              (cache.issue.empty() ? std::to_wstring(cache.bytes)
                                                   : L"unknown: " + wide(cache.issue));
            if (const auto record = known_library(cache.library_id))
            {
                const auto references = state.references.find(record->id);
                if (references != state.references.end())
                {
                    for (const auto& project : references->second)
                    {
                        values[2].value += L"\r\nReference: " + project.wstring();
                    }
                }
            }
        }
    };
    options.changed(fields);
    while (project_form(context.owner, L"Dependency Cache", fields, options))
    {
        if (fields[1].value == L"Inspect")
        {
            continue;
        }
        const auto index = static_cast<size_t>(
            std::find(labels.begin(), labels.end(), fields[0].value) - labels.begin());
        const auto& entry = entries.at(index);
        std::vector<FormField> approval{{L"Selected cache", fields[2].value, {}, true},
            {L"Allow deletion if shared by several known projects", L"No", {L"No", L"Yes"}}};
        approval[0].read_only = true;
        if (!confirm(context,
                L"Delete Managed Cache",
                L"Delete only this registered cache entry? Current builds/programs and links are "
                L"checked. "
                L"Project pins, source, assets and unrelated caches remain. A later build may need "
                L"Repair "
                L"and a download. Shared versions are protected unless you explicitly allow "
                L"deletion.",
                L"Delete Selected Entry",
                approval))
        {
            return;
        }
        bool deleted = false;
        run_library_work(context.owner,
            context.theme,
            L"Deleting selected dependency cache",
            [&]
            {
                deleted = delete_dependency_cache(entry.id, true, approval[1].value == L"Yes");
            });
        context.changed();
        context.output(deleted ? L"Selected cache deletion verified. Project pins retained; Repair "
                                 L"can restore the original version.\n"
                               : L"Selected cache was already missing. Nothing was deleted; Repair "
                                 L"can restore its exact pin.\n");
        return;
    }
}
} // namespace

void add_project_libraries_dialog(const LibraryManagerContext& context)
{
    add(context, snapshot(context.project));
}

void manage_libraries_dialog(const LibraryManagerContext& context)
{
    while (!context.restarting())
    {
        Snapshot state;
        if (!run_library_work(context.owner,
                context.theme,
                L"Inspecting managed libraries",
                [&]
                {
                    state = snapshot(context.project);
                }) ||
            context.restarting())
        {
            return;
        }
        std::vector<std::wstring> labels;
        for (const auto& record : state.libraries)
        {
            labels.push_back(wide(record.name) + (record.builtin ? L" [built-in]" : L"") +
                             (library_hidden(record.id) ? L" [hidden]" : L""));
        }
        if (labels.empty())
        {
            context.output(L"No known library entries. Use Add Library to register one.\n");
            return;
        }
        FormOptions options;
        options.theme = context.theme;
        options.accept_label = L"Continue";
        options.note =
            L"Add attaches libraries; management changes only an explicitly selected entry. "
            L"Project-specific pins survive catalog deletion. Cache actions require separate "
            L"confirmation. "
            L"Alt+R requests a safe application restart, including during background tasks.";
        std::vector<FormField> fields{{L"Known or project library", labels.front(), labels},
            {L"Action",
                L"Inspect",
                {L"Inspect",
                    L"Add Library",
                    L"Edit Integration",
                    L"Update Version",
                    L"Change Target",
                    L"Remove from Project",
                    L"Delete Known Library / Hide Built-in",
                    L"Show Hidden Built-in",
                    L"Delete Cache",
                    L"Reinstall / Repair",
                    L"All Managed Caches",
                    L"Recover Interrupted Change"}},
            {L"Selected library details", details(state, state.libraries.front()), {}, true}};
        fields[2].read_only = true;
        fields[2].multiline_height = 150;
        options.changed = [&](auto& values)
        {
            const auto index = static_cast<size_t>(
                std::find(labels.begin(), labels.end(), values[0].value) - labels.begin());
            if (index < state.libraries.size())
            {
                values[2].value = details(state, state.libraries[index]);
            }
        };
        if (!project_form(context.owner, L"Manage Libraries", fields, options) ||
            context.restarting())
        {
            return;
        }
        const auto index = static_cast<size_t>(
            std::find(labels.begin(), labels.end(), fields[0].value) - labels.begin());
        const auto record = state.libraries.at(index);
        const auto action = fields[1].value;
        try
        {
            if (action == L"Inspect")
            {
                continue;
            }
            if (action == L"Add Library")
            {
                add(context, state);
            }
            else if (action == L"All Managed Caches" || action == L"Delete Cache")
            {
                caches(context,
                    state,
                    action == L"Delete Cache" ? std::optional(record.id) : std::nullopt);
            }
            else if (action == L"Show Hidden Built-in")
            {
                show_builtin_library(record.id);
            }
            else if (action == L"Delete Known Library / Hide Built-in")
            {
                std::vector<FormField> approval{
                    {L"Entry and known references", details(state, record), {}, true}};
                approval[0].read_only = true;
                if (confirm(context,
                        L"Remove Catalog Entry",
                        L"Remove this custom entry from Add Library, or hide this protected "
                        L"built-in? "
                        L"Existing projects retain independent pins and configuration. Caches are "
                        L"not deleted. "
                        L"References shown are known projects, not an exhaustive disk scan.",
                        record.builtin ? L"Hide Built-in" : L"Delete Known Entry",
                        approval))
                {
                    run_library_work(context.owner,
                        context.theme,
                        L"Updating known-library catalog",
                        [&]
                        {
                            delete_known_library(record.id, true);
                        });
                    context.output(
                        L"Catalog entry removed/hidden. Project pins and caches preserved.\n");
                }
            }
            else if (action == L"Recover Interrupted Change")
            {
                std::vector<FormField> approval;
                if (confirm(context,
                        L"Recover Library Configuration",
                        L"Restore the exact backup from the previously approved interrupted "
                        L"transaction? "
                        L"Later user edits are never overwritten.",
                        L"Recover Backup",
                        approval))
                {
                    recover_library_transaction(context.project);
                    context.changed();
                }
            }
            else
            {
                auto records = state.project_records;
                auto current = std::find_if(records.begin(),
                    records.end(),
                    [&](const auto& item)
                    {
                        return item.id == record.id;
                    });
                if (action == L"Remove from Project")
                {
                    if (current == records.end())
                    {
                        throw std::runtime_error(
                            "The selected entry is not attached to this project.");
                    }
                    records.erase(current);
                    apply_library_change(context, records, false);
                }
                else if (action == L"Edit Integration" ||
                         ((action == L"Update Version" || action == L"Change Target") &&
                             current != records.end() && current->integration != "fetch"))
                {
                    if (current == records.end() || current->builtin)
                    {
                        throw std::runtime_error("Select a project-managed custom integration. "
                                                 "Built-in recipes remain protected.");
                    }
                    std::wstring targets;
                    for (const auto& target : current->targets)
                    {
                        targets += wide(target) + L"\n";
                    }
                    std::wstring settings;
                    for (const auto& option : current->cmake_options)
                    {
                        settings += wide(option) + L"\n";
                    }
                    std::vector<FormField> edit{{L"Version", wide(current->version)},
                        {L"Local path (local/package integrations)", wide(current->local_path)},
                        {L"Linked targets (one per line)", targets, {}, true},
                        {L"CMake settings (one NAME=VALUE per line)", settings, {}, true}};
                    edit[1].browse = BrowseKind::Folder;
                    if (current->integration == "fetch")
                    {
                        edit[0].read_only = true;
                        edit[1].read_only = true;
                    }
                    if (confirm(context,
                            L"Edit Library Integration",
                            L"FetchContent versions/source changes use Update Version and its "
                            L"pinned repository verification. The next step previews project "
                            L"changes; upstream source is never edited.",
                            L"Preview Changes",
                            edit))
                    {
                        current->version = path_support::path_utf8(edit[0].value);
                        current->local_path = path_support::path_utf8(edit[1].value);
                        current->targets =
                            parse_library_targets(path_support::path_utf8(edit[2].value));
                        current->cmake_options.clear();
                        std::istringstream lines(path_support::path_utf8(edit[3].value));
                        for (std::string line; std::getline(lines, line);)
                        {
                            if (!line.empty() && line.back() == '\r')
                            {
                                line.pop_back();
                            }
                            if (!line.empty())
                            {
                                current->cmake_options.push_back(line);
                            }
                        }
                        apply_library_change(context, records);
                    }
                }
                else if (action == L"Reinstall / Repair" && current != records.end() &&
                         current->integration != "fetch")
                {
                    apply_library_change(context, records);
                }
                else if (action == L"Update Version" || action == L"Change Target" ||
                         action == L"Reinstall / Repair")
                {
                    if (context.busy())
                    {
                        throw std::runtime_error(
                            "Stop active operations before verifying a library change.");
                    }
                    const auto editing = current == records.end() ? record : *current;
                    const auto mode = action == L"Update Version"  ? LibraryEdit::UpdateVersion
                                      : action == L"Change Target" ? LibraryEdit::ChangeTargets
                                                                   : LibraryEdit::Repair;
                    if (mode == LibraryEdit::Repair && editing.builtin)
                    {
                        if (current == records.end())
                        {
                            throw std::runtime_error("Attach the protected built-in with Add "
                                                     "Library before repairing this project.");
                        }
                        // Built-ins pin a release archive hash, not a GitHub ZIP
                        // commit. Repair must rebuild that exact protected recipe
                        // instead of silently resolving a potentially moved tag.
                        apply_library_change(context, records);
                        continue;
                    }
                    const auto selected = github_library_dialog(
                        context.owner, context.theme, context.cli, context.output, editing, mode);
                    if (selected && !context.restarting())
                    {
                        auto updated = *known_library(*selected);
                        updated.consumer_target = editing.consumer_target;
                        if (current != records.end())
                        {
                            *current = updated;
                            try
                            {
                                if (!apply_library_change(context, records) &&
                                    updated.id == editing.id && !editing.builtin)
                                {
                                    save_known_library(editing);
                                }
                            }
                            catch (...)
                            {
                                if (updated.id == editing.id && !editing.builtin)
                                {
                                    save_known_library(editing);
                                }
                                throw;
                            }
                        }
                        else
                        {
                            context.output(L"Known integration verified. Use Add Library to attach "
                                           L"it to this project.\n");
                        }
                    }
                }
            }
        }
        catch (const std::exception& error)
        {
            context.output(L"Library operation failed: " + wide(error.what()) + L"\n");
            try
            {
                context.changed();
            }
            catch (const std::exception& refresh_error)
            {
                context.output(L"Project refresh failed: " + wide(refresh_error.what()) + L"\n");
            }
        }
    }
}
} // namespace cpplivelab
