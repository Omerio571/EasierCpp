#pragma once

#include "appearance.hpp"
#include <commctrl.h>
#include <set>

namespace cpplivelab
{
// Owner drawing changes appearance, not button/combobox behavior or accessibility.
// Windows still handles clicks, keyboard navigation, dropdowns and focus messages.
class UiTheme
{
public:
    ~UiTheme();
    ThemePalette palette;
    void apply(HWND parent, ThemeChoice choice, UINT dpi);
    void active_button(HWND button, bool active);
    void round_controls();
    void paint_panels(HDC dc);
    bool draw_item(const DRAWITEMSTRUCT& item);
    HBRUSH control_colors(HDC dc, bool input, bool enabled) const;
    HBRUSH background_brush() const;

private:
    HWND parent_ = nullptr;
    HWND hovered_ = nullptr;
    HBRUSH background_ = nullptr;
    HBRUSH panel_ = nullptr;
    UINT dpi_ = 96;
    std::set<HWND> active_buttons_;
    static LRESULT CALLBACK subclass(
        HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR id, DWORD_PTR reference);
    void draw_combo(HWND window, HDC dc);
};
} // namespace cpplivelab
