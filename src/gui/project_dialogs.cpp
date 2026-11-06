#include "gui/project_dialogs.hpp"
#include "cpplivelab/workspace.hpp"
#include "cpplivelab/session.hpp"
#include <shobjidl.h>
#include <algorithm>
#include <stdexcept>
#include <initguid.h>
#include <oleacc.h>
#include <uxtheme.h>
#include <sstream>

namespace cpplivelab
{
namespace
{
constexpr int Heading = 1000, Note = 1001, HelpText = 1002, Advanced = 1500;
constexpr int AddLibrary = 1600, RecentList = 1700, RemoveRecent = 1701;
constexpr int CustomAction = 1800;
constexpr UINT RequestInlineClose = WM_APP + 1;

std::wstring text(HWND control)
{
    std::wstring value(GetWindowTextLengthW(control) + 1, L'\0');
    value.resize(GetWindowTextW(control, value.data(), static_cast<int>(value.size())));
    return value;
}
} // namespace

std::optional<std::filesystem::path> browse_path(
    HWND owner, BrowseKind kind, const std::filesystem::path& initial)
{
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(
            CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
    {
        throw std::runtime_error("Windows file/folder selection is unavailable.");
    }
    DWORD flags = 0;
    dialog->GetOptions(&flags);
    dialog->SetOptions(flags | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST |
                       (kind == BrowseKind::Folder ? FOS_PICKFOLDERS : FOS_FILEMUSTEXIST));
    COMDLG_FILTERSPEC filter{L"Executable programs", L"*.exe"};
    if (kind == BrowseKind::Executable)
    {
        dialog->SetFileTypes(1, &filter);
    }
    if (!initial.empty())
    {
        IShellItem* folder = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(initial.c_str(), nullptr, IID_PPV_ARGS(&folder))))
        {
            dialog->SetFolder(folder);
            folder->Release();
        }
    }
    std::optional<std::filesystem::path> result;
    if (SUCCEEDED(dialog->Show(owner)))
    {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item)))
        {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
            {
                result = path;
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dialog->Release();
    return result;
}

HWND ProjectFormWindow::add(const wchar_t* type, const std::wstring& label, int id, DWORD style)
{
    if (std::wstring_view(type) == L"STATIC")
    {
        // Explanations contain literal names such as "Build & System"; they
        // are not accelerator labels and must not consume ampersands.
        style |= SS_NOPREFIX;
    }
    if (std::wstring_view(type) == L"COMBOBOX")
    {
        style |= CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL;
    }
    const auto control = CreateWindowW(type,
        label.c_str(),
        WS_CHILD | WS_VISIBLE | style,
        0,
        0,
        0,
        0,
        window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr),
        nullptr);
    if (!control)
    {
        throw std::runtime_error("Cannot create a project form control.");
    }
    return control;
}

