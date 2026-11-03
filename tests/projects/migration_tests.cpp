#include "projects/state_migration.hpp"
#include "appearance/appearance.hpp"
#include "tools/tool_workflows.hpp"
#include "cpplivelab/workspace.hpp"
#include "cpplivelab/libraries.hpp"
#include "projects/project_paths.hpp"
#include "build/build_freshness.hpp"
#include "projects/project_templates.hpp"
#include "cpplivelab/session.hpp"
#include "../workspace_test_support.hpp"
#include <fstream>
#include <iostream>
#include <atomic>
#include <thread>

namespace
{
namespace fs = std::filesystem;
using namespace cpplivelab;

void require(bool value, const char* reason)
{
    if (!value)
    {
        throw std::runtime_error(reason);
    }
}

void write(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    require(
        static_cast<bool>(std::ofstream(path, std::ios::binary) << text), "Fixture write failed");
}

std::string read(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

template <typename Operation> void refuses(Operation operation, const char* reason)
{
    try
    {
        operation();
    }
    catch (const std::exception&)
    {
        return;
    }
    throw std::runtime_error(reason);
}

void tests()
{
    workspace_test::Fixture fixture;
    const auto old = fixture.root / L"CppLiveLab";
    const auto current = fixture.root / L"EasierCpp";
    const auto project = fixture.root / L"unchanged project";
    write(project / L"CMakeLists.txt", "project(UserProject LANGUAGES CXX)\n");
    SetEnvironmentVariableW(L"CPPLIVELAB_STATE_HOME", old.c_str());
    ProjectOptions options;
    options.target = L"user_app";
    options.arguments = {L"--user-argument", L"value with spaces"};
    options.working_directory = project / L"assets";
    options.pch_config = workspace_directory(project) / L"pch/stable.cmake";
    write(options.pch_config, "# immutable legacy PCH snapshot\n");
    save_project_options(project, L"Debug", options);
    remember_project(project);
    const auto relative = workspace_directory(project).lexically_relative(old);
    write(old / relative / L"libraries.manifest", "0\n");
    write(old / relative / L"window-placement/1.settings", "legacy placement\n");
    write(old / relative / L"build-Debug-old/CMakeCache.txt", "ABSOLUTE OLD CACHE\n");
    write(old / relative / L"build-Debug-old.active", "old-slot");
    write(old / L"d/cache/CMakeCache.txt", "do not migrate dependency caches\n");
    write(old / L"integrations/known.library", "original library metadata\n");
    write(old / L"integrations/known.cmake", "original pinned CMake recipe\n");
    write(old / L"integrations/known.name", "legacy display name\n");
    write(old / L"integrations/hidden.hidden", "hidden entry\n");
    write(old / L"build-workers.settings", "2 2\n");
    write(old / L"editor.settings", "old editor\n");
    write(current / L"editor.settings", "new editor wins\n");
    // A failed prior staging attempt must not be mistaken for committed metadata.
    write(current / L"integrations.migration-abandoned/known.library", "partial");
    write(current / L"legacy-migration.complete", "EasierCpp metadata");
    migrate_state_metadata(old, current);
    require(read(current / L"legacy-migration.complete") == "EasierCpp metadata migration 1\n",
        "Completion marker is not canonical binary LF; text-mode CRLF causes repeated migration");
    require(read(current / L"editor.settings") == "new editor wins\n", "New editor overwritten");
    require(read(old / L"editor.settings") == "old editor\n", "Original editor changed");
    require(read(current / L"integrations/known.cmake") == read(old / L"integrations/known.cmake"),
        "Pinned library recipe was not preserved");
    require(read(current / L"integrations/known.name") == "legacy display name\n" &&
                read(current / L"integrations/hidden.hidden") == "hidden entry\n",
        "Legacy known/hidden library metadata lost");
    require(!fs::exists(current / L"d") && !fs::exists(current / relative / L"build-Debug-old") &&
                !fs::exists(current / relative / L"build-Debug-old.active"),
        "Absolute build caches/slot pointers must not migrate");
    require(read(current / L"migration-report.txt").find("Conflict;") != std::string::npos,
        "Migration conflict was not reported");
    SetEnvironmentVariableW(L"CPPLIVELAB_STATE_HOME", current.c_str());
    const auto restored = load_project_options(project, L"Debug");
    require(restored.target == options.target && restored.arguments == options.arguments &&
                restored.working_directory == options.working_directory &&
                restored.pch_config == options.pch_config &&
                fs::is_regular_file(restored.pch_config),
        "Existing project settings/PCH reference lost");
    require(recent_projects() == std::vector<fs::path>{project}, "Recent projects lost");
    const auto stamp = fs::last_write_time(current / relative / L"Debug.settings");
    migrate_state_metadata(old, current);
    require(fs::last_write_time(current / relative / L"Debug.settings") == stamp,
        "Idempotent migration rewrote project settings");
    // Resume after a partial unit-level migration: complete units win untouched.
    fs::remove(current / L"legacy-migration.complete");
    write(old / L"projects/pending/library-transaction/pending", "uncommitted transaction");
    refuses(
        [&]
        {
            migrate_state_metadata(old, current);
        },
        "Pending library transaction ignored");
    require(
        !fs::exists(current / L"legacy-migration.complete"), "Failed migration marked complete");
    fs::remove(old / L"projects/pending/library-transaction/pending");
    write(old / L"projects/pending/Release.settings", "new recovered metadata");
    migrate_state_metadata(old, current);
    require(read(current / L"projects/pending/Release.settings") == "new recovered metadata",
        "Interrupted migration did not resume");
    require(read(project / L"CMakeLists.txt") == "project(UserProject LANGUAGES CXX)\n",
        "Migration changed a user project");
    refuses(
        [&]
        {
            migrate_state_metadata(old, old / L"nested");
        },
        "Nested state migration allowed");
    auto old_identity = fs::weakly_canonical(old).wstring();
    for (auto& character : old_identity)
    {
        character = static_cast<wchar_t>(std::towlower(character));
    }
    const auto legacy_mutex =
        L"Local\\CppLiveLab-" +
        path_support::from_utf8(library_key(path_support::path_utf8(old_identity))).wstring();
    std::atomic<int> ready = 0;
    std::atomic<bool> release = false;
    std::thread legacy_owner(
        [&]
        {
            windows_support::UniqueHandle handle;
            handle.reset(CreateMutexW(nullptr, TRUE, legacy_mutex.c_str()));
            ready = handle.value ? 1 : 2;
            while (!release)
            {
                Sleep(1);
            }
            if (handle.value)
            {
                ReleaseMutex(handle.value);
            }
        });
    while (!ready)
    {
        Sleep(1);
    }
    const auto completed_report = read(current / L"migration-report.txt");
    write(current / L"legacy-migration.complete", "EasierCpp metadata migration 1\r\n");
    bool completed_allowed = false;
    try
    {
        migrate_state_metadata(old, current);
        completed_allowed = true;
    }
    catch (const std::exception&)
    {
    }
    bool live_refused = false;
    try
    {
        migrate_state_metadata(old, fixture.root / L"Blocked");
    }
    catch (const std::exception&)
    {
        live_refused = true;
    }
    release = true;
    legacy_owner.join();
    require(ready == 1 && live_refused && !fs::exists(fixture.root / L"Blocked"),
        "Live legacy manager did not block migration before writes");
    require(
        completed_allowed && read(current / L"migration-report.txt") == completed_report &&
            read(current / L"legacy-migration.complete") == "EasierCpp metadata migration 1\r\n",
        "Completed CRLF migration reacquired the GUI guard, rewrote its report or altered the "
        "marker");
    write(current / L"legacy-migration.complete", "EasierCpp metadata migration 99\n");
    refuses(
        [&]
        {
            migrate_state_metadata(old, current);
        },
        "Unknown marker version was overwritten");
    require(read(current / L"legacy-migration.complete") == "EasierCpp metadata migration 99\n",
        "Unsupported completion marker was changed");

    const auto registry =
        L"Software\\EasierCppMigrationTests\\Migration-" + std::to_wstring(GetCurrentProcessId());

    struct RegistryCleanup
    {
        std::wstring key;

        ~RegistryCleanup()
        {
            RegDeleteTreeW(HKEY_CURRENT_USER, key.c_str());
        }
    } cleanup{registry};

    require(save_preferences({ThemeChoice::Dark, 700}, registry + L"\\Old"), "Seed old registry");
    require(migrate_preference_store(registry + L"\\Old", registry + L"\\New"),
        "Registry preferences did not migrate atomically");
    auto preferences = load_preferences(registry + L"\\New");
    require(preferences.theme == ThemeChoice::Dark && preferences.debounce_ms == 700,
        "Migrated preferences differ");
    require(
        save_preferences({ThemeChoice::Light, 800}, registry + L"\\New"), "Save new preferences");
    require(!migrate_preference_store(registry + L"\\Old", registry + L"\\New") &&
                load_preferences(registry + L"\\New").theme == ThemeChoice::Light &&
                load_preferences(registry + L"\\Old").theme == ThemeChoice::Dark,
        "Registry conflict/idempotence changed old or new values");
    require(save_preferences({ThemeChoice::Light, 650}, registry + L"\\InvalidOld"),
        "Seed malformed legacy preferences");
    const wchar_t invalid[] = L"unknown theme format";
    require(RegSetKeyValueW(HKEY_CURRENT_USER,
                (registry + L"\\InvalidOld").c_str(),
                L"Theme",
                REG_SZ,
                invalid,
                sizeof(invalid)) == ERROR_SUCCESS,
        "Seed unsupported legacy value type");
    require(migrate_preference_store(registry + L"\\InvalidOld", registry + L"\\InvalidNew") &&
                load_preferences(registry + L"\\InvalidNew").theme == ThemeChoice::FollowWindows &&
                load_preferences(registry + L"\\InvalidNew").debounce_ms == 650,
        "Unsupported legacy value did not retain the existing safe default behavior");

    const auto checkout = fixture.root / L"moved source";
    write(checkout / L"CMakeLists.txt", "project(EasierCpp)\n");
    write(checkout / L"AGENTS.md", "guidance");
    write(checkout / L"scripts/build.ps1", "# first-party fixture");
    write(checkout / L"include/cpplivelab/workspace.hpp", "// fixture");
    require(find_development_repository(
                fixture.root / L"missing old source", checkout / L"build/Release") == checkout,
        "Relocated Release/Debug executable ancestors were not discovered");
    require(find_development_repository({}, fixture.root / L"outside").empty(),
        "Discovery searched unrelated current directories");
    wchar_t test_child[32768]{};
    GetEnvironmentVariableW(L"CPPLIVELAB_TEST_CHILD", test_child, 32768);
    SetEnvironmentVariableW(L"CPPLIVELAB_TEST_CHILD", nullptr);
    const auto repository = source_repository();
    if (fs::is_regular_file(repository / L"scripts/build.ps1"))
    {
        const auto command = test_command(L"Quick", 1);
        require(command.repository == repository &&
                    std::find(command.arguments.begin(),
                        command.arguments.end(),
                        (repository / L"scripts/build.ps1").wstring()) != command.arguments.end(),
            "Development test command did not use the validated checkout");
    }
    else
    {
        bool portable_message = false;
        try
        {
            test_command(L"Quick", 1);
        }
        catch (const std::exception& error)
        {
            portable_message =
                std::string(error.what()).find("portable EasierCpp") != std::string::npos;
        }
        require(portable_message, "Portable development-test diagnostic is not actionable");
    }
    SetEnvironmentVariableW(L"CPPLIVELAB_TEST_CHILD", test_child[0] ? test_child : nullptr);
    BuildIdentity identity;
    identity.project = project;
    identity.configuration = L"Debug";
    require(
        identity.directory() == project / L".easiercpp/build-Debug", "New scratch identity wrong");
    fs::create_directory(project / L".cpplivelab");
    require(identity.directory() == project / L".cpplivelab/build-Debug", "Legacy scratch lost");
    require(project_templates::cmake_project("UserProject", {"main.cpp"})
                    .find("# EasierCpp starter:") != std::string::npos,
        "New project template still uses old branding");
    SetEnvironmentVariableW(L"CPPLIVELAB_STATE_HOME", nullptr);
    SetEnvironmentVariableW(L"EASIERCPP_STATE_HOME", current.c_str());
    require(manager_state_directory() == current, "New state override not recognized");
    SetEnvironmentVariableW(L"EASIERCPP_STATE_HOME", nullptr);
}

struct ScopedEnvironment
{
    std::vector<std::pair<std::wstring, std::wstring>> previous;

    void set(const wchar_t* name, const std::wstring& value)
    {
        wchar_t buffer[32768]{};
        GetEnvironmentVariableW(name, buffer, 32768);
        previous.emplace_back(name, buffer);
        require(SetEnvironmentVariableW(name, value.empty() ? nullptr : value.c_str()) != FALSE,
            "Cannot set isolated process environment");
    }

    ~ScopedEnvironment()
    {
        for (auto item = previous.rbegin(); item != previous.rend(); ++item)
        {
            SetEnvironmentVariableW(
                item->first.c_str(), item->second.empty() ? nullptr : item->second.c_str());
        }
    }
};

HWND process_window(DWORD process)
{
    struct Search
    {
        DWORD process;
        HWND window = nullptr;
    } search{process};

    EnumWindows(
        [](HWND window, LPARAM parameter) -> BOOL
        {
            auto& search = *reinterpret_cast<Search*>(parameter);
            DWORD process = 0;
            GetWindowThreadProcessId(window, &process);
            if (process == search.process && IsWindowVisible(window) &&
                GetPropW(window, L"CppLiveLabInstance"))
            {
                search.window = window;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&search));
    return search.window;
}

HWND wait_for_gui(ConsoleSession& gui)
{
    const auto started = GetTickCount64();
    std::string output;
    while (GetTickCount64() - started < 15000)
    {
        output += gui.poll();
        if (const auto window = process_window(gui.process_id()))
        {
            return window;
        }
        require(gui.running(), ("Isolated GUI exited before ready: " + output).c_str());
        Sleep(10);
    }
    throw std::runtime_error("Isolated GUI did not become ready: " + output);
}

std::string finish(ConsoleSession& process)
{
    const auto started = GetTickCount64();
    std::string output;
    while (process.running() || process.output_pending())
    {
        output += process.poll();
        if (process.root_exited())
        {
            process.finish_build();
        }
        require(GetTickCount64() - started < 90000, "Isolated process timed out");
        Sleep(10);
    }
    return output;
}

void process_tests(const fs::path& cli, const fs::path& gui, const fs::path& legacy_gui)
{
    workspace_test::Fixture fixture;

    struct RegistryCleanup
    {
        std::vector<std::wstring> keys;

        void add(const fs::path& state)
        {
            keys.push_back(
                L"Software\\CppLiveLab\\Tests\\" +
                path_support::from_utf8(library_key(path_support::path_utf8(state))).wstring());
        }

        ~RegistryCleanup()
        {
            for (const auto& key : keys)
            {
                RegDeleteTreeW(HKEY_CURRENT_USER, key.c_str());
            }
        }
    } registry;

    for (const auto* scenario : {L"fresh", L"legacy", L"both", L"completed", L"interrupted"})
    {
        const auto local = fixture.root / scenario;
        const auto old = local / L"CppLiveLab";
        const auto current = local / L"EasierCpp";
        const auto project = fixture.root / (std::wstring(scenario) + L" project 計算");
        write(project / L"CMakeLists.txt",
            "cmake_minimum_required(VERSION 3.24)\n"
            "project(MigrationConsumer LANGUAGES CXX)\nadd_executable(app main.cpp)\n"
            "target_compile_features(app PRIVATE cxx_std_20)\n");
        write(project / L"main.cpp", "int main() { return 0; }\n");
        const auto source_before = read(project / L"CMakeLists.txt");
        if (std::wstring_view(scenario) != L"fresh")
        {
            write(old / L"build-workers.settings", "1 1\n");
        }
        if (std::wstring_view(scenario) == L"both")
        {
            write(current / L"build-workers.settings", "0 0\n");
        }
        if (std::wstring_view(scenario) == L"completed")
        {
            write(current / L"legacy-migration.complete", "EasierCpp metadata migration 1\r\n");
            write(current / L"build-workers.settings", "0 0\n");
        }
        if (std::wstring_view(scenario) == L"interrupted")
        {
            write(current / L"legacy-migration.complete", "EasierCpp metadata");
        }
        ScopedEnvironment environment;
        environment.set(L"LOCALAPPDATA", local.wstring());
        environment.set(L"CPPLIVELAB_STATE_HOME", L"");
        environment.set(L"EASIERCPP_STATE_HOME", L"");
        environment.set(L"CPPLIVELAB_TEST_CHILD", L"1");
        registry.add(current);
        ConsoleSession manager;
        manager.start_worker(gui, {}, fixture.root);
        const auto window = wait_for_gui(manager);
        const auto report_before = fs::is_regular_file(current / L"migration-report.txt")
                                       ? read(current / L"migration-report.txt")
                                       : std::string{};
        ConsoleSession companion;
        companion.start_worker(cli, {L"targets", project.wstring()}, fixture.root);
        const auto output = finish(companion);
        require(companion.exit_code() == 0,
            ("GUI blocked its own CLI in " + path_support::path_utf8(local) + ": " + output)
                .c_str());
        if (std::wstring_view(scenario) == L"legacy")
        {
            const auto second = fixture.root / L"independent consumer";
            write(second / L"CMakeLists.txt", source_before);
            write(second / L"main.cpp", "int main() { return 0; }\n");
            ConsoleSession first_build, second_build;
            first_build.start_worker(
                cli, {L"manage-run", project.wstring(), L"--target", L"app"}, fixture.root);
            second_build.start_worker(cli,
                {L"manage-run", second.wstring(), L"--target", L"app", L"--config", L"Release"},
                fixture.root);
            const auto first_output = finish(first_build);
            const auto second_output = finish(second_build);
            require(first_build.exit_code() == 0 && second_build.exit_code() == 0,
                ("Independent Debug/Release consumers failed:\n" + first_output + second_output)
                    .c_str());
        }
        ConsoleSession duplicate;
        duplicate.start_worker(gui, {}, fixture.root);
        const auto duplicate_output = finish(duplicate);
        require(duplicate.exit_code() == 0 && manager.running(),
            ("Default-profile single-instance protection failed: " + duplicate_output).c_str());
        require(!fs::exists(current / L"migration-report.txt") ||
                    read(current / L"migration-report.txt") == report_before,
            "Companion/duplicate startup repeated completed migration");
        require(
            read(project / L"CMakeLists.txt") == source_before, "Migration changed project CMake");
        if (std::wstring_view(scenario) == L"both")
        {
            require(
                read(current / L"build-workers.settings") == "0 0\n", "New settings overwritten");
        }
        PostMessageW(window, WM_CLOSE, 0, 0);
        finish(manager);
        require(manager.exit_code() == 0, "Default-profile manager did not close safely");
        std::cout << "Default-profile GUI + CLI + duplicate: "
                  << path_support::path_utf8(fs::path(scenario)) << " passed\n";
    }
    if (fs::is_regular_file(legacy_gui))
    {
        const auto local = fixture.root / L"actual legacy owner";
        const auto old = local / L"CppLiveLab";
        const auto current = local / L"EasierCpp";
        write(old / L"build-workers.settings", "0 0\n");
        const auto project = fixture.root / L"legacy refusal consumer";
        write(project / L"CMakeLists.txt", "project(LegacyRefusal LANGUAGES CXX)\n");
        ScopedEnvironment environment;
        environment.set(L"LOCALAPPDATA", local.wstring());
        environment.set(L"CPPLIVELAB_TEST_CHILD", L"1");
        environment.set(L"EASIERCPP_STATE_HOME", L"");
        environment.set(L"CPPLIVELAB_STATE_HOME", old.wstring());
        registry.add(old);
        ConsoleSession legacy;
        legacy.start_worker(legacy_gui, {}, fixture.root);
        const auto window = wait_for_gui(legacy);
        environment.set(L"CPPLIVELAB_STATE_HOME", L"");
        ConsoleSession companion;
        companion.start_worker(cli, {L"targets", project.wstring()}, fixture.root);
        const auto output = finish(companion);
        require(companion.exit_code() != 0 &&
                    output.find("Close the legacy CppLiveLab") != output.npos &&
                    !fs::exists(current) && read(old / L"build-workers.settings") == "0 0\n",
            ("Actual legacy owner was not safely detected: " + output).c_str());
        PostMessageW(window, WM_CLOSE, 0, 0);
        finish(legacy);
        std::cout
            << "Actual pre-migration CppLiveLab GUI: migration refused without metadata writes\n";
    }
    else
    {
        std::cout
            << "Actual legacy executable unavailable; unit mutex-contract coverage still runs\n";
    }
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    try
    {
        if (argc >= 4 && std::wstring_view(argv[1]) == L"--processes")
        {
            process_tests(argv[2], argv[3], argc > 4 ? fs::path(argv[4]) : fs::path{});
            return 0;
        }
        tests();
        std::cout
            << "EasierCpp migration: metadata, conflicts, resume, registry, source discovery, "
               "new/legacy scratch paths passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Migration failure: " << error.what() << '\n';
        return 1;
    }
}
