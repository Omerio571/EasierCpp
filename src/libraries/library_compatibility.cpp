#include "libraries/library_compatibility.hpp"
#include "build/auto_build.hpp"
#include "projects/directory_scan.hpp"
#include <algorithm>
#include <fstream>
#include <regex>
#include <set>

namespace cpplivelab
{
namespace
{
namespace fs = std::filesystem;

bool public_header(const fs::path& file, const fs::path& root, unsigned depth = 0)
{
    static const std::set<fs::path> extensions{L".h", L".hpp", L".hh", L".hxx", L".inl"};
    if (!extensions.contains(file.extension()))
    {
        return false;
    }
    std::ifstream input(file, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("A library header could not be read.");
    }
    std::string bytes(128 * 1024, '\0');
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    bytes.resize(static_cast<size_t>(input.gcount()));
    bytes = std::regex_replace(bytes, std::regex(R"(/\*[\s\S]*?\*/|//[^\n]*)"), "");
    const bool interface_found = std::regex_search(bytes,
        std::regex(
            R"((#\s*define\s+[A-Za-z_][A-Za-z0-9_]*\s*\()|\b(namespace|class|struct|template|typedef|enum|extern|inline|constexpr)\b|[A-Za-z_][A-Za-z0-9_]*\s*\([^;{}]*\)\s*[;{])"));
    if (!interface_found && depth < 4)
    {
        // A real umbrella header can export local headers. Merely including an
        // OS/STL header or setting a platform macro is not a repository API.
        const std::regex include(R"(#\s*include\s*[<"]([A-Za-z0-9_./+-]+)[>"])");
        for (std::sregex_iterator item(bytes.begin(), bytes.end(), include), end; item != end;
            ++item)
        {
            for (const auto& directory : {file.parent_path(), root, root / L"include"})
            {
                const auto header = directory / path_support::from_utf8((*item)[1].str());
                if (path_support::is_within_directory(
                        fs::weakly_canonical(header), fs::weakly_canonical(root)) &&
                    fs::is_regular_file(header) && public_header(header, root, depth + 1))
                {
                    return true;
                }
            }
        }
    }
    if (!interface_found && input.peek() != std::char_traits<char>::eof())
    {
        throw std::runtime_error(
            "Header evidence exceeds the bounded scan; verification is incomplete.");
    }
    return interface_found;
}
} // namespace

std::string compatibility_label(LibraryCompatibility status)
{
    switch (status)
    {
        case LibraryCompatibility::Compatible:
            return "Compatible";
        case LibraryCompatibility::NotLibrary:
            return "Incompatible — Not a library";
        case LibraryCompatibility::RequiresDecision:
            return "Unable to Verify — non-C++ package requires compatibility decision";
        default:
            return "Unable to Verify";
    }
}

LibraryAssessment assess_library(const fs::path& source,
    const std::vector<LibraryTarget>& targets,
    bool consumer_verified,
    const fs::path& selected_header)
{
    try
    {
        if (!fs::is_directory(source))
        {
            return {LibraryCompatibility::UnableToVerify, "Source/metadata is unavailable."};
        }
        bool native_source = false;
        bool header_api = false;
        bool native_library_declaration = false;
        bool application = false;
        bool other_package = false;
        bool other_source = false;
        bool incomplete = false;
        const bool cmake_project = fs::is_regular_file(source / L"CMakeLists.txt");
        size_t visited = 0;
        scan_directory(source,
            [&](const fs::path& file, bool directory)
            {
                if (++visited > 20000)
                {
                    throw std::runtime_error("Library evidence scan limit reached.");
                }
                if (directory)
                {
                    return !ProjectWatcher::ignored_directory(file.filename());
                }
                const auto extension = file.extension();
                const auto name = file.filename();
                if (extension == L".h" || extension == L".hpp" || extension == L".hh" ||
                    extension == L".hxx" || extension == L".inl")
                {
                    header_api = public_header(file, source) || header_api;
                }
                native_source = native_source || extension == L".c" || extension == L".cpp" ||
                                extension == L".cc" || extension == L".cxx";
                other_package = other_package || name == L"pyproject.toml" || name == L"setup.py" ||
                                name == L"Cargo.toml" || name == L"package.json" ||
                                extension == L".csproj";
                other_source = other_source || extension == L".py" || extension == L".rs" ||
                               extension == L".cs" || extension == L".f90" || extension == L".go";
                if (name == L"CMakeLists.txt" || extension == L".cmake" || extension == L".cpp" ||
                    extension == L".c")
                {
                    std::ifstream input(file, std::ios::binary);
                    if (!input)
                    {
                        throw std::runtime_error("Repository build metadata could not be read.");
                    }
                    std::string text(128 * 1024, '\0');
                    input.read(text.data(), static_cast<std::streamsize>(text.size()));
                    text.resize(static_cast<size_t>(input.gcount()));
                    incomplete = incomplete || input.peek() != std::char_traits<char>::eof();
                    native_library_declaration =
                        native_library_declaration ||
                        std::regex_search(
                            text, std::regex(R"(add_library\s*\()", std::regex::icase));
                    application =
                        application ||
                        std::regex_search(text,
                            std::regex(R"(add_executable\s*\(|\b(main|WinMain|wWinMain)\s*\()",
                                std::regex::icase));
                }
                return true;
            });
        const bool reusable_target = std::any_of(targets.begin(),
            targets.end(),
            [&](const auto& target)
            {
                const bool kind =
                    target.type == "STATIC_LIBRARY" || target.type == "SHARED_LIBRARY" ||
                    target.type == "INTERFACE_LIBRARY" || target.type == "OBJECT_LIBRARY";
                return kind && target.primary && !target.source.empty() &&
                       path_support::is_within_directory(
                           fs::weakly_canonical(target.source), fs::weakly_canonical(source));
            });
        bool selected_api = false;
        if (!selected_header.empty())
        {
            selected_api = path_support::is_within_directory(fs::weakly_canonical(selected_header),
                               fs::weakly_canonical(source)) &&
                           public_header(selected_header, source);
        }
        const bool public_interface = header_api || !application;
        if (consumer_verified &&
            ((reusable_target && (native_source || header_api) && public_interface) ||
                selected_api))
        {
            return {LibraryCompatibility::Compatible,
                "Reusable integration passed the consumer compile/link probe.",
                true};
        }
        if (consumer_verified && cmake_project && targets.empty() && application && !header_api)
        {
            return {LibraryCompatibility::NotLibrary,
                "Configured application exports no reusable library targets or public header "
                "interface."};
        }
        if (application && !header_api && (reusable_target || native_library_declaration))
        {
            return {LibraryCompatibility::UnableToVerify,
                "Application/helper targets were found without a public interface. A private "
                "application target is not evidence of a reusable library."};
        }
        if (reusable_target || native_library_declaration || header_api)
        {
            return {LibraryCompatibility::UnableToVerify,
                "Native library evidence found; a supported consumer compile/link probe is still "
                "required.",
                true};
        }
        if (other_package || (other_source && !native_source))
        {
            return {LibraryCompatibility::RequiresDecision,
                "Non-C++ package metadata found. No supported native interface was verified; no "
                "permanent language policy is assumed."};
        }
        if (incomplete)
        {
            return {LibraryCompatibility::UnableToVerify,
                "Build metadata exceeds the bounded scan; evidence is incomplete."};
        }
        if (cmake_project && native_source && !consumer_verified)
        {
            return {LibraryCompatibility::UnableToVerify,
                "CMake source candidate requires actual reusable-target discovery, not language "
                "guessing.",
                true};
        }
        if (application || !native_source)
        {
            return {LibraryCompatibility::NotLibrary,
                application ? "Application code provides no verified reusable library target or "
                              "public header interface."
                            : "No native reusable library source/interface or supported package "
                              "evidence was found."};
        }
        return {LibraryCompatibility::UnableToVerify,
            "C/C++ files alone do not establish an integratable library. Supply supported CMake or "
            "public header metadata."};
    }
    catch (const std::exception& error)
    {
        return {LibraryCompatibility::UnableToVerify, error.what()};
    }
}
} // namespace cpplivelab
