#pragma once
#include "process/windows_support.hpp"
#include <objidl.h>
#include <objbase.h>
#include <gdiplus.h>
#include <filesystem>
#include <stdexcept>

namespace ui_test
{
// Render only a test-owned off-screen window. No desktop/window belonging to
// the user is captured or controlled. PNG output is an optional QA artifact.
inline void capture(HWND window, const std::filesystem::path& destination)
{
    RECT bounds{};
    GetWindowRect(window, &bounds);
    const auto dc = GetDC(window);
    const auto memory = CreateCompatibleDC(dc);
    const auto bitmap =
        CreateCompatibleBitmap(dc, bounds.right - bounds.left, bounds.bottom - bounds.top);
    const auto previous = SelectObject(memory, bitmap);
    const bool printed = PrintWindow(window, memory, 2) != FALSE;
    SelectObject(memory, previous);
    DeleteDC(memory);
    ReleaseDC(window, dc);
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartup(&token, &input, nullptr);
    Gdiplus::Status status = Gdiplus::GenericError;
    {
        Gdiplus::Bitmap image(bitmap, nullptr);
        const CLSID png{
            0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
        std::filesystem::create_directories(destination.parent_path());
        status = image.Save(destination.c_str(), &png, nullptr);
    }
    Gdiplus::GdiplusShutdown(token);
    DeleteObject(bitmap);
    if (!printed || status != Gdiplus::Ok)
    {
        throw std::runtime_error("Cannot render test-owned UI snapshot.");
    }
}
} // namespace ui_test
