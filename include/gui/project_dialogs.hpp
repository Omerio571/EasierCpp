#pragma once
#include "appearance/ui_theme.hpp"
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>
struct IAccPropServices;

namespace cpplivelab
{
enum class BrowseKind
{
    None,
    Folder,
    Executable
};

struct FormField
{
    std::wstring label;
    std::wstring value;
    std::vector<std::wstring> choices;
    bool multiline = false;
    std::wstring help;
    BrowseKind browse = BrowseKind::None;
    bool advanced = false;
    bool read_only = false;
    std::wstring placeholder;
    bool multiple_selection = false;
    int multiline_height = 76;
    bool checkbox = false;
};

struct FormOptions
{
    ThemeChoice theme = ThemeChoice::FollowWindows;
    std::wstring note;
    std::wstring accept_label = L"OK";
    bool expanded = false;
    bool library_picker = false;
    int custom_editor_field = -1;
    int appearance_field = -1;
    bool cancel_default = false;
    bool progress_only = false;
    bool disable_invalid_accept = false;
    std::vector<std::string> libraries;
    std::vector<std::filesystem::path> recents;
    bool recent_list = false;
    std::filesystem::path selected_recent;
    std::function<void(const std::filesystem::path&)> forget_recent;
    std::function<std::wstring(const std::vector<FormField>&)> validate;
    std::function<void(std::vector<FormField>&)> changed;
    std::wstring action_label;
    std::function<void(HWND, std::vector<FormField>&)> action;
    std::function<std::optional<std::string>(HWND)> add_github_library;
};

std::optional<std::filesystem::path> browse_path(
    HWND owner, BrowseKind kind, const std::filesystem::path& initial = {});

// Shared native controls, palette, layout and actions for every application form.
// Also permits test-owned off-screen windows without desktop automation.
class ProjectFormWindow
{
public:
    ProjectFormWindow(HWND owner,
        const std::wstring& title,
        std::vector<FormField>& fields,
        FormOptions& options);
    ~ProjectFormWindow();
    HWND window = nullptr;
    UiTheme appearance;
    UINT dpi = 96;
    bool finished = false;
    bool accepted = false;
    void layout();
    void command(int id, int notification = BN_CLICKED);
    void apply_theme();
    bool select_library(const std::string& id);
    void ensure_visible(HWND control);
    void close_native_window();
    static LRESULT CALLBACK procedure(HWND, UINT, WPARAM, LPARAM);

private:
    std::vector<FormField>& fields_;
    FormOptions& options_;
    HFONT font_ = nullptr;
    HWND tooltip_ = nullptr;
    IAccPropServices* accessibility_ = nullptr;
    int scroll_offset_ = 0;
    int content_height_ = 0;
    int viewport_height_ = 0;
    bool layout_in_progress_ = false;
    bool manually_sized_ = false;
    int preferred_width_ = 624;
    int preferred_height_ = 0;
    HWND add(const wchar_t* type, const std::wstring& text, int id, DWORD style = 0);
    void read_fields();
    void refresh_libraries();
    void refresh_recents();
    void show_help(const std::wstring& text);
};

bool project_form(
    HWND owner, const std::wstring& title, std::vector<FormField>& fields, FormOptions& options);
bool project_form(HWND owner, const std::wstring& title, std::vector<FormField>& fields);
bool forward_restart_shortcut(HWND window, const MSG& message);
} // namespace cpplivelab
