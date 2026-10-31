#include "../src/library_compatibility.hpp"
#include "../src/tool_workflows.hpp"
#include "workspace_test_support.hpp"
#include <fstream>
#include <iostream>

namespace
{
namespace fs = std::filesystem;
using namespace cpplivelab;

void require(bool passed, const char* reason)
{
    if (!passed)
    {
        throw std::runtime_error(reason);
    }
}

void write(const fs::path& file, const std::string& text)
{
    fs::create_directories(file.parent_path());
    require(static_cast<bool>(std::ofstream(file) << text), "Fixture write failed");
}

void unit(const fs::path& root)
{
    require(assess_library(root / L"missing").status == LibraryCompatibility::UnableToVerify,
        "Missing metadata was accepted or called a non-library");
    const auto docs = root / L"docs";
    write(docs / L"README.md", "Documentation only.\n");
    require(assess_library(docs).status == LibraryCompatibility::NotLibrary,
        "Documentation repository was accepted");
    const auto application = root / L"application";
    write(application / L"main.cpp", "int main() { return 0; }\n");
    require(assess_library(application).status == LibraryCompatibility::NotLibrary,
        "C++ application source was treated as library evidence");
    write(application / L"CMakeLists.txt", "add_executable(app main.cpp)\n");
    require(assess_library(application).status != LibraryCompatibility::Compatible &&
                assess_library(application, {}, true).status == LibraryCompatibility::NotLibrary,
        "Configured application without library targets was accepted");
    const auto python = root / L"python";
    write(python / L"pyproject.toml", "[project]\nname = 'valid_python_package'\n");
    write(python / L"package/__init__.py", "def answer():\n    return 42\n");
    require(assess_library(python).status == LibraryCompatibility::RequiresDecision,
        "Non-C++ package was falsely labelled not a library or integrated natively");
    const auto headers = root / L"headers";
    write(headers / L"answer.hpp", "#pragma once\ninline int answer() { return 42; }\n");
    require(assess_library(headers).native_candidate &&
                assess_library(headers).status == LibraryCompatibility::UnableToVerify,
        "Passive header evidence was claimed verified");
    require(assess_library(headers, {}, true, headers / L"answer.hpp").status ==
                LibraryCompatibility::Compatible,
        "Explicit verified local header was rejected (no version tags are required)");
    require(assess_library(headers, {}, true, application / L"main.cpp").status !=
                LibraryCompatibility::Compatible,
        "Header selection escaped the library source");
    const auto empty = root / L"empty-header";
    write(empty / L"empty.hpp", "#pragma once\n// no public interface\n");
    require(assess_library(empty, {}, true, empty / L"empty.hpp").status !=
                LibraryCompatibility::Compatible,
        "Empty/comment-only header was accepted as a verified API");
    write(empty / L"platform.hpp",
        "#pragma once\n#define WIN32_LEAN_AND_MEAN\n#include <windows.h>\n");
    require(!assess_library(empty).native_candidate &&
                assess_library(empty, {}, true, empty / L"platform.hpp").status !=
                    LibraryCompatibility::Compatible,
        "An OS include wrapper was mistaken for a repository public API");
    write(headers / L"umbrella.hpp", "#pragma once\n#include \"answer.hpp\"\n");
    require(assess_library(headers, {}, true, headers / L"umbrella.hpp").status ==
                LibraryCompatibility::Compatible,
        "A verified umbrella header exporting a local API was rejected");
    write(application / L"private.cpp", "int private_helper() { return 1; }\n");
    const std::vector<LibraryTarget> private_target{
        {"PrivateCore", "STATIC_LIBRARY", "", application, false, true}};
    require(assess_library(application, private_target, true).status !=
                LibraryCompatibility::Compatible,
        "An application-private target with no public interface was accepted");
    ProjectOptions options;
    options.arguments = {L"two words", L"", L"quote\"value", L"trailing\\"};
    save_project_options(headers, L"Debug", options);
    require(load_project_options(headers, L"Debug").arguments == options.arguments,
        "Program arguments did not round-trip exactly");
    write(
        workspace_directory(headers) / L"Debug.settings", "\"old-preset\"\n\"app\"\n\"\"\n\"\"\n");
    const auto older = load_project_options(headers, L"Debug");
    require(older.preset == L"old-preset" && older.target == L"app" && older.arguments.empty(),
        "Older four-field settings were not preserved");
    options.arguments.resize(257);
    bool oversized_rejected = false;
    try
    {
        save_project_options(headers, L"Debug", options);
    }
    catch (const std::exception&)
    {
        oversized_rejected = true;
    }
    require(oversized_rejected && load_project_options(headers, L"Debug").preset == L"old-preset",
        "Oversized argument input overwrote valid existing settings");
    std::cout << "Compatibility statuses, metadata failure, native/header/non-C++ separation and "
                 "arguments passed\n";
}

void configured(const fs::path& root)
{
    for (const bool library : {false, true})
    {
        const auto source = root / (library ? L"library" : L"application");
        write(source / L"CMakeLists.txt",
            "cmake_minimum_required(VERSION 3.24)\nproject(Fixture LANGUAGES CXX)\n" +
                std::string(
                    library
                        ? "add_library(reusable STATIC main.cpp)\n"
                        : "add_executable(application main.cpp)\nset_target_properties(application "
                          "PROPERTIES ENABLE_EXPORTS ON)\n"));
        write(source / L"main.cpp",
            library ? "int answer() { return 42; }\n" : "int main() { return 0; }\n");
        const auto probe = root / (library ? L"library-probe" : L"application-probe");
        write(probe / L"CMakeLists.txt",
            cmake_dependency_probe(source) +
                (library ? "\ntarget_link_libraries(cpplivelab_probe PRIVATE reusable)\n" : ""));
        write(probe / L"main.cpp",
            library ? "int answer();\nint main() { return answer() == 42 ? 0 : 1; }\n"
                    : "int main() { return 0; }\n");
        ProjectOptions options;
        options.allow_configure_writes = true;
        options.target = L"cpplivelab_probe";
        require(managed_build(probe, L"Debug", options, false, false) == 0,
            "Owned configure/link probe failed");
        const auto targets = discovered_library_targets(probe);
        const auto result = assess_library(source, targets, true);
        require(result.status ==
                    (library ? LibraryCompatibility::Compatible : LibraryCompatibility::NotLibrary),
            "Actual CMake reusable-target classification failed");
    }
    std::cout << "Actual CMake library accepted and exported application-only target rejected\n";
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    try
    {
        workspace_test::Fixture fixture;
        if (argc == 2 && std::wstring_view(argv[1]) == L"configured")
        {
            configured(fixture.root);
        }
        else
        {
            unit(fixture.root);
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
