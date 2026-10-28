#pragma once
#include <filesystem>
#include <string>

namespace cpplivelab
{
// Only a new or empty external folder is accepted. Existing user source is never
// overwritten; calculator selects the approved multi-file example instead of hello-world.
void create_project(const std::filesystem::path& folder, bool calculator);

// Configure/build with discovered absolute tool paths. A per-configuration file lock
// remains owned through execution, so a second CppLiveLab cannot rebuild a running app.
// executable is optional and relative to the build directory unless it is absolute.
int build_project(const std::filesystem::path& input,
    const std::string& configuration,
    bool run,
    const std::filesystem::path& executable = {});

// Auto Build can skip an explicit configure for ordinary source edits. Ninja still
// regenerates CMake when its tracked configuration inputs require it. The approved
// four-argument API above retains its existing configure-before-build behavior.
int build_project(const std::filesystem::path& input,
    const std::string& configuration,
    bool run,
    const std::filesystem::path& executable,
    bool incremental);
} // namespace cpplivelab
