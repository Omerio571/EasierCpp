#include "auto_build.hpp"
#include "directory_scan.hpp"

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <optional>
#include <stdexcept>

namespace cpplivelab
{
namespace
{
namespace fs = std::filesystem;

std::wstring lowercase(std::wstring text)
{
    for (auto& character : text)
    {
        character = static_cast<wchar_t>(std::towlower(character));
    }

    return text;
}

std::optional<std::uint64_t> fingerprint(const fs::path& path)
{
    std::error_code error;
    const auto native_path = windows_support::extended_path(path);
    const bool regular = fs::is_regular_file(native_path, error);
    if (error && error != std::errc::no_such_file_or_directory)
    {
        throw std::runtime_error(
            "Source status unavailable; freshness is unverified: " + error.message());
    }
    if (!regular)
    {
        return std::nullopt;
    }

    std::ifstream source(native_path, std::ios::binary);
    if (!source)
    {
        throw std::runtime_error(
            "A changed source file could not be read; save it again after the editor releases it.");
    }

    // FNV-1a is used only to suppress duplicate content notifications, not for security.
    std::uint64_t hash = 14695981039346656037ULL;
    std::array<char, 8192> buffer{};
    while (source.read(buffer.data(), buffer.size()) || source.gcount() != 0)
    {
        for (std::streamsize index = 0; index < source.gcount(); ++index)
        {
            hash ^= static_cast<unsigned char>(buffer[static_cast<size_t>(index)]);
            hash *= 1099511628211ULL;
        }
    }

    if (source.bad())
    {
        throw std::runtime_error("Source read incomplete; freshness is unverified.");
    }
    return hash;
}

bool generated_tree(const fs::path& root, const fs::path& relative)
{
    auto directory = root;
    for (const auto& component : relative)
    {
        directory /= component;
        std::error_code error;
        if (fs::is_regular_file(directory / L"CMakeCache.txt", error))
        {
            return true;
        }
    }
    return false;
}
} // namespace

bool ProjectWatcher::ignored_directory(const fs::path& component)
{
    const auto name = lowercase(component.wstring());
    return name == L".git" || name == L".vs" || name == L".cpplivelab" || name == L"build" ||
           name == L"out" || name == L"_deps" || name == L"cmakefiles" || name == L"debug" ||
           name == L"release" || name.starts_with(L"cmake-build-") || name.starts_with(L"build-");
}

bool ProjectWatcher::relevant(const fs::path& relative_path)
{
    for (const auto& component : relative_path.parent_path())
    {
        if (ignored_directory(component))
        {
            return false;
        }
    }

    const auto filename = lowercase(relative_path.filename().wstring());
    if (filename.empty() || filename.starts_with(L"~") || filename.starts_with(L".#") ||
        filename.ends_with(L"~") || filename == L"cmake_install.cmake" ||
        filename == L"ctesttestfile.cmake")
    {
        return false;
    }

    if (filename == L"cmakelists.txt" || filename == L"cmakepresets.json" ||
        filename == L"cmakeuserpresets.json")
    {
        return true;
    }

    const auto extension = lowercase(relative_path.extension().wstring());
    return extension == L".cpp" || extension == L".c" || extension == L".cc" ||
           extension == L".cxx" || extension == L".h" || extension == L".hpp" ||
           extension == L".hxx" || extension == L".cmake";
}

std::map<fs::path, std::uint64_t> ProjectWatcher::snapshot() const
{
    std::map<fs::path, std::uint64_t> files;
    scan_directory(project_,
        [&](const fs::path& path, bool directory)
        {
            if (directory)
            {
                return !ignored_directory(path.filename()) &&
                       !generated_tree(project_, path.lexically_relative(project_));
            }
            const auto relative_path = path.lexically_relative(project_);
            if (relevant(relative_path))
            {
                if (const auto hash = fingerprint(path))
                {
                    files[relative_path] = *hash;
                }
            }
            return true;
        });

    return files;
}

std::map<fs::path, std::uint64_t> ProjectWatcher::source_fingerprints(const fs::path& project)
{
    ProjectWatcher reader;
    reader.project_ = fs::weakly_canonical(project);
    return reader.snapshot();
}

ProjectWatcher::~ProjectWatcher()
{
    stop();
}

void ProjectWatcher::start(const fs::path& project)
{
    stop();
    project_ = fs::weakly_canonical(project);
    directory_handle_.reset(CreateFileW(project_.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr));
    if (directory_handle_.value == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error("Cannot watch this project directory.");
    }

    event_handle_.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event_handle_.value)
    {
        throw std::runtime_error("Cannot create the watcher notification event.");
    }

