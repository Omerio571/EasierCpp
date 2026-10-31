#pragma once
#include "cpplivelab/doctor.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <functional>

namespace cpplivelab
{
struct TestCommand
{
    std::filesystem::path executable;
    std::filesystem::path repository;
    std::vector<std::wstring> arguments;
};

// Uses the manager's source tree, never the selected user's project. The isolated
// build output allows a running GUI to build its own tests without replacing itself.
TestCommand test_command(const std::wstring& mode, int workers);
std::filesystem::path source_repository();
std::string diagnostics_summary(const Toolchain& toolchain);
// Call only after displaying the exact snippet and receiving explicit approval.
// A failed/cancelled verification restores the exact original CMake bytes.
std::string cmake_dependency_probe(const std::filesystem::path& source);
bool apply_approved_dependencies(const std::filesystem::path& project,
    const std::string& expected_original,
    const std::string& snippet,
    const std::function<bool()>& verify);
} // namespace cpplivelab
