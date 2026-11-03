#include "projects/state_migration.hpp"
#include "process/windows_support.hpp"
#include "cpplivelab/libraries.hpp"
#include "projects/project_paths.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>
#include <vector>

namespace cpplivelab
{
namespace
{
namespace fs = std::filesystem;

void ordinary_path(const fs::path& path)
{
    const auto attributes = GetFileAttributesW(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
    {
        throw std::runtime_error("Settings migration refuses linked directories/files: " +
                                 path_support::path_utf8(path));
    }
}

std::string bytes(const fs::path& file)
{
    if (fs::file_size(file) > 16 * 1024 * 1024)
    {
        throw std::runtime_error("Settings migration metadata exceeds the safety size limit.");
    }
    std::ifstream input(file, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("Cannot read settings migration metadata.");
    }
    return {std::istreambuf_iterator<char>(input), {}};
}

constexpr std::string_view completion_text = "EasierCpp metadata migration 1\n";
constexpr std::string_view legacy_completion_text = "EasierCpp metadata migration 1\r\n";

bool migration_completed(const fs::path& marker)
{
    if (!fs::exists(marker))
    {
        return false;
    }
    if (!fs::is_regular_file(marker))
    {
        throw std::runtime_error("Migration completion marker is not a regular file: " +
                                 path_support::path_utf8(marker));
    }
    const auto value = bytes(marker);
    // Older releases wrote in Windows text mode. Accept their exact CRLF
    // signature without rewriting data or reacquiring the GUI's legacy mutex.
    if (value == completion_text || value == legacy_completion_text)
    {
        return true;
    }
    if (completion_text.starts_with(value) || legacy_completion_text.starts_with(value))
    {
        return false; // Empty/truncated publication can be retried conservatively.
    }
    throw std::runtime_error(
        "Unrecognized migration completion marker: " + path_support::path_utf8(marker) +
        ". Original metadata retained; review the marker before retrying.");
}

void publish_completion(const fs::path& marker)
{
    const auto temporary =
        fs::path(marker.wstring() + L".new-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                 std::to_wstring(GetTickCount64()));
    windows_support::UniqueHandle file;
    file.reset(CreateFileW(
        temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (file.value == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error("Cannot stage migration completion marker; Windows error " +
                                 std::to_string(GetLastError()));
    }
    DWORD written = 0;
    const bool saved = WriteFile(file.value,
                           completion_text.data(),
                           static_cast<DWORD>(completion_text.size()),
                           &written,
                           nullptr) &&
                       written == completion_text.size() && FlushFileBuffers(file.value);
    auto error = saved ? ERROR_SUCCESS : GetLastError();
    file.reset(); // Release exclusive ownership before the atomic rename.
    if (!saved ||
        !MoveFileExW(
            temporary.c_str(), marker.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        if (saved)
        {
            error = GetLastError();
        }
        DeleteFileW(temporary.c_str()); // Only this call's exclusively created staging file.
        throw std::runtime_error("Cannot publish migration completion marker; Windows error " +
                                 std::to_string(error ? error : ERROR_WRITE_FAULT) +
                                 ". Validated metadata retained for retry.");
    }
}

bool metadata_file(const fs::path& relative, bool project)
{
    if (!project)
    {
        return relative.extension() == L".library" || relative.extension() == L".cmake" ||
               relative.extension() == L".name" || relative.extension() == L".hidden";
    }
    const auto parent = relative.parent_path();
    const auto name = relative.filename().wstring();
    if (parent.empty())
    {
        return name == L"Debug.settings" || name == L"Release.settings" ||
               name == L"auto-run.settings" || name == L"last-config.txt" ||
               name == L"project.path" || name == L"libraries.manifest" ||
               name.starts_with(L"CMakeLists.before-libraries-");
    }
    return (parent == L"window-placement" && relative.extension() == L".settings") ||
           (parent == L"pch" && relative.extension() == L".cmake");
}

void migrate_unit(
    const fs::path& source, const fs::path& target, bool project, std::ostream& report)
{
    ordinary_path(source);
    ordinary_path(target);
    if (project && fs::exists(source / L"library-transaction/pending"))
    {
        throw std::runtime_error("Recover the interrupted library transaction in the legacy "
                                 "manager before migrating: " +
                                 path_support::path_utf8(source));
    }
    std::vector<fs::path> files;
    if (fs::is_regular_file(source))
    {
        files.emplace_back();
    }
    else
    {
        for (fs::recursive_directory_iterator item(source), end; item != end; ++item)
        {
            const auto relative = item->path().lexically_relative(source);
            if (item->is_directory())
            {
                if (!project || (relative != L"pch" && relative != L"window-placement"))
                {
                    item.disable_recursion_pending();
                    continue;
                }
                ordinary_path(item->path());
            }
            else if (metadata_file(relative, project))
            {
                ordinary_path(item->path());
                if (!item->is_regular_file() || files.size() >= 4096)
                {
                    throw std::runtime_error("Invalid or excessive settings migration metadata.");
                }
                files.push_back(relative);
            }
        }
    }
    if (files.empty())
    {
        return;
    }
    if (fs::exists(target))
    {
        bool identical = true;
        for (const auto& file : files)
        {
            const auto old_file = file.empty() ? source : source / file;
            const auto new_file = file.empty() ? target : target / file;
            ordinary_path(new_file);
            identical =
                identical && fs::is_regular_file(new_file) && bytes(old_file) == bytes(new_file);
        }
        report << (identical ? "Already present: " : "Conflict; retained new data and original: ")
               << path_support::path_utf8(target) << '\n';
        return;
    }
    // A complete unit is staged on the destination volume, then published without
    // replacement. A crash cannot expose half a project/catalog. Abandoned staging
    // is retained; a subsequent run retries from the unchanged original metadata.
    const auto staging =
        fs::path(target.wstring() + L".migration-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                 std::to_wstring(GetTickCount64()));
    fs::create_directories(target.parent_path());
    if (fs::is_directory(source) && !fs::create_directory(staging))
    {
        throw std::runtime_error("Cannot reserve settings migration staging directory.");
    }
    for (const auto& file : files)
    {
        const auto from = file.empty() ? source : source / file;
        const auto to = file.empty() ? staging : staging / file;
        fs::create_directories(to.parent_path());
        if (!CopyFileW(from.c_str(), to.c_str(), TRUE) || bytes(from) != bytes(to))
        {
            throw std::runtime_error("Settings migration copy/validation failed; originals "
                                     "retained. Retry after closing the legacy manager.");
        }
    }
    if (!MoveFileExW(staging.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH))
    {
        throw std::runtime_error("Settings migration publication failed; both copies retained.");
    }
    report << "Copied validated metadata: " << path_support::path_utf8(target) << '\n';
}
} // namespace

void migrate_state_metadata(const fs::path& legacy, const fs::path& destination)
{
    ordinary_path(legacy.parent_path());
    ordinary_path(legacy);
    ordinary_path(destination);
    if (fs::weakly_canonical(legacy.parent_path()) !=
        fs::weakly_canonical(destination.parent_path()))
    {
        throw std::runtime_error("Settings migration requires sibling application directories.");
    }
    if (!fs::exists(legacy))
    {
        return;
    }
    if (fs::weakly_canonical(legacy) == fs::weakly_canonical(destination))
    {
        throw std::runtime_error("Settings migration source and destination must differ.");
    }
    const auto marker = destination / L"legacy-migration.complete";
    ordinary_path(marker);
    if (migration_completed(marker))
    {
        return;
    }
    // Coordinate with the legacy GUI's existing instance contract. Refuse a live
    // legacy manager rather than snapshotting preferences during a mutation.
    auto identity = fs::weakly_canonical(legacy).wstring();
    for (auto& character : identity)
    {
        character = static_cast<wchar_t>(std::towlower(character));
    }
    const auto mutex_name =
        L"Local\\CppLiveLab-" +
        path_support::from_utf8(library_key(path_support::path_utf8(identity))).wstring();
    windows_support::UniqueHandle legacy_mutex;
    legacy_mutex.reset(CreateMutexW(nullptr, FALSE, mutex_name.c_str()));
    const auto acquired =
        legacy_mutex.value ? WaitForSingleObject(legacy_mutex.value, 0) : WAIT_FAILED;
    if (acquired == WAIT_TIMEOUT)
    {
        throw std::runtime_error("Close the legacy CppLiveLab manager before settings migration.");
    }
    if (acquired != WAIT_OBJECT_0 && acquired != WAIT_ABANDONED)
    {
        throw std::runtime_error(
            "Cannot acquire the legacy settings migration guard; Windows error " +
            std::to_string(GetLastError()));
    }

    struct Release
    {
        HANDLE mutex;

        ~Release()
        {
            ReleaseMutex(mutex);
        }
    } release{legacy_mutex.value};

    fs::create_directories(destination);
    ordinary_path(destination / L"migration.lock");
    ordinary_path(destination / L"migration-report.txt");
    // Exclusive per-destination lease serializes CLI/GUI first starts. Nothing
    // under the old manager is deleted, renamed or used as a new build directory.
    windows_support::UniqueHandle lease;
    lease.reset(CreateFileW((destination / L"migration.lock").c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr));
    if (lease.value == INVALID_HANDLE_VALUE)
    {
        const auto error = GetLastError();
        if (error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION)
        {
            throw std::runtime_error(
                "Another EasierCpp settings migration is active; retry shortly.");
        }
        throw std::runtime_error("Cannot open the settings migration lease at " +
                                 path_support::path_utf8(destination) + "; Windows error " +
                                 std::to_string(error));
    }
    if (migration_completed(marker))
    {
        return;
    }
    std::ofstream report(destination / L"migration-report.txt", std::ios::app);
    if (!report)
    {
        throw std::runtime_error("Cannot create the settings migration report.");
    }
    for (const auto* name : {L"recent.txt", L"editor.settings", L"build-workers.settings"})
    {
        if (fs::exists(legacy / name))
        {
            migrate_unit(legacy / name, destination / name, false, report);
        }
    }
    for (const auto* name : {L"integrations", L"retired-libraries"})
    {
        if (fs::is_directory(legacy / name))
        {
            migrate_unit(legacy / name, destination / name, false, report);
        }
    }
    ordinary_path(legacy / L"projects");
    ordinary_path(destination / L"projects");
    if (fs::is_directory(legacy / L"projects"))
    {
        for (const auto& project : fs::directory_iterator(legacy / L"projects"))
        {
            ordinary_path(project.path());
            if (project.is_directory())
            {
                migrate_unit(project.path(),
                    destination / L"projects" / project.path().filename(),
                    true,
                    report);
            }
        }
    }
    report << "Original metadata and all legacy build/download caches retained at "
           << path_support::path_utf8(legacy) << "\n";
    report << "Appearance registry: HKCU/Software/EasierCpp/Preferences takes precedence as a "
              "complete key; "
              "HKCU/Software/CppLiveLab is retained. Existing new preferences are never merged "
              "or overwritten by legacy values.\n";
    report.close();
    if (!report)
    {
        throw std::runtime_error(
            "Cannot finish migration; validated copies are retained for retry.");
    }
    publish_completion(marker);
    std::cout << "EasierCpp settings migration: "
              << path_support::path_utf8(destination / L"migration-report.txt") << '\n';
}
} // namespace cpplivelab
