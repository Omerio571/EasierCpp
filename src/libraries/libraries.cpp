#include "cpplivelab/libraries.hpp"
#include "projects/project_paths.hpp"
#include "projects/directory_scan.hpp"
#include "build/build_lease.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <regex>
#include <set>
#include <sstream>
#include <memory>
#include <tuple>

namespace cpplivelab
{
namespace
{
namespace fs = std::filesystem;

bool safe_id(const std::string& id)
{
    return !id.empty() && id.size() < 220 && id != "." && id != ".." &&
           id.find_first_not_of(
               "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.+-") == id.npos;
}

std::string read(const fs::path& file)
{
    std::ifstream input(windows_support::extended_path(file), std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

void atomic_write(const fs::path& file, const std::string& bytes)
{
    fs::create_directories(file.parent_path());
    const auto temporary =
        fs::path(file.wstring() + L".new-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                 std::to_wstring(GetTickCount64()));
    windows_support::UniqueHandle handle;
    handle.reset(CreateFileW(windows_support::extended_path(temporary).c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        nullptr));
    if (handle.value == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error("Cannot reserve an atomic library metadata file.");
    }
    DWORD written = 0;
    const bool saved =
        bytes.size() <= MAXDWORD &&
        WriteFile(
            handle.value, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
        written == bytes.size() && FlushFileBuffers(handle.value);
    handle.reset();
    if (!saved || !MoveFileExW(windows_support::extended_path(temporary).c_str(),
                      windows_support::extended_path(file).c_str(),
                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(windows_support::extended_path(temporary).c_str());
        throw std::runtime_error(
            "Could not atomically save library metadata; previous file retained.");
    }
}

std::string serialize(const LibraryRecord& record)
{
    std::ostringstream output;
    output << "CPPLIVELAB_LIBRARY_3\n"
           << std::quoted(record.id) << '\n'
           << std::quoted(record.name) << '\n'
           << std::quoted(record.repository) << '\n'
           << std::quoted(record.version) << '\n'
           << std::quoted(record.revision) << '\n'
           << std::quoted(record.archive_hash) << '\n'
           << std::quoted(record.include_directory) << '\n'
           << record.builtin << '\n'
           << record.targets.size() << '\n';
    for (const auto& target : record.targets)
    {
        output << std::quoted(target) << '\n';
    }
    output << record.cmake_options.size() << '\n';
    for (const auto& option : record.cmake_options)
    {
        output << std::quoted(option) << '\n';
    }
    output << std::quoted(record.header) << '\n';
    output << std::quoted(record.consumer_target) << '\n';
    output << std::quoted(record.legacy_cmake) << '\n';
    output << std::quoted(record.integration) << '\n'
           << std::quoted(record.package) << '\n'
           << std::quoted(record.local_path) << '\n'
           << std::quoted(record.components) << '\n';
    return output.str();
}

LibraryRecord deserialize(std::istream& input)
{
    LibraryRecord record;
    size_t count = 0;
    std::string schema;
    input >> schema;
    if (schema != "CPPLIVELAB_LIBRARY_1" && schema != "CPPLIVELAB_LIBRARY_2" &&
        schema != "CPPLIVELAB_LIBRARY_3")
    {
        throw std::runtime_error("Unsupported library metadata schema; previous files retained.");
    }
    input >> std::quoted(record.id) >> std::quoted(record.name) >> std::quoted(record.repository) >>
        std::quoted(record.version) >> std::quoted(record.revision) >>
        std::quoted(record.archive_hash) >> std::quoted(record.include_directory) >>
        record.builtin >> count;
    if (!input || !safe_id(record.id) || count > 256)
    {
        throw std::runtime_error("Invalid library metadata; no files changed.");
    }
    for (size_t index = 0; index < count; ++index)
    {
        std::string target;
        input >> std::quoted(target);
        record.targets.push_back(target);
    }
    input >> count;
    if (!input || count > 256)
    {
        throw std::runtime_error("Invalid library option metadata.");
    }
    for (size_t index = 0; index < count; ++index)
    {
        std::string option;
        input >> std::quoted(option);
        record.cmake_options.push_back(option);
    }
    if (!input)
    {
        throw std::runtime_error("Incomplete library metadata.");
    }
    if (!(input >> std::quoted(record.header) >> std::quoted(record.consumer_target)))
    {
        throw std::runtime_error("Incomplete library consumer metadata.");
    }
    if (schema != "CPPLIVELAB_LIBRARY_1" && !(input >> std::quoted(record.legacy_cmake)))
    {
        throw std::runtime_error("Incomplete legacy migration metadata.");
    }
    if (schema == "CPPLIVELAB_LIBRARY_3" &&
        !(input >> std::quoted(record.integration) >> std::quoted(record.package) >>
            std::quoted(record.local_path) >> std::quoted(record.components)))
    {
        throw std::runtime_error("Incomplete library integration metadata.");
    }
    return record;
}

fs::path catalog_file(const std::string& id, const std::string& extension)
{
    if (!safe_id(id))
    {
        throw std::runtime_error("Invalid catalog identifier.");
    }
    return manager_state_directory() / L"integrations" / (id + extension);
}

std::string project_manifest(const std::vector<LibraryRecord>& records)
{
    std::ostringstream output;
    output << records.size() << '\n';
    for (const auto& record : records)
    {
        output << serialize(record);
    }
    return output.str();
}

void owned_path(const fs::path& path, bool require_entry = true)
{
    const auto state = fs::weakly_canonical(manager_state_directory());
    const auto canonical = fs::weakly_canonical(path);
    bool allowed = false;
    for (const auto* category : {L"d", L"library-sources", L"dependency-probes"})
    {
        const auto root = manager_state_directory() / category;
        const auto relative = path.lexically_relative(root);
        const auto depth = static_cast<size_t>(std::distance(relative.begin(), relative.end()));
        const bool precise_entry =
            !require_entry || (std::wstring_view(category) == L"d" ? depth == 2 : depth == 1);
        if (!relative.empty() && !relative.is_absolute() && *relative.begin() != L".." &&
            precise_entry && canonical != fs::weakly_canonical(root) &&
            path_support::is_within_directory(canonical, fs::weakly_canonical(root)) &&
            path_support::is_within_directory(fs::weakly_canonical(root), state))
        {
            allowed = true;
        }
    }
    if (!allowed)
    {
        throw std::runtime_error(
            "Cache is not a registered child of an approved manager cache root.");
    }
    auto current = path;
    while (current != manager_state_directory() && !current.empty())
    {
        const auto attributes = GetFileAttributesW(windows_support::extended_path(current).c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
        {
            throw std::runtime_error(
                "Cache junction/symlink refused; no external paths will be followed.");
        }
        current = current.parent_path();
    }
}

std::vector<fs::path> project_build_profiles(const fs::path& project)
{
    std::vector<fs::path> result;
    std::error_code error;
    for (fs::directory_iterator item(workspace_directory(project), error), end;
        !error && item != end;
        item.increment(error))
    {
        if (!item->path().filename().wstring().starts_with(L"build-"))
        {
            continue;
        }
        const auto attributes = GetFileAttributesW(item->path().c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
        {
            throw std::runtime_error(
                "Owning build profile is linked or inaccessible; operation refused.");
        }
        if (attributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            result.push_back(item->path());
        }
    }
    if (error && error != std::errc::no_such_file_or_directory)
    {
        throw std::runtime_error("Cannot inspect owning build profiles: " + error.message());
    }
    return result;
}

std::vector<std::unique_ptr<BuildLease>> lock_project_builds(const fs::path& project)
{
    std::vector<std::unique_ptr<BuildLease>> guards;
    for (const auto& directory : project_build_profiles(project))
    {
        guards.push_back(std::make_unique<BuildLease>(directory));
        ensure_executables_stopped(directory);
    }
    return guards;
}

std::string managed_text(const std::vector<LibraryRecord>& records, const std::string& target)
{
    std::string result = "# CppLiveLab-managed pinned dependencies.\n";
    std::set<std::string> identities;
    for (const auto& record : records)
    {
        const auto identity = record.integration == "package" ? "package:" + record.package
                              : record.integration != "fetch"
                                  ? record.integration + ":" + record.local_path
                              : record.repository.empty() ? record.id
                                                          : record.repository;
        if (!identities.insert(identity).second)
        {
            throw std::runtime_error("The same library repository is selected more than once.");
        }
        result += "\n# BEGIN CppLiveLab library " + record.id + "\n" +
                  library_snippet(
                      record, record.consumer_target.empty() ? target : record.consumer_target) +
                  "# END CppLiveLab library " + record.id + "\n";
    }
    return result;
}
} // namespace

std::string library_key(const std::string& text)
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char character : text)
    {
        hash ^= character;
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << std::hex << hash;
    return output.str();
}

std::string managed_dependency_file(
    const std::vector<LibraryRecord>& records, const std::string& executable_target)
{
    return managed_text(records, executable_target);
}

std::vector<std::string> parse_library_targets(const std::string& text)
{
    auto normalized = text;
    std::replace(normalized.begin(), normalized.end(), ',', ' ');
    std::replace(normalized.begin(), normalized.end(), ';', ' ');
    std::istringstream input(normalized);
    std::vector<std::string> targets;
    for (std::string target; input >> target;)
    {
        if (!std::regex_match(target, std::regex("[A-Za-z0-9_.+-]+(?:::[A-Za-z0-9_.+-]+)*")))
        {
            throw std::runtime_error(
                "Use exact CMake targets (Namespace::Target), separated by lines.");
        }
        if (std::find(targets.begin(), targets.end(), target) != targets.end())
        {
            throw std::runtime_error("Duplicate library target selection.");
        }
        targets.push_back(target);
    }
    return targets;
}

std::vector<LibraryTarget> discovered_library_targets(const fs::path& probe)
{
    std::vector<LibraryTarget> result;
    std::istringstream input(read(probe / L"target-details.txt"));
    for (std::string line; std::getline(input, line);)
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        std::istringstream fields(line);
        std::vector<std::string> values;
        for (std::string value; std::getline(fields, value, '|');)
        {
            values.push_back(value);
        }
        if (values.size() == 6)
        {
            result.push_back({values[0],
                values[1],
                values[2],
                path_support::from_utf8(values[3]),
                values[4] == "TRUE" || values[4] == "1",
                values[5] == "1"});
        }
    }
    // If a real public alias exists, keep its backing implementation in Advanced.
    for (auto& target : result)
    {
        if (std::any_of(result.begin(),
                result.end(),
                [&](const auto& alias)
                {
                    return alias.primary && alias.alias_of == target.name;
                }))
        {
            target.primary = false;
        }
    }
    std::stable_sort(result.begin(),
        result.end(),
        [](const auto& left, const auto& right)
        {
            if (left.primary != right.primary)
            {
                return left.primary;
            }
            return left.name < right.name;
        });
    return result;
}

bool library_hidden(const std::string& id)
{
    return fs::is_regular_file(catalog_file(id, ".hidden"));
}

void show_builtin_library(const std::string& id)
{
    if (id != "fmt" && id != "nlohmann_json")
    {
        throw std::runtime_error("Only a protected built-in entry can be unhidden.");
    }
    fs::remove(catalog_file(id, ".hidden"));
}

std::optional<LibraryRecord> known_library(const std::string& id)
{
    if (id == "fmt")
    {
        return LibraryRecord{id, "fmt", "fmtlib/fmt", "11.0.2", "", "", {"fmt::fmt"}, "", {}, true};
    }
    if (id == "nlohmann_json")
    {
        return LibraryRecord{id,
            "JSON for Modern C++",
            "nlohmann/json",
            "v3.11.3",
            "",
            "",
            {"nlohmann_json::nlohmann_json"},
            "",
            {},
            true};
    }
    if (fs::is_regular_file(catalog_file(id, ".library")))
    {
        std::istringstream input(read(catalog_file(id, ".library")));
        return deserialize(input);
    }
    if (!fs::is_regular_file(catalog_file(id, ".name")) ||
        !fs::is_regular_file(catalog_file(id, ".cmake")))
    {
        const auto retired = manager_state_directory() / L"retired-libraries" / (id + ".library");
        if (fs::is_regular_file(retired))
        {
            std::istringstream input(read(retired));
            return deserialize(input);
        }
        return {};
    }
    LibraryRecord record;
    record.id = id;
    record.name = read(catalog_file(id, ".name"));
    const auto snippet = read(catalog_file(id, ".cmake"));
    record.legacy_cmake = snippet;
    std::smatch match;
    if (!std::regex_search(snippet,
            match,
            std::regex("https://codeload.github.com/([A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+)/zip/"
                       "([a-f0-9]{40})")))
    {
        throw std::runtime_error(
            "Legacy catalog metadata is incomplete; inspect it before management.");
    }
    record.repository = match[1];
    record.revision = match[2];
    if (std::regex_search(snippet, match, std::regex("SHA256=([a-fA-F0-9]{64})")))
    {
        record.archive_hash = match[1];
    }
    if (std::regex_search(
            snippet, match, std::regex("target_link_libraries\\(app PRIVATE ([^)]+)\\)")))
    {
        record.targets = parse_library_targets(match[1]);
    }
    else if (std::regex_search(snippet, match, std::regex("_SOURCE_DIR}[/]([A-Za-z0-9_./+-]+)")))
    {
        record.include_directory = match[1];
    }
    if (std::regex_search(record.name, match, std::regex(" @ ([^ ]+)")))
    {
        record.version = match[1];
    }
    return record;
}

std::string library_snippet(
    const LibraryRecord& record, const std::string& target, bool validate_target_types)
{
    if (record.builtin)
    {
        return dependency_cmake(record.id, target);
    }
    if (record.integration != "fetch")
    {
        if (parse_library_targets(target).size() != 1)
        {
            throw std::runtime_error("Choose one valid consumer target.");
        }
        const auto bracket = [](const std::string& text)
        {
            if (text.find("]==]") != text.npos || text.find('\n') != text.npos ||
                text.find('\r') != text.npos)
            {
                throw std::runtime_error("Invalid local library path.");
            }
            return "[==[" + text + "]==]";
        };
        std::string result;
        for (const auto& option : record.cmake_options)
        {
            std::smatch match;
            if (!std::regex_match(
                    option, match, std::regex("([A-Za-z_][A-Za-z0-9_]*)=([A-Za-z0-9_.+-]+)")))
            {
                throw std::runtime_error("Use NAME=VALUE CMake settings without executable code.");
            }
            result +=
                "set(" + match[1].str() + " \"" + match[2].str() + "\" CACHE STRING \"\" FORCE)\n";
        }
        if (record.integration == "package")
        {
            if (!std::regex_match(record.package, std::regex("[A-Za-z_][A-Za-z0-9_+-]*")) ||
                (!record.version.empty() &&
                    !std::regex_match(record.version, std::regex("[0-9]+(\\.[0-9]+)*"))))
            {
                throw std::runtime_error(
                    "Use a CMake package name and an optional numeric version.");
            }
            if (!record.local_path.empty())
            {
                result += "list(PREPEND CMAKE_PREFIX_PATH " + bracket(record.local_path) + ")\n";
            }
            result += "find_package(" + record.package + " " + record.version + " CONFIG REQUIRED";
            const auto components = parse_library_targets(record.components);
            if (!components.empty())
            {
                result += " COMPONENTS";
                for (const auto& component : components)
                {
                    result += " " + component;
                }
            }
            result += ")\n";
        }
        else if (record.integration == "source")
        {
            if (!path_support::from_utf8(record.local_path).is_absolute())
            {
                throw std::runtime_error(
                    "Local source library needs an absolute CMake source directory.");
            }
            result += "add_subdirectory(" + bracket(record.local_path) +
                      " \"${CMAKE_CURRENT_BINARY_DIR}/cpp-libraries/" + library_key(record.id) +
                      "\" EXCLUDE_FROM_ALL)\n";
        }
        else if (record.integration == "headers")
        {
            if (!path_support::from_utf8(record.local_path).is_absolute())
            {
                throw std::runtime_error(
                    "Header-only integration needs an absolute include directory.");
            }
            const auto interface_target = "cpp_headers_" + library_key(record.id);
            result += "if(NOT IS_DIRECTORY " + bracket(record.local_path) +
                      ")\n    message(FATAL_ERROR \"Header-only include directory is "
                      "missing\")\nendif()\n";
            result += "add_library(" + interface_target + " INTERFACE)\n";
            result += "target_include_directories(" + interface_target + " INTERFACE " +
                      bracket(record.local_path) + ")\n";
            return result + "target_link_libraries(" + target + " PRIVATE " + interface_target +
                   ")\n";
        }
        else
        {
            throw std::runtime_error("Unsupported integration method. Supply a CMake package, "
                                     "source target, or header-only path.");
        }
        if (record.targets.empty())
        {
            throw std::runtime_error("Specify the actual CMake targets exported by this library.");
        }
        for (const auto& linked : record.targets)
        {
            if (parse_library_targets(linked).size() != 1)
            {
                throw std::runtime_error("Invalid library target.");
            }
            result += "if(NOT TARGET " + linked +
                      ")\n    message(FATAL_ERROR \"Required library target missing: " + linked +
                      "\")\nendif()\n";
            if (validate_target_types)
            {
                result += "get_target_property(cpplivelab_library_type " + linked +
                          " TYPE)\n"
                          "if(NOT cpplivelab_library_type MATCHES "
                          "\"^(STATIC|SHARED|INTERFACE|OBJECT)_LIBRARY$\")\n"
                          "    message(FATAL_ERROR \"Incompatible - Not a library: " +
                          linked + "\")\nendif()\n";
            }
            result += "target_link_libraries(" + target + " PRIVATE " + linked + ")\n";
        }
        return result;
    }
    const auto selected = parse_library_targets(
        [&]
        {
            std::string value;
            for (const auto& name : record.targets)
            {
                value += name + "\n";
            }
            return value;
        }());
    std::string snippet = "if(POLICY CMP0168)\n    cmake_policy(SET CMP0168 NEW)\nendif()\n";
    for (const auto& option : record.cmake_options)
    {
        std::smatch match;
        if (!std::regex_match(
                option, match, std::regex("([A-Za-z_][A-Za-z0-9_]*)=(ON|OFF|[A-Za-z0-9_.+-]+)")))
        {
            throw std::runtime_error("Use validated NAME=VALUE CMake build options.");
        }
        snippet +=
            "set(" + match[1].str() + " \"" + match[2].str() + "\" CACHE STRING \"\" FORCE)\n";
    }
    snippet += github_dependency_cmake(record.repository,
        record.revision,
        record.archive_hash,
        selected.empty() ? "" : selected.front(),
        record.include_directory);
    if (selected.size() > 1)
    {
        snippet += "target_link_libraries(app PRIVATE\n";
        for (size_t index = 1; index < selected.size(); ++index)
        {
            snippet += "    " + selected[index] + "\n";
        }
        snippet += ")\n";
    }
    const auto validated = parse_library_targets(target);
    if (validated.size() != 1)
    {
        throw std::runtime_error("Select one consumer executable target.");
    }
    size_t position = 0;
    while ((position = snippet.find("target_link_libraries(app PRIVATE", position)) != snippet.npos)
    {
        const auto replacement = "target_link_libraries(" + target + " PRIVATE";
        snippet.replace(
            position, std::string("target_link_libraries(app PRIVATE").size(), replacement);
        position += replacement.size();
    }
    return snippet;
}

void save_known_library(const LibraryRecord& record)
{
    if (record.builtin)
    {
        throw std::runtime_error("Built-in integration code is protected.");
    }
    const auto snippet = library_snippet(record, "app");
    const auto previous = known_library(record.id);
    if (previous)
    {
        // Capture legacy recipes and consumer targets while the old catalog is
        // still available. A same-pin target change must not erase the evidence
        // needed to preview/migrate or roll back an existing project safely.
        for (const auto& project : library_references(*previous))
        {
            remember_project_libraries(project, project_libraries(project));
        }
    }
    auto catalog_record = record;
    catalog_record.consumer_target.clear();
    if (catalog_record.legacy_cmake.empty() && previous)
    {
        catalog_record.legacy_cmake = previous->legacy_cmake;
    }
    atomic_write(catalog_file(record.id, ".library"), serialize(catalog_record));
    atomic_write(catalog_file(record.id, ".cmake"), snippet);
    // Publish visibility last, only after metadata and verified snippet are durable.
    atomic_write(catalog_file(record.id, ".name"), record.name);
}

void delete_known_library(const std::string& id, bool confirmed)
{
    if (!confirmed)
    {
        throw std::runtime_error(
            "Catalog removal needs confirmation; project pins and caches are retained.");
    }
    if (id == "fmt" || id == "nlohmann_json")
    {
        const auto record = known_library(id);
        for (const auto& project : library_references(*record))
        {
            remember_project_libraries(project, project_libraries(project));
        }
        atomic_write(
            catalog_file(id, ".hidden"), "Hidden by user; built-in implementation protected.\n");
        return;
    }
    const auto record = known_library(id);
    if (!record || !fs::is_regular_file(catalog_file(id, ".name")))
    {
        throw std::runtime_error(
            "Known library entry is already removed; project pins are retained.");
    }
    for (const auto& project : library_references(*record))
    {
        // Preserve independent project pins before hiding/deleting the catalog
        // recipe, including older projects discovered from known recent paths.
        remember_project_libraries(project, project_libraries(project));
    }
    atomic_write(
        manager_state_directory() / L"retired-libraries" / (id + ".library"), serialize(*record));
    atomic_write(manager_state_directory() / L"retired-libraries" / (id + ".cmake"),
        read(catalog_file(id, ".cmake")));
    // Project manifests/snippets contain independent pins; never remove those.
    for (const auto* extension : {".name", ".library", ".cmake"})
    {
        fs::remove(catalog_file(id, extension));
    }
}

std::vector<LibraryRecord> project_libraries(
    const fs::path& project, const std::string& transaction)
{
    std::vector<LibraryRecord> result;
    const auto workspace = workspace_directory(project);
    const auto journal = workspace / L"library-transaction";
    if (!transaction.empty() && read(journal / L"pending") != transaction)
    {
        throw std::runtime_error("Library transaction is expired; proposed pins were not used.");
    }
    std::istringstream input(read(
        transaction.empty() ? workspace / L"libraries.manifest" : journal / L"candidate.manifest"));
    size_t count = 0;
    if (!(input >> count))
    {
        const auto content =
            read(project / L"CMakeLists.txt") + read(project / L"cmake/Dependencies.cmake");
        if (content.starts_with("# CppLiveLab dependency target discovery\n"))
        {
            return result;
        }
        for (const auto& item : supported_libraries())
        {
            if (auto record = known_library(item.id))
            {
                const bool pinned = record->builtin
                                        ? content.find(record->repository + "/tar.gz/refs/tags/" +
                                                       record->version) != content.npos
                                        : !record->revision.empty() &&
                                              content.find(record->revision) != content.npos;
                if ((record->repository.empty() ||
                        content.find(record->repository) != content.npos) &&
                    pinned)
                {
                    if (!record->legacy_cmake.empty() &&
                        content.find(record->legacy_cmake) != content.npos)
                    {
                        std::smatch old_links;
                        if (std::regex_search(record->legacy_cmake,
                                old_links,
                                std::regex("target_link_libraries\\(app PRIVATE ([^)]+)\\)")))
                        {
                            record->targets = parse_library_targets(old_links[1]);
                        }
                    }
                    const std::regex links("target_link_libraries\\(\\s*([A-Za-z0-9_.+-]+)\\s+("
                                           "PRIVATE|PUBLIC|INTERFACE)\\s+([^)]*)\\)");
                    for (std::sregex_iterator link(content.begin(), content.end(), links), end;
                        link != end;
                        ++link)
                    {
                        if (!record->targets.empty() &&
                            (*link)[3].str().find(record->targets.front()) != std::string::npos)
                        {
                            record->consumer_target = (*link)[1];
                            break;
                        }
                    }
                    result.push_back(*record);
                }
            }
        }
        return result;
    }
    if (count > 256)
    {
        throw std::runtime_error("Project library manifest is invalid.");
    }
    for (size_t index = 0; index < count; ++index)
    {
        result.push_back(deserialize(input));
    }
    return result;
}

void remember_project_libraries(const fs::path& project, const std::vector<LibraryRecord>& records)
{
    atomic_write(workspace_directory(project) / L"libraries.manifest", project_manifest(records));
    atomic_write(workspace_directory(project) / L"project.path",
        path_support::path_utf8(fs::absolute(project)));
}

std::vector<fs::path> library_references(const LibraryRecord& record)
{
    std::vector<fs::path> roots = recent_projects();
    const auto projects = manager_state_directory() / L"projects";
    std::error_code error;
    for (fs::directory_iterator item(projects, error), end; !error && item != end;
        item.increment(error))
    {
        const auto path = read(item->path() / L"project.path");
        if (!path.empty())
        {
            roots.push_back(path_support::from_utf8(path));
        }
    }
    std::set<fs::path> result;
    for (const auto& root : roots)
    {
        auto text = read(root / L"CMakeLists.txt") + read(root / L"cmake/Dependencies.cmake");
        auto repository = record.repository;
        const auto lower = [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        };
        std::transform(text.begin(), text.end(), text.begin(), lower);
        std::transform(repository.begin(), repository.end(), repository.begin(), lower);
        const bool same_repository = repository.empty() || text.find(repository) != text.npos;
        const bool pinned =
            record.builtin
                ? text.find(repository + "/tar.gz/refs/tags/" + record.version) != text.npos
                : !record.revision.empty() && text.find(record.revision) != text.npos;
        if (same_repository && pinned)
        {
            result.insert(root);
        }
    }
    return {result.begin(), result.end()};
}

LibraryChange preview_library_change(
    const fs::path& project, const std::vector<LibraryRecord>& records, const std::string& target)
{
    LibraryChange change;
    change.project = inspect_project(project).root;
    change.token =
        library_key(path_support::path_utf8(project) + std::to_string(GetCurrentProcessId()) +
                    std::to_string(GetTickCount64()));
    change.old_root = read(project / L"CMakeLists.txt");
    const auto dependencies = project / L"cmake/Dependencies.cmake";
    for (const auto& file : {project / L"CMakeLists.txt", dependencies, dependencies.parent_path()})
    {
        const auto attributes = GetFileAttributesW(windows_support::extended_path(file).c_str());
        if (!path_support::is_within_directory(
                fs::weakly_canonical(file), fs::weakly_canonical(project)) ||
            (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT)))
        {
            throw std::runtime_error("Library configuration is linked or outside the project; "
                                     "automatic writes refused.");
        }
    }
    change.dependencies_existed = fs::exists(dependencies);
    change.dependency_directory_existed = fs::exists(dependencies.parent_path());
    change.old_dependencies = read(dependencies);
    change.new_dependencies = change.old_dependencies;
    change.new_root = change.old_root;
    change.libraries = records;
    const auto previous = project_libraries(project);
    // Replace only exact recorded sections. Unrelated text, even inside the
    // dependency file, remains byte-for-byte intact. Edited managed blocks refuse.
    for (const auto& record : previous)
    {
        const auto begin = "# BEGIN CppLiveLab library " + record.id + "\n";
        const auto end = "# END CppLiveLab library " + record.id + "\n";
        const auto start = change.new_dependencies.find(begin);
        const auto finish = change.new_dependencies.find(end, start);
        if (start == std::string::npos || finish == std::string::npos)
        {
            // Older CppLiveLab recipes can be migrated only when their exact
            // recorded bytes match. The complete removal/addition is previewed.
            auto legacy = !record.legacy_cmake.empty() ? record.legacy_cmake
                          : record.builtin             ? dependency_cmake(record.id, target)
                                                       : read(catalog_file(record.id, ".cmake"));
            if (legacy.empty() && !record.builtin)
            {
                legacy =
                    read(manager_state_directory() / L"retired-libraries" / (record.id + ".cmake"));
            }
            const std::string original = "target_link_libraries(app PRIVATE";
            size_t position = 0;
            while ((position = legacy.find(original, position)) != legacy.npos)
            {
                const auto replacement =
                    "target_link_libraries(" +
                    (record.consumer_target.empty() ? target : record.consumer_target) + " PRIVATE";
                legacy.replace(position, original.size(), replacement);
                position += replacement.size();
            }
            bool matched = false;
            if (!legacy.empty())
            {
                for (auto* bytes : {&change.new_dependencies, &change.new_root})
                {
                    const auto at = bytes->find(legacy);
                    if (at != bytes->npos)
                    {
                        bytes->erase(at, legacy.size());
                        matched = true;
                        break;
                    }
                }
            }
            if (!matched)
            {
                throw std::runtime_error("Managed library boundaries/legacy recipe do not match. "
                                         "Review CMake manually; no files changed.");
            }
            continue;
        }
        const auto actual =
            change.new_dependencies.substr(start + begin.size(), finish - start - begin.size());
        const auto consumer = record.consumer_target.empty() ? target : record.consumer_target;
        if (actual != library_snippet(record, consumer) &&
            actual != library_snippet(record, consumer, false))
        {
            throw std::runtime_error(
                "A managed library block was edited. No user changes were overwritten.");
        }
        change.new_dependencies.erase(start, finish + end.size() - start);
    }
    if (previous.empty() && change.dependencies_existed &&
        change.old_dependencies != "# CppLiveLab-managed pinned dependencies.\n")
    {
        // Imported or older generated files have no ownership ledger. Retain
        // them rather than assuming their declarations are ours to remove.
        if (change.old_dependencies.find("FetchContent_Declare") != std::string::npos)
        {
            throw std::runtime_error(
                "This dependency file predates per-library ownership. Existing declarations are "
                "preserved; explicit migration is required.");
        }
    }
    for (auto& record : change.libraries)
    {
        const auto unmanaged = change.new_root + change.new_dependencies;
        const std::regex links("target_link_libraries\\s*\\(([^)]*)\\)", std::regex::icase);
        for (std::sregex_iterator link(unmanaged.begin(), unmanaged.end(), links), end; link != end;
            ++link)
        {
            // Conservative overlap detection: an existing target mention could
            // be manually owned or transitive. Never adopt or rewrite it silently.
            std::istringstream arguments((*link)[1].str());
            for (std::string argument; arguments >> argument;)
            {
                if (std::find(record.targets.begin(), record.targets.end(), argument) !=
                    record.targets.end())
                {
                    throw std::runtime_error("Existing CMake already links library target " +
                                             argument +
                                             ". Inspect the manual/transitive integration before "
                                             "adding it; no files changed.");
                }
            }
        }
        if (record.consumer_target.empty())
        {
            record.consumer_target = target;
        }
    }
    const auto generated = managed_text(change.libraries, target);
    change.new_dependencies += generated.substr(generated.find('\n') + 1);
    if (!change.dependencies_existed)
    {
        change.new_dependencies.insert(0, "# CppLiveLab-managed pinned dependencies.\n");
    }
    if (change.new_root.find("cmake/Dependencies.cmake") == change.new_root.npos &&
        !records.empty())
    {
        change.new_root += "\n# BEGIN CppLiveLab dependency module\n"
                           "include(\"${CMAKE_CURRENT_SOURCE_DIR}/cmake/Dependencies.cmake\")\n"
                           "# END CppLiveLab dependency module\n";
    }
    return change;
}

bool apply_library_change(
    const LibraryChange& change, bool approved, const std::function<bool()>& verify)
{
    if (!approved)
    {
        throw std::runtime_error("Library changes require exact preview approval.");
    }
    const auto workspace = workspace_directory(change.project);
    const BuildLease mutation(workspace / L"library-mutation");
    const auto journal = workspace / L"library-transaction";
    if (fs::exists(journal / L"pending"))
    {
        throw std::runtime_error("An interrupted library transaction needs recovery first.");
    }
    const auto root = change.project / L"CMakeLists.txt";
    const auto dependencies = change.project / L"cmake/Dependencies.cmake";
    if (read(root) != change.old_root || read(dependencies) != change.old_dependencies ||
        fs::exists(dependencies) != change.dependencies_existed)
    {
        throw std::runtime_error("Project changed since preview; nothing was overwritten.");
    }
    auto build_guards = lock_project_builds(change.project);
    atomic_write(journal / L"old-root", change.old_root);
    atomic_write(journal / L"new-root", change.new_root);
    atomic_write(journal / L"old-deps", change.old_dependencies);
    atomic_write(journal / L"new-deps", change.new_dependencies);
    atomic_write(journal / L"existed", change.dependencies_existed ? "1" : "0");
    atomic_write(journal / L"directory-existed", change.dependency_directory_existed ? "1" : "0");
    atomic_write(journal / L"candidate.manifest", project_manifest(change.libraries));
    atomic_write(journal / L"pending", change.token);
    const auto rollback = [&]
    {
        if ((read(root) != change.new_root && read(root) != change.old_root) ||
            (read(dependencies) != change.new_dependencies &&
                read(dependencies) != change.old_dependencies))
        {
            throw std::runtime_error("Files changed during verification. Backups retained; "
                                     "automatic overwrite refused.");
        }
        atomic_write(root, change.old_root);
        if (change.dependencies_existed)
        {
            atomic_write(dependencies, change.old_dependencies);
        }
        else
        {
            fs::remove(dependencies);
            if (!change.dependency_directory_existed &&
                fs::is_directory(dependencies.parent_path()) &&
                fs::is_empty(dependencies.parent_path()))
            {
                fs::remove(dependencies.parent_path());
            }
        }
        fs::remove(journal / L"pending");
    };
    try
    {
        atomic_write(dependencies, change.new_dependencies);
        atomic_write(root, change.new_root);
        // Pending is already durable: ordinary builds are blocked at the project
        // gate. Release output leases so only the token-authorized validator can
        // configure a fresh build while the mutation gate remains held.
        build_guards.clear();
        if (!verify())
        {
            rollback();
            return false;
        }
        if (read(root) != change.new_root || read(dependencies) != change.new_dependencies)
        {
            throw std::runtime_error(
                "Configuration changed during verification. User edits and backups retained; "
                "review the interrupted transaction before building.");
        }
        remember_project_libraries(change.project, change.libraries);
        fs::remove(journal / L"pending");
        return true;
    }
    catch (...)
    {
        if (fs::exists(journal / L"pending"))
        {
            rollback();
        }
        throw;
    }
}

void recover_library_transaction(const fs::path& project)
{
    const auto workspace = workspace_directory(project);
    const BuildLease mutation(workspace / L"library-mutation");
    const auto journal = workspace / L"library-transaction";
    if (!fs::is_regular_file(journal / L"pending"))
    {
        return;
    }
    const auto build_guards = lock_project_builds(project);
    for (const auto& [file, old_name, new_name] :
        std::vector<std::tuple<fs::path, std::wstring, std::wstring>>{
            {project / L"CMakeLists.txt", L"old-root", L"new-root"},
            {project / L"cmake/Dependencies.cmake", L"old-deps", L"new-deps"}})
    {
        const auto current = read(file);
        if (current != read(journal / old_name) && current != read(journal / new_name))
        {
            throw std::runtime_error("Interrupted transaction contains later user edits. Inspect "
                                     "its backups; no overwrite performed.");
        }
    }
    atomic_write(project / L"CMakeLists.txt", read(journal / L"old-root"));
    if (read(journal / L"existed") == "1")
    {
        atomic_write(project / L"cmake/Dependencies.cmake", read(journal / L"old-deps"));
    }
    else
    {
        fs::remove(project / L"cmake/Dependencies.cmake");
        if (read(journal / L"directory-existed") == "0" && fs::is_directory(project / L"cmake") &&
            fs::is_empty(project / L"cmake"))
        {
            fs::remove(project / L"cmake");
        }
    }
    fs::remove(journal / L"pending");
}

void register_dependency_cache(const DependencyCache& cache)
{
    if (!safe_id(cache.id) || !safe_id(cache.library_id))
    {
        throw std::runtime_error("Invalid dependency cache registration.");
    }
    owned_path(cache.path);
    if (!fs::is_directory(cache.path))
    {
        return;
    }
    const auto stamp = cache.path / L".cpplivelab-owned";
    atomic_write(stamp, cache.id);
    std::ostringstream output;
    output << std::quoted(cache.library_id) << '\n'
           << std::quoted(cache.kind) << '\n'
           << std::quoted(path_support::path_utf8(fs::absolute(cache.path))) << '\n'
           << std::quoted(path_support::path_utf8(cache.build_directory)) << '\n'
           << std::quoted(cache.identity) << '\n';
    atomic_write(
        manager_state_directory() / L"cache-registry" / (cache.id + ".cache"), output.str());
}

std::vector<DependencyCache> dependency_caches(const std::string& library_id)
{
    std::vector<DependencyCache> result;
    const auto registry = manager_state_directory() / L"cache-registry";
    std::error_code error;
    for (fs::directory_iterator item(registry, error), end; !error && item != end;
        item.increment(error))
    {
        if (item->path().extension() != L".cache")
        {
            continue;
        }
        DependencyCache cache;
        cache.id = item->path().stem().string();
        std::string path, build;
        std::istringstream input(read(item->path()));
        if (!(input >> std::quoted(cache.library_id) >> std::quoted(cache.kind) >>
                std::quoted(path) >> std::quoted(build) >> std::quoted(cache.identity)) ||
            (!library_id.empty() && cache.library_id != library_id))
        {
            continue;
        }
        cache.path = path_support::from_utf8(path);
        cache.build_directory = path_support::from_utf8(build);
        try
        {
            owned_path(cache.path);
            cache.present = fs::is_directory(cache.path);
            if (cache.present)
            {
                scan_directory(cache.path,
                    [&](const fs::path& file, bool directory)
                    {
                        if (!directory)
                        {
                            std::error_code size_error;
                            const auto size =
                                fs::file_size(windows_support::extended_path(file), size_error);
                            if (size_error)
                            {
                                throw std::runtime_error(
                                    "Cache size is unavailable: " + size_error.message());
                            }
                            cache.bytes += size;
                        }
                        return true;
                    });
            }
        }
        catch (const std::exception& failure)
        {
            cache.issue = failure.what();
            cache.bytes = 0;
        }
        result.push_back(cache);
    }
    return result;
}

bool delete_dependency_cache(const std::string& id, bool confirmed, bool allow_shared)
{
    if (!confirmed || !safe_id(id))
    {
        throw std::runtime_error("Deleting a selected cache requires explicit confirmation.");
    }
    const BuildLease cache_lease(manager_state_directory() / L"cache-locks" / id);
    const auto caches = dependency_caches();
    const auto selected = std::find_if(caches.begin(),
        caches.end(),
        [&](const auto& cache)
        {
            return cache.id == id;
        });
    if (selected == caches.end())
    {
        throw std::runtime_error("Cache is not registered; deletion refused.");
    }
    if (!selected->issue.empty())
    {
        throw std::runtime_error(selected->issue);
    }
    owned_path(selected->path);
    if (!selected->present)
    {
        return false;
    }
    if (read(selected->path / L".cpplivelab-owned") != id)
    {
        throw std::runtime_error("Cache ownership marker is absent or changed; deletion refused.");
    }
    auto library = known_library(selected->library_id);
    if (!library)
    {
        std::smatch match;
        if (std::regex_search(selected->library_id, match, std::regex("[a-f0-9]{40}")))
        {
            library = LibraryRecord{};
            library->revision = match[0];
        }
    }
    if (library)
    {
        if (library_references(*library).size() > 1 && !allow_shared)
        {
            throw std::runtime_error(
                "This pinned version is shared. Explicit shared-cache approval is required.");
        }
    }
    std::set<fs::path> build_paths;
    if (library)
    {
        // Header-only integrations may have no compiled dependency directory.
        // Their project's actual build/program lease still protects source-cache
        // deletion, including older profiles and separately launched GUI programs.
        for (const auto& project : library_references(*library))
        {
            const auto profiles = project_build_profiles(project);
            build_paths.insert(profiles.begin(), profiles.end());
        }
    }
    for (const auto& cache : caches)
    {
        if (!cache.build_directory.empty() &&
            (cache.library_id == selected->library_id ||
                (library && !library->revision.empty() &&
                    cache.library_id.find(library->revision) != cache.library_id.npos)))
        {
            build_paths.insert(cache.build_directory);
        }
    }
    std::vector<std::unique_ptr<BuildLease>> builds;
    for (const auto& directory : build_paths)
    {
        if (!path_support::is_within_directory(fs::weakly_canonical(directory),
                fs::weakly_canonical(manager_state_directory() / L"projects")))
        {
            throw std::runtime_error("Invalid owning build directory; cache deletion refused.");
        }
        builds.push_back(std::make_unique<BuildLease>(directory));
        ensure_executables_stopped(directory);
    }
    // Refuse any reparse point, including children; remove_all must never be
    // allowed to turn a cached link into traversal of someone else's tree.
    std::error_code error;
    fs::recursive_directory_iterator item(windows_support::extended_path(selected->path), error),
        end;
    while (!error && item != end)
    {
        const auto attributes = GetFileAttributesW(item->path().c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
            item->path().filename() == L".git")
        {
            throw std::runtime_error(
                "Cache contains a link, .git directory or inaccessible entry; deletion refused.");
        }
        item.increment(error);
    }
    if (error)
    {
        throw std::runtime_error("Cache traversal incomplete; nothing deleted.");
    }
    fs::remove_all(windows_support::extended_path(selected->path), error);
    if (error || fs::exists(selected->path))
    {
        if (fs::is_directory(selected->path))
        {
            owned_path(selected->path);
            atomic_write(selected->path / L".cpplivelab-owned", id);
        }
        throw std::runtime_error(
            "Cache removal incomplete (possibly locked files). Registration retained for retry.");
    }
    return true;
}

fs::path library_source_cache(const LibraryRecord& record)
{
    const auto path = manager_state_directory() / L"library-sources" /
                      library_key(record.repository + "|" + record.revision);
    owned_path(path);
    return path;
}

fs::path cached_library_source(const LibraryRecord& record)
{
    if (record.revision.empty())
    {
        return {};
    }
    const auto cache = library_source_cache(record);
    std::ifstream metadata(cache / L"downloaded.txt");
    std::string source, hash;
    if (!std::getline(metadata, source) || !std::getline(metadata, hash) ||
        hash != record.archive_hash)
    {
        return {};
    }
    const auto path = path_support::from_utf8(source);
    owned_path(path, false);
    if (!path_support::is_within_directory(
            fs::weakly_canonical(path), fs::weakly_canonical(cache)) ||
        !fs::is_directory(path) ||
        read(cache / L".cpplivelab-owned") != library_key(path_support::path_utf8(cache)))
    {
        return {};
    }
    return path;
}

void register_build_dependency_caches(const fs::path& project,
    const fs::path& build,
    const fs::path& dependencies,
    const std::string& configuration,
    const std::string& transaction)
{
    for (const auto& record : project_libraries(project, transaction))
    {
        if (record.integration != "fetch")
        {
            // Local sources and installed packages are user-owned, not caches.
            continue;
        }
        const auto name =
            record.builtin ? record.id : "cpplivelab_" + record.revision.substr(0, 12);
        for (const auto* kind : {"src", "build"})
        {
            const auto path = dependencies / (name + "-" + kind);
            const auto identity = record.repository + "|" + record.revision + "|" + configuration +
                                  "|" + read(build / L"generator.txt") + "|" +
                                  read(build / L"CMakeCache.txt");
            register_dependency_cache({library_key(path_support::path_utf8(path)),
                record.id,
                kind == std::string("src") ? "Downloaded source" : "Compiled dependency",
                path,
                build,
                library_key(identity)});
        }
    }
}

void register_probe_dependency_caches(const fs::path& probe, const LibraryRecord& record)
{
    const auto build = managed_build_directory(probe, L"Debug", {});
    const auto path = path_support::from_utf8(read(build / L"dependency-cache.path"));
    if (path.empty())
    {
        return;
    }
    const auto relative = path.lexically_relative(manager_state_directory() / L"d");
    if (relative.empty() || std::distance(relative.begin(), relative.end()) != 1 ||
        *relative.begin() == L"..")
    {
        throw std::runtime_error("Probe dependency root is outside its registered cache category.");
    }
    std::error_code error;
    for (fs::directory_iterator item(path, error), end; !error && item != end;
        item.increment(error))
    {
        if (!item->is_directory(error))
        {
            continue;
        }
        const auto name = path_support::path_utf8(item->path().filename());
        register_dependency_cache({library_key(path_support::path_utf8(item->path())),
            record.id,
            name.ends_with("-src") ? "Temporary verification source"
                                   : "Temporary verification build",
            item->path(),
            build,
            library_key(record.repository + record.revision + read(build / L"CMakeCache.txt"))});
    }
    if (error)
    {
        throw std::runtime_error("Probe cache registration failed: " + error.message());
    }
}
} // namespace cpplivelab
