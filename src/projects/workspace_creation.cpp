#include "cpplivelab/workspace.hpp"
#include "cpplivelab/libraries.hpp"
#include "cpplivelab/projects.hpp"
#include "projects/project_paths.hpp"
#include "projects/project_templates.hpp"
#include "process/windows_support.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <regex>

namespace cpplivelab
{
const std::vector<LibraryIntegration>& supported_libraries()
{
    static thread_local std::vector<LibraryIntegration> libraries;
    libraries = {
        {"fmt",
            L"fmt",
            L"Modern C++ text formatting. Supported pinned integration; download required."},
        {"nlohmann_json",
            L"JSON for Modern C++",
            L"Read and write JSON data. Supported pinned integration; download required."}};
    const auto registry = manager_state_directory() / L"integrations";
    std::erase_if(libraries,
        [](const auto& library)
        {
            return library_hidden(library.id);
        });
    std::error_code error;
    for (std::filesystem::directory_iterator entry(registry, error), end; !error && entry != end;
        entry.increment(error))
    {
        if (entry->path().extension() != L".name")
        {
            continue;
        }
        const auto id = entry->path().stem().string();
        if ((!id.starts_with("github-") && !id.starts_with("gitlab-")) ||
            id.find_first_not_of(
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.+-") != id.npos ||
            !std::filesystem::is_regular_file(registry / (id + ".cmake")))
        {
            continue;
        }
        if (library_hidden(id))
        {
            continue;
        }
        std::ifstream input(entry->path(), std::ios::binary);
        std::string name;
        std::getline(input, name);
        libraries.push_back({id,
            path_support::from_utf8(name).wstring(),
            L"Pinned repository integration; configure/link probe passed. Project build still "
            L"required."});
    }
    return libraries;
}

std::vector<std::string> parse_libraries(const std::string& libraries)
{
    std::vector<std::string> result;
    std::istringstream input(libraries);
    for (std::string id; std::getline(input, id, ',');)
    {
        if (id.empty() || id == "none")
        {
            continue;
        }
        if (id == "https://github.com/fmtlib/fmt.git")
        {
            id = "fmt";
        }
        const auto& catalog = supported_libraries();
        if (std::none_of(catalog.begin(),
                catalog.end(),
                [&](const auto& item)
                {
                    return item.id == id;
                }) &&
            !known_library(id))
        {
            throw std::runtime_error(
                "Unsupported library or Git URL: choose a validated catalog integration.");
        }
        if (std::find(result.begin(), result.end(), id) != result.end())
        {
            throw std::runtime_error("Duplicate library selection: " + id);
        }
        result.push_back(id);
    }
    return result;
}

std::string github_dependency_cmake(const std::string& repository,
    const std::string& commit,
    const std::string& archive_hash,
    const std::string& library_target,
    const std::string& include_directory)
{
    const bool gitlab = repository.starts_with("gitlab.com/");
    const auto project = gitlab ? repository.substr(11) : repository;
    const auto path_pattern =
        gitlab ? "[A-Za-z0-9_.-]+(/[A-Za-z0-9_.-]+)+" : "[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+";
    bool invalid_segment = false;
    std::istringstream segments(project);
    for (std::string segment; std::getline(segments, segment, '/');)
    {
        invalid_segment = invalid_segment || segment == "." || segment == ".." || segment == "-";
    }
    if (!std::regex_match(project, std::regex(path_pattern)) || invalid_segment ||
        !std::regex_match(commit, std::regex("[a-fA-F0-9]{40}")) ||
        !std::regex_match(archive_hash, std::regex("[a-fA-F0-9]{64}")) ||
        (!library_target.empty() &&
            !std::regex_match(library_target, std::regex("[A-Za-z0-9_.:+-]+"))) ||
        (library_target.empty() &&
            (include_directory.empty() ||
                !std::regex_match(include_directory, std::regex("[A-Za-z0-9_./+-]+")) ||
                include_directory.find("..") != std::string::npos ||
                std::filesystem::path(include_directory).is_absolute())))
    {
        throw std::runtime_error("Invalid pinned repository integration metadata.");
    }
    const auto name = "cpplivelab_" + commit.substr(0, 12);
    // Both providers use the same hash-pinned FetchContent and compatibility workflow.
    std::string encoded_project;
    for (const auto character : project)
    {
        encoded_project += character == '/' ? "%2F" : std::string(1, character);
    }
    const auto archive_url = gitlab
                                 ? "https://gitlab.com/api/v4/projects/" + encoded_project +
                                       "/repository/archive.zip?sha=" + commit
                                 : "https://codeload.github.com/" + repository + "/zip/" + commit;
    std::string result = "\n# Pinned verified probe: " + repository + " " + commit +
                         "\ninclude(FetchContent)\nFetchContent_Declare(" + name + "\n    URL " +
                         archive_url + "\n    URL_HASH SHA256=" + archive_hash +
                         "\n    DOWNLOAD_EXTRACT_TIMESTAMP FALSE\n)\n";
    if (!library_target.empty())
    {
        result += "FetchContent_MakeAvailable(" + name +
                  ")\n"
                  "target_link_libraries(app PRIVATE " +
                  library_target + ")\n";
    }
    else
    {
        result += "FetchContent_GetProperties(" + name + ")\nif(NOT " + name +
                  "_POPULATED)\n    FetchContent_Populate(" + name +
                  ")\nendif()\n"
                  "target_include_directories(app PRIVATE \"${" +
                  name + "_SOURCE_DIR}/" + include_directory + "\")\n";
    }
    return result;
}

std::string dependency_cmake(const std::string& libraries, const std::string& target)
{
    if (target.empty() ||
        target.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.+-") != target.npos)
    {
        throw std::runtime_error("Choose a normal CMake executable target for the preview.");
    }
    const auto selected = parse_libraries(libraries);
    std::string result;
    if (!selected.empty())
    {
        result = "# Use direct population when supported, avoiding nested Ninja subbuild paths.\n"
                 "if(POLICY CMP0168)\n"
                 "    cmake_policy(SET CMP0168 NEW)\n"
                 "endif()\n";
    }
    for (const auto& id : selected)
    {
        if (id.starts_with("github-") || id.starts_with("gitlab-"))
        {
            std::ifstream input(
                manager_state_directory() / L"integrations" / (id + ".cmake"), std::ios::binary);
            std::string snippet{std::istreambuf_iterator<char>(input), {}};
            // Upgrade directory keys in copied manager metadata only. The source
            // project's existing CMake and the full pinned URL/hash are untouched.
            snippet = std::regex_replace(snippet,
                std::regex("cpplivelab_([a-fA-F0-9]{12})[a-fA-F0-9]{28}"),
                "cpplivelab_$1");
            result += snippet;
        }
    }
    if (std::find(selected.begin(), selected.end(), "fmt") != selected.end())
    {
        result += R"cmake(
include(FetchContent)
set(FMT_TEST OFF CACHE BOOL "" FORCE)
set(FMT_DOC OFF CACHE BOOL "" FORCE)
FetchContent_Declare(fmt
    URL https://codeload.github.com/fmtlib/fmt/tar.gz/refs/tags/11.0.2
    URL_HASH SHA256=6cb1e6d37bdcb756dbbe59be438790db409cdb4868c66e888d5df9f13f7c027f
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(fmt)
target_link_libraries(app PRIVATE fmt::fmt)
)cmake";
    }
    if (std::find(selected.begin(), selected.end(), "nlohmann_json") != selected.end())
    {
        result += R"cmake(
include(FetchContent)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_Declare(nlohmann_json
    URL https://codeload.github.com/nlohmann/json/tar.gz/refs/tags/v3.11.3
    URL_HASH SHA256=0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(nlohmann_json)
target_link_libraries(app PRIVATE nlohmann_json::nlohmann_json)
)cmake";
    }
    const std::string original = "target_link_libraries(app PRIVATE";
    const std::string replacement = "target_link_libraries(" + target + " PRIVATE";
    size_t position = 0;
    while ((position = result.find(original, position)) != result.npos)
    {
        result.replace(position, original.size(), replacement);
        position += replacement.size();
    }
    return result;
}

void create_managed_project(const std::filesystem::path& destination,
    const std::string& project_template,
    const std::string& library)
{
    namespace fs = std::filesystem;
    // Validate before Windows path normalization can silently trim a trailing dot.
    const auto name = destination.filename().wstring();
    if (name.empty() || name == L"." || name == L".." || name.back() == L'.' ||
        name.back() == L' ' || name.find_first_of(L"<>:\"/\\|?*;\r\n\t$") != name.npos)
    {
        throw std::runtime_error("Invalid project name. Use a normal Windows folder name.");
    }
    auto upper = name;
    for (auto& character : upper)
    {
        character = static_cast<wchar_t>(std::towupper(character));
    }
    const auto base = upper.substr(0, upper.find(L'.'));
    if (base == L"CON" || base == L"PRN" || base == L"AUX" || base == L"NUL" ||
        (base.size() == 4 && (base.starts_with(L"COM") || base.starts_with(L"LPT")) &&
            base[3] >= L'1' && base[3] <= L'9'))
    {
        throw std::runtime_error("Reserved Windows device name.");
    }
    const auto root = fs::absolute(destination).lexically_normal();
    if (project_template != "empty" && project_template != "multi")
    {
        throw std::runtime_error(
            "Supported templates: empty, multi. Graphical template is not validated.");
    }
    const auto libraries = parse_libraries(library);
    std::vector<LibraryRecord> records;
    for (const auto& id : libraries)
    {
        auto record = known_library(id);
        if (!record)
        {
            throw std::runtime_error("Selected library metadata is missing.");
        }
        record->consumer_target = "app";
        records.push_back(*record);
    }
    const bool use_fmt = std::find(libraries.begin(), libraries.end(), "fmt") != libraries.end();
    const bool use_json =
        std::find(libraries.begin(), libraries.end(), "nlohmann_json") != libraries.end();
    if (!fs::is_directory(root.parent_path()) || fs::exists(root))
    {
        throw std::runtime_error(
            "Choose an existing parent and a NEW project folder. Nothing was overwritten.");
    }
    // Create in a previously nonexistent sibling and publish by rename only after
    // every generated file succeeds. Failures never delete or edit the destination.
    const auto staging =
        root.parent_path() / (L".easiercpp-new-" + std::to_wstring(GetCurrentProcessId()));
    if (fs::exists(staging))
    {
        throw std::runtime_error(
            "A previous creation staging folder exists; inspect it before retrying.");
    }
    try
    {
        create_project(staging, false);
        fs::create_directories(staging / L"include");
        fs::create_directories(staging / L"assets");
        const auto write = [&](const fs::path& file, const std::string& text)
        {
            std::ofstream output(staging / file, std::ios::binary);
            if (!(output << text))
            {
                throw std::runtime_error("Could not finish generating the new project.");
            }
        };
        std::vector<std::string> sources{"src/main.cpp"};
        if (project_template == "multi")
        {
            sources.push_back("src/greeting.cpp");
        }
        const auto cmake = project_templates::cmake_project(
            path_support::path_utf8(root.filename()), sources, !libraries.empty(), true, true);
        if (!libraries.empty())
        {
            fs::create_directories(staging / L"cmake");
            write("cmake/Dependencies.cmake", managed_dependency_file(records, "app"));
        }
        if (project_template == "multi")
        {
            write("include/greeting.hpp", "#pragma once\n\nconst char* greeting();\n");
            write("src/greeting.cpp",
                "#include \"greeting.hpp\"\n\nconst char* greeting()\n{\n"
                "    return \"Hello from two C++ files!\";\n}\n");
            write("src/main.cpp",
                "#include \"greeting.hpp\"\n\n#include <iostream>\n\nint main()\n{\n"
                "    std::cout << greeting() << '\\n';\n}\n");
        }
        if (use_fmt)
        {

            write("src/main.cpp",
                "#include <fmt/core.h>\n\nint main()\n{\n"
                "    fmt::print(\"fmt dependency verified: {}\\n\", 42);\n}\n");
        }
        if (use_json)
        {

            const std::string includes =
                use_fmt ? "#include <fmt/core.h>\n" : "#include <iostream>\n";
            const std::string output =
                use_fmt ? "    fmt::print(\"fmt dependency verified: {}\\n\", value.dump());\n"
                        : "    std::cout << value.dump() << '\\n';\n";
            write("src/main.cpp",
                "#include <nlohmann/json.hpp>\n" + includes +
                    "\nint main()\n{\n    nlohmann::json value = {{\"answer\", 42}};\n" + output +
                    "}\n");
        }
        for (const auto& record : records)
        {
            // Only this explicitly selected, verified integration gets a Raylib
            // starter. Arbitrary repository names are not evidence of an API.
            if (record.repository == "raysan5/raylib" && std::any_of(record.targets.begin(),
                                                             record.targets.end(),
                                                             [](const auto& target)
                                                             {
                                                                 return target == "raylib" ||
                                                                        target == "raylib_static" ||
                                                                        target == "raylib_shared";
                                                             }))
            {
                write("src/main.cpp", std::string(project_templates::raylib_main));
            }
        }
        write("CMakeLists.txt", cmake);
        write("README.md", project_templates::readme(path_support::path_utf8(root.filename())));
        // MoveFileEx without REPLACE_EXISTING prevents a racing destination from
        // being overwritten. The staging path and destination share a parent/volume.
        if (!MoveFileExW(staging.c_str(), root.c_str(), 0))
        {
            throw std::runtime_error(
                "Could not publish the new project; destination was not overwritten.");
        }
    }
    catch (...)
    {
        std::error_code error;
        if (fs::weakly_canonical(staging, error).parent_path() ==
            fs::weakly_canonical(root.parent_path()))
        {
            fs::remove_all(staging, error);
        }
        throw;
    }
    std::cout << "Created new project: " << path_support::path_utf8(root)
              << ". Dependencies will be verified during Build.\n";
    remember_project_libraries(root, records);
}
} // namespace cpplivelab