ProjectFormWindow::ProjectFormWindow(
    HWND owner, const std::wstring& title, std::vector<FormField>& fields, FormOptions& options)
    : fields_(fields), options_(options)
{
    WNDCLASSW type{};
    type.lpfnWndProc = procedure;
    type.hInstance = GetModuleHandleW(nullptr);
    type.lpszClassName = L"CppLiveLabProjectForm";
    type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&type);
    dpi = owner ? GetDpiForWindow(owner) : 96;
    const bool embedded = options.inline_host != nullptr;
    RECT host{};
    if (embedded)
    {
        GetClientRect(options.inline_host, &host);
        manually_sized_ = true;
        preferred_width_ = MulDiv(host.right, 96, dpi);
        preferred_height_ = MulDiv(host.bottom, 96, dpi);
    }
    window = CreateWindowExW(WS_EX_CONTROLPARENT | (embedded ? 0 : WS_EX_DLGMODALFRAME),
        type.lpszClassName,
        title.c_str(),
        WS_VSCROLL | WS_CLIPCHILDREN |
            (embedded ? WS_CHILD : WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_THICKFRAME),
        embedded ? 0 : -30000,
        embedded ? 0 : -30000,
        embedded ? host.right : scale_pixels(640, dpi),
        embedded ? host.bottom : scale_pixels(400, dpi),
        embedded ? options.inline_host : owner,
        nullptr,
        type.hInstance,
        this);
    if (!window)
    {
        throw std::runtime_error("Cannot open project settings form.");
    }
    CoCreateInstance(
        CLSID_AccPropServices, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&accessibility_));
    add(L"STATIC", title, Heading);
    add(L"STATIC", options.note, Note);
    add(L"STATIC", L"", HelpText);
    for (size_t index = 0; index < fields.size(); ++index)
    {
        auto& field = fields[index];
        const auto id = static_cast<int>(index);
        add(L"STATIC", field.label, 3000 + id, SS_NOPREFIX);
        const auto control =
            add(field.checkbox             ? L"BUTTON"
                : field.multiple_selection ? L"LISTBOX"
                                           : (field.choices.empty() ? L"EDIT" : L"COMBOBOX"),
                field.checkbox ? field.label : field.value,
                2000 + id,
                WS_TABSTOP | (field.checkbox ? BS_AUTOCHECKBOX
                                 : field.multiple_selection
                                     ? (LBS_EXTENDEDSEL | LBS_NOTIFY | WS_VSCROLL | WS_BORDER)
                                     : (field.choices.empty()
                                               ? (field.multiline ? ES_MULTILINE | ES_AUTOVSCROLL |
                                                                        ES_WANTRETURN | WS_VSCROLL
                                                                  : ES_AUTOHSCROLL)
                                               : CBS_DROPDOWNLIST)));
        if (field.checkbox)
        {
            SendMessageW(
                control, BM_SETCHECK, field.value == L"Yes" ? BST_CHECKED : BST_UNCHECKED, 0);
        }
        if (!field.placeholder.empty() && field.choices.empty())
        {
            SendMessageW(control,
                EM_SETCUEBANNER,
                FALSE,
                reinterpret_cast<LPARAM>(field.placeholder.c_str()));
        }
        if (field.read_only)
        {
            SendMessageW(control, EM_SETREADONLY, TRUE, 0);
        }
        for (size_t item = 0; item < field.choices.size(); ++item)
        {
            SendMessageW(control,
                field.multiple_selection ? LB_ADDSTRING : CB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(field.choices[item].c_str()));
            if (field.multiple_selection)
            {
                std::wistringstream selected(field.value);
                for (std::wstring value; std::getline(selected, value);)
                {
                    if (value == field.choices[item])
                    {
                        SendMessageW(control, LB_SETSEL, TRUE, item);
                    }
                }
            }
            else if (field.choices[item] == field.value)
            {
                SendMessageW(control, CB_SETCURSEL, item, 0);
            }
        }
        if (!field.help.empty())
        {
            const auto help = add(L"BUTTON", L"i", 4000 + id, WS_TABSTOP);
            if (accessibility_)
            {
                const auto name = L"Help: " + field.label;
                accessibility_->SetHwndPropStr(
                    help, OBJID_CLIENT, CHILDID_SELF, PROPID_ACC_NAME, name.c_str());
            }
        }
        if (field.browse != BrowseKind::None)
        {
            add(L"BUTTON", L"Browse...", 5000 + id, WS_TABSTOP);
        }
    }
    if (std::any_of(fields.begin(),
            fields.end(),
            [](const auto& field)
            {
                return field.advanced;
            }))
    {
        add(L"BUTTON", L"Advanced Options...", Advanced, WS_TABSTOP);
        add(L"STATIC", L"Basic", 1510);
        add(L"STATIC", L"Advanced", 1511);
    }
    if (options.library_picker)
    {
        add(L"STATIC", L"Optional libraries", 1601);
        add(L"BUTTON", L"Add Library...", AddLibrary, WS_TABSTOP);
        refresh_libraries();
    }
    if (options.recent_list)
    {
        add(L"LISTBOX",
            L"Recent projects",
            RecentList,
            WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS);
        add(L"BUTTON", L"Remove from Recent", RemoveRecent, WS_TABSTOP);
        refresh_recents();
    }
    add(L"BUTTON", options.accept_label, IDOK, WS_TABSTOP | BS_DEFPUSHBUTTON);
    if (options.action)
    {
        add(L"BUTTON", options.action_label, CustomAction, WS_TABSTOP);
    }
    add(L"BUTTON", L"Cancel", IDCANCEL, WS_TABSTOP);
    if (options.disable_invalid_accept && options.validate)
    {
        EnableWindow(GetDlgItem(window, IDOK), options.validate(fields).empty());
    }
    if (options.recent_list)
    {
        refresh_recents();
    }
    tooltip_ = CreateWindowExW(WS_EX_TOPMOST,
        TOOLTIPS_CLASSW,
        nullptr,
        WS_POPUP | TTS_ALWAYSTIP,
        0,
        0,
        0,
        0,
        window,
        nullptr,
        type.hInstance,
        nullptr);
    SendMessageW(tooltip_, TTM_SETMAXTIPWIDTH, 0, scale_pixels(420, dpi));
    for (size_t index = 0; index < fields.size(); ++index)
    {
        if (!fields[index].help.empty())
        {
            TOOLINFOW tip{};
            tip.cbSize = TTTOOLINFOW_V2_SIZE;
            tip.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
            tip.hwnd = window;
            tip.uId =
                reinterpret_cast<UINT_PTR>(GetDlgItem(window, 4000 + static_cast<int>(index)));
            tip.lpszText = fields[index].help.data();
            SendMessageW(tooltip_, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tip));
        }
    }
    apply_theme();
    layout();
    if (embedded && options.confirm_inline_discard)
    {
        read_fields();
        for (const auto& field : fields_)
        {
            initial_values_.push_back(field.value);
        }
        initial_libraries_ = options.libraries;
    }
}

ProjectFormWindow::~ProjectFormWindow()
{
    close_native_window();
    if (font_)
    {
        DeleteObject(font_);
    }
}

void ProjectFormWindow::close_native_window()
{
    if (accessibility_)
    {
        const MSAAPROPID property = PROPID_ACC_NAME;
        for (size_t index = 0; index < fields_.size(); ++index)
        {
            if (const auto help = GetDlgItem(window, 4000 + static_cast<int>(index)))
            {
                accessibility_->ClearHwndProps(help, OBJID_CLIENT, CHILDID_SELF, &property, 1);
            }
        }
        accessibility_->Release();
        accessibility_ = nullptr;
    }
    if (IsWindow(window))
    {
        DestroyWindow(window);
    }
}

