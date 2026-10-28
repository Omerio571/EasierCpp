#include "console_model.hpp"

#include <algorithm>
#include <cwctype>

namespace cpplivelab
{
ConsoleHistory::ConsoleHistory(size_t maximum_characters, size_t maximum_entries)
    : maximum_characters_((std::max)(maximum_characters, size_t{16})),
      maximum_entries_((std::max)(maximum_entries, size_t{1}))
{
}

size_t ConsoleHistory::append(MessageKind kind, std::wstring_view text)
{
    std::wstring normalized;
    for (wchar_t character : text)
    {
        // Rich Edit uses a single CR internally for a paragraph. Normalize CRLF
        // even if a pipe read splits the two bytes, so model and selection offsets agree.
        if (character == L'\n')
        {
            if (!previous_was_cr_)
            {
                normalized += L'\r';
            }
            previous_was_cr_ = false;
        }
        else
        {
            normalized += character;
            previous_was_cr_ = character == L'\r';
        }
    }
    if (normalized.empty())
    {
        return 0;
    }
    if (normalized.size() > maximum_characters_)
    {
        normalized.erase(0, normalized.size() - maximum_characters_);
        if (!normalized.empty() && normalized.front() >= 0xDC00 && normalized.front() <= 0xDFFF)
        {
            normalized.erase(0, 1); // Do not retain half of a UTF-16 surrogate pair.
        }
    }

    entries_.push_back({kind, std::move(normalized)});
    characters_ += entries_.back().text.size();
    size_t removed_characters = 0;
    while (entries_.size() > 1 &&
           (characters_ > maximum_characters_ || entries_.size() > maximum_entries_))
    {
        removed_characters += entries_.front().text.size();
        characters_ -= entries_.front().text.size();
        entries_.pop_front();
    }
    if (!active_)
    {
        unread_ = true;
        unread_error_ =
            unread_error_ || kind == MessageKind::Error || kind == MessageKind::ProgramError;
    }
    return removed_characters;
}

void ConsoleHistory::clear()
{
    entries_.clear();
    characters_ = 0;
    unread_ = false;
    unread_error_ = false;
    previous_was_cr_ = false;
}

void ConsoleHistory::set_active(bool active)
{
    active_ = active;
    if (active)
    {
        unread_ = false;
        unread_error_ = false;
    }
}

bool ConsoleHistory::unread() const
{
    return unread_;
}

bool ConsoleHistory::unread_error() const
{
    return unread_error_;
}

size_t ConsoleHistory::characters() const
{
    return characters_;
}

const std::deque<ConsoleEntry>& ConsoleHistory::entries() const
{
    return entries_;
}

MessageKind classify_build_line(std::wstring_view line)
{
    std::wstring lowered(line);
    for (auto& character : lowered)
    {
        character = static_cast<wchar_t>(std::towlower(character));
    }
    if (lowered.find(L"already building or running") != lowered.npos ||
        lowered.find(L"executable is running or not writable") != lowered.npos)
    {
        return MessageKind::Warning;
    }
    if (lowered.find(L": error") != lowered.npos || lowered.find(L"fatal error") != lowered.npos ||
        lowered.starts_with(L"cmake error") || lowered.starts_with(L"failed:") ||
        lowered.find(L"ninja: error") != lowered.npos ||
        lowered.find(L"compilation failed") != lowered.npos ||
        lowered.find(L"configuration failed") != lowered.npos ||
        lowered.find(L"build stopped") != lowered.npos)
    {
        return MessageKind::Error;
    }
    if (lowered.find(L": warning") != lowered.npos || lowered.starts_with(L"cmake warning"))
    {
        return MessageKind::Warning;
    }
    return MessageKind::Build;
}

std::wstring_view message_label(MessageKind kind)
{
    switch (kind)
    {
        case MessageKind::System:
            return L"[SYSTEM] ";
        case MessageKind::Build:
            return L"[BUILD] ";
        case MessageKind::Success:
            return L"[SUCCESS] ";
        case MessageKind::Warning:
            return L"[WARNING] ";
        case MessageKind::Error:
            return L"[ERROR] ";
        case MessageKind::ProgramError:
            return L"[stderr] ";
        default:
            return L"";
    }
}
} // namespace cpplivelab