    notification_ = {};
    notification_.hEvent = event_handle_.value;
    arm();
    fingerprints_ = snapshot();
}

void ProjectWatcher::arm()
{
    ResetEvent(event_handle_.value);
    constexpr DWORD changes = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                              FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE;
    if (!ReadDirectoryChangesW(directory_handle_.value,
            notification_buffer_.data(),
            static_cast<DWORD>(notification_buffer_.size()),
            TRUE,
            changes,
            nullptr,
            &notification_,
            nullptr))
    {
        throw std::runtime_error("Windows could not start directory-change notifications.");
    }
    pending_read_ = true;
}

void ProjectWatcher::stop()
{
    if (pending_read_)
    {
        // OVERLAPPED and its buffer must survive until Windows acknowledges cancellation.
        CancelIoEx(directory_handle_.value, &notification_);
        DWORD ignored_bytes = 0;
        GetOverlappedResult(directory_handle_.value, &notification_, &ignored_bytes, TRUE);
    }

    pending_read_ = false;
    directory_handle_.reset();
    event_handle_.reset();
    fingerprints_.clear();
    pending_paths_.clear();
    rescan_required_ = false;
    project_.clear();
}

bool ProjectWatcher::poll()
{
    if (!pending_read_ || WaitForSingleObject(event_handle_.value, 0) != WAIT_OBJECT_0)
    {
        return false;
    }

    DWORD bytes = 0;
    if (!GetOverlappedResult(directory_handle_.value, &notification_, &bytes, FALSE))
    {
        pending_read_ = false;
        throw std::runtime_error("The project watcher stopped receiving directory notifications.");
    }
    pending_read_ = false;
    bool relevant_event = false;

    if (bytes == 0)
    {
        // Overflow is a request to reconcile a snapshot, not evidence that nothing changed.
        rescan_required_ = true;
        relevant_event = true;
    }
    else
    {
        size_t offset = 0;
        while (offset + offsetof(FILE_NOTIFY_INFORMATION, FileName) <= bytes)
        {
            const auto* change = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(
                notification_buffer_.data() + offset);
            const auto name_bytes = change->FileNameLength;
            if (offset + offsetof(FILE_NOTIFY_INFORMATION, FileName) + name_bytes > bytes)
            {
                rescan_required_ = true;
                relevant_event = true;
                break;
            }

            const fs::path relative_path(
                std::wstring(change->FileName, name_bytes / sizeof(wchar_t)));
            if (generated_tree(project_, relative_path.parent_path()) ||
                generated_tree(project_, relative_path))
            {
                // An existing CMake cache identifies custom-named generated directories.
            }
            else if (relevant(relative_path))
            {
                pending_paths_.insert(relative_path);
                relevant_event = true;
            }
            else if (std::none_of(relative_path.begin(), relative_path.end(), ignored_directory) &&
                     (change->Action == FILE_ACTION_RENAMED_OLD_NAME ||
                         change->Action == FILE_ACTION_RENAMED_NEW_NAME ||
                         change->Action == FILE_ACTION_REMOVED ||
                         change->Action == FILE_ACTION_ADDED))
            {
                // A directory rename/removal may represent many files. Only request
                // reconciliation for previously watched descendants or real source dirs.
                const auto under_path = relative_path.wstring() + L"\\";
                const bool had_sources = std::any_of(fingerprints_.begin(),
                    fingerprints_.end(),
                    [&](const auto& file)
                    {
                        return file.first.wstring().starts_with(under_path);
                    });
                std::error_code error;
                if (had_sources || (fs::is_directory(project_ / relative_path, error) &&
                                       !ignored_directory(relative_path.filename())))
                {
                    rescan_required_ = true;
                    relevant_event = true;
                }
            }

            if (change->NextEntryOffset == 0)
            {
                break;
            }
            offset += change->NextEntryOffset;
        }
    }

    arm();
    return relevant_event;
}

