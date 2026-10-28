#pragma once

#include "windows_support.hpp"
#include "console_model.hpp"
#include "appearance.hpp"
#include <richedit.h>

namespace cpplivelab
{
// A read-only native Rich Edit control: selection/copy and scrolling remain Windows
// behavior. Appending never calls SetFocus and restores the selection/scroll position
// if the reader has moved away from the bottom.
class ConsolePane
{
public:
    HWND window = nullptr;
    ConsoleHistory history;
    void create(HWND parent, int id, HFONT font);
    void append(MessageKind kind, std::wstring_view text);
    void clear();
    void set_colors(const ConsoleColors& colors);
    void set_visible(bool visible);

private:
    ConsoleColors colors_;
    bool follow_tail_ = true;
    bool reading_tail() const;
    void insert(const ConsoleEntry& entry);
};
} // namespace cpplivelab
