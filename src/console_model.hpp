#pragma once

#include <deque>
#include <string>
#include <string_view>

namespace cpplivelab
{
enum class MessageKind
{
    ProgramOutput,
    ProgramError,
    System,
    Build,
    Success,
    Warning,
    Error,
    Input
};

struct ConsoleEntry
{
    MessageKind kind;
    std::wstring text;
};

// Owned and updated on the UI thread. Keeping a bounded model lets theme changes
// recolor retained text without losing severity, and makes independent tab clearing
// and unread/error indicators testable without a desktop automation dependency.
class ConsoleHistory
{
public:
    explicit ConsoleHistory(size_t maximum_characters = 400000, size_t maximum_entries = 3000);
    size_t append(MessageKind kind, std::wstring_view text);
    void clear();
    void set_active(bool active);
    bool unread() const;
    bool unread_error() const;
    size_t characters() const;
    const std::deque<ConsoleEntry>& entries() const;

private:
    std::deque<ConsoleEntry> entries_;
    size_t characters_ = 0;
    size_t maximum_characters_;
    size_t maximum_entries_;
    bool active_ = false;
    bool unread_ = false;
    bool unread_error_ = false;
    bool previous_was_cr_ = false;
};

MessageKind classify_build_line(std::wstring_view line);
std::wstring_view message_label(MessageKind kind);
} // namespace cpplivelab
