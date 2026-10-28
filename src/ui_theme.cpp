#include "ui_theme.hpp"
#include <uxtheme.h>
#include <dwmapi.h>

namespace cpplivelab
{
namespace
{
std::wstring class_name(HWND window)
{
    wchar_t name[80]{};
    GetClassNameW(window, name, 80);
    return name;
}

void rounded_rectangle(HDC dc, RECT rectangle, int diameter, COLORREF fill, COLORREF border)
{
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    const auto previous_brush = SelectObject(dc, brush);
    const auto previous_pen = SelectObject(dc, pen);
    RoundRect(
        dc, rectangle.left, rectangle.top, rectangle.right, rectangle.bottom, diameter, diameter);
    SelectObject(dc, previous_pen);
    SelectObject(dc, previous_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}
} // namespace

UiTheme::~UiTheme()
{
    if (background_)
    {
        DeleteObject(background_);
    }
    if (panel_)
    {
        DeleteObject(panel_);
    }
}

void UiTheme::apply(HWND parent, ThemeChoice choice, UINT dpi)
{
    parent_ = parent;
    dpi_ = dpi;
    palette = theme_palette(choice);
    if (background_)
    {
        DeleteObject(background_);
    }
    if (panel_)
    {
        DeleteObject(panel_);
    }
    background_ = CreateSolidBrush(palette.background);
    panel_ = CreateSolidBrush(palette.panel);
    BOOL dark = palette.dark;
    // Documented DWM title-bar preference. Unsupported Windows versions simply
    // retain their OS-owned title bar; client controls are still painted by us.
    DwmSetWindowAttribute(parent, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    EnumChildWindows(
        parent,
        [](HWND child, LPARAM data) -> BOOL
        {
            auto& theme = *reinterpret_cast<UiTheme*>(data);
            const auto name = class_name(child);
            if (name == L"Button")
            {
                const auto style = GetWindowLongPtrW(child, GWL_STYLE);
                SetWindowLongPtrW(child, GWL_STYLE, (style & ~BS_TYPEMASK) | BS_OWNERDRAW);
            }
            if (name == L"Edit")
            {
                SetWindowLongPtrW(
                    child, GWL_EXSTYLE, GetWindowLongPtrW(child, GWL_EXSTYLE) & ~WS_EX_CLIENTEDGE);
                SendMessageW(child,
                    EM_SETMARGINS,
                    EC_LEFTMARGIN | EC_RIGHTMARGIN,
                    MAKELPARAM(scale_pixels(8, theme.dpi_), scale_pixels(8, theme.dpi_)));
            }
            // Standard native scrollbars are best-effort themed by Windows. All console
            // content, selection and the surrounding panel use the explicit palette.
            SetWindowTheme(child, theme.palette.dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
            SetWindowSubclass(child, subclass, 1, reinterpret_cast<DWORD_PTR>(&theme));
            return TRUE;
        },
        reinterpret_cast<LPARAM>(this));
    round_controls();
    RedrawWindow(parent, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

void UiTheme::active_button(HWND button, bool active)
{
    if (!button)
    {
        return;
    }
    if (active)
    {
        active_buttons_.insert(button);
    }
    else
    {
        active_buttons_.erase(button);
    }
    InvalidateRect(button, nullptr, FALSE);
}

void UiTheme::round_controls()
{
    if (!parent_)
    {
        return;
    }
    EnumChildWindows(
        parent_,
        [](HWND child, LPARAM data) -> BOOL
        {
            auto& theme = *reinterpret_cast<UiTheme*>(data);
            const auto name = class_name(child);
            if (name == L"Button" || name == L"Edit" || name == L"ComboBox" ||
                name == L"RICHEDIT50W")
            {
                RECT client{};
                GetWindowRect(child, &client);
                const int diameter = scale_pixels(16, theme.dpi_);
                HRGN region = CreateRoundRectRgn(0,
                    0,
                    client.right - client.left + 1,
                    client.bottom - client.top + 1,
                    diameter,
                    diameter);
                // Windows owns a region after successful SetWindowRgn.
                if (!SetWindowRgn(child, region, FALSE))
                {
                    DeleteObject(region);
                }
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(this));
}

void UiTheme::paint_panels(HDC dc)
{
    for (HWND child = GetWindow(parent_, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
    {
        const auto name = class_name(child);
        if (!IsWindowVisible(child) || (name != L"Edit" && name != L"RICHEDIT50W"))
        {
            continue;
        }
        RECT bounds{};
        GetWindowRect(child, &bounds);
        MapWindowPoints(nullptr, parent_, reinterpret_cast<POINT*>(&bounds), 2);
        InflateRect(&bounds, 1, 1);
        rounded_rectangle(dc,
            bounds,
            scale_pixels(16, dpi_),
            palette.panel,
            GetFocus() == child ? palette.accent : palette.border);
    }
}

bool UiTheme::draw_item(const DRAWITEMSTRUCT& item)
{
    if (item.CtlType != ODT_BUTTON && item.CtlType != ODT_COMBOBOX)
    {
        return false;
    }
    const bool enabled = (item.itemState & ODS_DISABLED) == 0;
    const bool active = active_buttons_.contains(item.hwndItem);
    COLORREF background = palette.button;
    if (item.CtlType == ODT_COMBOBOX)
    {
        background = palette.panel;
    }
    if (item.itemState & ODS_SELECTED)
    {
        background = palette.pressed;
    }
    else if (hovered_ == item.hwndItem)
    {
        background = palette.hover;
    }
    if (active || (item.CtlType == ODT_COMBOBOX && (item.itemState & ODS_SELECTED)))
    {
        background = palette.accent;
    }
    FillRect(item.hDC, &item.rcItem, background_);
    rounded_rectangle(item.hDC, item.rcItem, scale_pixels(16, dpi_), background, palette.border);
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC,
        !enabled ? palette.disabled
        : active || (item.CtlType == ODT_COMBOBOX && (item.itemState & ODS_SELECTED))
            ? palette.accent_text
            : palette.text);
    std::wstring caption;
    if (item.CtlType == ODT_COMBOBOX && item.itemID != static_cast<UINT>(-1))
    {
        const auto length = SendMessageW(item.hwndItem, CB_GETLBTEXTLEN, item.itemID, 0);
        if (length >= 0)
        {
            caption.resize(static_cast<size_t>(length) + 1);
            SendMessageW(
                item.hwndItem, CB_GETLBTEXT, item.itemID, reinterpret_cast<LPARAM>(caption.data()));
            caption.resize(static_cast<size_t>(length));
        }
    }
    else
    {
        caption.resize(GetWindowTextLengthW(item.hwndItem) + 1);
        caption.resize(
            GetWindowTextW(item.hwndItem, caption.data(), static_cast<int>(caption.size())));
    }
    auto rectangle = item.rcItem;
    InflateRect(&rectangle, -scale_pixels(8, dpi_), -2);
    DrawTextW(item.hDC,
        caption.c_str(),
        -1,
        &rectangle,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    if (item.itemState & ODS_FOCUS)
    {
        InflateRect(&rectangle, -2, -2);
        DrawFocusRect(item.hDC, &rectangle);
    }
    return true;
}

void UiTheme::draw_combo(HWND window, HDC dc)
{
    RECT rectangle{};
    GetClientRect(window, &rectangle);
    rounded_rectangle(dc,
        rectangle,
        scale_pixels(16, dpi_),
        palette.panel,
        GetFocus() == window ? palette.accent : palette.border);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, IsWindowEnabled(window) ? palette.text : palette.disabled);
    const auto font = reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0));
    const auto previous_font = SelectObject(dc, font);
    wchar_t caption[128]{};
    GetWindowTextW(window, caption, 128);
    RECT text = rectangle;
    text.left += scale_pixels(10, dpi_);
    text.right -= scale_pixels(25, dpi_);
    DrawTextW(dc,
        caption,
        -1,
        &text,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    const int x = rectangle.right - scale_pixels(15, dpi_);
    const int y = (rectangle.top + rectangle.bottom) / 2;
    HPEN pen = CreatePen(PS_SOLID, scale_pixels(2, dpi_), palette.text);
    const auto old_pen = SelectObject(dc, pen);
    MoveToEx(dc, x - scale_pixels(4, dpi_), y - 2, nullptr);
    LineTo(dc, x, y + scale_pixels(3, dpi_));
    LineTo(dc, x + scale_pixels(4, dpi_), y - 2);
    SelectObject(dc, old_pen);
    DeleteObject(pen);
    SelectObject(dc, previous_font);
    if (GetFocus() == window)
    {
        InflateRect(&text, -2, -scale_pixels(4, dpi_));
        DrawFocusRect(dc, &text);
    }
}

HBRUSH UiTheme::control_colors(HDC dc, bool input, bool enabled) const
{
    SetTextColor(dc, enabled ? palette.text : palette.disabled);
    SetBkColor(dc, input ? palette.panel : palette.background);
    SetBkMode(dc, input ? OPAQUE : TRANSPARENT);
    return input ? panel_ : background_;
}

HBRUSH UiTheme::background_brush() const
{
    return background_;
}

LRESULT CALLBACK UiTheme::subclass(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR id, DWORD_PTR reference)
{
    auto& theme = *reinterpret_cast<UiTheme*>(reference);
    if (message == WM_MOUSEMOVE)
    {
        if (theme.hovered_ != window)
        {
            const auto previous = theme.hovered_;
            theme.hovered_ = window;
            if (previous)
            {
                InvalidateRect(previous, nullptr, FALSE);
            }
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window, 0};
            TrackMouseEvent(&tracking);
            InvalidateRect(window, nullptr, FALSE);
        }
    }
    else if (message == WM_MOUSELEAVE)
    {
        theme.hovered_ = nullptr;
        InvalidateRect(window, nullptr, FALSE);
    }
    else if (message == WM_SETFOCUS || message == WM_KILLFOCUS || message == WM_ENABLE)
    {
        InvalidateRect(window, nullptr, TRUE);
        InvalidateRect(theme.parent_, nullptr, FALSE);
    }
    else if (message == WM_PAINT && class_name(window) == L"ComboBox")
    {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        theme.draw_combo(window, dc);
        EndPaint(window, &paint);
        return 0;
    }
    else if (message == WM_NCDESTROY)
    {
        RemoveWindowSubclass(window, subclass, id);
    }
    return DefSubclassProc(window, message, wparam, lparam);
}
} // namespace cpplivelab
