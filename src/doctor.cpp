#include "cpplivelab/doctor.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <fstream>
#include <array>

namespace cpplivelab {
namespace {
namespace fs = std::filesystem;
bool file_exists(const fs::path& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}
std::wstring environment(const wchar_t* name) {
    const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
    if (!size) return {};
    std::wstring value(size, L'\0');
    const DWORD length = GetEnvironmentVariableW(name, value.data(), size);
    value.resize(length);
    return value;
}
std::vector<fs::path> split_paths(const std::wstring& value) {
    std::vector<fs::path> result;
    size_t begin = 0;
    while (begin < value.size()) {
        const auto end = value.find(L';', begin);
        auto part = value.substr(begin, end == value.npos ? value.npos : end - begin);
        if (part.size() >= 2 && part.front() == L'"' && part.back() == L'"')
            part = part.substr(1, part.size() - 2);
        if (!part.empty()) result.emplace_back(part);
        if (end == value.npos) break;
        begin = end + 1;
    }
    return result;
}
// Launch vswhere directly (no shell); capture UTF-8 installation paths.
std::vector<fs::path> visual_studios(const fs::path& vswhere) {
    if (!file_exists(vswhere)) return {};
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE read = nullptr, write = nullptr;
    if (!CreatePipe(&read, &write, &security, 0)) return {};
    SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = write;
    startup.hStdError = write;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    std::wstring command = L"\"" + vswhere.wstring() + L"\" -all -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath -utf8";
    const BOOL started = CreateProcessW(vswhere.c_str(), command.data(), nullptr, nullptr,
        TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    CloseHandle(write);
    if (!started) { CloseHandle(read); return {}; }
    std::string output;
    std::array<char, 4096> buffer{};
    DWORD count = 0;
    while (ReadFile(read, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr) && count)
        output.append(buffer.data(), count);
    CloseHandle(read);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit = 1;
    GetExitCodeProcess(process.hProcess, &exit);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (exit != 0) return {};
    std::vector<fs::path> result;
    size_t begin = 0;
    while (begin < output.size()) {
        const auto end = output.find('\n', begin);
        auto line = output.substr(begin, end == output.npos ? output.npos : end - begin);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) result.push_back(fs::path(std::u8string(line.begin(), line.end())));
        if (end == output.npos) break;
        begin = end + 1;
    }
    return result;
}
fs::path find(const std::vector<fs::path>& directories, const fs::path& executable) {
    for (const auto& directory : directories)
        if (file_exists(directory / executable)) return directory / executable;
    return {};
}
}
DiscoveryInputs system_discovery_inputs() {
    DiscoveryInputs inputs;
    inputs.terminal_directories = split_paths(environment(L"PATH"));
    const fs::path program_files = environment(L"ProgramFiles");
    const fs::path program_files_x86 = environment(L"ProgramFiles(x86)");
    if (!program_files_x86.empty())
        inputs.visual_studios = visual_studios(program_files_x86 / L"Microsoft Visual Studio/Installer/vswhere.exe");
    for (const auto& root : {program_files, program_files_x86}) {
        if (root.empty()) continue;
        inputs.search_directories.push_back(root / L"CMake/bin");
        inputs.search_directories.push_back(root / L"Ninja");
        inputs.search_directories.push_back(root / L"LLVM/bin");
    }
    const fs::path local = environment(L"LOCALAPPDATA");
    if (!local.empty()) {
        inputs.search_directories.push_back(local / L"Microsoft/WinGet/Links");
        for (const auto& name : {L"CMake/bin", L"Ninja", L"LLVM/bin"})
            inputs.search_directories.push_back(local / L"Programs" / name);
    }
    const fs::path user = environment(L"USERPROFILE");
    if (!user.empty()) inputs.search_directories.push_back(user / L"scoop/shims");
    const fs::path chocolatey = environment(L"ChocolateyInstall");
    if (!chocolatey.empty()) inputs.search_directories.push_back(chocolatey / L"bin");
    return inputs;
}
Toolchain discover_toolchain(const DiscoveryInputs& inputs) {
    Toolchain result;
    auto directories = inputs.search_directories;
    for (const auto& vs : inputs.visual_studios) {
        const auto devcmd = vs / L"Common7/Tools/VsDevCmd.bat";
        std::ifstream version_file(vs / L"VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt");
        std::string version;
        version_file >> version;
        const auto compiler_dir = vs / L"VC/Tools/MSVC" / version / L"bin/Hostx64/x64";
        if (!version.empty() && file_exists(compiler_dir / L"cl.exe") && file_exists(devcmd)) {
            directories.push_back(compiler_dir);
            if (result.developer_command.empty()) result.developer_command = devcmd;
        }
        const auto cmake = vs / L"Common7/IDE/CommonExtensions/Microsoft/CMake";
        directories.push_back(cmake / L"CMake/bin");
        directories.push_back(cmake / L"Ninja");
        directories.push_back(vs / L"VC/Tools/Llvm/x64/bin");
        directories.push_back(vs / L"VC/Tools/Llvm/bin");
    }
    for (const auto& name : {"cmake", "ninja", "cl", "clang++", "g++"}) {
        Tool tool{name, {}, find(inputs.terminal_directories, std::string(name) + ".exe")};
        tool.installed_path = tool.terminal_path.empty()
            ? find(directories, std::string(name) + ".exe") : tool.terminal_path;
        result.tools.push_back(std::move(tool));
    }
    return result;
}
int run_doctor(std::ostream& out, const Toolchain& toolchain) {
    out << "CppLiveLab environment doctor\n";
    bool cmake = false, ninja = false, compiler = false;
    for (const auto& tool : toolchain.tools) {
        const bool installed = !tool.installed_path.empty();
        out << tool.name << ": Installed: " << (installed ? "Yes" : "No")
            << " | Available in current terminal: " << (tool.terminal_path.empty() ? "No" : "Yes") << '\n';
        if (installed) out << "  Path: " << tool.installed_path.string() << '\n';
        if (tool.name == "cmake") cmake = installed;
        else if (tool.name == "ninja") ninja = installed;
        else compiler = compiler || installed;
    }
    if (!toolchain.developer_command.empty())
        out << "MSVC build environment: " << toolchain.developer_command.string()
            << " -arch=x64 -host_arch=x64\n";
    const bool ready = cmake && ninja && compiler;
    out << (ready ? "Build tools found.\n" : "Build tools missing. Install CMake, Ninja and a C++20 compiler.\n");
    out << "Use scripts/build.ps1 to initialize the build environment automatically.\n"
        << "Discovery checks files, not compiler/SDK compatibility; a build verifies usability.\n";
    return ready ? 0 : 1;
}
}