std::vector<SourceChange> ProjectWatcher::consume_changes()
{
    auto current = rescan_required_ ? snapshot() : fingerprints_;
    for (const auto& path : pending_paths_)
    {
        if (const auto hash = fingerprint(project_ / path))
        {
            current[path] = *hash;
        }
        else
        {
            current.erase(path);
        }
    }

    std::set<fs::path> candidates = pending_paths_;
    if (rescan_required_)
    {
        for (const auto& [path, hash] : fingerprints_)
        {
            candidates.insert(path);
        }
        for (const auto& [path, hash] : current)
        {
            candidates.insert(path);
        }
    }

    std::vector<SourceChange> changes;
    for (const auto& path : candidates)
    {
        const auto previous = fingerprints_.find(path);
        const auto next = current.find(path);
        if ((previous == fingerprints_.end()) != (next == current.end()) ||
            (previous != fingerprints_.end() && next != current.end() &&
                previous->second != next->second))
        {
            const auto name = lowercase(path.filename().wstring());
            changes.push_back({path,
                name == L"cmakelists.txt" || name.ends_with(L".cmake") ||
                    name == L"cmakepresets.json" || name == L"cmakeuserpresets.json"});
        }
    }

    fingerprints_ = std::move(current);
    pending_paths_.clear();
    rescan_required_ = false;
    return changes;
}

void AutoBuildScheduler::enable(bool enabled)
{
    enabled_ = enabled;
    cancel();
}

bool AutoBuildScheduler::enabled() const
{
    return enabled_;
}

void AutoBuildScheduler::set_debounce(std::uint64_t milliseconds)
{
    debounce_ = milliseconds;
}

void AutoBuildScheduler::notify(std::uint64_t now)
{
    if (enabled_)
    {
        notification_pending_ = true;
        last_notification_ = now;
    }
}

bool AutoBuildScheduler::settled(std::uint64_t now) const
{
    return enabled_ && notification_pending_ && now - last_notification_ >= debounce_;
}

void AutoBuildScheduler::confirm(bool changed, bool reconfigure)
{
    notification_pending_ = false;
    build_pending_ = build_pending_ || (enabled_ && changed);
    reconfigure_ = reconfigure_ || (changed && reconfigure);
}

bool AutoBuildScheduler::ready(std::uint64_t now, bool operation_busy, bool output_available) const
{
    return enabled_ && build_pending_ && !notification_pending_ && now >= retry_after_ &&
           !operation_busy && output_available;
}

void AutoBuildScheduler::begin_build()
{
    build_pending_ = false;
    reconfigure_ = false;
    retry_after_ = 0;
}

void AutoBuildScheduler::defer(std::uint64_t now)
{
    build_pending_ = enabled_;
    retry_after_ = now + 1000;
}

void AutoBuildScheduler::cancel()
{
    notification_pending_ = false;
    build_pending_ = false;
    reconfigure_ = false;
    retry_after_ = 0;
}

bool AutoBuildScheduler::pending() const
{
    return build_pending_ || notification_pending_;
}

bool AutoBuildScheduler::needs_reconfigure() const
{
    return reconfigure_;
}
} // namespace cpplivelab
