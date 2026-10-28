#pragma once
#include <ostream>
#include <string>
#include <filesystem>
#include <vector>

namespace cpplivelab
{
// Separating discovery inputs from the filesystem search lets tests supply temporary
// installations without changing PATH or relying on a particular Visual Studio version.
struct DiscoveryInputs
{
    std::vector<std::filesystem::path> visual_studios;
    std::vector<std::filesystem::path> search_directories;
    std::vector<std::filesystem::path> terminal_directories;
};

// installed_path answers "where is it installed?"; terminal_path answers "would this
// terminal find it?". An empty path means that discovery did not find an executable.
struct Tool
{
    std::string name;
    std::filesystem::path installed_path;
    std::filesystem::path terminal_path;
};

// Discovery describes available files. It does not prove compiler/SDK compatibility;
// configuring and building a real project performs that check.
struct Toolchain
{
    std::vector<Tool> tools;
    // Run this for child builds to initialize INCLUDE, LIB, SDK and PATH.
    std::filesystem::path developer_command;
};

// Use vswhere for Visual Studio installations, then inspect standard standalone
// installation folders and the current process's PATH. No persistent environment changes.
DiscoveryInputs system_discovery_inputs();
std::filesystem::path find_visual_studio_2022();
Toolchain discover_toolchain(const DiscoveryInputs& inputs);
// Returns 0 when CMake, Ninja and at least one compiler were found; GCC is optional.
int run_doctor(std::ostream& out, const Toolchain& toolchain);
} // namespace cpplivelab
