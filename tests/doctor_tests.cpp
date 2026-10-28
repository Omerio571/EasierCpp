#include "cpplivelab/doctor.hpp"
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
    fs::remove_all(temporary_directory);
    return failures ? 1 : 0;
}
