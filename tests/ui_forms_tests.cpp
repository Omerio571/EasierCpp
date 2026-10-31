#include "../src/project_dialogs.hpp"
#include "cpplivelab/workspace.hpp"
#include "workspace_test_support.hpp"
#include "ui_capture.hpp"
#include "../src/tool_workflows.hpp"
#include <fstream>
#include <iostream>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

bool shown(HWND window)
{
    return (GetWindowLongPtrW(window, GWL_STYLE) & WS_VISIBLE) != 0;
}

std::wstring text(HWND window)
{
    std::wstring value(GetWindowTextLengthW(window) + 1, L'\0');
    value.resize(GetWindowTextW(window, value.data(), static_cast<int>(value.size())));
    return value;
}

RECT bounds(HWND window)
{
    RECT result{};
    GetWindowRect(window, &result);
    return result;
}

void check_combo_alignment(cpplivelab::ProjectFormWindow& form, HWND combo)
{
    const auto dc = CreateCompatibleDC(nullptr);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 400;
    info.bmiHeader.biHeight = -32;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    void* pixels = nullptr;
    const auto bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    const auto old_bitmap = SelectObject(dc, bitmap);
    const auto old_font =
        SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(combo, WM_GETFONT, 0, 0)));
    DRAWITEMSTRUCT item{ODT_COMBOBOX,
        static_cast<UINT>(GetDlgCtrlID(combo)),
        0,
        ODA_DRAWENTIRE,
        0,
        combo,
        dc,
        {0, 0, 400, 32},
        0};
    form.appearance.draw_item(item);
    int left_text_pixels = 0;
    for (int y = 6; y < 26; ++y)
    {
        for (int x = 12; x < 75; ++x)
        {
            if (GetPixel(dc, x, y) == form.appearance.palette.text)
            {
                ++left_text_pixels;
            }
        }
    }
    SelectObject(dc, old_font);
    SelectObject(dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    require(left_text_pixels > 5,
        "Combo text moved to the center during selection instead of matching left-aligned repaint");
}

void check_edit_outline(cpplivelab::ProjectFormWindow& form, HWND edit)
{
    const auto rectangle = bounds(edit);
    const auto dc = CreateCompatibleDC(nullptr);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = rectangle.right - rectangle.left;
    info.bmiHeader.biHeight = -(rectangle.bottom - rectangle.top);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    void* pixels = nullptr;
    const auto bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    const auto previous = SelectObject(dc, bitmap);
    // PrintWindow refuses a child of a hidden test host. Send the native print
    // request directly, retaining the real subclass/text/border rendering path.
    SendMessageW(
        edit, WM_PRINT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT | PRF_NONCLIENT | PRF_ERASEBKGND);
    int curve_pixels = 0;
    const auto expected =
        GetFocus() == edit ? form.appearance.palette.accent : form.appearance.palette.border;
    for (int y = 1; y < 7; ++y)
    {
        for (int x = 1; x < 7; ++x)
        {
            if (GetPixel(dc, x, y) == expected)
            {
                ++curve_pixels;
            }
        }
    }
    SelectObject(dc, previous);
    DeleteObject(bitmap);
    DeleteDC(dc);
    if (curve_pixels == 0)
    {
        throw std::runtime_error(
            "Rounded edit corner missing: border pixels=" + std::to_string(curve_pixels) +
            "; expected COLORREF=" + std::to_string(expected));
    }
}

