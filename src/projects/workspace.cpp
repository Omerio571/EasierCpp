#include "cpplivelab/workspace.hpp"
#include "cpplivelab/libraries.hpp"
#include "cpplivelab/doctor.hpp"
#include "cpplivelab/process.hpp"
#include "cpplivelab/session.hpp"
#include "build/auto_build.hpp"
#include "projects/directory_scan.hpp"
#include "build/build_lease.hpp"
#include "build/build_profile.hpp"
#include "projects/project_paths.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <regex>
#include <memory>
#include <cctype>

namespace cpplivelab
{
namespace
{
namespace fs = std::filesystem;

std::wstring wide(const std::string& text)
{
    return path_support::from_utf8(text).wstring();
}

std::string narrow(const std::wstring& text)
{
    return path_support::path_utf8(fs::path(text));
}

fs::path state_root()
{
    wchar_t buffer[32768]{};
    if (GetEnvironmentVariableW(L"CPPLIVELAB_STATE_HOME", buffer, 32768))
    {
        return fs::absolute(buffer);
    }
    if (!GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, 32768))
    {
        throw std::runtime_error("LOCALAPPDATA is unavailable; cannot isolate manager metadata.");
    }
    return fs::path(buffer) / L"CppLiveLab";
}

std::string text_key(std::wstring canonical)
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (auto character : canonical)
    {
        hash ^= static_cast<unsigned>(character);
        hash *= 1099511628211ULL;
    }
    std::ostringstream value;
    value << std::hex << hash;
    return value.str();
}

std::string key(const fs::path& path)
{
    auto canonical = fs::weakly_canonical(fs::absolute(path)).wstring();
    for (auto& character : canonical)
    {
        character = static_cast<wchar_t>(std::towlower(character));
    }
    return text_key(canonical);
}

void write(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!(output << text))
    {
        throw std::runtime_error(
            "Cannot write manager-owned file: " + path_support::path_utf8(path));
    }
}

std::string read(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {(std::istreambuf_iterator<char>(input)), {}};
}

