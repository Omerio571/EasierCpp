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
// Runtime support is resolved beside the executable. Only development builds
// may fall back to the checkout; portable builds contain no checkout path.
std::filesystem::path runtime_resources();
// Development-only discovery: validate the compiled root, then executable ancestors.
// Portable builds never call this to find or execute source-level tests.
std::filesystem::path find_development_repository(
    const std::filesystem::path& compiled, const std::filesystem::path& application_directory);
std::string diagnostics_summary(const Toolchain& toolchain);
// Call only after displaying the exact snippet and receiving explicit approval.
// A failed/cancelled verification restores the exact original CMake bytes.
std::string cmake_dependency_probe(const std::filesystem::path& source);
bool apply_approved_dependencies(const std::filesystem::path& project,
    const std::string& expected_original,
    const std::string& snippet,
    const std::function<bool()>& verify);
} // namespace cpplivelab
