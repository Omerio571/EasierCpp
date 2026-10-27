#include "cpplivelab/doctor.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>

int main() {
    namespace fs = std::filesystem;
    int failures = 0;
    auto check = [&](bool ok, const char* message) {
        if (!ok) { std::cerr << message << '\n'; ++failures; }
    };
    const auto root = fs::temp_directory_path() / ("cpplivelab-discovery-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto touch = [](const fs::path& path) {
        fs::create_directories(path.parent_path());
        std::ofstream(path) << "fixture";
    };
    const auto vs = root / "Visual Studio With Spaces";
    touch(vs / "Common7/Tools/VsDevCmd.bat");
    const auto version = vs / "VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt";
    touch(version);
    std::ofstream(version) << "14.44.35207\n";
    touch(vs / "VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe");
    touch(vs / "VC/Tools/Llvm/x64/bin/clang++.exe");
    const auto bundled = vs / "Common7/IDE/CommonExtensions/Microsoft/CMake";
    touch(bundled / "CMake/bin/cmake.exe");
    touch(bundled / "Ninja/ninja.exe");
    cpplivelab::DiscoveryInputs inputs{{vs}, {}, {}};
    auto tools = cpplivelab::discover_toolchain(inputs);
    std::ostringstream report;
    check(cpplivelab::run_doctor(report, tools) == 0, "Installed MSVC should work with empty PATH and no GCC");
    check(!tools.developer_command.empty(), "MSVC environment command missing");
    for (size_t i = 0; i < 4; ++i) {
        check(!tools.tools[i].installed_path.empty(), "VS tool not discovered");
        check(tools.tools[i].terminal_path.empty(), "Installed-only tool claimed available in terminal");
    }
    check(tools.tools[4].installed_path.empty(), "GCC should be optional and missing");
    check(report.str().find("Installed: Yes | Available in current terminal: No") != std::string::npos,
        "Installed/terminal distinction missing");
    const auto standalone = root / "standalone/bin";
    for (const auto name : {"cmake.exe", "ninja.exe", "clang++.exe"}) touch(standalone / name);
    inputs = {{}, {standalone}, {}};
    tools = cpplivelab::discover_toolchain(inputs);
    report.str("");
    check(cpplivelab::run_doctor(report, tools) == 0, "Standalone tools not discovered outside PATH");
    inputs.terminal_directories = {standalone};
    tools = cpplivelab::discover_toolchain(inputs);
    check(!tools.tools[0].terminal_path.empty(), "PATH tool not detected");
    for (const auto name : {"cmake.exe", "ninja.exe", "clang++.exe"}) {
        fs::remove(standalone / name);
        tools = cpplivelab::discover_toolchain(inputs);
        report.str("");
        check(cpplivelab::run_doctor(report, tools) == 1, "Missing required tool should fail");
        touch(standalone / name);
    }
    // A second VS instance with a broken toolset must not hide a complete one.
    inputs = {{root / "broken-vs", vs}, {}, {}};
    tools = cpplivelab::discover_toolchain(inputs);
    check(!tools.tools[2].installed_path.empty(), "Multiple VS instance discovery failed");
    fs::remove_all(root);
    return failures ? 1 : 0;
}
