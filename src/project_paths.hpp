#pragma once

#include <cwctype>
#include <filesystem>
#include <string>

namespace cpplivelab::path_support
{
// Callers resolve existing path components with weakly_canonical first. Comparing
// complete components avoids treating a sibling such as "CppLiveLab-UserProjects"
// as a child of "CppLiveLab". Different drive/UNC roots are simply outside the tree.
inline bool is_within_directory(
    const std::filesystem::path& candidate, const std::filesystem::path& directory)
{
    const auto normalized_candidate = candidate.lexically_normal();
    const auto normalized_directory = directory.lexically_normal();
    auto candidate_component = normalized_candidate.begin();

    auto lowercase = [](std::wstring component)
    {
        for (wchar_t& character : component)
        {
            character = static_cast<wchar_t>(std::towlower(character));
        }

        return component;
    };

    for (const auto& directory_component : normalized_directory)
    {
        // A trailing directory separator can produce an empty final component.
        if (directory_component.empty())
        {
            continue;
        }

        if (candidate_component == normalized_candidate.end())
        {
            return false;
        }

        if (lowercase(candidate_component->wstring()) != lowercase(directory_component.wstring()))
        {
            return false;
        }

        ++candidate_component;
    }

    return true;
}

// Windows stores paths as UTF-16. path.string() can fail when the system ANSI code
// page cannot represent a character. Keep diagnostic paths in UTF-8 instead.
inline std::string path_utf8(const std::filesystem::path& path)
{
    const auto encoded_path = path.u8string();
    return {reinterpret_cast<const char*>(encoded_path.data()), encoded_path.size()};
}
} // namespace cpplivelab::path_support
