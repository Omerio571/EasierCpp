#include "tools/tool_workflows.hpp"
#include <windows.h>
#include <sstream>
#include <stdexcept>
#include "cpplivelab/workspace.hpp"
#include <fstream>
#include <regex>
#include "projects/project_paths.hpp"

namespace cpplivelab
{
std::filesystem::path find_development_repository(
    const std::filesystem::path& compiled, const std::filesystem::path& application_directory)
{
    const auto valid = [](const std::filesystem::path& root)
    {
        return !root.empty() && std::filesystem::is_regular_file(root / L"CMakeLists.txt") &&
               std::filesystem::is_regular_file(root / L"AGENTS.md") &&
               std::filesystem::is_regular_file(root / L"scripts/build.ps1") &&
               std::filesystem::is_regular_file(root / L"include/cpplivelab/workspace.hpp");
    };
    if (valid(compiled))
    {
        return compiled;
    }
    auto ancestor = application_directory;
    for (unsigned depth = 0; depth < 12 && !ancestor.empty(); ++depth)
    {
        if (valid(ancestor))
        {
            return ancestor;
        }
        const auto parent = ancestor.parent_path();
        if (parent == ancestor)
        {
            break;
        }
        ancestor = parent;
    }
    return {};
}

std::filesystem::path source_repository()
{
    std::vector<wchar_t> path(32768);
    const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size())
    {
        throw std::runtime_error("Cannot locate the EasierCpp application directory.");
    }
    const auto application = std::filesystem::path(path.data()).parent_path();
#ifdef EASIERCPP_SOURCE_DIR
    const auto repository = find_development_repository(EASIERCPP_SOURCE_DIR, application);
    if (!repository.empty())
    {
        return repository;
    }
#endif
    return application;
}

std::filesystem::path runtime_resources()
{
    std::vector<wchar_t> path(32768);
    const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size())
    {
        throw std::runtime_error("Cannot locate EasierCpp runtime resources.");
    }
    const auto resources = std::filesystem::path(path.data()).parent_path() / L"resources";
    if (std::filesystem::is_regular_file(resources / L"scripts/file_api.cmake") &&
        std::filesystem::is_regular_file(resources / L"cmake/EasierCppPchHook.cmake"))
    {
        return resources;
    }
#ifdef EASIERCPP_SOURCE_DIR
    return source_repository();
#else
    throw std::runtime_error("EasierCpp support files are missing. Re-extract the complete ZIP "
                             "and keep the resources folder beside both executables.");
#endif
}

TestCommand test_command(const std::wstring& mode, int workers)
{
    if ((mode != L"Quick" && mode != L"Full") || workers < 1 || workers > 16)
    {
        throw std::runtime_error("Choose Quick/Full and between 1 and 16 workers.");
    }
    if (GetEnvironmentVariableW(L"CPPLIVELAB_TEST_CHILD", nullptr, 0))
    {
        throw std::runtime_error("Recursive test-suite execution is disabled inside CTest.");
    }
#ifndef EASIERCPP_SOURCE_DIR
    throw std::runtime_error("This portable EasierCpp build does not include development "
                             "tests. Use Tools > Toolchain Diagnostics or EasierCpp.exe "
                             "doctor. Comprehensive tests require a non-portable source build.");
#endif
    TestCommand result;
    result.repository = source_repository();
    const auto script = result.repository / L"scripts/build.ps1";
    if (!std::filesystem::is_regular_file(script))
    {
#ifdef EASIERCPP_SOURCE_DIR
        throw std::runtime_error("EasierCpp development source scripts are unavailable. Rebuild "
                                 "from the current source checkout. Toolchain Diagnostics and "
                                 "EasierCpp.exe doctor remain available.");
#else
        throw std::runtime_error(
            "This portable EasierCpp build does not include development "
            "tests. Use Tools > Toolchain Diagnostics or EasierCpp.exe "
            "doctor. Comprehensive tests require a non-portable source build.");
#endif
    }
    wchar_t system[MAX_PATH]{};
    if (!GetSystemDirectoryW(system, MAX_PATH))
    {
        throw std::runtime_error("Cannot locate Windows PowerShell.");
    }
    result.executable = std::filesystem::path(system) / L"WindowsPowerShell/v1.0/powershell.exe";
    result.arguments = {L"-NoProfile",
        L"-ExecutionPolicy",
        L"Bypass",
        L"-File",
        script.wstring(),
        L"-TestMode",
        mode,
        L"-Jobs",
        std::to_wstring(workers),
        L"-BuildDirectory",
        (result.repository / L"build/integrated-tests").wstring()};
    if (mode == L"Full")
    {
        result.arguments.push_back(L"-Doctor");
    }
    return result;
}

std::string diagnostics_summary(const Toolchain& toolchain)
{
    std::ostringstream result;
    result << "Toolchain Diagnostics\n";
    for (const auto& tool : toolchain.tools)
    {
        const auto name = tool.name == "cl"        ? "MSVC"
                          : tool.name == "clang++" ? "Clang"
                          : tool.name == "g++"     ? "GCC"
                          : tool.name == "cmake"   ? "CMake"
                                                   : "Ninja";
        result << name << ": " << (tool.installed_path.empty() ? "Not installed" : "Found") << '\n';
    }
    result << "MSVC developer environment: "
           << (toolchain.developer_command.empty() ? "Not found" : "Available")
           << "\nCompatibility: Not checked by discovery; a successful build verifies usability.\n"
           << "Tools outside terminal PATH can be used through discovered paths and child "
              "environment setup.\n"
           << "Tools > Detailed Toolchain Diagnostics shows paths and terminal availability.\n";
    return result.str();
}

