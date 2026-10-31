#include "appearance/appearance.hpp"

#include <algorithm>

namespace cpplivelab
{
namespace
{
DWORD read_dword(HKEY root, const std::wstring& key, const wchar_t* value, DWORD fallback)
{
    DWORD result = fallback;
    DWORD bytes = sizeof(result);
    if (RegGetValueW(root, key.c_str(), value, RRF_RT_REG_DWORD, nullptr, &result, &bytes) !=
        ERROR_SUCCESS)
    {
        return fallback;
    }
    return result;
}
} // namespace

Preferences load_preferences(const std::wstring& key)
{
    Preferences result;
    const DWORD theme = read_dword(HKEY_CURRENT_USER, key, L"Theme", 2);
    result.theme = theme <= 2 ? static_cast<ThemeChoice>(theme) : ThemeChoice::FollowWindows;
    result.debounce_ms = (std::clamp)(
        read_dword(HKEY_CURRENT_USER, key, L"DebounceMilliseconds", 150), DWORD{100}, DWORD{5000});
    return result;
}

bool legacy_auto_build_enabled(const std::wstring& key)
{
    return read_dword(HKEY_CURRENT_USER, key, L"AutoBuild", 0) != 0;
}

bool save_preferences(const Preferences& preferences, const std::wstring& key)
{
    HKEY settings = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
            key.c_str(),
            0,
            nullptr,
            0,
            KEY_SET_VALUE,
            nullptr,
            &settings,
            nullptr) != ERROR_SUCCESS)
    {
        return false;
    }
    // Registry keys require RegCloseKey, not the CloseHandle resource owner.
    const auto write = [&](const wchar_t* name, DWORD value)
    {
        return RegSetValueExW(settings,
                   name,
                   0,
                   REG_DWORD,
                   reinterpret_cast<const BYTE*>(&value),
                   sizeof(value)) == ERROR_SUCCESS;
    };
    const bool saved = write(L"Theme", static_cast<DWORD>(preferences.theme)) &&
                       write(L"DebounceMilliseconds", preferences.debounce_ms);
    RegCloseKey(settings);
    return saved;
}

bool windows_uses_dark_theme()
{
    return read_dword(HKEY_CURRENT_USER,
               L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
               L"AppsUseLightTheme",
               1) == 0;
}

ThemePalette theme_palette(ThemeChoice choice)
{
    return theme_palette(choice, choice == ThemeChoice::FollowWindows && windows_uses_dark_theme());
}

ThemePalette theme_palette(ThemeChoice choice, bool windows_dark)
{
    ThemePalette result;
    result.dark =
        choice == ThemeChoice::Dark || (choice == ThemeChoice::FollowWindows && windows_dark);
    if (result.dark)
    {
        result.background = RGB(24, 27, 33);
        result.panel = RGB(32, 36, 43);
        result.text = RGB(235, 240, 245);
        result.border = RGB(80, 91, 108);
        result.button = RGB(45, 51, 61);
        result.hover = RGB(59, 68, 82);
        result.pressed = RGB(36, 42, 50);
        result.disabled = RGB(145, 153, 165);
        result.accent = RGB(38, 85, 140);
        result.console.background = result.panel;
        result.console.neutral = result.text;
        result.console.information = RGB(96, 165, 250); // #60A5FA
        result.console.success = RGB(74, 222, 128);     // #4ADE80
        result.console.warning = RGB(251, 191, 36);     // #FBBF24
        // A small white tint retains the customized red hue at >= 4.5:1.
        result.console.error = RGB(255, 68, 25);
    }
    return result;
}

COLORREF message_color(const ConsoleColors& colors, MessageKind kind)
{
    switch (kind)
    {
        case MessageKind::System:
            return colors.information;
        case MessageKind::Success:
            return colors.success;
        case MessageKind::Warning:
            return colors.warning;
        case MessageKind::Error:
        case MessageKind::ProgramError:
            return colors.error;
        default:
            return colors.neutral;
    }
}
} // namespace cpplivelab
