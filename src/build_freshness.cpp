#include "build_freshness.hpp"

namespace cpplivelab
{
std::filesystem::path BuildIdentity::directory() const
{
    return output_directory.empty() ? project / L".cpplivelab" / (L"build-" + configuration)
                                    : output_directory;
}

std::vector<BuildFreshness::FileStamp> BuildFreshness::artifacts(const BuildIdentity& identity)
{
    std::vector<FileStamp> result;
    for (const auto& path : {identity.executable,
             identity.directory() / L"CMakeCache.txt",
             identity.directory() / L"build.ninja",
             identity.directory() / L".ninja_log",
             identity.directory() / L".ninja_deps"})
    {
        FileStamp stamp;
        std::error_code error;
        if (std::filesystem::is_regular_file(path, error))
        {
            stamp.size = std::filesystem::file_size(path, error);
            stamp.written = std::filesystem::last_write_time(path, error);
            stamp.exists = !error;
        }
        result.push_back(stamp);
    }
    return result;
}

SourceFingerprints BuildFreshness::begin(const BuildIdentity& identity)
{
    invalidate(identity);
    return ProjectWatcher::source_fingerprints(identity.project);
}

bool BuildFreshness::succeeded(const BuildIdentity& identity, const SourceFingerprints& before)
{
    invalidate(identity);
    auto now = ProjectWatcher::source_fingerprints(identity.project);
    const auto outputs = artifacts(identity);
    const bool visual_studio =
        std::filesystem::is_regular_file(identity.directory() / L"ALL_BUILD.vcxproj");
    if (before != now || !outputs[0].exists || !outputs[1].exists ||
        (!outputs[2].exists && !visual_studio))
    {
        return false;
    }
    successful_[identity] = {std::move(now), outputs};
    return true;
}

void BuildFreshness::invalidate(const BuildIdentity& identity)
{
    successful_.erase(identity);
}

bool BuildFreshness::can_probe(const BuildIdentity& identity) const
{
    const auto found = successful_.find(identity);
    return found != successful_.end() &&
           found->second.sources == ProjectWatcher::source_fingerprints(identity.project) &&
           found->second.artifacts == artifacts(identity) && found->second.artifacts.front().exists;
}

bool ninja_reports_current(std::string_view output, int exit_code)
{
    const auto first = output.find_first_not_of(" \t\r\n");
    const auto last = output.find_last_not_of(" \t\r\n");
    return exit_code == 0 && first != output.npos &&
           output.substr(first, last - first + 1) == "ninja: no work to do.";
}
} // namespace cpplivelab
