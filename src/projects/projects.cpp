#include "cpplivelab/projects.hpp"
#include "projects/project_templates.hpp"
#include "projects/project_paths.hpp"
#include "build/build_lease.hpp"
#include "build/build_workers.hpp"
#include "cpplivelab/doctor.hpp"
#include "cpplivelab/process.hpp"
#include "tools/tool_workflows.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cwctype>

namespace cpplivelab
{
namespace
{
namespace fs = std::filesystem;

void require_external_project_directory(const fs::path& path)
{
    const auto manager_directory = fs::weakly_canonical(source_repository());
    const auto selected_directory = fs::weakly_canonical(fs::absolute(path));

    if (path_support::is_within_directory(selected_directory, manager_directory))
    {
        throw std::runtime_error("Choose a user project folder outside the EasierCpp source tree.");
    }
}

void write_text_file(const fs::path& path, std::string_view text)
{
    std::ofstream file(path, std::ios::binary);
    if (!(file << text))
    {
        throw std::runtime_error("Cannot write " + path_support::path_utf8(path));
    }
}

std::string escape_cmake_path(const fs::path& path)
{
    const auto utf8 = path.generic_u8string();
    std::string value(utf8.begin(), utf8.end());
    // Semicolons are CMake list separators even in source-file arguments.
    if (value.find_first_of(";\r\n") != value.npos)
    {
        throw std::runtime_error("CMake source paths cannot contain semicolons or newlines.");
    }
    std::string escaped;
    for (char ch : value)
    {
        if (ch == '"' || ch == '$')
        {
            escaped += '\\';
        }
        escaped += ch;
    }
    return escaped;
}

fs::path find_tool_executable(const Toolchain& discovered_toolchain, const std::string& name)
{
    for (const auto& tool : discovered_toolchain.tools)
    {
        if (tool.name == name)
        {
            return tool.installed_path;
        }
    }
    return {};
}

} // namespace

void create_project(const fs::path& folder, bool calculator)
{
    const auto root = fs::absolute(folder);
    require_external_project_directory(root);
    if (fs::exists(root) && (!fs::is_directory(root) || !fs::is_empty(root)))
    {
        throw std::runtime_error(
            "Destination must be a new or empty folder; existing files are never overwritten.");
    }
    fs::create_directories(root / "src");
    fs::create_directories(root / "include");
    fs::create_directories(root / "assets");
    std::vector<std::string> sources{"src/main.cpp"};
    if (calculator)
    {
        sources.push_back("src/calculator.cpp");
    }
    write_text_file(root / "CMakeLists.txt",
        project_templates::cmake_project(
            path_support::path_utf8(root.filename()), sources, false, true, true));
    write_text_file(root / ".gitignore", project_templates::gitignore);
    write_text_file(
        root / "README.md", project_templates::readme(path_support::path_utf8(root.filename())));

    if (calculator)
    {
        write_text_file(root / "src/calculator.hpp", project_templates::calculator_header);
        write_text_file(root / "src/calculator.cpp", project_templates::calculator_implementation);
        write_text_file(root / "src/main.cpp", project_templates::calculator_main);
    }
    else
    {
        write_text_file(root / "src/main.cpp", project_templates::hello_main);
    }

    std::cout << "Created C++20 project: " << path_support::path_utf8(root) << '\n';
}

int build_project(const fs::path& input,
    const std::string& configuration,
    bool should_run,
    const fs::path& executable)
{
    return build_project(input, configuration, should_run, executable, false);
}

int build_project(const fs::path& input,
    const std::string& configuration,
    bool should_run,
    const fs::path& executable,
    bool incremental)
{
    initialize_build_workers();
    const auto build_workers = load_build_workers();
    if (configuration != "Debug" && configuration != "Release")
    {
        throw std::runtime_error("Configuration must be Debug or Release.");
    }
    const auto selected_input_path = fs::weakly_canonical(fs::absolute(input));
    require_external_project_directory(selected_input_path);
    if (!fs::exists(selected_input_path))
    {
        throw std::runtime_error(
            "Input does not exist: " + path_support::path_utf8(selected_input_path));
    }
    const bool is_single_file = fs::is_regular_file(selected_input_path);
    auto cmake_source_directory = selected_input_path;
    fs::path build_output_directory;
    if (is_single_file)
    {
        if (selected_input_path.extension() != ".cpp" && selected_input_path.extension() != ".cc" &&
            selected_input_path.extension() != ".cxx")
        {
            throw std::runtime_error("Single-file input must be .cpp, .cc, or .cxx.");
        }
        const auto owner = fs::exists(selected_input_path.parent_path() / ".cpplivelab")
                               ? ".cpplivelab"
                               : ".easiercpp";
        cmake_source_directory =
            selected_input_path.parent_path() / owner / selected_input_path.filename() / "source";
        fs::create_directories(cmake_source_directory);
        const auto wrapper = project_templates::cmake_project(
            "SingleFile", {escape_cmake_path(selected_input_path)});
        std::ifstream existing_wrapper(cmake_source_directory / "CMakeLists.txt", std::ios::binary);
        const std::string previous_wrapper_text(
            (std::istreambuf_iterator<char>(existing_wrapper)), {});
        if (previous_wrapper_text != wrapper)
        {
            write_text_file(cmake_source_directory / "CMakeLists.txt", wrapper);
        }
        build_output_directory = cmake_source_directory.parent_path() / ("build-" + configuration);
    }
    else
    {
        if (!fs::is_regular_file(cmake_source_directory / "CMakeLists.txt"))
        {
            throw std::runtime_error("Folder has no CMakeLists.txt.");
        }
        const auto owner =
            fs::exists(cmake_source_directory / ".cpplivelab") ? ".cpplivelab" : ".easiercpp";
        build_output_directory = cmake_source_directory / owner / ("build-" + configuration);
    }
    // Keep this owner in scope through Run, not just through the build commands.
    const BuildLease lock(
        build_output_directory); // Held through execution, released even on failure/Stop.
    ensure_executables_stopped(build_output_directory);
    // Use discovered absolute paths. Initializing MSVC also supplies SDK binaries,
    // INCLUDE and LIB through an environment used only by the child processes.
    const auto discovered_toolchain = discover_toolchain(system_discovery_inputs());
    const auto cmake_executable = find_tool_executable(discovered_toolchain, "cmake");
    const auto ninja_executable = find_tool_executable(discovered_toolchain, "ninja");
    auto compiler_executable = find_tool_executable(discovered_toolchain, "cl");
    Environment child_environment;
    if (!compiler_executable.empty() && !discovered_toolchain.developer_command.empty())
    {
        // Select cl from the same VS instance whose environment is initialized.
        const auto visual_studio_directory =
            discovered_toolchain.developer_command.parent_path().parent_path().parent_path();
        std::ifstream version_file(
            visual_studio_directory / "VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt");
        std::string toolset_version;
        version_file >> toolset_version;
        compiler_executable =
            visual_studio_directory / "VC/Tools/MSVC" / toolset_version / "bin/Hostx64/x64/cl.exe";
        child_environment = developer_environment(discovered_toolchain.developer_command);
    }
    else if (compiler_executable.empty())
    {
        compiler_executable = find_tool_executable(discovered_toolchain, "clang++");
        if (compiler_executable.empty())
        {
            compiler_executable = find_tool_executable(discovered_toolchain, "g++");
        }
    }
    if (cmake_executable.empty() || ninja_executable.empty() || compiler_executable.empty())
    {
        throw std::runtime_error(
            "Build requires discovered CMake, Ninja, and a C++ compiler. Run doctor.");
    }
    std::cout << "Configuring " << configuration << ": "
              << path_support::path_utf8(cmake_source_directory) << '\n'
              << std::flush;
    std::vector<std::wstring> configure_arguments{
        L"-S", cmake_source_directory.wstring(), L"-B", build_output_directory.wstring()};
    // Once configured, preserve the project's cached choices and custom options.
    if (!fs::is_regular_file(build_output_directory / L"CMakeCache.txt"))
    {
        configure_arguments.insert(configure_arguments.end(),
            {L"-G",
                L"Ninja",
                L"-DCMAKE_CXX_STANDARD=20",
                L"-DCMAKE_CXX_STANDARD_REQUIRED=ON",
                L"-DCMAKE_CXX_EXTENSIONS=OFF",
                L"-DCMAKE_BUILD_TYPE=" + std::wstring(configuration.begin(), configuration.end()),
                L"-DCMAKE_MAKE_PROGRAM=" + ninja_executable.wstring(),
                L"-DCMAKE_CXX_COMPILER=" + compiler_executable.wstring()});
    }
    // Configuration generates Ninja's build files; the separate build step compiles
    // and links. Preserve each tool's diagnostics and stop immediately on failure.
    int exit_code = 0;
    if (!incremental || !fs::is_regular_file(build_output_directory / L"CMakeCache.txt"))
    {
        exit_code = run_process(
            cmake_executable, configure_arguments, cmake_source_directory, child_environment);
        if (exit_code != 0)
        {
            std::cerr << "Configuration failed (exit " << exit_code << ").\n";
            return exit_code;
        }
    }
    std::vector<std::wstring> build_arguments{L"--build", build_output_directory.wstring()};
    append_build_workers(build_arguments, build_workers);
    exit_code =
        run_process(cmake_executable, build_arguments, cmake_source_directory, child_environment);
    if (exit_code != 0)
    {
        std::cerr << "Compilation failed (exit " << exit_code << "). Program was not run.\n";
        return exit_code;
    }
    std::cout << "Build succeeded: " << path_support::path_utf8(build_output_directory) << '\n'
              << std::flush;
    if (!should_run)
    {
        return 0;
    }
    const auto program_executable =
        executable.empty()
            ? build_output_directory / "app.exe"
            : (executable.is_absolute() ? executable : build_output_directory / executable);
    if (!fs::is_regular_file(program_executable))
    {
        throw std::runtime_error(
            "Executable not found: " + path_support::path_utf8(program_executable) +
            ". For existing CMake projects use --exe <path relative to build directory>.");
    }
    std::cout << "Running: " << path_support::path_utf8(program_executable) << '\n' << std::flush;
    return run_process(program_executable,
        {},
        is_single_file ? selected_input_path.parent_path() : cmake_source_directory,
        child_environment);
}
} // namespace cpplivelab
