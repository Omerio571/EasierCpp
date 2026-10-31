#pragma once

#include <filesystem>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace cpplivelab
{
struct BuildDiagnostic
{
    std::filesystem::path file;
    unsigned line = 1;
    unsigned column = 1;
    std::wstring code;
    std::wstring message;
    std::wstring raw;
    bool recovery_suggested = false;
};

// Parsing augments the original log; it never replaces or suppresses output.
inline std::vector<BuildDiagnostic> parse_build_diagnostics(
    const std::wstring& output, const std::filesystem::path& root)
{
    const std::wregex msvc(
        LR"(^\s*(.+)\(([0-9]+)(?:,([0-9]+))?\)\s*:\s*(?:fatal error|error|warning)\s*([A-Za-z]+[0-9]+)?\s*:\s*(.*)$)");
    const std::wregex clang(
        LR"(^\s*(.+?):([0-9]{1,9}):(?:([0-9]{1,9}):)?\s*(?:fatal error|error|warning):\s*(.*)$)");
    const std::wregex cmake(
        LR"(^CMake (?:Error|Warning) at (.+):([0-9]+)(?:\s*\([^)]*\))?:\s*(.*)$)");
    std::vector<BuildDiagnostic> result;
    std::wistringstream input(output);
    for (std::wstring text; std::getline(input, text);)
    {
        if (!text.empty() && text.back() == L'\r')
        {
            text.pop_back();
        }
        std::wsmatch match;
        BuildDiagnostic diagnostic;
        diagnostic.raw = text;
        for (const auto* label : {L"[ERROR] ", L"[WARNING] ", L"[BUILD] ", L"[SYSTEM] "})
        {
            if (text.starts_with(label))
            {
                text.erase(0, std::wstring_view(label).size());
                break;
            }
        }
        if (std::regex_match(text, match, msvc))
        {
            diagnostic.file = match[1].str();
            diagnostic.line = std::stoul(match[2]);
            diagnostic.column = match[3].matched ? std::stoul(match[3]) : 1;
            diagnostic.code = match[4];
            diagnostic.message = match[5];
        }
        else if (std::regex_match(text, match, clang))
        {
            diagnostic.file = match[1].str();
            diagnostic.line = std::stoul(match[2]);
            diagnostic.column = match[3].matched ? std::stoul(match[3]) : 1;
            diagnostic.message = match[4];
        }
        else if (std::regex_match(text, match, cmake))
        {
            diagnostic.file = match[1].str();
            diagnostic.line = std::stoul(match[2]);
            diagnostic.message = match[3];
        }
        else if (text.starts_with(L"ninja: error:") || text.starts_with(L"CMake Error:") ||
                 text.find(L"fatal error LNK") != text.npos ||
                 text.find(L"error LNK") != text.npos || text.starts_with(L"lld-link: error:"))
        {
            diagnostic.message = text;
            diagnostic.recovery_suggested =
                text.find(L"build.ninja") != text.npos || text.find(L"CMakeCache") != text.npos ||
                text.find(L"generator") != text.npos || text.find(L"does not match") != text.npos;
        }
        else
        {
            if (!result.empty() && result.back().message.empty() && !text.empty())
            {
                result.back().message = text;
                result.back().raw += L"\n" + text;
            }
            continue;
        }
        if (!diagnostic.file.empty() && diagnostic.file.is_relative())
        {
            diagnostic.file = (root / diagnostic.file).lexically_normal();
        }
        result.push_back(std::move(diagnostic));
    }
    return result;
}
} // namespace cpplivelab
