#pragma once

#include "../src/appearance.hpp"
#include <richedit.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace theme_test
{
inline const char* theme_name(cpplivelab::ThemeChoice choice)
{
    return choice == cpplivelab::ThemeChoice::Dark ? "Dark" : "Light";
}

inline const char* semantic_name(cpplivelab::MessageKind kind)
{
    using cpplivelab::MessageKind;
    switch (kind)
    {
        case MessageKind::System:
            return "information";
        case MessageKind::Success:
            return "success";
        case MessageKind::Warning:
            return "warning";
        case MessageKind::Error:
            return "error";
        case MessageKind::ProgramError:
            return "stderr/error";
        default:
            return "neutral/stdout";
    }
}

inline std::string rgb(COLORREF color)
{
    return "RGB(" + std::to_string(GetRValue(color)) + "," + std::to_string(GetGValue(color)) +
           "," + std::to_string(GetBValue(color)) + ")";
}

inline std::string decimal(double value)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(6) << value;
    return output.str();
}

inline double contrast_ratio(COLORREF foreground, COLORREF background)
{
    // WCAG relative luminance: linearize sRGB before weighting the channels.
    const auto luminance = [](COLORREF color)
    {
        const auto linear = [](BYTE channel)
        {
            const double value = channel / 255.0;
            return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * linear(GetRValue(color)) + 0.7152 * linear(GetGValue(color)) +
               0.0722 * linear(GetBValue(color));
    };
    const double text = luminance(foreground);
    const double surface = luminance(background);
    return ((std::max)(text, surface) + 0.05) / ((std::min)(text, surface) + 0.05);
}

inline std::string context(cpplivelab::ThemeChoice theme, cpplivelab::MessageKind kind)
{
    return std::string(theme_name(theme)) + " " + semantic_name(kind);
}

inline std::string contrast_description(cpplivelab::ThemeChoice theme,
    cpplivelab::MessageKind kind,
    const cpplivelab::ConsoleColors& colors)
{
    const auto foreground = cpplivelab::message_color(colors, kind);
    return context(theme, kind) + ": foreground " + rgb(foreground) + ", background " +
           rgb(colors.background) + ", contrast " +
           decimal(contrast_ratio(foreground, colors.background)) + ":1 (minimum 4.5:1)";
}

inline void require_rendered_color(cpplivelab::ThemeChoice theme,
    cpplivelab::MessageKind kind,
    const cpplivelab::ConsoleColors& colors,
    const CHARFORMAT2W& format)
{
    const auto expected = cpplivelab::message_color(colors, kind);
    if (!(format.dwMask & CFM_COLOR) || (format.dwEffects & CFE_AUTOCOLOR) ||
        format.crTextColor != expected)
    {
        throw std::runtime_error(context(theme, kind) + " Rich Edit palette mismatch: expected " +
                                 rgb(expected) + ", actual " + rgb(format.crTextColor) + ", mask " +
                                 std::to_string(format.dwMask) + ", effects " +
                                 std::to_string(format.dwEffects));
    }
}
} // namespace theme_test
