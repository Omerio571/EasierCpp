#pragma once

#include "windows_support.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace cpplivelab
{
struct SourceChange
{
    std::filesystem::path relative_path;
    bool affects_cmake = false;
};

// One overlapped directory notification, no watcher thread and no periodic tree scan.
// The GUI checks its event on the existing timer. Content fingerprints are checked
// only after notifications settle, so duplicate events/timestamp-only touches do not
// launch builds and atomic editor save/rename sequences are treated as one change.
class ProjectWatcher
{
public:
    ~ProjectWatcher();
    void start(const std::filesystem::path& project);
    void stop();
    bool poll();
    std::vector<SourceChange> consume_changes();
    static bool relevant(const std::filesystem::path& relative_path);
    static bool ignored_directory(const std::filesystem::path& component);
    static std::map<std::filesystem::path, std::uint64_t> source_fingerprints(
        const std::filesystem::path& project);
    // Includes added/removed configuration inputs, not just modified files.
    static bool configuration_changed(const std::map<std::filesystem::path, std::uint64_t>& before,
        const std::map<std::filesystem::path, std::uint64_t>& after);

private:
    windows_support::UniqueHandle directory_handle_;
    windows_support::UniqueHandle event_handle_;
    OVERLAPPED notification_{};
    alignas(DWORD) std::array<std::byte, 65536> notification_buffer_{};
    std::filesystem::path project_;
    std::map<std::filesystem::path, std::uint64_t> fingerprints_;
    std::set<std::filesystem::path> pending_paths_;
    bool pending_read_ = false;
    bool rescan_required_ = false;

    void arm();
    std::map<std::filesystem::path, std::uint64_t> snapshot() const;
};

// Deterministic scheduling state; ticks are milliseconds supplied by the caller.
// A single pending flag coalesces edits; begin_build consumes only edits known at
// that point. Later edits remain queued through a build or running program.
class AutoBuildScheduler
{
public:
    void enable(bool enabled);
    bool enabled() const;
    void set_debounce(std::uint64_t milliseconds);
    void notify(std::uint64_t now);
    bool settled(std::uint64_t now) const;
    void confirm(bool changed, bool reconfigure);
    bool ready(std::uint64_t now, bool operation_busy, bool output_available) const;
    void begin_build();
    void defer(std::uint64_t now);
    void cancel();
    bool pending() const;
    bool needs_reconfigure() const;

private:
    bool enabled_ = false;
    bool notification_pending_ = false;
    bool build_pending_ = false;
    bool reconfigure_ = false;
    std::uint64_t debounce_ = 400;
    std::uint64_t last_notification_ = 0;
    std::uint64_t retry_after_ = 0;
};
} // namespace cpplivelab
