#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace cpplivelab
{
inline constexpr int artifact_missing_exit_code = 76;

struct ProjectInspection
{
    std::filesystem::path root;
    std::vector<std::filesystem::path> sources;
    std::vector<std::filesystem::path> assets;
    std::vector<std::string> dependency_evidence;
    bool presets = false;
    bool user_presets = false;
    bool configure_writes = false;
};

struct ProjectOptions
{
    std::wstring preset;
    std::wstring target;
    std::filesystem::path working_directory;
    std::filesystem::path vcpkg;
    bool allow_configure_writes = false;
    // Internal fresh-build slot. Never loaded from imported project configuration.
    std::wstring recovery_slot;
    std::string library_transaction;
    // Each argument is passed directly to CreateProcess, never through a shell.
    std::vector<std::wstring> arguments;
};

struct ExecutableTarget
{
    std::wstring name;
    std::filesystem::path artifact;
};

// Inspecting never executes CMake or writes within the selected project.
ProjectInspection inspect_project(const std::filesystem::path& root);
std::filesystem::path manager_state_directory();
std::filesystem::path workspace_directory(const std::filesystem::path& root);
std::filesystem::path managed_build_directory(const std::filesystem::path& root,
    const std::wstring& configuration,
    const ProjectOptions& options);
// Activate a verified manager-owned slot without changing project configuration.
void activate_build_slot(const std::filesystem::path& root,
    const std::wstring& configuration,
    const ProjectOptions& options);
ProjectOptions load_project_options(
    const std::filesystem::path& root, const std::wstring& configuration);
void save_project_options(const std::filesystem::path& root,
    const std::wstring& configuration,
    const ProjectOptions& options);
std::vector<ExecutableTarget> executable_targets(const std::filesystem::path& root,
    const std::wstring& configuration,
    const ProjectOptions& options);
int managed_build(const std::filesystem::path& root,
    const std::wstring& configuration,
    const ProjectOptions& options,
    bool run,
    bool incremental);
// Explicit approval is required. Preserves all old outputs and switches the
// manager's active directory only after the fresh build succeeds.
int recover_managed_build(const std::filesystem::path& root,
    const std::wstring& configuration,
    const ProjectOptions& options,
    bool approved);
std::vector<std::filesystem::path> recent_projects();
void remember_project(const std::filesystem::path& root, bool remove = false);

struct LibraryIntegration
{
    std::string id;
    std::wstring name;
    std::wstring description;
};

const std::vector<LibraryIntegration>& supported_libraries();
std::vector<std::string> parse_libraries(const std::string& libraries);
std::string dependency_cmake(const std::string& libraries, const std::string& target = "app");
std::string github_dependency_cmake(const std::string& repository,
    const std::string& commit,
    const std::string& archive_hash,
    const std::string& library_target,
    const std::string& include_directory = "");
// New destinations only. A comma-separated list retains the existing CLI/API.
void create_managed_project(const std::filesystem::path& destination,
    const std::string& project_template,
    const std::string& library = "none");

struct EditorOptions
{
    std::wstring choice = L"Visual Studio 2022";
    std::filesystem::path executable;
    // Each argument is separate; {project} is substituted without invoking a shell.
    std::vector<std::wstring> arguments;
};

EditorOptions load_editor();
void save_editor(const EditorOptions& options);
std::filesystem::path discover_editor(const std::wstring& choice);
std::vector<std::wstring> editor_arguments(const EditorOptions& options,
    const std::filesystem::path& project,
    const std::filesystem::path& file = {},
    unsigned line = 1,
    unsigned column = 1);
void launch_editor(const EditorOptions& options,
    const std::filesystem::path& project,
    const std::filesystem::path& file = {},
    unsigned line = 1,
    unsigned column = 1);
} // namespace cpplivelab