void ProjectFormWindow::apply_theme()
{
    const auto old = font_;
    font_ = CreateFontW(-scale_pixels(16, dpi),
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH,
        L"Segoe UI");
    for (auto child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
    {
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font_), FALSE);
    }
    if (old)
    {
        DeleteObject(old);
    }
    appearance.apply(window, options_.theme, dpi, true);
    SetWindowTheme(tooltip_, L"", nullptr);
    SendMessageW(tooltip_, TTM_SETTIPBKCOLOR, appearance.palette.panel, 0);
    SendMessageW(tooltip_, TTM_SETTIPTEXTCOLOR, appearance.palette.text, 0);
    SendDlgItemMessageW(window, RecentList, LB_SETITEMHEIGHT, 0, scale_pixels(58, dpi));
}

void ProjectFormWindow::layout()
{
    if (layout_in_progress_)
    {
        return;
    }

    struct Guard
    {
        bool& active;

        ~Guard()
        {
            active = false;
        }
    } guard{layout_in_progress_};

    layout_in_progress_ = true;

    struct Placement
    {
        HWND control;
        int x, y, width, height;
        bool visible;
    };

    std::vector<Placement> placements;
    const int width = (std::max)(options_.inline_host ? 340 : 520, preferred_width_);
    const int content_width = width - 40;
    int y = 18;
    const auto place = [&](int id, int x, int top, int w, int h, bool visible = true)
    {
        if (auto control = GetDlgItem(window, id))
        {
            placements.push_back({control, x, top, w, h, visible});
        }
    };
    place(Heading, 20, y, content_width, 24);
    y += 32;
    const auto wrapped_height = [&](const std::wstring& value, int minimum)
    {
        const auto dc = GetDC(window);
        const auto previous = SelectObject(dc, font_);
        RECT measured{0, 0, scale_pixels(content_width, dpi), 0};
        DrawTextW(dc, value.c_str(), -1, &measured, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        SelectObject(dc, previous);
        ReleaseDC(window, dc);
        return (std::max)(minimum, MulDiv(measured.bottom, 96, static_cast<int>(dpi)) + 4);
    };
    if (!options_.note.empty())
    {
        const int height = wrapped_height(options_.note, 24);
        place(Note, 20, y, content_width, height);
        y += height + 8;
    }
    else
    {
        place(Note, 0, 0, 0, 0, false);
    }
    const bool custom = options_.custom_editor_field >= 0 &&
                        text(GetDlgItem(window, 2000 + options_.custom_editor_field)) == L"Custom";
    bool advanced_heading = false;
    const bool grouped = std::any_of(fields_.begin(),
        fields_.end(),
        [](const auto& field)
        {
            return field.advanced;
        });
    for (size_t index = 0; index < fields_.size(); ++index)
    {
        const auto& field = fields_[index];
        const bool visible = !field.advanced || options_.expanded || custom;
        if (index == 0 && grouped)
        {
            place(1510, 20, y, content_width, 24);
            y += 30;
        }
        if (field.advanced && !advanced_heading)
        {
            place(1511, 20, y, content_width, 24, visible);
            if (visible)
            {
                y += 30;
            }
            advanced_heading = true;
        }
        const int id = static_cast<int>(index);
        if (field.checkbox)
        {
            place(3000 + id, 0, 0, 0, 0, false);
            place(2000 + id, 20, y, content_width - 34, 32, visible);
            place(4000 + id, width - 48, y + 4, 24, 24, visible);
            if (visible)
            {
                y += 44;
            }
            continue;
        }
        place(3000 + id, 20, y, content_width - 34, 22, visible);
        place(4000 + id, width - 48, y - 1, 24, 24, visible);
        const bool browse = field.browse != BrowseKind::None;
        place(2000 + id,
            20,
            y + 26,
            content_width - (browse ? 104 : 0),
            field.multiple_selection
                ? 150
                : (field.choices.empty() ? (field.multiline ? field.multiline_height : 32) : 210),
            visible);
        place(5000 + id, width - 114, y + 26, 94, 32, visible);
        if (visible)
        {
            y += field.multiple_selection ? 192
                                          : (field.multiline ? field.multiline_height + 38 : 72);
        }
    }
    if (GetDlgItem(window, Advanced))
    {
        place(Advanced, 20, y, (std::min)(280, content_width), 32);
        SetDlgItemTextW(window,
            Advanced,
            custom              ? L"Advanced Options (Custom editor)"
            : options_.expanded ? L"Hide Advanced Options"
                                : L"Advanced Options...");
        EnableWindow(GetDlgItem(window, Advanced), !custom);
        y += 44;
    }
    if (options_.library_picker)
    {
        place(1601, 20, y, content_width, 22);
        y += 28;
        for (size_t index = 0; index < options_.libraries.size(); ++index)
        {
            place(6000 + static_cast<int>(index), 20, y, content_width - 116, 28);
            place(7000 + static_cast<int>(index), width - 120, y - 2, 100, 32);
            y += 40;
        }
        place(AddLibrary, 20, y, 180, 32);
        y += 44;
    }
    if (options_.recent_list)
    {
        place(RecentList, 20, y, content_width, 240);
        y += 252;
        place(RemoveRecent, 20, y, 208, 32);
        y += 44;
    }
    if (options_.action)
    {
        place(CustomAction, 20, y, content_width, 34);
        y += 44;
    }
    const int help_height = wrapped_height(text(GetDlgItem(window, HelpText)), 64);
    place(HelpText, 20, y, content_width, help_height);
    y += help_height + 8;
    const auto action_dc = GetDC(window);
    const auto previous_font = SelectObject(action_dc, font_);
    SIZE action_text{};
    GetTextExtentPoint32W(action_dc,
        options_.accept_label.c_str(),
        static_cast<int>(options_.accept_label.size()),
        &action_text);
    SelectObject(action_dc, previous_font);
    ReleaseDC(window, action_dc);
    const int accept_width = (std::clamp)(
        MulDiv(action_text.cx, 96, static_cast<int>(dpi)) + 32, 128, content_width - 112);
    place(IDOK, width - 132 - accept_width, y, accept_width, 34, !options_.progress_only);
    place(IDCANCEL, width - 120, y, 100, 34);
    content_height_ = y + 52;
    MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor);
    const int available = (std::max)(
        240, MulDiv(monitor.rcWork.bottom - monitor.rcWork.top, 96, static_cast<int>(dpi)) - 80);
    viewport_height_ =
        options_.inline_host
            ? preferred_height_
            : (std::max)(200,
                  (std::min)(manually_sized_ ? preferred_height_ : content_height_, available));
    scroll_offset_ =
        (std::clamp)(scroll_offset_, 0, (std::max)(0, content_height_ - viewport_height_));
    // Plan first, then move all children without intermediate paints. The old
    // implementation left old parent-drawn borders because it never erased them.
    const bool visible = IsWindowVisible(window) != FALSE;
    if (visible)
    {
        SendMessageW(window, WM_SETREDRAW, FALSE, 0);
    }
    SCROLLINFO scroll{sizeof(scroll),
        SIF_RANGE | SIF_PAGE | SIF_POS,
        0,
        content_height_ - 1,
        static_cast<UINT>(viewport_height_),
        scroll_offset_,
        0};
    SetScrollInfo(window, SB_VERT, &scroll, TRUE);
    RECT desired{0, 0, scale_pixels(width, dpi), scale_pixels(viewport_height_, dpi)};
    const auto style = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE));
    AdjustWindowRectExForDpi(
        &desired, style, FALSE, WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, dpi);
    RECT previous{};
    GetWindowRect(window, &previous);
    const int outer_width = desired.right - desired.left,
              outer_height = desired.bottom - desired.top;
    const int x = (std::max)(
        monitor.rcWork.left, (std::min)(previous.left, monitor.rcWork.right - outer_width));
    const int top = (std::max)(
        monitor.rcWork.top, (std::min)(previous.top, monitor.rcWork.bottom - outer_height));
    // Test windows remain off-screen; only already visible user forms are clamped.
    if (!options_.inline_host)
    {
        SetWindowPos(window,
            nullptr,
            visible ? x : previous.left,
            visible ? top : previous.top,
            outer_width,
            outer_height,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
    }
    auto deferred = BeginDeferWindowPos(static_cast<int>(placements.size()));
    for (const auto& item : placements)
    {
        SetWindowRgn(item.control, nullptr, FALSE);
        const auto flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW |
                           (item.visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW);
        if (deferred)
        {
            deferred = DeferWindowPos(deferred,
                item.control,
                nullptr,
                scale_pixels(item.x, dpi),
                scale_pixels(item.y - scroll_offset_, dpi),
                scale_pixels(item.width, dpi),
                scale_pixels(item.height, dpi),
                flags);
        }
        else
        {
            SetWindowPos(item.control,
                nullptr,
                scale_pixels(item.x, dpi),
                scale_pixels(item.y - scroll_offset_, dpi),
                scale_pixels(item.width, dpi),
                scale_pixels(item.height, dpi),
                flags);
        }
    }
    const bool positioned = deferred && EndDeferWindowPos(deferred);
    if (!positioned)
    {
        // A failed batch discards earlier entries too. Reapply the complete plan,
        // rather than leaving the first controls at their previous positions.
        for (const auto& item : placements)
        {
            SetWindowPos(item.control,
                nullptr,
                scale_pixels(item.x, dpi),
                scale_pixels(item.y - scroll_offset_, dpi),
                scale_pixels(item.width, dpi),
                scale_pixels(item.height, dpi),
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW |
                    (item.visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
        }
    }
    appearance.round_controls();
    if (visible)
    {
        SendMessageW(window, WM_SETREDRAW, TRUE, 0);
    }
    RedrawWindow(window,
        nullptr,
        nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

void ProjectFormWindow::read_fields()
{
    for (size_t index = 0; index < fields_.size(); ++index)
    {
        const auto control = GetDlgItem(window, 2000 + static_cast<int>(index));
        if (fields_[index].checkbox)
        {
            fields_[index].value =
                SendMessageW(control, BM_GETCHECK, 0, 0) == BST_CHECKED ? L"Yes" : L"No";
        }
        else if (fields_[index].multiple_selection)
        {
            fields_[index].value.clear();
            for (size_t item = 0; item < fields_[index].choices.size(); ++item)
            {
                if (SendMessageW(control, LB_GETSEL, item, 0) > 0)
                {
                    fields_[index].value += fields_[index].choices[item] + L"\n";
                }
            }
        }
        else
        {
            fields_[index].value = text(control);
        }
    }
}

void ProjectFormWindow::ensure_visible(HWND control)
{
    if (!IsChild(window, control))
    {
        return;
    }
    RECT bounds{}, client{};
    GetWindowRect(control, &bounds);
    GetClientRect(window, &client);
    MapWindowPoints(window, nullptr, reinterpret_cast<POINT*>(&client), 2);
    const int delta = bounds.bottom > client.bottom ? bounds.bottom - client.bottom + 8
                      : bounds.top < client.top     ? bounds.top - client.top - 8
                                                    : 0;
    if (delta)
    {
        scroll_offset_ = (std::clamp)(scroll_offset_ + MulDiv(delta, 96, static_cast<int>(dpi)),
            0,
            (std::max)(0, content_height_ - viewport_height_));
        layout();
    }
}

void ProjectFormWindow::show_help(const std::wstring& explanation)
{
    SetDlgItemTextW(window, HelpText, explanation.c_str());
    layout();
    ensure_visible(GetDlgItem(window, HelpText));
    NotifyWinEvent(
        EVENT_OBJECT_NAMECHANGE, GetDlgItem(window, HelpText), OBJID_CLIENT, CHILDID_SELF);
}

void ProjectFormWindow::refresh_libraries()
{
    for (int index = 0; GetDlgItem(window, 6000 + index); ++index)
    {
        DestroyWindow(GetDlgItem(window, 6000 + index));
        DestroyWindow(GetDlgItem(window, 7000 + index));
    }
    for (size_t index = 0; index < options_.libraries.size(); ++index)
    {
        const auto& id = options_.libraries[index];
        const auto& catalog = supported_libraries();
        const auto item = std::find_if(catalog.begin(),
            catalog.end(),
            [&](const auto& entry)
            {
                return entry.id == id;
            });
        if (item != catalog.end())
        {
            add(L"STATIC", L"Added Library: " + item->name, 6000 + static_cast<int>(index));
            add(L"BUTTON", L"Remove", 7000 + static_cast<int>(index), WS_TABSTOP);
        }
    }
    EnableWindow(GetDlgItem(window, AddLibrary),
        options_.add_github_library || options_.libraries.size() < supported_libraries().size());
}

void ProjectFormWindow::refresh_recents()
{
    SendDlgItemMessageW(window, RecentList, LB_RESETCONTENT, 0, 0);
    for (const auto& path : options_.recents)
    {
        const auto name = path.filename().wstring() + L"\n" + path.wstring();
        SendDlgItemMessageW(
            window, RecentList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
    }
    SendDlgItemMessageW(window, RecentList, LB_SETCURSEL, 0, 0);
    EnableWindow(GetDlgItem(window, IDOK), !options_.recents.empty());
    EnableWindow(GetDlgItem(window, RemoveRecent), !options_.recents.empty());
}

bool ProjectFormWindow::select_library(const std::string& id)
{
    const auto& catalog = supported_libraries();
    if (std::none_of(catalog.begin(),
            catalog.end(),
            [&](const auto& entry)
            {
                return entry.id == id;
            }) ||
        std::find(options_.libraries.begin(), options_.libraries.end(), id) !=
            options_.libraries.end())
    {
        return false;
    }
    options_.libraries.push_back(id);
    refresh_libraries();
    apply_theme();
    layout();
    return true;
}

void ProjectFormWindow::command(int id, int notification)
{
    if (notification == EN_CHANGE && options_.disable_invalid_accept && options_.validate)
    {
        read_fields();
        EnableWindow(GetDlgItem(window, IDOK), options_.validate(fields_).empty());
        return;
    }
    if (id == CustomAction && options_.action)
    {
        read_fields();
        options_.action(window, fields_);
        for (size_t index = 0; index < fields_.size(); ++index)
        {
            const auto control = GetDlgItem(window, 2000 + static_cast<int>(index));
            if (fields_[index].checkbox)
            {
                SendMessageW(control,
                    BM_SETCHECK,
                    fields_[index].value == L"Yes" ? BST_CHECKED : BST_UNCHECKED,
                    0);
            }
            else if (fields_[index].choices.empty())
            {
                SetWindowTextW(control, fields_[index].value.c_str());
            }
            else
            {
                // An action can expand a resource-dependent selector's range.
                SendMessageW(control, CB_RESETCONTENT, 0, 0);
                for (const auto& choice : fields_[index].choices)
                {
                    SendMessageW(
                        control, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(choice.c_str()));
                }
                SendMessageW(control,
                    CB_SELECTSTRING,
                    static_cast<WPARAM>(-1),
                    reinterpret_cast<LPARAM>(fields_[index].value.c_str()));
            }
        }
        layout();
    }
    else if (id == IDCANCEL)
    {
        finished = true;
    }
    else if (id == Advanced)
    {
        options_.expanded = !options_.expanded;
        layout();
    }
    else if (id >= 4000 && id < 4000 + static_cast<int>(fields_.size()))
    {
        show_help(fields_[id - 4000].help);
    }
    else if (id >= 5000 && id < 5000 + static_cast<int>(fields_.size()))
    {
        const int index = id - 5000;
        if (const auto selected =
                browse_path(window, fields_[index].browse, text(GetDlgItem(window, 2000 + index))))
        {
            SetDlgItemTextW(window, 2000 + index, selected->c_str());
        }
    }
    else if (id == AddLibrary)
    {
        if (options_.add_github_library)
        {
            std::vector<FormField> source{{L"Library source",
                L"Known Library",
                {L"Known Library", L"GitHub / GitLab Repository"}}};
            FormOptions choice;
            choice.theme = options_.theme;
            choice.accept_label = L"Continue";
            if (!project_form(window, L"Add Library", source, choice))
            {
                return;
            }
            if (source[0].value == L"GitHub / GitLab Repository")
            {
                if (const auto selected = options_.add_github_library(window))
                {
                    select_library(*selected);
                }
                return;
            }
        }
        std::vector<FormField> fields{{L"Supported library", L"", {}}};
        std::vector<std::string> ids;
        for (const auto& entry : supported_libraries())
        {
            if (std::find(options_.libraries.begin(), options_.libraries.end(), entry.id) ==
                options_.libraries.end())
            {
                ids.push_back(entry.id);
                fields[0].choices.push_back(entry.name);
            }
        }
        if (ids.empty())
        {
            return;
        }
        fields[0].value = fields[0].choices.front();
        FormOptions picker;
        picker.theme = options_.theme;
        for (const auto& entry : supported_libraries())
        {
            picker.note += entry.name + L": " + entry.description + L"\n";
        }
        picker.note += L"Verified only after Build succeeds.";
        picker.accept_label = L"Add";
        if (project_form(window, L"Add Library", fields, picker))
        {
            const auto selected =
                std::find(fields[0].choices.begin(), fields[0].choices.end(), fields[0].value);
            select_library(ids[static_cast<size_t>(selected - fields[0].choices.begin())]);
            SetFocus(GetDlgItem(window, AddLibrary));
        }
    }
    else if (id >= 7000 && id < 7000 + static_cast<int>(options_.libraries.size()))
    {
        options_.libraries.erase(options_.libraries.begin() + (id - 7000));
        refresh_libraries();
        apply_theme();
        layout();
        SetFocus(GetDlgItem(window, AddLibrary));
    }
    else if (id == RemoveRecent)
    {
        const auto selected = SendDlgItemMessageW(window, RecentList, LB_GETCURSEL, 0, 0);
        if (selected >= 0 && selected < static_cast<LRESULT>(options_.recents.size()))
        {
            if (options_.forget_recent)
            {
                options_.forget_recent(options_.recents[static_cast<size_t>(selected)]);
            }
            options_.recents.erase(options_.recents.begin() + selected);
            refresh_recents();
            SetFocus(GetDlgItem(window, options_.recents.empty() ? IDCANCEL : RecentList));
        }
    }
    else if (id == IDOK)
    {
        if (options_.progress_only)
        {
            return;
        }
        read_fields();
        if (options_.recent_list)
        {
            const auto selected = SendDlgItemMessageW(window, RecentList, LB_GETCURSEL, 0, 0);
            if (selected < 0 || selected >= static_cast<LRESULT>(options_.recents.size()))
            {
                return;
            }
            options_.selected_recent = options_.recents[static_cast<size_t>(selected)];
            try
            {
                validate_gui_project(options_.selected_recent);
            }
            catch (...)
            {
                show_help(L"This project is missing or unsupported. Choose another or remove its "
                          L"recent entry.");
                return;
            }
        }
        if (options_.validate)
        {
            const auto error = options_.validate(fields_);
            if (!error.empty())
            {
                show_help(error);
                return;
            }
        }
        accepted = true;
        finished = true;
    }
    else if (notification == CBN_SELCHANGE || notification == EN_CHANGE)
    {
        if (options_.changed)
        {
            read_fields();
            const auto previous_fields = fields_;
            options_.changed(fields_);
            for (size_t index = 0; index < fields_.size(); ++index)
            {
                if (fields_[index].read_only &&
                    fields_[index].value != previous_fields[index].value)
                {
                    SetDlgItemTextW(
                        window, 2000 + static_cast<int>(index), fields_[index].value.c_str());
                }
                else if (!fields_[index].choices.empty() && !fields_[index].multiple_selection &&
                         (fields_[index].choices != previous_fields[index].choices ||
                             fields_[index].value != previous_fields[index].value))
                {
                    // Dependent action lists must reflect ownership immediately,
                    // rather than leaving a stale destructive action selectable.
                    const auto control = GetDlgItem(window, 2000 + static_cast<int>(index));
                    SendMessageW(control, CB_RESETCONTENT, 0, 0);
                    for (const auto& choice : fields_[index].choices)
                    {
                        SendMessageW(
                            control, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(choice.c_str()));
                    }
                    SendMessageW(control,
                        CB_SELECTSTRING,
                        static_cast<WPARAM>(-1),
                        reinterpret_cast<LPARAM>(fields_[index].value.c_str()));
                }
            }
        }
        if (id == 2000 + options_.appearance_field)
        {
            options_.theme =
                static_cast<ThemeChoice>(SendDlgItemMessageW(window, id, CB_GETCURSEL, 0, 0));
            apply_theme();
        }
        layout();
    }
}

LRESULT CALLBACK ProjectFormWindow::procedure(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    auto form = reinterpret_cast<ProjectFormWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        form = static_cast<ProjectFormWindow*>(
            reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(form));
    }
    if (!form)
    {
        return DefWindowProcW(window, message, wparam, lparam);
    }
    switch (message)
    {
        case WM_SIZE:
            if (form->font_ && !form->layout_in_progress_ && wparam != SIZE_MINIMIZED)
            {
                form->manually_sized_ = true;
                form->preferred_width_ = MulDiv(LOWORD(lparam), 96, form->dpi);
                form->preferred_height_ = MulDiv(HIWORD(lparam), 96, form->dpi);
                form->layout();
            }
            return 0;
        case WM_GETMINMAXINFO:
            reinterpret_cast<MINMAXINFO*>(lparam)->ptMinTrackSize = {
                scale_pixels(550, form->dpi), scale_pixels(280, form->dpi)};
            return 0;
        case WM_CLOSE:
            form->command(IDCANCEL);
            return 0;
        case RequestInlineClose:
        {
            bool modified = false;
            if (form->options_.confirm_inline_discard)
            {
                form->read_fields();
                modified = form->options_.libraries != form->initial_libraries_;
                for (size_t index = 0; index < form->initial_values_.size(); ++index)
                {
                    const auto& field = form->fields_[index];
                    modified |= !field.read_only && field.value != form->initial_values_[index];
                }
            }
            if (modified && MessageBoxW(GetAncestor(window, GA_ROOT),
                                L"Discard the unsaved changes in this section?",
                                L"Unsaved changes",
                                MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
            {
                return FALSE;
            }
            form->command(IDCANCEL);
            return TRUE;
        }
        case WM_VSCROLL:
        {
            SCROLLINFO position{sizeof(position), SIF_TRACKPOS};
            GetScrollInfo(window, SB_VERT, &position);
            const int code = LOWORD(wparam);
            const int requested =
                code == SB_THUMBTRACK
                    ? position.nTrackPos
                    : form->scroll_offset_ + (code == SB_LINEDOWN      ? 32
                                                 : code == SB_LINEUP   ? -32
                                                 : code == SB_PAGEDOWN ? form->viewport_height_
                                                 : code == SB_PAGEUP   ? -form->viewport_height_
                                                                       : 0);
            form->scroll_offset_ = (std::clamp)(
                requested, 0, (std::max)(0, form->content_height_ - form->viewport_height_));
            form->layout();
            return 0;
        }
        case WM_MOUSEWHEEL:
            SendMessageW(window,
                WM_VSCROLL,
                GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? SB_LINEUP : SB_LINEDOWN,
                0);
            return 0;
        case DM_GETDEFID:
            return MAKELONG(form->options_.cancel_default ? IDCANCEL : IDOK, DC_HASDEFID);
        case WM_COMMAND:
        {
            if (LOWORD(wparam) == RecentList && HIWORD(wparam) == LBN_DBLCLK)
            {
                form->command(IDOK);
                return 0;
            }
            const int field = static_cast<int>(LOWORD(wparam)) - 2000;
            const bool editable = field >= 0 && static_cast<size_t>(field) < form->fields_.size() &&
                                  !form->fields_[field].read_only;
            if (HIWORD(wparam) == BN_CLICKED || HIWORD(wparam) == CBN_SELCHANGE ||
                (HIWORD(wparam) == EN_CHANGE && (form->options_.disable_invalid_accept ||
                                                    (form->options_.observe_edits && editable))))
            {
                try
                {
                    form->command(LOWORD(wparam), HIWORD(wparam));
                }
                catch (const std::exception& error)
                {
                    const std::string description(error.what());
                    const auto size =
                        MultiByteToWideChar(CP_UTF8, 0, description.c_str(), -1, nullptr, 0);
                    std::wstring message_text(static_cast<size_t>((std::max)(1, size)), L'\0');
                    MultiByteToWideChar(
                        CP_UTF8, 0, description.c_str(), -1, message_text.data(), size);
                    form->show_help(message_text.c_str());
                }
            }
            return 0;
        }
        case WM_MEASUREITEM:
            reinterpret_cast<MEASUREITEMSTRUCT*>(lparam)->itemHeight = scale_pixels(
                reinterpret_cast<MEASUREITEMSTRUCT*>(lparam)->CtlType == ODT_LISTBOX ? 58 : 24,
                form->dpi);
            return TRUE;
        case WM_DRAWITEM:
        {
            const auto& item = *reinterpret_cast<DRAWITEMSTRUCT*>(lparam);
            if (item.CtlType == ODT_LISTBOX && item.itemID != static_cast<UINT>(-1))
            {
                const auto& palette = form->appearance.palette;
                const auto brush = CreateSolidBrush(
                    item.itemState & ODS_SELECTED ? palette.accent : palette.panel);
                FillRect(item.hDC, &item.rcItem, brush);
                DeleteObject(brush);
                SetBkMode(item.hDC, TRANSPARENT);
                SetTextColor(
                    item.hDC, item.itemState & ODS_SELECTED ? palette.accent_text : palette.text);
                auto rectangle = item.rcItem;
                InflateRect(&rectangle, -scale_pixels(10, form->dpi), -scale_pixels(5, form->dpi));
                const auto& path = form->options_.recents.at(item.itemID);
                DrawTextW(item.hDC,
                    path.filename().c_str(),
                    -1,
                    &rectangle,
                    DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                rectangle.top += scale_pixels(25, form->dpi);
                DrawTextW(item.hDC,
                    path.c_str(),
                    -1,
                    &rectangle,
                    DT_SINGLELINE | DT_PATH_ELLIPSIS | DT_NOPREFIX);
                if (item.itemState & ODS_FOCUS)
                {
                    DrawFocusRect(item.hDC, &item.rcItem);
                }
                return TRUE;
            }
            return form->appearance.draw_item(item);
        }
        case WM_ERASEBKGND:
        {
            RECT bounds{};
            GetClientRect(window, &bounds);
            if (form->appearance.background_brush())
            {
                FillRect(
                    reinterpret_cast<HDC>(wparam), &bounds, form->appearance.background_brush());
                return TRUE;
            }
            break;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT paint{};
            const auto dc = BeginPaint(window, &paint);
            form->appearance.paint_panels(dc);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        {
            wchar_t type[64]{};
            GetClassNameW(reinterpret_cast<HWND>(lparam), type, 64);
            // Read-only edits send CTLCOLORSTATIC too. They still need an opaque
            // input background; transparent repaint leaves old scrolled text.
            return reinterpret_cast<LRESULT>(
                form->appearance.control_colors(reinterpret_cast<HDC>(wparam),
                    message != WM_CTLCOLORSTATIC || std::wstring_view(type) == L"Edit",
                    IsWindowEnabled(reinterpret_cast<HWND>(lparam)) != FALSE));
        }
        case WM_DPICHANGED:
            form->dpi = HIWORD(wparam);
            form->apply_theme();
            form->layout();
            return 0;
        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
            if (form->options_.theme == ThemeChoice::FollowWindows)
            {
                form->apply_theme();
            }
            break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

bool project_form(
    HWND owner, const std::wstring& title, std::vector<FormField>& fields, FormOptions& options)
{
    const auto previous = fields;
    const auto previous_focus = GetFocus();
    const auto owner_enabled = IsWindowEnabled(owner);
    ProjectFormWindow form(owner, title, fields, options);
    RECT bounds{}, parent{};
    GetWindowRect(form.window, &bounds);
    GetWindowRect(owner, &parent);
    MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST), &monitor);
    const int x = (std::max)(monitor.rcWork.left,
        (std::min)(parent.left + 30, monitor.rcWork.right - (bounds.right - bounds.left)));
    const int y = (std::max)(monitor.rcWork.top,
        (std::min)(parent.top + 30, monitor.rcWork.bottom - (bounds.bottom - bounds.top)));
    if (!options.inline_host)
    {
        SetWindowPos(form.window, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        EnableWindow(owner, FALSE);
    }
    ShowWindow(form.window, SW_SHOW);
    SetFocus(options.cancel_default ? GetDlgItem(form.window, IDCANCEL)
                                    : GetNextDlgTabItem(form.window, nullptr, FALSE));
    MSG message{};
    while (!form.finished)
    {
        const int result = GetMessageW(&message, nullptr, 0, 0);
        if (result <= 0)
        {
            if (result == 0)
            {
                PostQuitMessage(static_cast<int>(message.wParam));
            }
            break;
        }
        if (message.message == WM_KEYDOWN && message.wParam == VK_F1)
        {
            const auto id = GetDlgCtrlID(GetFocus());
            if (id >= 2000 && id < 2000 + static_cast<int>(fields.size()))
            {
                form.command(4000 + id - 2000);
                continue;
            }
        }
        if (forward_restart_shortcut(form.window, message))
        {
            continue;
        }
        // Tab traverses the page and its detail panel. Enter and Escape use
        // the subsection's dialog buttons when focus is inside it.
        const bool section_key = message.message == WM_KEYDOWN &&
                                 (message.wParam == VK_RETURN || message.wParam == VK_ESCAPE) &&
                                 IsChild(form.window, GetFocus());
        if (!IsDialogMessageW(options.inline_host && !section_key ? owner : form.window, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (message.message == WM_KEYDOWN)
        {
            form.ensure_visible(GetFocus());
        }
    }
    if (!options.inline_host)
    {
        EnableWindow(owner, owner_enabled);
    }
    form.close_native_window();
    if (owner_enabled)
    {
        SetActiveWindow(owner);
    }
    if (IsWindow(previous_focus) && IsWindowEnabled(previous_focus))
    {
        SetFocus(previous_focus);
    }
    if (!form.accepted)
    {
        fields = previous;
    }
    return form.accepted;
}

bool project_form(HWND owner, const std::wstring& title, std::vector<FormField>& fields)
{
    FormOptions options;
    return project_form(owner, title, fields, options);
}

bool request_inline_form_close(HWND form)
{
    return SendMessageW(form, RequestInlineClose, 0, 0) != FALSE;
}

bool forward_restart_shortcut(HWND window, const MSG& message)
{
    const auto root = GetAncestor(window, GA_ROOTOWNER);
    if (message.message == WM_SYSKEYDOWN && message.wParam == 'R' && GetDlgItem(root, 144))
    {
        PostMessageW(root, WM_COMMAND, 144, 0);
        return true;
    }
    return false;
}
} // namespace cpplivelab
