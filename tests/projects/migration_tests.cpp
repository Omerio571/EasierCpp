#include "projects/state_migration.hpp"
#include "appearance/appearance.hpp"
#include "tools/tool_workflows.hpp"
#include "cpplivelab/workspace.hpp"
#include "cpplivelab/libraries.hpp"
#include "projects/project_paths.hpp"
#include "build/build_freshness.hpp"
#include "projects/project_templates.hpp"
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
} // namespace

int main()
{
    try
    {
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
