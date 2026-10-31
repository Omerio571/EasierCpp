#pragma once
#include "windows_support.hpp"
#include <filesystem>
#include <stdexcept>

namespace workspace_test
{
// Every process owns a new fixture root, including its manager state. Do not
// change the user's recent projects/editor preferences during automated tests.
struct Fixture
{
    std::filesystem::path root;
    std::wstring previous_state;

    Fixture()
    {
        wchar_t previous[32768]{};
        GetEnvironmentVariableW(L"CPPLIVELAB_STATE_HOME", previous, 32768);
        previous_state = previous;
        const auto prefix = L"cll5-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                            std::to_wstring(GetTickCount64()) + L"-";
        // Keep the base compact: legacy MSBuild tracking paths still have a
        // MAX_PATH limit. Long-path scanner tests create their own deep subtree.
        // Windows reuses process IDs. Atomically reserve a new directory rather
        // than colliding with leftovers from a crashed or long-path test run.
        for (unsigned attempt = 0;; ++attempt)
        {
            root = std::filesystem::temp_directory_path() / (prefix + std::to_wstring(attempt));
            std::error_code error;
            if (std::filesystem::create_directory(root, error))
            {
                break;
            }
            if (error || attempt >= 1000)
            {
                throw std::runtime_error("Cannot reserve an isolated Stage 5 fixture directory.");
            }
        }
        SetEnvironmentVariableW(L"CPPLIVELAB_STATE_HOME", (root / L"manager state").c_str());
    }

    ~Fixture()
    {
        SetEnvironmentVariableW(
            L"CPPLIVELAB_STATE_HOME", previous_state.empty() ? nullptr : previous_state.c_str());
        std::error_code error;
        if (std::filesystem::weakly_canonical(root, error).parent_path() ==
            std::filesystem::weakly_canonical(std::filesystem::temp_directory_path()))
        {
            std::filesystem::remove_all(cpplivelab::windows_support::extended_path(root), error);
        }
    }
};
} // namespace workspace_test
