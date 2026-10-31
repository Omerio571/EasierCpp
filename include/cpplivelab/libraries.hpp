#pragma once

#include "workspace.hpp"
#include <functional>
#include <optional>
#include <cstdint>

namespace cpplivelab
{
struct LibraryTarget
{
    std::string name;
    std::string type;
    std::string alias_of;
    std::filesystem::path source;
    bool imported = false;
    bool primary = false;
};

struct LibraryRecord
{
    std::string id;
    std::string name;
    std::string repository;
    std::string version;
    std::string revision;
    std::string archive_hash;
    std::vector<std::string> targets;
    std::string include_directory;
    std::vector<std::string> cmake_options;
    bool builtin = false;
    std::string header;
    std::string consumer_target;
    std::string legacy_cmake;
    // Older records default to their original pinned FetchContent integration.
    std::string integration = "fetch";
    std::string package;
    std::string local_path;
    std::string components;
};

struct DependencyCache
{
    std::string id;
    std::string library_id;
    std::string kind;
    std::filesystem::path path;
    std::filesystem::path build_directory;
    std::string identity;
    std::uintmax_t bytes = 0;
    bool present = false;
    std::string issue;
};

std::string library_key(const std::string& text);
std::vector<LibraryTarget> discovered_library_targets(const std::filesystem::path& probe);
std::vector<std::string> parse_library_targets(const std::string& text);
std::optional<LibraryRecord> known_library(const std::string& id);
void save_known_library(const LibraryRecord& record);
void delete_known_library(const std::string& id, bool confirmed);
bool library_hidden(const std::string& id);
void show_builtin_library(const std::string& id);
// The legacy spelling is reconstructed only to recognize unchanged owned blocks
// during an explicitly approved update; newly generated integrations validate TYPE.
std::string library_snippet(const LibraryRecord& record,
    const std::string& executable_target,
    bool validate_target_types = true);
std::string managed_dependency_file(
    const std::vector<LibraryRecord>& records, const std::string& executable_target);
std::vector<LibraryRecord> project_libraries(
    const std::filesystem::path& project, const std::string& transaction = "");
void remember_project_libraries(
    const std::filesystem::path& project, const std::vector<LibraryRecord>& libraries);
std::vector<std::filesystem::path> library_references(const LibraryRecord& record);

struct LibraryChange
{
    std::filesystem::path project;
    std::string old_root;
    std::string new_root;
    std::string old_dependencies;
    std::string new_dependencies;
    bool dependencies_existed = false;
    bool dependency_directory_existed = false;
    std::string token;
    std::vector<LibraryRecord> libraries;
};

// Preview exact managed-file changes. Imports and unmanaged edits are never guessed.
LibraryChange preview_library_change(const std::filesystem::path& project,
    const std::vector<LibraryRecord>& libraries,
    const std::string& executable_target);
bool apply_library_change(
    const LibraryChange& change, bool approved, const std::function<bool()>& verify);
void recover_library_transaction(const std::filesystem::path& project);

void register_dependency_cache(const DependencyCache& cache);
std::vector<DependencyCache> dependency_caches(const std::string& library_id = "");
bool delete_dependency_cache(const std::string& id, bool confirmed, bool allow_shared = false);
void register_build_dependency_caches(const std::filesystem::path& project,
    const std::filesystem::path& build,
    const std::filesystem::path& dependencies,
    const std::string& configuration,
    const std::string& transaction = "");
std::filesystem::path library_source_cache(const LibraryRecord& record);
std::filesystem::path cached_library_source(const LibraryRecord& record);
void register_probe_dependency_caches(
    const std::filesystem::path& probe, const LibraryRecord& record);
} // namespace cpplivelab
