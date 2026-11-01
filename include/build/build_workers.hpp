#pragma once
#include <filesystem>
#include <vector>
#include <string>

namespace cpplivelab
{
struct BuildWorkers
{
    unsigned count = 0; // Zero preserves the existing native/preset/environment default.
    unsigned recommended = 0;
};

unsigned maximum_build_workers();
unsigned build_worker_limit(unsigned long processors, unsigned long long available_bytes);
unsigned worker_benchmark_units(unsigned workers);
BuildWorkers load_build_workers();
void initialize_build_workers();
void save_build_workers(BuildWorkers value);
void append_build_workers(std::vector<std::wstring>& arguments, BuildWorkers value);
// The caller owns this empty temporary directory and applies results only on success.
unsigned benchmark_build_workers(const std::filesystem::path& directory);
} // namespace cpplivelab
