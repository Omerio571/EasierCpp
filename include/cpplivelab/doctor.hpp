#pragma once
#include <functional>
#include <ostream>
#include <string_view>
#include <filesystem>
#include <vector>

namespace cpplivelab {
struct DiscoveryInputs {
    std::vector<std::filesystem::path> visual_studios;
    std::vector<std::filesystem::path> search_directories;
    std::vector<std::filesystem::path> terminal_directories;
};
struct Tool {
    std::string name;
    std::filesystem::path installed_path;
    std::filesystem::path terminal_path;
};
struct Toolchain {
    std::vector<Tool> tools;
    // Run this for child builds to initialize INCLUDE, LIB, SDK and PATH.
    std::filesystem::path developer_command;
};
DiscoveryInputs system_discovery_inputs();
Toolchain discover_toolchain(const DiscoveryInputs& inputs);
int run_doctor(std::ostream& out, const Toolchain& toolchain);
}
