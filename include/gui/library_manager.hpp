#pragma once

#include "gui/library_workflow.hpp"

namespace cpplivelab
{
struct LibraryManagerContext
{
    HWND owner = nullptr;
    ThemeChoice theme = ThemeChoice::FollowWindows;
    std::filesystem::path project;
    std::filesystem::path cli;
    std::wstring configuration = L"Debug";
    ProjectOptions options;
    LibraryOutput output;
    std::function<void()> changed;
    std::function<bool()> busy;
    std::function<bool()> restarting;
};

void manage_libraries_dialog(const LibraryManagerContext& context);
void add_project_libraries_dialog(const LibraryManagerContext& context);
std::vector<std::wstring> library_manager_actions(
    bool read_only, bool attached, const LibraryRecord& record);
} // namespace cpplivelab
