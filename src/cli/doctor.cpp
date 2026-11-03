#include "cpplivelab/doctor.hpp"
#include "process/child_startup.hpp"
#include "projects/project_paths.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <fstream>
#include <array>

namespace cpplivelab
{
namespace
{
namespace fs = std::filesystem;

bool file_exists(const fs::path& path)
{
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

std::wstring read_environment_variable(const wchar_t* name)
{
    const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
    if (!size)
    {
        return {};
    }
    std::wstring value(size, L'\0');
    const DWORD length = GetEnvironmentVariableW(name, value.data(), size);
    value.resize(length);
    return value;
}

std::vector<fs::path> split_paths(const std::wstring& value)
{
    std::vector<fs::path> result;
    size_t begin = 0;
    while (begin < value.size())
    {
        const auto end = value.find(L';', begin);
        auto part = value.substr(begin, end == value.npos ? value.npos : end - begin);
        if (part.size() >= 2 && part.front() == L'"' && part.back() == L'"')
        {
            part = part.substr(1, part.size() - 2);
        }
        if (!part.empty())
        {
            result.emplace_back(part);
        }
        if (end == value.npos)
        {
            break;
        }
        begin = end + 1;
    }
    return result;
}

// vswhere is Microsoft's existing Visual Studio installation-discovery utility.
// Launch it directly (no shell), so spaces in the installer path are not commands.
// Its UTF-8 output is a list of installation directories, not executable names.
std::vector<fs::path> visual_studios(
    const fs::path& vswhere_executable, bool require_visual_studio_2022 = false)
{
    if (!file_exists(vswhere_executable))
    {
        return {};
    }
    SECURITY_ATTRIBUTES pipe_security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE output_read_handle = nullptr, output_write_handle = nullptr;
    if (!CreatePipe(&output_read_handle, &output_write_handle, &pipe_security, 0))
    {
        return {};
    }
    SetHandleInformation(output_read_handle, HANDLE_FLAG_INHERIT, 0);
    windows_support::ChildStartup startup(
        GetStdHandle(STD_INPUT_HANDLE), output_write_handle, output_write_handle);
    PROCESS_INFORMATION process_information{};
    std::wstring command_line_arguments =
        L"\"" + vswhere_executable.wstring() +
        L"\" -all -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 "
        L"-property installationPath -utf8";
    if (require_visual_studio_2022)
    {
        command_line_arguments += L" -version [17.0,18.0)";
    }
    const BOOL started = CreateProcessW(vswhere_executable.c_str(),
        command_line_arguments.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT,
        nullptr,
        nullptr,
        &startup.information.StartupInfo,
        &process_information);
    startup.release_handles();
    // Only vswhere keeps the write end now. Otherwise ReadFile would never see EOF
    // after vswhere exits because our own write handle would keep the pipe alive.
    CloseHandle(output_write_handle);
    if (!started)
    {
        CloseHandle(output_read_handle);
        return {};
    }
    const auto output =
        windows_support::capture_root_output(process_information.hProcess, output_read_handle);
    CloseHandle(output_read_handle);
    WaitForSingleObject(process_information.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(process_information.hProcess, &exit_code);
    CloseHandle(process_information.hThread);
    CloseHandle(process_information.hProcess);
    if (exit_code != 0)
    {
        return {};
    }
    std::vector<fs::path> result;
    size_t begin = 0;
    while (begin < output.size())
    {
        const auto end = output.find('\n', begin);
        auto line = output.substr(begin, end == output.npos ? output.npos : end - begin);
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (!line.empty())
        {
            result.push_back(fs::path(std::u8string(line.begin(), line.end())));
        }
        if (end == output.npos)
        {
            break;
        }
        begin = end + 1;
    }
    return result;
}

fs::path find(const std::vector<fs::path>& directories, const fs::path& executable)
{
    for (const auto& directory : directories)
    {
        if (file_exists(directory / executable))
        {
            return directory / executable;
        }
    }
    return {};
}
} // namespace

std::filesystem::path find_visual_studio_2022()
{
    const fs::path root = read_environment_variable(L"ProgramFiles(x86)");
    if (root.empty())
    {
        return {};
    }
    for (const auto& visual_studio_directory :
        visual_studios(root / L"Microsoft Visual Studio/Installer/vswhere.exe", true))
    {
        const auto ide_executable = visual_studio_directory / L"Common7/IDE/devenv.exe";
        if (file_exists(ide_executable))
        {
            return ide_executable;
        }
    }
    return {};
}

DiscoveryInputs system_discovery_inputs(bool reuse_prepared_environment)
{
    DiscoveryInputs inputs;
    inputs.terminal_directories = split_paths(read_environment_variable(L"PATH"));
    const fs::path program_files = read_environment_variable(L"ProgramFiles");
    const fs::path program_files_x86 = read_environment_variable(L"ProgramFiles(x86)");
    if (reuse_prepared_environment)
    {
        const fs::path installation = read_environment_variable(L"VSINSTALLDIR");
        const fs::path prepared = read_environment_variable(L"CPPLIVELAB_PREPARED_VS");
        const fs::path toolset = read_environment_variable(L"VCToolsInstallDir");
        std::ifstream version_file(
            installation / L"VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt");
        std::string version;
        version_file >> version;
        std::error_code script_error, toolset_error;
        // Only the same validated instance/toolset that supplied the child's SDK
        // block can replace vswhere. Missing or inconsistent hints fail closed to
        // ordinary discovery. Nothing is persisted and files are checked each time.
        if (installation.is_absolute() && prepared.is_absolute() && toolset.is_absolute() &&
            !version.empty() && read_environment_variable(L"VSCMD_ARG_TGT_ARCH") == L"x64" &&
            read_environment_variable(L"VSCMD_ARG_HOST_ARCH") == L"x64" &&
            !read_environment_variable(L"INCLUDE").empty() &&
            !read_environment_variable(L"LIB").empty() &&
            file_exists(toolset / L"bin/Hostx64/x64/cl.exe") &&
            fs::equivalent(prepared, installation / L"Common7/Tools/VsDevCmd.bat", script_error) &&
            !script_error &&
            fs::equivalent(toolset, installation / L"VC/Tools/MSVC" / version, toolset_error) &&
            !toolset_error)
        {
            inputs.visual_studios.push_back(installation);
        }
    }
    if (inputs.visual_studios.empty() && !program_files_x86.empty())
    {
        inputs.visual_studios =
            visual_studios(program_files_x86 / L"Microsoft Visual Studio/Installer/vswhere.exe");
    }
    for (const auto& root : {program_files, program_files_x86})
    {
        if (root.empty())
        {
            continue;
        }
        inputs.search_directories.push_back(root / L"CMake/bin");
        inputs.search_directories.push_back(root / L"Ninja");
        inputs.search_directories.push_back(root / L"LLVM/bin");
    }
    const fs::path local_app_data = read_environment_variable(L"LOCALAPPDATA");
    if (!local_app_data.empty())
    {
        inputs.search_directories.push_back(local_app_data / L"Microsoft/WinGet/Links");
        for (const auto& name : {L"CMake/bin", L"Ninja", L"LLVM/bin"})
        {
            inputs.search_directories.push_back(local_app_data / L"Programs" / name);
        }
    }
    const fs::path user_profile = read_environment_variable(L"USERPROFILE");
    if (!user_profile.empty())
    {
        inputs.search_directories.push_back(user_profile / L"scoop/shims");
    }
    const fs::path chocolatey = read_environment_variable(L"ChocolateyInstall");
    if (!chocolatey.empty())
    {
        inputs.search_directories.push_back(chocolatey / L"bin");
    }
    return inputs;
}

Toolchain discover_toolchain(const DiscoveryInputs& inputs)
{
    // PATH is only one source of discovery. A normal PowerShell terminal often
    // lacks every VS tool, even though a complete installation is available.
    Toolchain result;
    auto directories = inputs.search_directories;
    for (const auto& visual_studio_directory : inputs.visual_studios)
    {
        const auto developer_setup_script = visual_studio_directory / L"Common7/Tools/VsDevCmd.bat";
        // This small file selects the default installed MSVC toolset. Do not guess
        // a compiler version or assume that the newest-looking directory is active.
        std::ifstream version_file(
            visual_studio_directory / L"VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt");
        std::string version;
        version_file >> version;
        const auto compiler_directory =
            visual_studio_directory / L"VC/Tools/MSVC" / version / L"bin/Hostx64/x64";
        if (!version.empty() && file_exists(compiler_directory / L"cl.exe") &&
            file_exists(developer_setup_script))
        {
            directories.push_back(compiler_directory);
            if (result.developer_command.empty())
            {
                result.developer_command = developer_setup_script;
            }
        }
        const auto cmake =
            visual_studio_directory / L"Common7/IDE/CommonExtensions/Microsoft/CMake";
        directories.push_back(cmake / L"CMake/bin");
        directories.push_back(cmake / L"Ninja");
        directories.push_back(visual_studio_directory / L"VC/Tools/Llvm/x64/bin");
        directories.push_back(visual_studio_directory / L"VC/Tools/Llvm/bin");
    }
    for (const auto& name : {"cmake", "ninja", "cl", "clang++", "g++", "clang-format"})
    {
        Tool tool{name, {}, find(inputs.terminal_directories, std::string(name) + ".exe")};
        tool.installed_path = tool.terminal_path.empty()
                                  ? find(directories, std::string(name) + ".exe")
                                  : tool.terminal_path;
        result.tools.push_back(std::move(tool));
    }
    return result;
}

int run_doctor(std::ostream& out, const Toolchain& toolchain)
{
    out << "EasierCpp environment doctor\n";
    bool cmake = false, ninja = false, compiler = false;
    for (const auto& tool : toolchain.tools)
    {
        const bool installed = !tool.installed_path.empty();
        out << tool.name << ": Installed: " << (installed ? "Yes" : "No")
            << " | Available in current terminal: " << (tool.terminal_path.empty() ? "No" : "Yes")
            << '\n';
        if (tool.name == "clang-format")
        {
            out << "  Optional formatting utility; not required to create or build projects.\n";
        }
        if (installed)
        {
            out << "  Path: " << path_support::path_utf8(tool.installed_path) << '\n';
        }
        if (tool.name == "cmake")
        {
            cmake = installed;
        }
        else if (tool.name == "ninja")
        {
            ninja = installed;
        }
        else if (tool.name == "cl" || tool.name == "clang++" || tool.name == "g++")
        {
            compiler = compiler || installed;
        }
    }
    if (!toolchain.developer_command.empty())
    {
        out << "MSVC build environment: " << path_support::path_utf8(toolchain.developer_command)
            << " -arch=x64 -host_arch=x64\n";
    }
    const bool ready = cmake && ninja && compiler;
    out << (ready ? "Build tools found.\n"
                  : "Build tools missing. Install CMake, Ninja and a C++20 compiler.\n");
    out << "Use scripts/build.ps1 to initialize the build environment automatically.\n"
        << "Discovery checks files, not compiler/SDK compatibility; a build verifies usability.\n";
    return ready ? 0 : 1;
}
} // namespace cpplivelab
