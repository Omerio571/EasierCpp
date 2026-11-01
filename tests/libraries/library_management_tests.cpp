#include "cpplivelab/libraries.hpp"
#include "build/build_lease.hpp"
#include "tools/tool_workflows.hpp"
#include "projects/project_paths.hpp"
#include "../workspace_test_support.hpp"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <winioctl.h>

namespace
{
namespace fs = std::filesystem;
using namespace cpplivelab;

void require(bool condition, const std::string& reason)
{
    if (!condition)
    {
        throw std::runtime_error(reason);
    }
}

void write(const fs::path& path, const std::string& bytes)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    require(static_cast<bool>(output << bytes), "Cannot write isolated fixture");
}

std::string read(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

template <class Action> void refuses(Action action, const std::string& reason)
{
    try
    {
        action();
    }
    catch (const std::exception&)
    {
        return;
    }
    throw std::runtime_error(reason);
}

LibraryRecord custom()
{
    return {"github-fixture",
        "Fixture @ 1",
        "example/fixture",
        "1",
        std::string(40, 'a'),
        std::string(64, 'b'),
        {"Fixture::Core", "Fixture::Headers"}};
}

void metadata(const fs::path& root)
{
    const auto imported = root / L"passive-import";
    const std::string imported_cmake = "find_package(External CONFIG REQUIRED)\n";
    write(imported / L"CMakeLists.txt", imported_cmake);
    const auto observations = detect_project_libraries(imported, {}, L"Debug");
    require(observations.size() == 1 && observations.front().name == "External" &&
                observations.front().ownership == LibraryOwnership::Unknown &&
                read(imported / L"CMakeLists.txt") == imported_cmake,
        "Passive detection changed project files or claimed unevaluated ownership");
    auto package = custom();
    package.integration = "package";
    package.package = "External";
    require(library_observation_conflicts(package, observations.front()),
        "Exact external package conflict was not identified");
    package.package = "ExternalSimilar";
    require(!library_observation_conflicts(package, observations.front()),
        "Similar display name was incorrectly treated as a dependency identity");
    auto configured_target = observations.front();
    configured_target.target = "Fixture::Core";
    require(library_observation_conflicts(custom(), configured_target),
        "Configured exact CMake target conflict was not identified");
    auto gitlab = custom();
    gitlab.id = "gitlab-fixture";
    gitlab.repository = "gitlab.com/group/sub/library";
    save_known_library(gitlab);
    const auto gitlab_recipe = github_dependency_cmake(
        gitlab.repository, gitlab.revision, gitlab.archive_hash, "Fixture::Core");
    require(gitlab_recipe.find("https://gitlab.com/api/v4/projects/group%2Fsub%2Flibrary/"
                               "repository/archive.zip?sha=" +
                               gitlab.revision) != std::string::npos &&
                gitlab_recipe.find("SHA256=" + gitlab.archive_hash) != std::string::npos,
        "GitLab recipe must use the same exact archive URL and hash as the download worker");
    const auto& catalog = supported_libraries();
    require(std::any_of(catalog.begin(),
                catalog.end(),
                [&](const auto& entry)
                {
                    return entry.id == gitlab.id;
                }),
        "GitLab library missing from shared catalog");
    require(parse_libraries(gitlab.id).at(0) == gitlab.id,
        "GitLab library was rejected by existing project generation");
    refuses(
        [&]
        {
            github_dependency_cmake("gitlab.com/group/../library",
                gitlab.revision,
                gitlab.archive_hash,
                "Fixture::Core");
        },
        "GitLab traversal path was accepted");
    auto record = custom();
    require(parse_library_targets("Fixture::Core;Fixture::Headers").size() == 2,
        "Multi-target parsing lost exact identifiers");
    refuses(
        []
        {
            parse_library_targets("Fixture::Core;Fixture::Core");
        },
        "Duplicate target selection was accepted");
    refuses(
        []
        {
            parse_library_targets("Fixture:Core");
        },
        "Single-colon target accepted");
    refuses(
        []
        {
            parse_library_targets("Core)\nmessage(FATAL_ERROR)");
        },
        "CMake injection accepted");
    save_known_library(record);
    require(known_library(record.id)->targets == record.targets, "Catalog lost selected targets");
    const auto snippet = library_snippet(record, "consumer");
    require(snippet.find("target_link_libraries(consumer PRIVATE") != snippet.npos &&
                snippet.find("Fixture::Headers") != snippet.npos,
        "Multi-target snippet omitted links");
    refuses(
        [&]
        {
            managed_dependency_file({record, record}, "app");
        },
        "Duplicate repository declarations accepted");

    const auto first = root / L"first";
    const auto second = root / L"second";
    create_managed_project(first, "empty", record.id);
    create_managed_project(second, "empty", record.id);
    const auto project_bytes = read(first / L"cmake/Dependencies.cmake");
    require(library_references(record).size() == 2, "Project reference ledger is incomplete");
    refuses(
        [&]
        {
            delete_known_library(record.id, false);
        },
        "Unapproved catalog removal accepted");
    delete_known_library(record.id, true);
    require(project_libraries(first).at(0).revision == record.revision &&
                read(first / L"cmake/Dependencies.cmake") == project_bytes,
        "Catalog deletion altered a project's pin or CMake");
    refuses(
        [&]
        {
            delete_known_library(record.id, true);
        },
        "Repeated removal rewrote retired metadata");
    require(
        known_library(record.id)->targets == record.targets, "Retired project metadata was lost");
    delete_known_library("fmt", true);
    require(library_hidden("fmt") && known_library("fmt")->builtin,
        "Built-in implementation was deleted");
    show_builtin_library("fmt");
    require(!library_hidden("fmt"), "Built-in visibility could not be restored");

    write(root / L"probe/target-details.txt",
        "core|STATIC_LIBRARY||source|FALSE|1\n"
        "Fixture::Core|STATIC_LIBRARY|core|source|FALSE|1\n"
        "Fixture::Headers|INTERFACE_LIBRARY||source|FALSE|1\n"
        "helper-main|STATIC_LIBRARY||source|FALSE|0\n");
    const auto targets = discovered_library_targets(root / L"probe");
    require(targets.size() == 4 && targets.at(0).name == "Fixture::Core" &&
                targets.at(1).name == "Fixture::Headers" && !targets.at(2).primary,
        "Public aliases/interfaces were not preferred over backing/internal targets");
    write(root / L"probe/target-details.txt", "Changed::Only|INTERFACE_LIBRARY||source|FALSE|1\n");
    require(discovered_library_targets(root / L"probe").size() == 1,
        "Refreshed discovery retained stale targets");
    auto raylib = record;
    raylib.id = "github-raysan5-raylib-new-pin-format";
    raylib.repository = "raysan5/raylib";
    raylib.targets = {"raylib"};
    save_known_library(raylib);
    const auto raylib_project = root / L"raylib-starter";
    create_managed_project(raylib_project, "empty", raylib.id);
    require(read(raylib_project / L"src/main.cpp").find("InitWindow") != std::string::npos,
        "New catalog pin format lost the existing Raylib starter");
    require(library_references(record).size() == 2,
        "Reference matching confused different repositories with the same revision");
}

void transactions(const fs::path& root)
{
    const auto project = root / L"transaction";
    create_managed_project(project, "empty");
    const auto original = read(project / L"CMakeLists.txt");
    const auto source = read(project / L"src/main.cpp");
    auto record = custom();
    save_known_library(record);
    const auto proposed = preview_library_change(project, {record}, "app");
    refuses(
        [&]
        {
            apply_library_change(proposed,
                false,
                []
                {
                    return true;
                });
        },
        "Unapproved project writes accepted");
    {
        const BuildLease active(managed_build_directory(project, L"Debug", {}));
        refuses(
            [&]
            {
                apply_library_change(proposed,
                    true,
                    []
                    {
                        return true;
                    });
            },
            "Library mutation changed configuration during another build/program lease");
        require(read(project / L"CMakeLists.txt") == original &&
                    !fs::exists(project / L"cmake/Dependencies.cmake"),
            "Refused active-build mutation still wrote project configuration");
    }
    require(!apply_library_change(proposed,
                true,
                []
                {
                    return false;
                }),
        "Failed consumer validation reported success");
    require(read(project / L"CMakeLists.txt") == original &&
                !fs::exists(project / L"cmake/Dependencies.cmake"),
        "Rollback did not restore original bytes");
    require(apply_library_change(proposed,
                true,
                [&]
                {
                    require(project_libraries(project).empty(),
                        "Unverified pin was published too early");
                    require(project_libraries(project, proposed.token).at(0).revision ==
                                record.revision,
                        "Validation could not read its exact proposed pin");
                    refuses(
                        [&]
                        {
                            managed_build(project, L"Debug", {}, false, false);
                        },
                        "An ordinary build bypassed the active library transaction");
                    return true;
                }),
        "Approved integration failed");
    refuses(
        [&]
        {
            project_libraries(project, proposed.token);
        },
        "Expired transaction token was accepted");
    const auto working = read(project / L"cmake/Dependencies.cmake");
    record.version = "2";
    record.revision = std::string(40, 'c');
    const auto update = preview_library_change(project, {record}, "app");
    require(!apply_library_change(update,
                true,
                []
                {
                    return false;
                }) &&
                read(project / L"cmake/Dependencies.cmake") == working &&
                project_libraries(project).at(0).version == "1",
        "Failed version update lost the previous pin");
    require(apply_library_change(update,
                true,
                []
                {
                    return true;
                }) &&
                project_libraries(project).at(0).version == "2",
        "Validated version update was not persisted");
    auto dependencies = read(project / L"cmake/Dependencies.cmake");
    write(project / L"cmake/Dependencies.cmake", dependencies + "\n# User addition\n");
    const auto remove = preview_library_change(project, {}, "app");
    require(apply_library_change(remove,
                true,
                []
                {
                    return true;
                }) &&
                read(project / L"cmake/Dependencies.cmake").find("# User addition") !=
                    std::string::npos &&
                read(project / L"src/main.cpp") == source,
        "Removal altered unrelated CMake or source");
    const auto readd = preview_library_change(project, {record}, "app");
    require(apply_library_change(readd,
                true,
                []
                {
                    return true;
                }),
        "Re-add failed");
    dependencies = read(project / L"cmake/Dependencies.cmake");
    const auto position = dependencies.find("target_link_libraries");
    dependencies.insert(position, "# User edit inside managed block\n");
    write(project / L"cmake/Dependencies.cmake", dependencies);
    refuses(
        [&]
        {
            preview_library_change(project, {}, "app");
        },
        "Edited managed block was overwritten");
    const auto concurrent_project = root / L"concurrent-edit";
    create_managed_project(concurrent_project, "empty");
    const auto concurrent = preview_library_change(concurrent_project, {record}, "app");
    const auto later_edit = concurrent.new_root + "\n# Later user edit\n";
    refuses(
        [&]
        {
            apply_library_change(concurrent,
                true,
                [&]
                {
                    write(concurrent_project / L"CMakeLists.txt", later_edit);
                    return true;
                });
        },
        "Verification success overwrote a concurrent configuration edit");
    require(read(concurrent_project / L"CMakeLists.txt") == later_edit,
        "Concurrent user edit was lost");
    refuses(
        [&]
        {
            recover_library_transaction(concurrent_project);
        },
        "Recovery overwrote later user edits without review");
    write(concurrent_project / L"CMakeLists.txt", concurrent.new_root);
    recover_library_transaction(concurrent_project);
    require(read(concurrent_project / L"CMakeLists.txt") == concurrent.old_root &&
                !fs::exists(concurrent_project / L"cmake/Dependencies.cmake"),
        "Reviewed interrupted transaction did not restore the original configuration");
}

void legacy_migration(const fs::path& root)
{
    auto record = custom();
    record.id = "github-legacy-target";
    record.repository = "example/legacy";
    record.revision = std::string(40, 'd');
    record.targets = {"Old::Core"};
    const auto recipe = github_dependency_cmake(
        record.repository, record.revision, record.archive_hash, record.targets.front());
    write(manager_state_directory() / L"integrations" / (record.id + ".name"), "Legacy @ 1");
    write(manager_state_directory() / L"integrations" / (record.id + ".cmake"), recipe);
    const auto project = root / L"legacy-project";
    const auto original =
        std::string("cmake_minimum_required(VERSION 3.24)\nproject(Legacy LANGUAGES CXX)\n"
                    "add_executable(app main.cpp)\n") +
        recipe;
    write(project / L"CMakeLists.txt", original);
    write(project / L"main.cpp", "int main()\n{\n    return 0;\n}\n");
    remember_project(project);
    record.targets = {"New::Core"};
    save_known_library(record);
    require(project_libraries(project).at(0).targets == std::vector<std::string>{"Old::Core"},
        "Same-pin catalog target update changed an existing legacy project's recorded links");
    const auto change = preview_library_change(project, {record}, "app");
    require(change.new_root.find(recipe) == std::string::npos &&
                change.new_dependencies.find("New::Core") != std::string::npos,
        "Legacy target change could not preview the exact original recipe after catalog update");
    require(!apply_library_change(change,
                true,
                []
                {
                    return false;
                }) &&
                read(project / L"CMakeLists.txt") == original,
        "Failed legacy migration did not restore exact root bytes");
    const auto unknown = root / L"previously-unopened-legacy-project";
    write(unknown / L"CMakeLists.txt", original);
    write(unknown / L"main.cpp", "int main()\n{\n    return 0;\n}\n");
    require(project_libraries(unknown).at(0).targets == std::vector<std::string>{"Old::Core"},
        "An old project opened after the catalog update lost its actual target selection");
    require(preview_library_change(unknown, {record}, "app").new_dependencies.find("New::Core") !=
                std::string::npos,
        "Previously unopened legacy project could not be migrated safely");
    auto custom_fmt = record;
    custom_fmt.id = "github-legacy-fmt";
    custom_fmt.repository = "fmtlib/fmt";
    custom_fmt.revision = std::string(40, 'e');
    custom_fmt.targets = {"fmt::fmt"};
    const auto fmt_recipe = github_dependency_cmake(custom_fmt.repository,
        custom_fmt.revision,
        custom_fmt.archive_hash,
        custom_fmt.targets.front());
    write(
        manager_state_directory() / L"integrations" / (custom_fmt.id + ".name"), "Custom fmt @ 1");
    write(manager_state_directory() / L"integrations" / (custom_fmt.id + ".cmake"), fmt_recipe);
    const auto fmt_project = root / L"legacy-custom-fmt";
    write(fmt_project / L"CMakeLists.txt",
        "cmake_minimum_required(VERSION 3.24)\n"
        "project(CustomFmt LANGUAGES CXX)\nadd_executable(app main.cpp)\n" +
            fmt_recipe);
    write(fmt_project / L"main.cpp", "int main()\n{\n    return 0;\n}\n");
    const auto fmt_records = project_libraries(fmt_project);
    require(fmt_records.size() == 1 && fmt_records.at(0).id == custom_fmt.id,
        "A custom GitHub fmt project was incorrectly adopted as the protected built-in version "
        "too");
}

void caches(const fs::path& root)
{
    const auto state = manager_state_directory();
    const auto cache_path = state / L"library-sources/fixture";
    write(cache_path / L"payload", "preserve me");
    const DependencyCache entry{"fixture-cache", "github-fixture", "Downloaded source", cache_path};
    register_dependency_cache(entry);
    const auto listed = dependency_caches(entry.library_id);
    require(listed.size() == 1 && listed.at(0).present && listed.at(0).bytes >= 11,
        "Registered cache disk usage/status is incorrect");
    refuses(
        [&]
        {
            delete_dependency_cache(entry.id, false);
        },
        "Unapproved cache deletion accepted");
    refuses(
        [&]
        {
            delete_dependency_cache("unregistered", true);
        },
        "Unregistered deletion accepted");
    const auto external = root / L"user-files";
    write(external / L"source.cpp", "protected");
    refuses(
        [&]
        {
            register_dependency_cache({"external", "github-fixture", "Source", external});
        },
        "External user directory was registered for deletion");
    {
        const BuildLease active(state / L"cache-locks" / entry.id);
        refuses(
            [&]
            {
                delete_dependency_cache(entry.id, true);
            },
            "Active cache lease was ignored");
    }
    write(cache_path / L".git/config", "protected Git metadata");
    refuses(
        [&]
        {
            delete_dependency_cache(entry.id, true);
        },
        "Cache containing .git was deleted");
    fs::remove(cache_path / L".git/config");
    fs::remove(cache_path / L".git");
    // A real junction needs no Developer Mode or symbolic-link privilege.
    // Create it only inside this test's newly reserved cache directory.
    const auto junction = cache_path / L"linked-user-folder";
    fs::create_directory(junction);
    windows_support::UniqueHandle link;
    link.reset(CreateFileW(junction.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS,
        nullptr));
    require(link.value != INVALID_HANDLE_VALUE, "Cannot open isolated junction fixture");

    struct MountPoint
    {
        DWORD tag;
        WORD length;
        WORD reserved;
        WORD substitute_offset;
        WORD substitute_length;
        WORD print_offset;
        WORD print_length;
        wchar_t paths[16376];
    } point{};

    const auto substitute = L"\\??\\" + fs::absolute(external).wstring();
    const auto printable = fs::absolute(external).wstring();
    point.tag = IO_REPARSE_TAG_MOUNT_POINT;
    point.substitute_length = static_cast<WORD>(substitute.size() * sizeof(wchar_t));
    point.print_offset = point.substitute_length + sizeof(wchar_t);
    point.print_length = static_cast<WORD>(printable.size() * sizeof(wchar_t));
    std::copy(substitute.begin(), substitute.end(), point.paths);
    std::copy(printable.begin(), printable.end(), point.paths + substitute.size() + 1);
    point.length = static_cast<WORD>(8 + point.print_offset + point.print_length + sizeof(wchar_t));
    DWORD transferred = 0;
    require(DeviceIoControl(link.value,
                FSCTL_SET_REPARSE_POINT,
                &point,
                point.length + 8,
                nullptr,
                0,
                &transferred,
                nullptr) != FALSE,
        "Cannot create actual junction safety fixture");
    link.reset();
    refuses(
        [&]
        {
            delete_dependency_cache(entry.id, true);
        },
        "Junction-containing cache was deleted");
    require(read(external / L"source.cpp") == "protected", "Junction deletion escaped cache root");
    require(RemoveDirectoryW(junction.c_str()) != FALSE, "Cannot unlink fixture junction safely");
    // Windows sharing denial gives a real partial-deletion failure, not a mocked result.
    windows_support::UniqueHandle locked;
    locked.reset(CreateFileW((cache_path / L"payload").c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr));
    require(locked.value != INVALID_HANDLE_VALUE, "Cannot lock fixture payload");
    refuses(
        [&]
        {
            delete_dependency_cache(entry.id, true);
        },
        "Locked-file deletion falsely succeeded");
    locked.reset();
    require(read(external / L"source.cpp") == "protected", "Deletion touched external source");
    require(delete_dependency_cache(entry.id, true) && !fs::exists(cache_path),
        "Retry did not delete cache");
    require(
        !delete_dependency_cache(entry.id, true), "Missing cache falsely reported newly deleted");
    require(!dependency_caches(entry.library_id).at(0).present, "Missing cache status is stale");
    const auto record = custom();
    save_known_library(record);
    create_managed_project(root / L"shared-one", "empty", record.id);
    create_managed_project(root / L"shared-two", "empty", record.id);
    write(cache_path / L"payload", "shared");
    register_dependency_cache(entry);
    refuses(
        [&]
        {
            delete_dependency_cache(entry.id, true);
        },
        "Shared cache protection was ignored");
    {
        const BuildLease running(
            workspace_directory(root / L"shared-one") / L"build-Debug-header-only");
        refuses(
            [&]
            {
                delete_dependency_cache(entry.id, true, true);
            },
            "A source-only cache ignored its referencing project's active build/program lease");
    }
    require(delete_dependency_cache(entry.id, true, true), "Explicit shared-cache deletion failed");
}

void discovery(const fs::path& root)
{
    const auto source = root / L"dependency";
    write(source / L"CMakeLists.txt",
        "add_library(core INTERFACE)\nadd_library(Fixture::Core ALIAS core)\n"
        "add_library(headers INTERFACE)\nadd_library(Fixture::Headers ALIAS headers)\n"
        "add_library(imported INTERFACE IMPORTED GLOBAL)\n"
        "add_library(Fixture::Imported ALIAS imported)\n"
        "target_link_libraries(core INTERFACE Fixture::Imported)\n"
        "add_subdirectory(extlibs)\n");
    write(source / L"extlibs/CMakeLists.txt", "add_library(internal INTERFACE)\n");
    const auto project = root / L"actual-probe";
    write(project / L"CMakeLists.txt", cmake_dependency_probe(source));
    write(project / L"main.cpp", "int main()\n{\n    return 0;\n}\n");
    ProjectOptions options;
    options.allow_configure_writes = true;
    options.target = L"cpplivelab_probe";
    require(managed_build(project, L"Debug", options, false, false) == 0,
        "Actual target-discovery configure/build failed");
    const auto targets = discovered_library_targets(project);
    std::cout << read(project / L"target-details.txt") << std::flush;
    for (const auto& target : targets)
    {
        std::cout << target.name << " | " << target.type << " | " << target.source
                  << " | primary=" << target.primary << " | imported=" << target.imported << '\n';
    }
    const auto find = [&](const std::string& name) -> const LibraryTarget&
    {
        for (const auto& target : targets)
        {
            if (target.name == name)
            {
                return target;
            }
        }
        throw std::runtime_error("Actual configured target missing: " + name);
    };
    require(find("Fixture::Core").primary && find("Fixture::Headers").type == "INTERFACE_LIBRARY",
        "Actual public aliases/interface targets were not discovered");
    require(!find("internal").primary && !find("core").primary,
        "Internal/backing targets were not separated");
    require(find("Fixture::Imported").imported, "Imported alias was not discovered");
    require(
        !fs::exists(project / L"target-trace.jsonl"), "Discovery saved an expanded command trace");
    write(source / L"CMakeLists.txt",
        "add_library(replacement INTERFACE)\n"
        "add_library(Changed::Only ALIAS replacement)\n");
    require(managed_build(project, L"Debug", options, false, false) == 0,
        "Refresh configure/build failed");
    const auto refreshed = discovered_library_targets(project);
    require(refreshed.size() == 2, "Refresh retained targets from the previous CMake graph");
}

void consumers(const fs::path& root, bool gitlab = false)
{
    auto first = custom();
    if (gitlab)
    {
        first.id = "gitlab-fixture";
        first.repository = "gitlab.com/group/sub/library";
    }
    save_known_library(first);
    const auto prepare = [&](const LibraryRecord& record, bool configure_ok, bool link_ok)
    {
        const auto cache = library_source_cache(record);
        const auto source = cache / L"source";
        write(source / L"CMakeLists.txt",
            configure_ok
                ? "cmake_minimum_required(VERSION 3.28)\nproject(Fixture LANGUAGES CXX)\n"
                  "add_library(core SHARED fixture.cpp)\n"
                  "set_target_properties(core PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)\n"
                  "add_library(Fixture::Core ALIAS core)\n"
                  "add_library(headers INTERFACE)\nadd_library(Fixture::Headers ALIAS headers)\n"
                : "message(FATAL_ERROR \"Controlled configuration failure\")\n");
        write(source / L"fixture.cpp",
            link_ok ? "int fixture_value()\n{\n    return 42;\n}\n"
                    : "int incompatible_api()\n{\n    return 42;\n}\n");
        write(cache / L"downloaded.txt",
            path_support::path_utf8(source) + "\n" + record.archive_hash + "\n");
        register_dependency_cache(
            {library_key(path_support::path_utf8(cache)), record.id, "Downloaded source", cache});
    };
    prepare(first, true, true);
    const auto project = root / L"real-consumer";
    const auto other = root / L"other-pinned-project";
    create_managed_project(project, "empty");
    create_managed_project(other, "empty", first.id);
    const std::string program = "int fixture_value();\n\nint main()\n{\n"
                                "    return fixture_value() == 42 ? 0 : 1;\n}\n";
    write(project / L"src/main.cpp", program);
    const auto change = [&](const std::vector<LibraryRecord>& records)
    {
        const auto proposed = preview_library_change(project, records, "app");
        ProjectOptions options;
        options.target = L"app";
        options.allow_configure_writes = true;
        options.library_transaction = proposed.token;
        return apply_library_change(proposed,
            true,
            [&]
            {
                return recover_managed_build(project, L"Debug", options, true) == 0;
            });
    };
    require(change({first}), "Actual offline consumer integration failed");
    ProjectOptions options;
    options.target = L"app";
    options.allow_configure_writes = true;
    require(managed_build(project, L"Debug", options, true, true) == 0,
        "Actually linked offline consumer did not execute");
    const auto usable = managed_build_directory(project, L"Debug", options);
    const auto detected = detect_project_libraries(project, usable, L"Debug", L"app");
    require(std::any_of(detected.begin(),
                detected.end(),
                [](const auto& observation)
                {
                    return observation.target == "core";
                }),
        "Actual configured dependency target was not found without reconfiguration");
    const auto original_dependencies = read(project / L"cmake/Dependencies.cmake");
    auto second = first;
    second.version = "2";
    second.revision = std::string(40, 'c');
    prepare(second, false, true);
    require(!change({second}), "Failed configure reported successful version update");
    prepare(second, true, false);
    require(!change({second}), "Failed link reported successful version update");
    require(read(project / L"cmake/Dependencies.cmake") == original_dependencies &&
                managed_build_directory(project, L"Debug", options) == usable &&
                project_libraries(project).at(0).revision == first.revision,
        "Failed real builds lost the previous working pin/cache");
    prepare(second, true, true);
    require(change({second}), "Valid real consumer update failed");
    require(project_libraries(other).at(0).revision == first.revision &&
                project_libraries(project).at(0).revision == second.revision,
        "Version update changed another project's pin");
    auto missing_link = second;
    missing_link.targets = {"Fixture::Headers"};
    require(!change({missing_link}) && project_libraries(project).at(0).targets == second.targets,
        "Failed target change did not restore working target links");
    const auto remove = preview_library_change(project, {}, "app");
    require(apply_library_change(remove,
                true,
                []
                {
                    return true;
                }),
        "Removal failed");
    require(read(project / L"src/main.cpp") == program &&
                fs::is_directory(library_source_cache(second)),
        "Removal altered source or deleted cache");
    require(managed_build(project, L"Debug", options, false, false) != 0,
        "A consumer using a removed library unexpectedly linked");
    require(change({second}), "Re-add did not rebuild the actual consumer");
    delete_known_library(first.id, true);
    require(project_libraries(project).at(0).revision == second.revision,
        "Deleting catalog entry changed project-specific update pin");
    const auto cache = library_source_cache(second);
    require(delete_dependency_cache(library_key(path_support::path_utf8(cache)), true),
        "Owned source cache deletion failed");
    require(cached_library_source(second).empty(), "Deleted source cache still appears installed");
    prepare(second, true, true);
    require(change({second}) && managed_build(project, L"Debug", options, true, true) == 0,
        "Repair with the unchanged pin did not rebuild/run the real consumer");
    require(read(project / L"src/main.cpp") == program, "Repair changed consumer source");
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    try
    {
        require(argc == 2, "Expected metadata, transactions or caches");
        workspace_test::Fixture fixture;
        const std::wstring mode = argv[1];
        if (mode == L"metadata")
        {
            metadata(fixture.root);
        }
        else if (mode == L"transactions")
        {
            transactions(fixture.root);
            legacy_migration(fixture.root);
        }
        else if (mode == L"caches")
        {
            caches(fixture.root);
        }
        else if (mode == L"discovery")
        {
            discovery(fixture.root);
        }
        else if (mode == L"consumers")
        {
            consumers(fixture.root);
        }
        else if (mode == L"gitlab")
        {
            consumers(fixture.root, true);
        }
        else
        {
            throw std::runtime_error("Unknown library test mode");
        }
        std::cout << "Library management checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Library management failure: " << error.what() << '\n';
        return 1;
    }
}
