#include "cpplivelab/doctor.hpp"
#include "process/windows_support.hpp"
#include <map>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>

int main()
{
    namespace fs = std::filesystem;
    int failures = 0;
    auto check = [&](bool ok, const char* message)
    {
        if (!ok)
        {
            std::cerr << message << '\n';
            ++failures;
        }
    };
    const auto temporary_directory =
        fs::temp_directory_path() /
        ("cpplivelab-discovery-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto create_fixture_file = [](const fs::path& path)
    {
        fs::create_directories(path.parent_path());
        std::ofstream(path) << "fixture";
    };
    const auto visual_studio_directory = temporary_directory / "Visual Studio With Spaces";
    create_fixture_file(visual_studio_directory / "Common7/Tools/VsDevCmd.bat");
    const auto version =
        visual_studio_directory / "VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt";
    create_fixture_file(version);
    std::ofstream(version) << "14.44.35207\n";
    create_fixture_file(
        visual_studio_directory / "VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe");
    create_fixture_file(visual_studio_directory / "VC/Tools/Llvm/x64/bin/clang++.exe");
    const auto bundled_cmake_directory =
        visual_studio_directory / "Common7/IDE/CommonExtensions/Microsoft/CMake";
    create_fixture_file(bundled_cmake_directory / "CMake/bin/cmake.exe");
    create_fixture_file(bundled_cmake_directory / "Ninja/ninja.exe");
    cpplivelab::DiscoveryInputs discovery_inputs{{visual_studio_directory}, {}, {}};
    auto discovered_toolchain = cpplivelab::discover_toolchain(discovery_inputs);
    std::ostringstream doctor_output;
    check(cpplivelab::run_doctor(doctor_output, discovered_toolchain) == 0,
        "Installed MSVC should work with empty PATH and no GCC");
    check(!discovered_toolchain.developer_command.empty(), "MSVC environment command missing");
    for (size_t tool_index = 0; tool_index < 4; ++tool_index)
    {
        check(!discovered_toolchain.tools[tool_index].installed_path.empty(),
            "VS tool not discovered");
        check(discovered_toolchain.tools[tool_index].terminal_path.empty(),
            "Installed-only tool claimed available in terminal");
    }
    check(
        discovered_toolchain.tools[4].installed_path.empty(), "GCC should be optional and missing");
    check(doctor_output.str().find("Installed: Yes | Available in current terminal: No") !=
              std::string::npos,
        "Installed/terminal distinction missing");
    const auto standalone_tool_directory = temporary_directory / "standalone/bin";
    for (const auto name : {"cmake.exe", "ninja.exe", "clang++.exe"})
    {
        create_fixture_file(standalone_tool_directory / name);
    }
    discovery_inputs = {{}, {standalone_tool_directory}, {}};
    discovered_toolchain = cpplivelab::discover_toolchain(discovery_inputs);
    doctor_output.str("");
    check(cpplivelab::run_doctor(doctor_output, discovered_toolchain) == 0,
        "Standalone tools not discovered outside PATH");
    discovery_inputs.terminal_directories = {standalone_tool_directory};
    discovered_toolchain = cpplivelab::discover_toolchain(discovery_inputs);
    check(!discovered_toolchain.tools[0].terminal_path.empty(), "PATH tool not detected");
    for (const auto name : {"cmake.exe", "ninja.exe", "clang++.exe"})
    {
        fs::remove(standalone_tool_directory / name);
        discovered_toolchain = cpplivelab::discover_toolchain(discovery_inputs);
        doctor_output.str("");
        check(cpplivelab::run_doctor(doctor_output, discovered_toolchain) == 1,
            "Missing required tool should fail");
        create_fixture_file(standalone_tool_directory / name);
    }
    // A second VS instance with a broken toolset must not hide a complete one.
    discovery_inputs = {{temporary_directory / "broken-vs", visual_studio_directory}, {}, {}};
    discovered_toolchain = cpplivelab::discover_toolchain(discovery_inputs);
    check(!discovered_toolchain.tools[2].installed_path.empty(),
        "Multiple VS instance discovery failed");
    {
        struct RestoreEnvironment
        {
            std::map<std::wstring, std::wstring> values;

            ~RestoreEnvironment()
            {
                for (const auto& [name, value] : values)
                {
                    SetEnvironmentVariableW(name.c_str(), value.empty() ? nullptr : value.c_str());
                }
            }
        } restore;

        const auto set = [&](const wchar_t* name, const std::wstring& value)
        {
            if (!restore.values.contains(name))
            {
                const auto length = GetEnvironmentVariableW(name, nullptr, 0);
                std::wstring previous(length, L'\0');
                if (length)
                {
                    previous.resize(GetEnvironmentVariableW(name, previous.data(), length));
                }
                restore.values[name] = previous;
            }
            SetEnvironmentVariableW(name, value.c_str());
        };
        const auto prepared = visual_studio_directory / L"Common7/Tools/VsDevCmd.bat";
        const auto toolset = visual_studio_directory / L"VC/Tools/MSVC/14.44.35207";
        set(L"VSINSTALLDIR", visual_studio_directory.wstring());
        set(L"CPPLIVELAB_PREPARED_VS", prepared.wstring());
        set(L"VCToolsInstallDir", toolset.wstring());
        set(L"VSCMD_ARG_TGT_ARCH", L"x64");
        set(L"VSCMD_ARG_HOST_ARCH", L"x64");
        set(L"INCLUDE", L"fixture includes");
        set(L"LIB", L"fixture libraries");
        const auto uses_prepared = [&]
        {
            const auto inputs = cpplivelab::system_discovery_inputs(true);
            return inputs.visual_studios.size() == 1 &&
                   inputs.visual_studios[0] == visual_studio_directory;
        };
        check(uses_prepared(), "Validated prepared VS instance was not reused");
        const auto fresh = cpplivelab::system_discovery_inputs();
        check(std::find(fresh.visual_studios.begin(),
                  fresh.visual_studios.end(),
                  visual_studio_directory) == fresh.visual_studios.end(),
            "Doctor incorrectly reused a prepared installation hint");
        set(L"VSCMD_ARG_TGT_ARCH", L"x86");
        check(!uses_prepared(), "Wrong architecture bypassed fresh discovery");
        set(L"VSCMD_ARG_TGT_ARCH", L"x64");
        set(L"CPPLIVELAB_PREPARED_VS", (temporary_directory / L"wrong.bat").wstring());
        check(!uses_prepared(), "Wrong setup marker bypassed discovery");
        set(L"CPPLIVELAB_PREPARED_VS", prepared.wstring());
        set(L"LIB", L"");
        check(!uses_prepared(), "Incomplete SDK environment bypassed discovery");
        set(L"LIB", L"fixture libraries");
        fs::remove(toolset / L"bin/Hostx64/x64/cl.exe");
        check(!uses_prepared(), "Removed compiler was reused from a stale hint");
    }
    fs::remove_all(temporary_directory);
    return failures ? 1 : 0;
}
