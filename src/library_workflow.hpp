#pragma once
#include "project_dialogs.hpp"
#include <functional>
#include "cpplivelab/libraries.hpp"

namespace cpplivelab
{
using LibraryOutput = std::function<void(const std::wstring&)>;
enum class LibraryEdit
{
    Add,
    UpdateVersion,
    ChangeTargets,
    Repair
};
bool library_task_active();
void cancel_library_task();
bool run_library_work(
    HWND owner, ThemeChoice theme, const std::wstring& title, const std::function<void()>& work);
void initialize_github_repository_form(std::vector<FormField>& fields, FormOptions& options);
bool run_library_task(HWND owner,
    ThemeChoice theme,
    const std::filesystem::path& executable,
    const std::vector<std::wstring>& arguments,
    const std::filesystem::path& directory,
    const LibraryOutput& output);
// Discovery is passive. A separate confirmation is required before configuring
// third-party CMake. Only a successful link probe registers an integration.
std::optional<std::string> github_library_dialog(HWND owner,
    ThemeChoice theme,
    const std::filesystem::path& manager_cli,
    const LibraryOutput& output,
    const std::optional<LibraryRecord>& existing = {},
    LibraryEdit operation = LibraryEdit::Add);
} // namespace cpplivelab
