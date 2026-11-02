#include "cpplivelab/libraries.hpp"
#include "cpplivelab/session.hpp"
#include "projects/project_paths.hpp"
#include "tools/tool_workflows.hpp"
#include <windows.h>
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace cpplivelab
{
bool library_observation_conflicts(
    const LibraryRecord& record, const LibraryObservation& observation)
{
    // Exact CMake target names and package identities are meaningful. Display
    // names, vendor names, include paths, and partial spellings are not identities.
    return (!observation.target.empty() &&
               std::find(record.targets.begin(), record.targets.end(), observation.target) !=
                   record.targets.end()) ||
           (record.integration == "package" && !record.package.empty() &&
               observation.identity == "find_package:" + record.package);
}

std::vector<LibraryObservation> detect_project_libraries(const std::filesystem::path& project,
    const std::filesystem::path& build,
    const std::wstring& configuration,
    const std::wstring& target)
{
    if (project.empty())
    {
        return {};
    }
    const auto directory = manager_state_directory() / L"library-detection";
    std::filesystem::create_directories(directory);
    // Only this unique manager-owned output file is removed; project/build files
    // are read-only. The existing modal background worker keeps the UI responsive.
    const auto output = directory / (std::to_wstring(GetCurrentProcessId()) + L"-" +
                                        std::to_wstring(GetTickCount64()) + L".txt");

    struct Cleanup
    {
        std::filesystem::path path;

        ~Cleanup()
        {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }
    } cleanup{output};

    wchar_t system[MAX_PATH]{};
    GetSystemDirectoryW(system, MAX_PATH);
    const auto powershell =
        std::filesystem::path(system) / L"WindowsPowerShell/v1.0/powershell.exe";
    // Reuse the hidden, job-owned reader to avoid console flashes and bound
    // failed metadata operations. This polling runs in the existing UI worker.
    ConsoleSession reader;
    reader.start(powershell,
        {L"-NoProfile",
            L"-NonInteractive",
            L"-ExecutionPolicy",
            L"Bypass",
            L"-File",
            (runtime_resources() / L"scripts/detect-project-libraries.ps1").wstring(),
            L"-Project",
            project.wstring(),
            L"-Build",
            build.wstring(),
            L"-Configuration",
            configuration,
            L"-Target",
            target,
            L"-Output",
            output.wstring()},
        runtime_resources());
    const auto deadline = GetTickCount64() + 15000;
    while (reader.running() || reader.output_pending())
    {
        reader.poll();
        if (GetTickCount64() > deadline)
        {
            reader.stop();
            throw std::runtime_error(
                "Passive dependency inspection timed out; project files unchanged.");
        }
        Sleep(10);
    }
    const auto result = reader.exit_code();
    reader.release_completed();
    if (result != 0)
    {
        throw std::runtime_error("Passive dependency inspection failed; project files unchanged.");
    }
    std::ifstream input(output, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("Dependency observation output is unavailable.");
    }
    std::vector<LibraryObservation> observations;
    for (std::string identity; std::getline(input, identity);)
    {
        std::string fields[8];
        fields[0] = identity;
        for (size_t index = 1; index < 8; ++index)
        {
            if (!std::getline(input, fields[index]))
            {
                throw std::runtime_error("Dependency observation is incomplete.");
            }
        }
        for (auto& field : fields)
        {
            if (!field.empty() && field.back() == '\r')
            {
                field.pop_back();
            }
        }
        const auto confidence =
            fields[7] == "ConfiguredSnapshot" ? LibraryDetectionConfidence::ConfiguredSnapshot
            : fields[7] == "StaleSnapshot"    ? LibraryDetectionConfidence::StaleSnapshot
            : fields[7] == "Unavailable"      ? LibraryDetectionConfidence::Unavailable
                                              : LibraryDetectionConfidence::Declaration;
        observations.push_back({fields[0],
            fields[1],
            fields[2],
            fields[3],
            fields[4] == "External" ? LibraryOwnership::External : LibraryOwnership::Unknown,
            fields[5],
            fields[6],
            confidence});
    }
    return observations;
}
} // namespace cpplivelab