std::string metadata_hex(const fs::path& path)
{
    // Bound cache validation work. Larger projects can still use the normal reader.
    if (!fs::is_regular_file(path) || fs::file_size(path) > 8 * 1024 * 1024)
    {
        throw std::runtime_error("Metadata snapshot is unavailable.");
    }
    std::ifstream input(path, std::ios::binary);
    const std::string bytes{std::istreambuf_iterator<char>(input), {}};
    if (!input || bytes.size() != fs::file_size(path))
    {
        throw std::runtime_error("Metadata snapshot could not be read.");
    }
    constexpr char digits[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(bytes.size() * 2);
    for (const unsigned char byte : bytes)
    {
        hex += digits[byte >> 4];
        hex += digits[byte & 15];
    }
    return hex;
}

bool reusable_target_metadata(const fs::path& project,
    const fs::path& directory,
    const std::wstring& configuration,
    const ProjectOptions& options)
{
    try
    {
        const auto generator = read(directory / L"generator.txt");
        if (generator != "Ninja" && generator != "Ninja Multi-Config" &&
            generator != "Visual Studio 17 2022")
        {
            return false;
        }
        const auto written = (std::min)(fs::last_write_time(directory / L"executables.txt"),
            fs::last_write_time(directory / L"generator.txt"));
        fs::path latest_index;
        for (const auto& entry : fs::directory_iterator(directory / L".cmake/api/v1/reply"))
        {
            if (entry.path().filename().wstring().starts_with(L"index-") &&
                entry.path().extension() == L".json")
            {
                if (entry.path().filename() > latest_index.filename())
                {
                    latest_index = entry.path();
                }
                if (entry.last_write_time() > written)
                {
                    return false;
                }
            }
        }
        if (latest_index.empty())
        {
            return false;
        }
        // CMake publishes a new unique index whenever it generates the buildsystem;
        // indexed replies are immutable (cmake-file-api(7)). Compare exact derived
        // bytes as well, so missing/damaged/legacy manifests cannot masquerade as a
        // current model. Never alter CMake-owned replies or bypass Ninja checks.
        std::string expected = "1\n" + narrow(configuration) + "\n" +
                               path_support::path_utf8(latest_index.filename()) + "\n";
        for (const auto* name :
            {L"executables.txt", L"generator.txt", L"libraries.txt", L"runtime-paths.txt"})
        {
            expected += metadata_hex(directory / name) + "\n";
        }
        expected +=
            metadata_hex(fs::path(CPPLIVELAB_SOURCE_DIR) / L"scripts/file_api.cmake") + "\n";
        const auto snapshot = directory / L"file-api.snapshot";
        if (!fs::is_regular_file(snapshot) || fs::file_size(snapshot) > expected.size() * 2)
        {
            return false;
        }
        // CMake's text writes use native newlines. Only snapshot separators are
        // normalized; manifest/reader contents above are compared as raw hex.
        auto cached = read(snapshot);
        cached.erase(std::remove(cached.begin(), cached.end(), '\r'), cached.end());
        if (cached != expected)
        {
            return false;
        }
        executable_targets(project, configuration, options);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

void configuration_valid(const std::wstring& configuration)
{
    if (configuration != L"Debug" && configuration != L"Release")
    {
        throw std::runtime_error("Choose Debug or Release.");
    }
}

fs::path tool(const Toolchain& tools, const std::string& name)
{
    for (const auto& item : tools.tools)
    {
        if (item.name == name)
        {
            return item.installed_path;
        }
    }
    return {};
}

Environment tool_environment(Environment environment, const Toolchain& tools)
{
    if (environment.empty())
    {
        const auto inherited = GetEnvironmentStringsW();
        if (!inherited)
        {
            throw std::runtime_error("Cannot read the child development environment.");
        }
        const wchar_t* end = inherited;
        while (*end)
        {
            end += std::wcslen(end) + 1;
        }
        environment.assign(static_cast<const wchar_t*>(inherited), end + 1);
        FreeEnvironmentStringsW(inherited);
    }
    std::wstring prefix;
    for (const auto& name : {"cmake", "ninja", "clang++", "g++"})
    {
        const auto executable = tool(tools, name);
        if (!executable.empty())
        {
            prefix += executable.parent_path().wstring() + L";";
        }
    }
    Environment result;
    unsigned git_config_count = 0;
    for (const wchar_t* entry = environment.data(); *entry; entry += std::wcslen(entry) + 1)
    {
        if (_wcsnicmp(entry, L"GIT_CONFIG_COUNT=", 17) == 0)
        {
            const std::wstring count(entry + 17);
            if (count.empty() || count.find_first_not_of(L"0123456789") != count.npos ||
                count.size() > 2)
            {
                throw std::runtime_error(
                    "Invalid child Git configuration count; review GIT_CONFIG_COUNT.");
            }
            git_config_count = static_cast<unsigned>(std::stoul(count));
        }
    }
    for (const wchar_t* entry = environment.data(); *entry; entry += std::wcslen(entry) + 1)
    {
        std::wstring value(entry);
        if (_wcsnicmp(entry, L"GIT_CONFIG_COUNT=", 17) == 0)
        {
            continue;
        }
        if (_wcsnicmp(entry, L"PATH=", 5) == 0)
        {
            value = L"PATH=" + prefix + value.substr(5);
        }
        result.insert(result.end(), value.begin(), value.end());
        result.push_back(L'\0');
    }
    // Git for Windows otherwise rejects long paths in third-party fixtures such
    // as font-test data. Append one child-only setting; never edit global Git or
    // discard the caller's existing per-command configuration.
    for (const auto& value : {L"GIT_CONFIG_COUNT=" + std::to_wstring(git_config_count + 1),
             L"GIT_CONFIG_KEY_" + std::to_wstring(git_config_count) + L"=core.longpaths",
             L"GIT_CONFIG_VALUE_" + std::to_wstring(git_config_count) + L"=true"})
    {
        result.insert(result.end(), value.begin(), value.end());
        result.push_back(L'\0');
    }
    result.push_back(L'\0');
    return result;
}
} // namespace

ProjectInspection inspect_project(const fs::path& folder)
{
    ProjectInspection result;
    result.root = validate_gui_project(folder);
    result.presets = fs::is_regular_file(result.root / L"CMakePresets.json");
    result.user_presets = fs::is_regular_file(result.root / L"CMakeUserPresets.json");
    size_t scanned = 0;
    scan_directory(result.root,
        [&](const fs::path& item, bool is_directory)
        {
            if (++scanned > 50000)
            {
                throw std::runtime_error(
                    "Project inspection limit reached; choose the CMake source folder.");
            }
            if (is_directory)
            {
                if (ProjectWatcher::ignored_directory(item.filename()))
                {
                    return false;
                }
                auto name = item.filename().wstring();
                std::transform(name.begin(), name.end(), name.begin(), std::towlower);
                if (name == L"assets" || name == L"resources" || name == L"data")
                {
                    result.assets.push_back(item);
                }
            }
            else
            {
                const auto extension = item.extension();
                if (extension == L".png" || extension == L".jpg" || extension == L".wav" ||
                    extension == L".ogg" || extension == L".ttf" || extension == L".obj")
                {
                    const auto directory = item.parent_path();
                    if (std::find(result.assets.begin(), result.assets.end(), directory) ==
                        result.assets.end())
                    {
                        result.assets.push_back(directory);
                    }
                }
                if (extension == L".cpp" || extension == L".h" || extension == L".hpp" ||
                    extension == L".cc" || extension == L".cxx")
                {
                    result.sources.push_back(item);
                }
                if (item.filename() == L"CMakeLists.txt" || extension == L".cmake")
                {
                    const auto content = read(item);
                    // Advisory evidence only: arbitrary CMake is not evaluated during import.
                    const std::regex writes(
                        R"(file\s*\(\s*(WRITE|APPEND|MAKE_DIRECTORY|RENAME|REMOVE|REMOVE_RECURSE|COPY)\b)",
                        std::regex::icase);
                    if (std::regex_search(content, writes))
                    {
                        result.configure_writes = true;
                    }
                    const std::regex dependencies(
                        R"((find_package|FetchContent_Declare)\s*\(\s*([A-Za-z0-9_+.-]+))",
                        std::regex::icase);
                    for (auto match =
                             std::sregex_iterator(content.begin(), content.end(), dependencies);
                        match != std::sregex_iterator();
                        ++match)
                    {
                        result.dependency_evidence.push_back(
                            (*match)[1].str() + ": " + (*match)[2].str());
                    }
                    if (content.find("GIT_REPOSITORY") != content.npos)
                    {
                        // Repository URLs may embed credentials. Never echo them.
                        result.dependency_evidence.push_back(
                            "Git-backed dependency (review URL in CMake)");
                    }
                }
            }
            return true;
        });
    return result;
}

fs::path manager_state_directory()
{
    return state_root();
}

fs::path workspace_directory(const fs::path& root)
{
    if (path_support::is_within_directory(
            fs::weakly_canonical(state_root()), fs::weakly_canonical(root)))
    {
        throw std::runtime_error("Manager metadata must be outside the imported project.");
    }
    const auto workspace = state_root() / L"projects" / key(root);
    if (!path_support::is_within_directory(
            fs::weakly_canonical(workspace), fs::weakly_canonical(state_root())))
    {
        throw std::runtime_error("Manager workspace redirects outside the owned state directory.");
    }
    return workspace;
}

fs::path managed_build_directory(
    const fs::path& root, const std::wstring& configuration, const ProjectOptions& options)
{
    configuration_valid(configuration);
    // Presets/toolchains have distinct caches; never reuse an IDE's build directory.
    const auto base = workspace_directory(root) /
                      (L"build-" + configuration + L"-" +
                          wide(text_key(options.preset + L"|" + options.vcpkg.wstring())));
    if (fs::weakly_canonical(base).parent_path() != fs::weakly_canonical(workspace_directory(root)))
    {
        throw std::runtime_error("Managed build directory redirects outside its owned workspace.");
    }
    auto slot = options.recovery_slot;
    if (slot.empty())
    {
        slot = wide(read(fs::path(base.wstring() + L".active")));
    }
    if (slot.empty())
    {
        return base;
    }
    if (!slot.starts_with(L"fresh-") ||
        slot.find_first_not_of(L"abcdefghijklmnopqrstuvwxyz0123456789-") != slot.npos)
    {
        throw std::runtime_error("Invalid managed build slot; inspect manager metadata.");
    }
    const auto directory = fs::path(base.wstring() + L"-" + slot);
    if (fs::weakly_canonical(directory).parent_path() !=
        fs::weakly_canonical(workspace_directory(root)))
    {
        throw std::runtime_error("Managed build slot escapes its owned workspace.");
    }
    return directory;
}

void activate_build_slot(
    const fs::path& root, const std::wstring& configuration, const ProjectOptions& options)
{
    if (options.recovery_slot.empty())
    {
        throw std::runtime_error("An explicit verified build slot is required.");
    }
    const auto directory = managed_build_directory(root, configuration, options);
    if (executable_targets(root, configuration, options).empty())
    {
        throw std::runtime_error("No executable metadata in replacement build.");
    }
    auto base = directory.wstring();
    base.resize(base.size() - options.recovery_slot.size() - 1);
    const auto temporary = fs::path(base + L".active-new");
    write(temporary, narrow(options.recovery_slot));
    if (!MoveFileExW(temporary.c_str(),
            fs::path(base + L".active").c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        throw std::runtime_error("Replacement build activation failed; previous slot retained.");
    }
}

int recover_managed_build(const fs::path& root,
    const std::wstring& configuration,
    const ProjectOptions& options,
    bool approved)
{
    if (!approved)
    {
        throw std::runtime_error(
            "Recovery requires explicit approval (--approve-recovery). "
            "It builds in a fresh manager directory and preserves old outputs.");
    }
    const auto previous = managed_build_directory(root, configuration, options);
    const auto workspace = fs::weakly_canonical(workspace_directory(root));
    if (fs::weakly_canonical(previous).parent_path() != workspace)
    {
        throw std::runtime_error("Recovery refused: build directory is not manager-owned.");
    }
    const BuildLease previous_lease(previous);
    ensure_executables_stopped(previous);
    auto fresh = options;
    fresh.recovery_slot = L"fresh-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                          std::to_wstring(GetTickCount64());
    const auto destination = managed_build_directory(root, configuration, fresh);
    if (fs::exists(destination))
    {
        throw std::runtime_error(
            "Fresh recovery directory already exists; nothing was overwritten.");
    }
    std::cout << "Recovery: old build preserved at " << path_support::path_utf8(previous) << '\n';
    const int result = managed_build(root, configuration, fresh, false, false);
    if (result != 0)
    {
        std::cerr << "Recovery failed; original active build and diagnostics preserved.\n";
        return result;
    }
    const auto base = workspace_directory(root) /
                      (L"build-" + configuration + L"-" +
                          wide(text_key(options.preset + L"|" + options.vcpkg.wstring())));
    const auto temporary = fs::path(base.wstring() + L".active-new");
    write(temporary, narrow(fresh.recovery_slot));
    if (!MoveFileExW(temporary.c_str(),
            fs::path(base.wstring() + L".active").c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        throw std::runtime_error("Fresh build succeeded but activation failed; original retained.");
    }
    std::cout << "Recovery succeeded; fresh build is active. No previous outputs were deleted.\n";
    return 0;
}

ProjectOptions load_project_options(const fs::path& root, const std::wstring& configuration)
{
    configuration_valid(configuration);
    std::ifstream input(workspace_directory(root) / (configuration + L".settings"));
    std::string preset, target, directory, vcpkg;
    ProjectOptions options;
    if (input >> std::quoted(preset) >> std::quoted(target) >> std::quoted(directory) >>
        std::quoted(vcpkg))
    {
        options.preset = wide(preset);
        options.target = wide(target);
        options.working_directory = path_support::from_utf8(directory);
        options.vcpkg = path_support::from_utf8(vcpkg);
    }
    size_t argument_count = 0;
    if (input >> argument_count)
    {
        if (argument_count > 256)
        {
            throw std::runtime_error("Too many saved program arguments.");
        }
        for (size_t index = 0; index < argument_count; ++index)
        {
            std::string argument;
            if (!(input >> std::quoted(argument)))
            {
                throw std::runtime_error("Incomplete saved program arguments.");
            }
            options.arguments.push_back(wide(argument));
        }
    }
    // Authorization for configure-time writes is deliberately never persisted.
    return options;
}

void save_project_options(
    const fs::path& root, const std::wstring& configuration, const ProjectOptions& options)
{
    configuration_valid(configuration);
    if (options.arguments.size() > 256)
    {
        throw std::runtime_error("At most 256 program arguments can be saved.");
    }
    std::ostringstream data;
    data << std::quoted(narrow(options.preset)) << '\n'
         << std::quoted(narrow(options.target)) << '\n'
         << std::quoted(path_support::path_utf8(options.working_directory)) << '\n'
         << std::quoted(path_support::path_utf8(options.vcpkg)) << '\n';
    data << options.arguments.size() << '\n';
    for (const auto& argument : options.arguments)
    {
        data << std::quoted(narrow(argument)) << '\n';
    }
    write(workspace_directory(root) / (configuration + L".settings"), data.str());
    write(workspace_directory(root) / L"last-config.txt", narrow(configuration));
}

std::vector<ExecutableTarget> executable_targets(
    const fs::path& root, const std::wstring& configuration, const ProjectOptions& options)
{
    std::vector<ExecutableTarget> targets;
    const auto manifest =
        managed_build_directory(root, configuration, options) / L"executables.txt";
    std::ifstream input(manifest);
    if (!input)
    {
        if (!fs::exists(manifest))
        {
            // Opening a project does not configure it. Missing metadata is an
            // expected state; an existing unreadable file needs a diagnostic.
            return targets;
        }
        throw std::runtime_error(
            "Target metadata could not be read: " + path_support::path_utf8(manifest));
    }
    std::string name, artifact;
    while (std::getline(input, name))
    {
        if (name.empty() || !std::getline(input, artifact) || artifact.empty())
        {
            throw std::runtime_error(
                "Target metadata is incomplete: " + path_support::path_utf8(manifest) +
                ". Build/configure the selected profile again.");
        }
        targets.push_back({wide(name), path_support::from_utf8(artifact)});
    }
    if (input.bad())
    {
        throw std::runtime_error(
            "Target metadata read failed: " + path_support::path_utf8(manifest));
    }
    return targets;
}

int managed_build(const fs::path& root,
    const std::wstring& configuration,
    const ProjectOptions& options,
    bool run,
    bool incremental)
{
    BuildProfile profile;
    const auto project = inspect_project(root);
    profile.checkpoint("project_inspection_ms");
    std::unique_ptr<BuildLease> project_gate;
    if (options.library_transaction.empty())
    {
        // Serialize startup with approved library mutations, not with another
        // configuration's whole build. The output lease takes over below.
        project_gate =
            std::make_unique<BuildLease>(workspace_directory(project.root) / L"library-mutation");
    }
    const auto transaction = workspace_directory(project.root) / L"library-transaction/pending";
    if (fs::exists(transaction) && options.library_transaction != read(transaction))
    {
        throw BuildDeferredError(
            "A library transaction is active or needs recovery. "
            "Finish it or use Manage Libraries > Recover Interrupted Change before building.");
    }
    if (!options.vcpkg.empty())
    {
        if (!fs::is_regular_file(options.vcpkg / L"scripts/buildsystems/vcpkg.cmake"))
        {
            throw std::runtime_error("vcpkg root has no scripts/buildsystems/vcpkg.cmake.");
        }
        if (!options.preset.empty() && options.preset != L"-")
        {
            throw std::runtime_error(
                "Configure vcpkg in the existing preset instead of overriding its toolchain.");
        }
    }
    if (project.configure_writes && !options.allow_configure_writes)
    {
        throw std::runtime_error(
            "CMake contains file-writing commands. Configuration may change "
            "project files. Review it and explicitly authorize with --allow-configure-writes.");
    }
    const auto directory = managed_build_directory(project.root, configuration, options);
    const BuildLease lease(directory);
    if (fs::exists(transaction) && options.library_transaction != read(transaction))
    {
        throw BuildDeferredError("Library configuration changed during build startup; retry after "
                                 "its transaction completes.");
    }
    project_gate.reset();
    ensure_executables_stopped(directory);
    if (incremental && fs::is_regular_file(directory / L"CMakeCache.txt"))
    {
        std::ifstream cache(directory / L"CMakeCache.txt");
        std::string source;
        std::string generator;
        for (std::string line; std::getline(cache, line);)
        {
            const std::string source_key = "CMAKE_HOME_DIRECTORY:INTERNAL=";
            const std::string generator_key = "CMAKE_GENERATOR:INTERNAL=";
            if (line.starts_with(source_key))
            {
                source = line.substr(source_key.size());
            }
            if (line.starts_with(generator_key))
            {
                generator = line.substr(generator_key.size());
            }
        }
        if (source.empty() || key(path_support::from_utf8(source)) != key(project.root) ||
            (generator != "Ninja" && generator != "Ninja Multi-Config" &&
                generator != "Visual Studio 17 2022"))
        {
            throw std::runtime_error(
                "CMake cache source/generator identity is invalid. "
                "Review Project Settings or explicitly approve Clean & Rebuild in a fresh "
                "manager directory. No stale program was launched.");
        }
    }
    profile.checkpoint("leases_options_cache_ms");
    const auto tools = discover_toolchain(system_discovery_inputs());
    profile.checkpoint("tool_discovery_ms");
    const auto cmake = tool(tools, "cmake");
    if (cmake.empty())
    {
        throw std::runtime_error("CMake not installed. Run doctor.");
    }
    Environment environment;
    if (!tools.developer_command.empty())
    {
        environment = developer_environment(tools.developer_command);
    }
    // Preset generators also need discovered tools in the CHILD's PATH; do not
    // depend on scripts/build.ps1 having initialized the caller's terminal.
    environment = tool_environment(std::move(environment), tools);
    profile.checkpoint("environment_ms");
    const auto query = directory / L".cmake/api/v1/query";
    write(query / L"codemodel-v2", "");
    auto configure = std::vector<std::wstring>{L"-S", project.root.wstring()};
    if (!options.preset.empty())
    {
        configure.insert(configure.end(), {L"--preset", options.preset});
    }
    else
    {
        if (project.presets || project.user_presets)
        {
            throw std::runtime_error(
                "This project has presets. Select an explicit configure preset "
                "(cmake --list-presets), or use '-' to explicitly choose the default Ninja "
                "profile.");
        }
        configure.insert(configure.end(),
            {L"-G", L"Ninja", L"-DCMAKE_MAKE_PROGRAM=" + tool(tools, "ninja").wstring()});
    }
    // '-' is an explicit no-preset choice. No language/optimization/compiler flags
    // are imposed on imported projects; presets retain their toolchain and options.
    if (options.preset == L"-")
    {
        configure = {L"-S",
            project.root.wstring(),
            L"-G",
            L"Ninja",
            L"-DCMAKE_MAKE_PROGRAM=" + tool(tools, "ninja").wstring()};
    }
    configure.insert(
        configure.end(), {L"-B", directory.wstring(), L"-DCMAKE_BUILD_TYPE=" + configuration});
    if ((options.preset.empty() || options.preset == L"-") &&
        !fs::is_regular_file(directory / L"CMakeCache.txt") && !tools.developer_command.empty() &&
        !tool(tools, "cl").empty() && GetEnvironmentVariableW(L"CC", nullptr, 0) == 0 &&
        GetEnvironmentVariableW(L"CXX", nullptr, 0) == 0)
    {
        // The ordinary Windows profile uses the discovered VS compiler, rather
        // than whichever optional compiler CMake happens to find first. Existing
        // caches, explicit presets and caller compiler selections retain control.
        configure.push_back(L"-DCMAKE_C_COMPILER=" + tool(tools, "cl").wstring());
        configure.push_back(L"-DCMAKE_CXX_COMPILER=" + tool(tools, "cl").wstring());
    }
    // On CMake >= 3.30 this avoids FetchContent's nested Ninja subbuild and its
    // Windows long-path regeneration failure. Explicit project policy settings
    // still win. Older CMake simply has no such policy; project files stay intact.
    configure.push_back(L"-DCMAKE_POLICY_DEFAULT_CMP0168=NEW");
    std::vector<std::unique_ptr<BuildLease>> source_leases;
    for (const auto& record : project_libraries(project.root, options.library_transaction))
    {
        if (record.revision.empty() || (!options.preset.empty() && options.preset != L"-"))
        {
            continue;
        }
        auto content = "cpplivelab_" + record.revision.substr(0, 12);
        std::transform(content.begin(),
            content.end(),
            content.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(std::toupper(character));
            });
        const auto source = cached_library_source(record);
        // Clear only our own default-profile override after an approved cache
        // deletion. A user's explicit preset remains authoritative.
        configure.push_back(
            L"-DFETCHCONTENT_SOURCE_DIR_" + wide(content) + L"=" + source.wstring());
        if (!source.empty())
        {
            const auto cache = library_source_cache(record);
            source_leases.push_back(
                std::make_unique<BuildLease>(manager_state_directory() / L"cache-locks" /
                                             library_key(path_support::path_utf8(cache))));
        }
    }
    const bool dependency_probe = read(project.root / L"CMakeLists.txt")
                                      .starts_with("# CppLiveLab dependency target discovery\n");
    if (dependency_probe)
    {
        configure.push_back(L"--graphviz=" + (project.root / L"target-graph.dot").wstring());
    }
    const bool fetch_content = std::any_of(project.dependency_evidence.begin(),
        project.dependency_evidence.end(),
        [](const auto& evidence)
        {
            return evidence.starts_with("FetchContent_Declare:");
        });
    if ((fetch_content || dependency_probe) && (options.preset.empty() || options.preset == L"-"))
    {
        // Legacy generated projects may contain a full SHA in their content
        // name. Keep downloads/builds in a compact, per-build owned directory
        // without rewriting that project's CMake or overriding a chosen preset.
        const auto dependencies = state_root() / L"d" / key(directory);
        if (!path_support::is_within_directory(
                fs::weakly_canonical(dependencies), fs::weakly_canonical(state_root())))
        {
            throw std::runtime_error("Dependency build path escapes manager-owned state.");
        }
        fs::create_directories(dependencies);
        const auto dependency_argument = dependencies.wstring();
        configure.push_back(L"-DFETCHCONTENT_BASE_DIR=" + dependency_argument);
        write(directory / L"dependency-cache.path", path_support::path_utf8(dependencies));
        if (dependency_probe)
        {
            configure.push_back(
                L"-DCPPLIVELAB_PROBE_BINARY_DIR=" + dependency_argument + L"/dependency");
        }
    }
    if (!options.vcpkg.empty())
    {
        const auto toolchain = options.vcpkg / L"scripts/buildsystems/vcpkg.cmake";
        if (!fs::is_regular_file(toolchain))
        {
            throw std::runtime_error("vcpkg root has no scripts/buildsystems/vcpkg.cmake.");
        }
        if (!options.preset.empty() && options.preset != L"-")
        {
            throw std::runtime_error(
                "Configure vcpkg in the existing preset instead of overriding its toolchain.");
        }
        configure.push_back(L"-DCMAKE_TOOLCHAIN_FILE=" + toolchain.wstring());
    }
    std::cout << "Configuring " << narrow(configuration) << ": "
              << path_support::path_utf8(project.root) << '\n'
              << std::flush;
    int result = 0;
    profile.checkpoint("configure_arguments_ms");
    if (!incremental || !fs::is_regular_file(directory / L"CMakeCache.txt"))
    {
        result = run_process(cmake, configure, project.root, environment);
        if (result != 0)
        {
            return result;
        }
        if (dependency_probe)
        {
            // Graphviz exports real aliases after generation. A bounded second
            // configure checks those candidates with TARGET in the same graph.
            // Never trace expanded commands: dependencies may read credentials.
            result = run_process(cmake, configure, project.root, environment);
            if (result != 0)
            {
                return result;
            }
        }
    }
    profile.checkpoint("configure_ms");
    if (!incremental || !reusable_target_metadata(project.root, directory, configuration, options))
    {
        result = run_process(cmake,
            {L"-DBUILD=" + directory.wstring(),
                L"-DCONFIG=" + configuration,
                L"-P",
                fs::path(CPPLIVELAB_SOURCE_DIR).wstring() + L"/scripts/file_api.cmake"},
            project.root,
            environment);
        if (result != 0)
        {
            return result;
        }
    }
    profile.checkpoint("metadata_before_ms");
    auto targets = executable_targets(project.root, configuration, options);
    const auto generator = read(directory / L"generator.txt");
    if (generator != "Ninja" && generator != "Ninja Multi-Config" &&
        generator != "Visual Studio 17 2022")
    {
        throw std::runtime_error(
            "Unsupported CMake generator: " + generator +
            ". Stage 5 supports Ninja, Ninja Multi-Config and Visual Studio 17 2022.");
    }
    for (const auto& target : targets)
    {
        if (!path_support::is_within_directory(
                fs::weakly_canonical(target.artifact), fs::weakly_canonical(directory)))
        {
            throw std::runtime_error(
                "Project places an executable outside the isolated build directory. "
                "Build refused to avoid overwriting existing outputs; review its output-directory "
                "configuration.");
        }
    }
    auto selected = targets.end();
    if (!options.target.empty())
    {
        selected = std::find_if(targets.begin(),
            targets.end(),
            [&](const auto& target)
            {
                return target.name == options.target;
            });
        if (selected == targets.end())
        {
            throw std::runtime_error(
                "Selected executable target is not present in this configuration.");
        }
    }
    else if (targets.size() == 1)
    {
        selected = targets.begin();
    }
    const auto selected_name = selected == targets.end() ? std::wstring{} : selected->name;
    std::vector<std::wstring> arguments{
        L"--build", directory.wstring(), L"--config", configuration};
    if (selected != targets.end())
    {
        arguments.insert(arguments.end(), {L"--target", selected->name});
    }
    else if (run)
    {
        throw std::runtime_error(
            "Select an executable target; this project has zero or multiple programs.");
    }
    profile.checkpoint("artifact_before_ms");
    profile.ninja_begin(directory);
    result = run_process(cmake, arguments, project.root, environment);
    profile.checkpoint("ninja_wall_ms");
    profile.ninja_end();
    if (result != 0)
    {
        return result;
    }
    // Ninja/MSBuild may regenerate CMake during the build. Only skip the reader
    // when its indexed graph and all derived manifests are still identical.
    if (!reusable_target_metadata(project.root, directory, configuration, options))
    {
        result = run_process(cmake,
            {L"-DBUILD=" + directory.wstring(),
                L"-DCONFIG=" + configuration,
                L"-P",
                fs::path(CPPLIVELAB_SOURCE_DIR).wstring() + L"/scripts/file_api.cmake"},
            project.root,
            environment);
        if (result != 0)
        {
            return result;
        }
    }
    profile.checkpoint("metadata_after_ms");
    targets = executable_targets(project.root, configuration, options);
    if (!selected_name.empty())
    {
        const auto artifact = std::find_if(targets.begin(),
            targets.end(),
            [&](const auto& target)
            {
                return target.name == selected_name;
            });
        if (artifact == targets.end() || !fs::is_regular_file(artifact->artifact))
        {
            std::cerr << "Compilation succeeded but executable discovery failed: the selected "
                         "File API artifact is missing. Review target/output configuration.\n";
            return artifact_missing_exit_code;
        }
    }
    profile.checkpoint("artifact_after_ms");
    const auto dependency_directory = state_root() / L"d" / key(directory);
    register_build_dependency_caches(project.root,
        directory,
        dependency_directory,
        narrow(configuration),
        options.library_transaction);
    std::cout << "Build succeeded: " << path_support::path_utf8(directory) << '\n' << std::flush;
    if (!run)
    {
        return 0;
    }
    targets = executable_targets(project.root, configuration, options);
    selected = std::find_if(targets.begin(),
        targets.end(),
        [&](const auto& target)
        {
            return options.target.empty() || target.name == options.target;
        });
    if (selected == targets.end() || !fs::is_regular_file(selected->artifact))
    {
        throw std::runtime_error("Selected artifact is missing after a successful build.");
    }
    const auto working =
        options.working_directory.empty() ? project.root : options.working_directory;
    if (!fs::is_directory(working))
    {
        throw std::runtime_error("Configured program working directory does not exist.");
    }
    std::cout << "Running: " << path_support::path_utf8(selected->artifact) << '\n' << std::flush;
    return run_process(selected->artifact,
        options.arguments,
        working,
        runtime_environment(std::move(environment), directory));
}

std::vector<fs::path> recent_projects()
{
    std::vector<fs::path> result;
    std::ifstream input(state_root() / L"recent.txt");
    std::string value;
    while (input >> std::quoted(value))
    {
        result.push_back(path_support::from_utf8(value));
    }
    return result;
}

void remember_project(const fs::path& root, bool remove)
{
    auto entries = recent_projects();
    const auto canonical = fs::weakly_canonical(root);
    std::erase_if(entries,
        [&](const auto& entry)
        {
            return key(entry) == key(canonical);
        });
    if (!remove)
    {
        entries.insert(entries.begin(), canonical);
    }
    std::ostringstream output;
    for (size_t index = 0; index < (std::min)(entries.size(), size_t{12}); ++index)
    {
        output << std::quoted(path_support::path_utf8(entries[index])) << '\n';
    }
    write(state_root() / L"recent.txt", output.str());
}
} // namespace cpplivelab
