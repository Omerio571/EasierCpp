#pragma once

#include "auto_build.hpp"
#include <string_view>

namespace cpplivelab
{
using SourceFingerprints = std::map<std::filesystem::path, std::uint64_t>;

struct BuildIdentity
{
    std::filesystem::path project;
    std::wstring configuration;
    std::filesystem::path executable;
    auto operator<=>(const BuildIdentity&) const = default;
    std::filesystem::path directory() const;
};

// An in-memory successful-build observation, never a persisted "fresh" flag.
// It permits a Ninja dry-run probe; it is not itself permission to launch a program.
class BuildFreshness
{
public:
    SourceFingerprints begin(const BuildIdentity& identity);
    bool succeeded(const BuildIdentity& identity, const SourceFingerprints& before);
    void invalidate(const BuildIdentity& identity);
    bool can_probe(const BuildIdentity& identity) const;

private:
    struct FileStamp
    {
        bool exists = false;
        std::uintmax_t size = 0;
        std::filesystem::file_time_type written{};
        bool operator==(const FileStamp&) const = default;
    };

    struct Observation
    {
        SourceFingerprints sources;
        std::vector<FileStamp> artifacts;
    };

    std::map<BuildIdentity, Observation> successful_;
    static std::vector<FileStamp> artifacts(const BuildIdentity& identity);
};

// Unrecognized or ambiguous Ninja output is conservatively treated as dirty.
bool ninja_reports_current(std::string_view output, int exit_code);
} // namespace cpplivelab