void check_forms(const std::filesystem::path& root, const std::filesystem::path& snapshots)
{
    using namespace cpplivelab;
    for (const auto theme : {ThemeChoice::Dark, ThemeChoice::Light})
    {
        std::wstring versions;
        for (int index = 0; index < 26; ++index)
        {
            versions += L"Release " + std::to_wstring(index) + L"\r\n";
        }
        std::vector<FormField> fields{{L"Available releases and tags", versions, {}, true}};
        fields[0].read_only = true;
        FormOptions options;
        options.theme = theme;
        ProjectFormWindow form(nullptr, L"Read-only version rendering", fields, options);
        const auto edit = GetDlgItem(form.window, 2000);
        const auto dc = GetDC(edit);
        SendMessageW(form.window,
            WM_CTLCOLORSTATIC,
            reinterpret_cast<WPARAM>(dc),
            reinterpret_cast<LPARAM>(edit));
        const bool opaque =
            GetBkMode(dc) == OPAQUE && GetBkColor(dc) == form.appearance.palette.panel;
        ReleaseDC(edit, dc);
        require(opaque,
            "Read-only multiline edit uses a transparent/static background and can ghost old text");
        SendMessageW(edit, EM_LINESCROLL, 0, 20);
        SendMessageW(edit, EM_LINESCROLL, 0, -20);
        if (!snapshots.empty())
        {
            ShowWindow(form.window, SW_SHOWNOACTIVATE);
            UpdateWindow(form.window);
            ui_test::capture(form.window,
                snapshots /
                    (theme == ThemeChoice::Dark ? L"versions-dark.png" : L"versions-light.png"));
        }
    }
    for (const auto* label : {L"Retrieve Versions", L"Approve Verification", L"Approve & Build"})
    {
        std::vector<FormField> fields;
        FormOptions options;
        options.accept_label = label;
        ProjectFormWindow form(nullptr, L"Action label regression", fields, options);
        const auto button = GetDlgItem(form.window, IDOK);
        const auto dc = GetDC(button);
        const auto font =
            SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(button, WM_GETFONT, 0, 0)));
        SIZE text_size{};
        GetTextExtentPoint32W(dc, label, static_cast<int>(wcslen(label)), &text_size);
        SelectObject(dc, font);
        ReleaseDC(button, dc);
        const auto rectangle = bounds(button);
        require(rectangle.right - rectangle.left >= text_size.cx + 16,
            "Confirmation button clips its meaningful action label");
    }
    for (const auto theme : {ThemeChoice::Dark, ThemeChoice::Light, ThemeChoice::FollowWindows})
    {
        std::vector<FormField> fields{{L"Project name (also the folder name)", L"SampleProject"},
            {L"Parent folder", root.wstring()},
            {L"Project template",
                L"Multi-file C++ Project",
                {L"Empty C++ Project", L"Multi-file C++ Project"}}};
        fields[1].browse = BrowseKind::Folder;
        fields[1].help = L"A new project folder is created inside the selected parent; existing "
                         L"folders are never overwritten.";
        fields[2].help = L"Multi-file separates declarations and implementations. Both templates "
                         L"are standard C++20/CMake.";
        FormOptions options;
        options.theme = theme;
        options.note =
            L"Creates a NEW folder. Libraries are optional and verified only after Build succeeds.";
        options.library_picker = true;
        options.accept_label = L"Create";
        ProjectFormWindow form(nullptr, L"New Project", fields, options);
        require(
            (GetWindowLongPtrW(GetDlgItem(form.window, 1001), GWL_STYLE) & SS_NOPREFIX) != 0 &&
                (GetWindowLongPtrW(GetDlgItem(form.window, 1002), GWL_STYLE) & SS_NOPREFIX) != 0,
            "Help/note controls consume literal ampersands as accelerators");
        check_combo_alignment(form, GetDlgItem(form.window, 2002));
        check_edit_outline(form, GetDlgItem(form.window, 2001));
        require(form.appearance.palette.console.error == theme_palette(theme).console.error &&
                    form.appearance.palette.console.success == theme_palette(theme).console.success,
            "A form replaced the shared customized palette");
        require(GetDlgItem(form.window, 5001) && GetDlgItem(form.window, 4001),
            "Browse/help controls missing");
        form.command(4001);
        require(text(GetDlgItem(form.window, 1002)) == fields[1].help,
            "Keyboard-usable help action lost its explanation");
        const auto original = bounds(form.window);
        require(form.select_library("fmt"),
            "Could not add first library through shared selection action");
        const auto first = bounds(form.window);
        require(!form.select_library("fmt") && !form.select_library("raymath"),
            "Duplicate/unsupported library accepted");
        require(
            form.select_library("nlohmann_json"), "Second library replaced the first selection");
        const auto second = bounds(form.window);
        const auto row1 = bounds(GetDlgItem(form.window, 6000));
        const auto row2 = bounds(GetDlgItem(form.window, 6001));
        const auto add = bounds(GetDlgItem(form.window, 1600));
        require(options.libraries.size() == 2 &&
                    first.bottom - first.top > original.bottom - original.top &&
                    second.bottom - second.top > first.bottom - first.top &&
                    row1.bottom <= row2.top && row2.bottom <= add.top,
            "Library rows do not grow independently above Add Library");
        require(GetDlgItem(form.window, 7000) && GetDlgItem(form.window, 7001),
            "Per-selection Remove controls missing");
        if (!snapshots.empty() && theme != ThemeChoice::FollowWindows)
        {
            ShowWindow(form.window, SW_SHOWNOACTIVATE);
            UpdateWindow(form.window);
            ui_test::capture(form.window,
                snapshots / (theme == ThemeChoice::Dark ? L"new-project-dark.png"
                                                        : L"new-project-light.png"));
        }
        form.command(7000);
        require(options.libraries.size() == 1 && options.libraries[0] == "nlohmann_json" &&
                    !GetDlgItem(form.window, 6001),
            "Removing a selected row removed another library or left a stale row");
        form.dpi = 144;
        form.apply_theme();
        form.layout();
        require(bounds(GetDlgItem(form.window, 1600)).top >=
                    bounds(GetDlgItem(form.window, 6000)).bottom,
            "DPI change overlaps library rows/Add action");
        form.dpi = 192;
        form.apply_theme();
        form.layout();
        form.ensure_visible(GetDlgItem(form.window, IDOK));
        RECT client{};
        GetClientRect(form.window, &client);
        MapWindowPoints(form.window, nullptr, reinterpret_cast<POINT*>(&client), 2);
        const auto accept = bounds(GetDlgItem(form.window, IDOK));
        require(accept.top >= client.top && accept.bottom <= client.bottom,
            "High-DPI form cannot scroll its keyboard accept action into view");
        form.command(IDCANCEL);
        require(
            form.finished && !form.accepted, "Cancel incorrectly accepts new-project selections");
    }
    for (const auto theme : {ThemeChoice::Dark, ThemeChoice::Light})
    {
        std::vector<FormField> settings{{L"Program working directory", root.wstring()},
            {L"CMake configure preset", L"default"},
            {L"vcpkg root", root.wstring()}};
        settings[0].browse = settings[2].browse = BrowseKind::Folder;
        settings[0].help = L"Relative assets resolve from this folder.";
        settings[1].advanced = settings[2].advanced = true;
        FormOptions options;
        options.theme = theme;
        ProjectFormWindow form(nullptr, L"Project Settings", settings, options);
        const auto resources = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
        for (UINT scale : {96U, 144U, 192U})
        {
            form.dpi = scale;
            form.apply_theme();
            for (int repeat = 0; repeat < 12; ++repeat)
            {
                form.command(1500);
                const bool expanded = options.expanded;
                require(shown(GetDlgItem(form.window, 2001)) == expanded,
                    "Project Settings advanced field visibility disagrees with expansion");
                RECT previous{};
                bool first = true;
                for (int id : {2000, 2001, 2002, 1500, IDOK})
                {
                    const auto child = GetDlgItem(form.window, id);
                    if (!shown(child))
                    {
                        continue;
                    }
                    const auto current = bounds(child);
                    require(first || previous.bottom <= current.top,
                        "Project Settings fields/actions overlap after repeated expansion or DPI "
                        "change");
                    previous = current;
                    first = false;
                }
                require(GetDlgItem(form.window, IDOK) && GetDlgItem(form.window, IDCANCEL),
                    "Project Settings lost its unique confirmation controls");
                RedrawWindow(form.window,
                    nullptr,
                    nullptr,
                    RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
            }
        }
        require(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) <= resources + 4,
            "Advanced toggles leaked GDI resources");
        form.dpi = 96;
        form.apply_theme();
        form.command(1500);
        if (!snapshots.empty())
        {
            ShowWindow(form.window, SW_SHOWNOACTIVATE);
            UpdateWindow(form.window);
            ui_test::capture(form.window,
                snapshots / (theme == ThemeChoice::Dark ? L"settings-expanded-dark.png"
                                                        : L"settings-expanded-light.png"));
        }
    }
    Toolchain tools{{{"cmake", L"C:/installed/cmake.exe", {}}, {"cl", L"C:/installed/cl.exe", {}}},
        L"C:/VS/VsDevCmd.bat"};
    const auto summary = diagnostics_summary(tools);
    require(summary.find("MSVC: Found") != summary.npos &&
                summary.find("Not checked") != summary.npos &&
                summary.find("C:/installed") == summary.npos &&
                summary.find("Ready") == summary.npos,
        "Diagnostics summary confuses discovered paths with compatibility verification");
    {
        std::vector<FormField> fields{{L"Detailed help", L""}};
        fields[0].help =
            std::wstring(12, L'\n') + L"The last line must remain visible after resizing.";
        FormOptions options;
        ProjectFormWindow form(nullptr, L"Long explanation regression", fields, options);
        form.command(4000);
        const auto help = bounds(GetDlgItem(form.window, 1002));
        require(
            help.bottom - help.top > 200, "Long help was clipped into the old fixed-height area");
        const auto accept = bounds(GetDlgItem(form.window, IDOK));
        require(help.bottom < accept.top, "Expanded help overlaps the confirmation actions");
    }
    std::vector<FormField> editor{
        {L"Editor", L"Visual Studio Code", {L"Visual Studio Code", L"Custom"}},
        {L"Executable override", L""},
        {L"Arguments", L"", {}, true}};
    editor[1].advanced = editor[2].advanced = true;
    editor[1].browse = BrowseKind::Executable;
    editor[2].help = L"{project} is one quoted folder argument.";
    FormOptions editor_options;
    editor_options.custom_editor_field = 0;
    ProjectFormWindow editor_form(nullptr, L"Preferred Editor", editor, editor_options);
    require(!shown(GetDlgItem(editor_form.window, 2001)),
        "Advanced editor override is visible for ordinary choices");
    editor_form.command(1500);
    require(shown(GetDlgItem(editor_form.window, 2001)),
        "Advanced Options cannot reveal executable override");
    editor_form.command(1500);
    SendDlgItemMessageW(editor_form.window, 2000, CB_SETCURSEL, 1, 0);
    editor_form.command(2000, CBN_SELCHANGE);
    require(
        shown(GetDlgItem(editor_form.window, 2001)) && shown(GetDlgItem(editor_form.window, 2002)),
        "Custom editor does not automatically reveal advanced fields");
    editor_options.validate = [](const auto&)
    {
        return L"Actionable validation failure";
    };
    editor_form.command(IDOK);
    require(!editor_form.finished &&
                text(GetDlgItem(editor_form.window, 1002)) == L"Actionable validation failure",
        "Form validation closed the window or hid the error");
    {
        std::vector<std::wstring> choices;
        for (int index = 0; index < 24; ++index)
        {
            choices.push_back(L"Fixture::Module" + std::to_wstring(index));
        }
        std::vector<FormField> targets{{L"Public targets", L"", choices}};
        targets[0].multiple_selection = true;
        targets.push_back({L"Selected library details",
            L"Repository\r\nVersion\r\nTargets\r\nReferences",
            {},
            true});
        targets[1].read_only = true;
        targets[1].multiline_height = 150;
        FormOptions target_options;
        ProjectFormWindow target_form(
            nullptr, L"Target selector regression", targets, target_options);
        const auto list = GetDlgItem(target_form.window, 2000);
        const auto detail_bounds = bounds(GetDlgItem(target_form.window, 2001));
        require(detail_bounds.bottom - detail_bounds.top == scale_pixels(150, target_form.dpi),
            "Library details height was reduced to the old three-line field");
        require((GetWindowLongPtrW(list, GWL_STYLE) & WS_VSCROLL) != 0 &&
                    SendMessageW(list, LB_GETCOUNT, 0, 0) == 24 &&
                    SendMessageW(list, LB_GETSELCOUNT, 0, 0) == 0,
            "Target list is truncated, cannot scroll, or silently selects a default");
        SendMessageW(list, LB_SETSEL, TRUE, 9);
        SendMessageW(list, LB_SETSEL, TRUE, 23);
        target_form.command(IDOK);
        require(
            target_form.accepted && targets[0].value == L"Fixture::Module9\nFixture::Module23\n",
            "Multiple selected targets were not preserved");
    }
    const auto project = root / L"Recent project";
    std::filesystem::create_directory(project);
    std::ofstream(project / L"CMakeLists.txt") << "project(ReadOnly)\n";
    remember_project(project);
    std::vector<FormField> empty;
    FormOptions recent;
    recent.recent_list = true;
    recent.recents = {root / L"Missing project", project};
    int forget_count = 0;
    recent.forget_recent = [&](const auto& path)
    {
        ++forget_count;
        remember_project(path, true);
    };
    ProjectFormWindow recent_form(nullptr, L"Recent Projects", empty, recent);
    recent_form.command(IDOK);
    require(!recent_form.finished, "Missing recent project opened without a diagnostic");
    recent_form.command(1701);
    require(forget_count == 1, "One Remove from Recent command executed more than once");
    require(
        recent.recents.size() == 1 && std::filesystem::is_regular_file(project / L"CMakeLists.txt"),
        "Remove from Recent deleted project data");
    recent_form.command(IDOK);
    require(recent_form.accepted && recent.selected_recent == project,
        "Direct recent Open action selected the wrong folder");
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int result = 0;
    try
    {
        workspace_test::Fixture fixture;
        check_forms(
            fixture.root, argc == 2 ? std::filesystem::path(argv[1]) : std::filesystem::path{});
        std::cout << "Shared forms: themes, multiple libraries, Browse/help, advanced options, "
                     "validation and recents passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    if (SUCCEEDED(com))
    {
        CoUninitialize();
    }
    return result;
}
