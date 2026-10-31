#include "cpplivelab/workspace.hpp"
#include "cpplivelab/doctor.hpp"
#include "cpplivelab/session.hpp"
#include "projects/project_paths.hpp"
#include "process/windows_support.hpp"
#include <fstream>
#include <iomanip>

namespace cpplivelab
{
namespace
{
namespace fs = std::filesystem;

fs::path environment_folder(const wchar_t* name)
{
    wchar_t value[32768]{};
    GetEnvironmentVariableW(name, value, 32768);
    return value;
}
} // namespace

EditorOptions load_editor()
{
    EditorOptions result;
    std::ifstream input(manager_state_directory() / L"editor.settings");
    std::string choice, executable, argument;
    if (input >> std::quoted(choice) >> std::quoted(executable))
    {
        result.choice = path_support::from_utf8(choice).wstring();
        result.executable = path_support::from_utf8(executable);
        while (input >> std::quoted(argument))
        {
            result.arguments.push_back(path_support::from_utf8(argument).wstring());
        }
    }
    return result;
}

void save_editor(const EditorOptions& options)
{
    if (options.choice != L"Visual Studio 2022" && options.choice != L"Visual Studio Code" &&
        options.choice != L"CLion" && options.choice != L"Custom")
    {
        throw std::runtime_error("Unsupported editor choice.");
    }
    fs::create_directories(manager_state_directory());
    std::ofstream output(manager_state_directory() / L"editor.settings");
    output << std::quoted(path_support::path_utf8(options.choice)) << '\n'
           << std::quoted(path_support::path_utf8(options.executable)) << '\n';
    for (const auto& argument : options.arguments)
    {
        output << std::quoted(path_support::path_utf8(argument)) << '\n';
    }
    if (!output)
    {
        throw std::runtime_error("Cannot save preferred editor.");
    }
}

fs::path discover_editor(const std::wstring& choice)
{
    if (choice == L"Visual Studio 2022")
    {
        return find_visual_studio_2022();
    }
    std::vector<fs::path> candidates;
    if (choice == L"Visual Studio Code")
    {
        candidates = {environment_folder(L"LOCALAPPDATA") / L"Programs/Microsoft VS Code/Code.exe",
            environment_folder(L"ProgramFiles") / L"Microsoft VS Code/Code.exe"};
    }
    if (choice == L"CLion")
    {
        const auto jetbrains = environment_folder(L"ProgramFiles") / L"JetBrains";
        std::error_code error;
        for (fs::directory_iterator item(jetbrains, error), end; !error && item != end;
            item.increment(error))
        {
            if (item->path().filename().wstring().starts_with(L"CLion"))
            {
                candidates.push_back(item->path() / L"bin/clion64.exe");
            }
        }
    }

    for (const auto& candidate : candidates)
    {
        if (fs::is_regular_file(candidate))
        {
            return candidate;
        }
    }
    return {};
}

std::vector<std::wstring> editor_arguments(const EditorOptions& options,
    const fs::path& root,
    const fs::path& file,
    unsigned line,
    unsigned column)
{
    auto arguments = options.arguments;
    if (arguments.empty() && !file.empty())
    {
        if (options.choice == L"Visual Studio Code")
        {
            return {L"--reuse-window",
                L"--goto",
                file.wstring() + L":" + std::to_wstring(line) + L":" + std::to_wstring(column)};
        }
        if (options.choice == L"Visual Studio 2022")
        {
            return {L"/Edit", file.wstring(), L"/Command", L"Edit.Goto " + std::to_wstring(line)};
        }
        if (options.choice == L"CLion")
        {
            return {L"--line", std::to_wstring(line), file.wstring()};
        }
    }
    bool substituted = false;
    const std::vector<std::pair<std::wstring, std::wstring>> replacements{
        {L"{project}", root.wstring()},
        {L"{file}", file.wstring()},
        {L"{line}", std::to_wstring(line)},
        {L"{column}", std::to_wstring(column)}};
    for (auto& argument : arguments)
    {
        // Scan the original argument once: a path containing placeholder text
        // must never be interpreted a second time as an argument template.
        const auto original = argument;
        argument.clear();
        for (size_t position = 0; position < original.size();)
        {
            bool replaced = false;
            for (const auto& [placeholder, value] : replacements)
            {
                if (original.compare(position, placeholder.size(), placeholder) == 0)
                {
                    argument += value;
                    position += placeholder.size();
                    substituted = true;
                    replaced = true;
                    break;
                }
            }
            if (!replaced)
            {
                argument += original[position++];
            }
        }
    }
    if (!substituted)
    {
        arguments.push_back(file.empty() ? root.wstring() : file.wstring());
    }
    return arguments;
}

void launch_editor(const EditorOptions& options,
    const fs::path& project,
    const fs::path& file,
    unsigned line,
    unsigned column)
{
    const auto root = validate_gui_project(project);
    const auto executable =
        options.executable.empty() ? discover_editor(options.choice) : options.executable;
    if (!fs::is_regular_file(executable))
    {
        throw std::runtime_error(
            "Preferred editor executable not found. Choose an installed editor.");
    }
    const auto arguments = editor_arguments(options, root, file, line, column);
    auto command = windows_support::make_command_line(executable, arguments);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    // Editors are independent of the manager's build/run job. Never terminate
    // them on Stop/project switch; no shell or project configuration is involved.
    if (!CreateProcessW(executable.c_str(),
            command.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            root.c_str(),
            &startup,
            &process))
    {
        throw std::runtime_error(
            "Editor launch failed (Windows error " + std::to_string(GetLastError()) + ").");
    }
    windows_support::UniqueHandle thread, child;
    thread.reset(process.hThread);
    child.reset(process.hProcess);
}
} // namespace cpplivelab
