#pragma once

#include "cpplivelab/libraries.hpp"

namespace cpplivelab
{
enum class LibraryCompatibility
{
    Compatible,
    NotLibrary,
    UnableToVerify,
    RequiresDecision
};

struct LibraryAssessment
{
    LibraryCompatibility status = LibraryCompatibility::UnableToVerify;
    std::string reason;
    bool native_candidate = false;
};

// Passive evidence never executes repository code or proves compatibility alone.
// Compatible requires a successful existing consumer probe and reusable targets
// owned by the selected repository, or an explicitly selected public C/C++ header.
LibraryAssessment assess_library(const std::filesystem::path& source,
    const std::vector<LibraryTarget>& targets = {},
    bool consumer_verified = false,
    const std::filesystem::path& selected_header = {});
std::string compatibility_label(LibraryCompatibility status);
} // namespace cpplivelab
