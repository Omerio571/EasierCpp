#include "../src/console_view.hpp"

#include <iostream>
#include <stdexcept>

int main()
{
    try
    {
        const auto library = LoadLibraryW(L"Msftedit.dll");
        if (!library)
        {
            throw std::runtime_error("Rich Edit unavailable");
        }
        // This hidden test window owns only test controls. No user app or focus is touched.
        HWND parent = CreateWindowExW(0,
            L"STATIC",
            L"CppLiveLab console test",
            WS_OVERLAPPED,
            0,
            0,
            600,
            400,
            nullptr,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);
        if (!parent)
        {
            throw std::runtime_error("Cannot create console test host");
        }
        cpplivelab::ConsolePane program, system;
        program.create(parent, 101, static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
        system.create(parent, 102, static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
        MoveWindow(program.window, 0, 0, 300, 200, FALSE);
        MoveWindow(system.window, 300, 0, 300, 200, FALSE);
        const auto require = [](bool ok, const char* message)
        {
            if (!ok)
            {
                throw std::runtime_error(message);
            }
        };
        program.append(cpplivelab::MessageKind::ProgramOutput, L"Name: ");
        require(GetWindowTextLengthW(program.window) == 6,
            "Prompt without newline was not rendered immediately");
        system.append(cpplivelab::MessageKind::Error, L"[ERROR] example.cpp(1): error C1234\n");
        system.set_visible(false);
        system.append(cpplivelab::MessageKind::System, L"Background update\n");
        system.set_colors(cpplivelab::ConsoleColors{});
        require((GetWindowLongPtrW(system.window, GWL_STYLE) & WS_VISIBLE) == 0,
            "Background append or theme change exposed an inactive console tab");
        for (int line = 0; line < 100; ++line)
        {
            system.append(cpplivelab::MessageKind::Build, L"background build line\n");
        }
        system.set_visible(true);
        POINT latest{};
        SendMessageW(system.window, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&latest));
        require(latest.y > 0, "Background tab lost its follow-tail position");
        require(system.history.unread_error(), "Inactive system console lacked an error badge");
        require(
            GetWindowTextLengthW(program.window) == 6, "Build error leaked into Program Console");

        SendMessageW(system.window, EM_SETSEL, 0, 7);
        CHARFORMAT2W format{};
        format.cbSize = sizeof(format);
        SendMessageW(
            system.window, EM_GETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
        if (format.crTextColor != RGB(175, 35, 50))
        {
            throw std::runtime_error(
                "Actual Rich Edit error color was not red: " + std::to_string(format.crTextColor) +
                " mask " + std::to_string(format.dwMask));
        }
        system.clear();
        require(
            GetWindowTextLengthW(system.window) == 0 && GetWindowTextLengthW(program.window) == 6,
            "Clearing one console cleared both");

        CHARRANGE selected{0, 4};
        SendMessageW(program.window, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&selected));
        program.append(cpplivelab::MessageKind::ProgramOutput, L"Ada\n");
        CHARRANGE after{};
        SendMessageW(program.window, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&after));
        require(after.cpMin == selected.cpMin && after.cpMax == selected.cpMax,
            "Incoming output destroyed text selection");
        program.clear();
        for (int line = 0; line < 250; ++line)
        {
            program.append(cpplivelab::MessageKind::ProgramOutput, L"scroll test output\n");
        }
        POINT top{};
        SendMessageW(program.window, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&top));
        selected = {0, 4};
        SendMessageW(program.window, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&selected));
        program.append(cpplivelab::MessageKind::ProgramOutput, L"background output\n");
        POINT position{};
        SendMessageW(program.window, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&position));
        require(position.y == 0, "Reviewing earlier output jumped to the bottom");
        for (int chunk = 0; chunk < 150; ++chunk)
        {
            program.append(cpplivelab::MessageKind::ProgramOutput, std::wstring(4096, L'x'));
        }
        require(program.history.characters() <= 400000 &&
                    GetWindowTextLengthW(program.window) <= 400000,
            "Native console history grew without a bound");
        cpplivelab::ConsoleColors dark;
        dark.background = RGB(30, 35, 42);
        dark.neutral = RGB(235, 240, 245);
        const auto retained = GetWindowTextLengthW(program.window);
        program.set_colors(dark);
        require(GetWindowTextLengthW(program.window) == retained, "Recoloring discarded output");
        DestroyWindow(parent);
        FreeLibrary(library);
        std::cout << "Native console: prompt, separation, severity, clear, selection, scroll, "
                     "bounded history and recolor passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