std::string cmake_dependency_probe(const std::filesystem::path& source)
{
    if (path_support::path_utf8(source).find("]==]") != std::string::npos)
    {
        throw std::runtime_error("Probe path contains an unsupported CMake bracket delimiter.");
    }
    std::string setup =
        "# EasierCpp dependency target discovery\n"
        "cmake_minimum_required(VERSION 3.24)\nproject(DependencyProbe LANGUAGES C "
        "CXX)\nset(CMAKE_CXX_SCAN_FOR_MODULES OFF)\nadd_executable(cpplivelab_probe "
        "main.cpp)\ntarget_compile_features(cpplivelab_probe PRIVATE cxx_std_20)\n"
        "file(WRITE ${CMAKE_BINARY_DIR}/CMakeGraphVizOptions.cmake\n"
        "    \"set(GRAPHVIZ_GENERATE_PER_TARGET FALSE)\\n"
        "set(GRAPHVIZ_GENERATE_DEPENDERS FALSE)\\n\")\n";
    setup += "if(NOT DEFINED CPPLIVELAB_PROBE_BINARY_DIR)\n"
             "    set(CPPLIVELAB_PROBE_BINARY_DIR \"${CMAKE_CURRENT_BINARY_DIR}/dependency\")\n"
             "endif()\nadd_subdirectory([==[" +
             path_support::path_utf8(source) +
             "]==] \"${CPPLIVELAB_PROBE_BINARY_DIR}\" EXCLUDE_FROM_ALL)\n";
    // Actual configured targets include INTERFACE libraries, which the File API
    // may omit. No exported name is inferred from the repository's name.
    setup += "set(CPPLIVELAB_DEPENDENCY_SOURCE [==[" + path_support::path_utf8(source) +
             "]==])\ninclude([==[" +
             path_support::path_utf8(runtime_resources() / L"scripts/discover_targets.cmake") +
             "]==])\n";
    return setup;
}

bool apply_approved_dependencies(const std::filesystem::path& project,
    const std::string& expected_original,
    const std::string& snippet,
    const std::function<bool()>& verify)
{
    const auto configuration = project / L"CMakeLists.txt";
    std::ifstream input(configuration, std::ios::binary);
    const std::string current{std::istreambuf_iterator<char>(input), {}};
    if (current != expected_original || snippet.empty())
    {
        throw std::runtime_error("CMake configuration changed since preview, or preview is empty. "
                                 "Reopen Manage Libraries.");
    }
    const std::string marker = "# BEGIN EasierCpp approved dependencies";
    if (current.find(marker) != std::string::npos ||
        current.find("# BEGIN CppLiveLab approved dependencies") != std::string::npos)
    {
        throw std::runtime_error("An approved dependency block already exists. Review it manually "
                                 "to avoid duplicate declarations.");
    }
    const std::regex declaration("FetchContent_Declare\\(([A-Za-z0-9_]+)", std::regex::icase);
    for (std::sregex_iterator entry(snippet.begin(), snippet.end(), declaration), end; entry != end;
        ++entry)
    {
        if (std::regex_search(
                current, std::regex("\\b" + (*entry)[1].str() + "\\b", std::regex::icase)))
        {
            throw std::runtime_error("This dependency may already be declared. Review existing "
                                     "CMake manually; no files changed.");
        }
    }
    const auto backup = workspace_directory(project) /
                        (L"CMakeLists.before-libraries-" + std::to_wstring(GetCurrentProcessId()) +
                            L"-" + std::to_wstring(GetTickCount64()) + L".txt");
    std::filesystem::create_directories(backup.parent_path());
    if (!(std::ofstream(backup, std::ios::binary) << current))
    {
        throw std::runtime_error(
            "Could not save dependency-change backup; project remains unchanged.");
    }
    const auto replace = [&](const std::string& bytes)
    {
        const auto temporary =
            project / (L".easiercpp-cmake-" + std::to_wstring(GetCurrentProcessId()) + L".tmp");
        // CREATE_NEW avoids overwriting an unrelated file. The temporary is owned
        // only after that succeeds; MoveFileEx atomically replaces this one file.
        const auto file = CreateFileW(temporary.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            throw std::runtime_error("Cannot create exclusive CMake transaction file.");
        }
        DWORD written = 0;
        const bool saved =
            WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
            written == bytes.size() && FlushFileBuffers(file);
        CloseHandle(file);
        if (!saved || !MoveFileExW(temporary.c_str(),
                          configuration.c_str(),
                          MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            DeleteFileW(temporary.c_str());
            throw std::runtime_error("Could not atomically save CMake configuration. Backup is in "
                                     "manager project metadata.");
        }
    };
    input.close();
    replace(current + "\n" + marker + "\n" + snippet + "\n# END EasierCpp approved dependencies\n");
    try
    {
        if (verify())
        {
            return true;
        }
    }
    catch (...)
    {
        replace(current);
        throw;
    }
    replace(current);
    return false;
}
} // namespace cpplivelab
