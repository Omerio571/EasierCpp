#pragma once

#include "process/windows_support.hpp"
#include "process/console_model.hpp"

namespace cpplivelab
{
enum class ThemeChoice : DWORD
{
    Dark = 0,
    Light = 1,
    FollowWindows = 2
};

struct Preferences
{
    ThemeChoice theme = ThemeChoice::FollowWindows;
    bool auto_build = false;
    DWORD debounce_ms = 400;
};

// Persist only three UI preferences, never project state, source, or environments.
// Tests pass a separate temporary key instead of changing the user's preferences.
Preferences load_preferences(const std::wstring& key = L"Software\\CppLiveLab");
bool save_preferences(
    const Preferences& preferences, const std::wstring& key = L"Software\\CppLiveLab");
bool windows_uses_dark_theme();

struct ConsoleColors
{
    COLORREF background = RGB(255, 255, 255);
    COLORREF neutral = RGB(25, 35, 50);
    COLORREF information = RGB(0, 78, 255);
    // Darken the customized hues only enough for 4.5:1 on white. Keep warning
    // slightly farther from red so the semantic hues remain 30 degrees apart.
    COLORREF success = RGB(43, 136, 0);
    COLORREF warning = RGB(158, 109, 0);
    COLORREF error = RGB(228, 43, 0);
};

struct ThemePalette
{
    bool dark = false;
    ConsoleColors console;
    COLORREF background = RGB(244, 246, 249);
    COLORREF panel = RGB(255, 255, 255);
    COLORREF text = RGB(25, 35, 50);
    COLORREF border = RGB(170, 180, 195);
    COLORREF button = RGB(236, 240, 246);
    COLORREF hover = RGB(220, 231, 245);
    COLORREF pressed = RGB(205, 220, 240);
    COLORREF disabled = RGB(130, 140, 150);
    COLORREF accent = RGB(28, 85, 155);
    COLORREF accent_text = RGB(255, 255, 255);
};

ThemePalette theme_palette(ThemeChoice choice);
// Pure resolver lets tests cover both Follow Windows states without changing OS settings.
ThemePalette theme_palette(ThemeChoice choice, bool windows_dark);
COLORREF message_color(const ConsoleColors& colors, MessageKind kind);

inline int scale_pixels(int pixels, UINT dpi)
{
    return MulDiv(pixels, static_cast<int>(dpi), 96);
}
} // namespace cpplivelab
