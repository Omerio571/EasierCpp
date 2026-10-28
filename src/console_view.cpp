#include "console_view.hpp"
#include <stdexcept>

namespace cpplivelab
{
bool ConsolePane::reading_tail() const
{
    CHARRANGE selection{};
    SendMessageW(window, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    SCROLLINFO information{sizeof(SCROLLINFO), SIF_ALL};
    GetScrollInfo(window, SB_VERT, &information);
    return information.nPos + static_cast<int>(information.nPage) >= information.nMax - 3 &&
           selection.cpMin == selection.cpMax;
}

void ConsolePane::set_visible(bool visible)
{
    if (!visible && (GetWindowLongPtrW(window, GWL_STYLE) & WS_VISIBLE))
    {
        follow_tail_ = reading_tail();
    }
    ShowWindow(window, visible ? SW_SHOW : SW_HIDE);
    if (visible && follow_tail_)
    {
        SendMessageW(window, EM_SETSEL, -1, -1);
        SendMessageW(window, EM_SCROLLCARET, 0, 0);
    }
}

void ConsolePane::create(HWND parent, int id, HFONT font)
{
    window = CreateWindowExW(0,
        MSFTEDIT_CLASS,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_READONLY | ES_MULTILINE | ES_AUTOVSCROLL |
            WS_VSCROLL,
        0,
        0,
        0,
        0,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr),
        nullptr);
    if (!window)
    {
        throw std::runtime_error("The Windows Rich Edit console could not be created.");
    }
    SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(window, EM_EXLIMITTEXT, 0, 500000);
    SendMessageW(window, EM_SETUNDOLIMIT, 0, 0);
    PARAFORMAT2 paragraph{};
    paragraph.cbSize = sizeof(paragraph);
    paragraph.dwMask = PFM_SPACEAFTER;
    paragraph.dySpaceAfter = 25;
    SendMessageW(window, EM_SETPARAFORMAT, 0, reinterpret_cast<LPARAM>(&paragraph));
    SendMessageW(window, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(8, 8));
    set_colors(colors_);
}

void ConsolePane::insert(const ConsoleEntry& entry)
{
    SendMessageW(window, EM_SETSEL, -1, -1);
    CHARFORMAT2W formatting{};
    formatting.cbSize = sizeof(formatting);
    formatting.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
    formatting.crTextColor = message_color(colors_, entry.kind);
    formatting.yHeight = 220;
    wcscpy_s(formatting.szFaceName, L"Consolas");
    SendMessageW(window, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&formatting));
    SendMessageW(window, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(entry.text.c_str()));
}

void ConsolePane::append(MessageKind kind, std::wstring_view text)
{
    if (text.empty())
    {
        return;
    }
    CHARRANGE selection{};
    POINT scroll{};
    SendMessageW(window, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    SendMessageW(window, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
    const bool visible = (GetWindowLongPtrW(window, GWL_STYLE) & WS_VISIBLE) != 0;
    if (visible)
    {
        follow_tail_ = reading_tail();
    }
    const bool follow_output = follow_tail_;

    const auto old_entries = history.entries().size();
    const auto old_characters = history.characters();
    const size_t removed = history.append(kind, text);
    if (old_entries == history.entries().size() && old_characters == history.characters() &&
        removed == 0)
    {
        return;
    }
    // WM_SETREDRAW(TRUE) adds WS_VISIBLE. Never send it to an inactive tab.
    if (visible)
    {
        SendMessageW(window, WM_SETREDRAW, FALSE, 0);
    }
    if (removed)
    {
        SendMessageW(window, EM_SETSEL, 0, static_cast<LPARAM>(removed));
        SendMessageW(window, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(L""));
    }
    insert(history.entries().back());
    if (follow_output)
    {
        SendMessageW(window, EM_SCROLLCARET, 0, 0);
    }
    else
    {
        selection.cpMin = (std::max)(0L, selection.cpMin - static_cast<LONG>(removed));
        selection.cpMax = (std::max)(0L, selection.cpMax - static_cast<LONG>(removed));
        SendMessageW(window, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&selection));
        SendMessageW(window, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
    }
    if (visible)
    {
        SendMessageW(window, WM_SETREDRAW, TRUE, 0);
    }
    InvalidateRect(window, nullptr, FALSE);
}

void ConsolePane::clear()
{
    history.clear();
    follow_tail_ = true;
    SetWindowTextW(window, L"");
}

void ConsolePane::set_colors(const ConsoleColors& colors)
{
    colors_ = colors;
    if (!window)
    {
        return;
    }
    CHARRANGE selection{};
    POINT scroll{};
    SendMessageW(window, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    SendMessageW(window, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
    const bool visible = (GetWindowLongPtrW(window, GWL_STYLE) & WS_VISIBLE) != 0;
    if (visible)
    {
        SendMessageW(window, WM_SETREDRAW, FALSE, 0);
    }
    SendMessageW(window, EM_SETBKGNDCOLOR, 0, colors.background);
    SetWindowTextW(window, L"");
    for (const auto& entry : history.entries())
    {
        insert(entry);
    }
    SendMessageW(window, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    SendMessageW(window, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
    if (visible)
    {
        SendMessageW(window, WM_SETREDRAW, TRUE, 0);
    }
    InvalidateRect(window, nullptr, FALSE);
}
} // namespace cpplivelab
