#include "gui/library_workflow.hpp"
#include "cpplivelab/process.hpp"
#include "cpplivelab/session.hpp"
#include "../workspace_test_support.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <map>
#include <exception>
#include <cctype>
#include <cwctype>

namespace
{
namespace fs = std::filesystem;
using namespace cpplivelab;

void require(bool condition, const std::string& reason)
{
    if (!condition)
    {
        throw std::runtime_error(reason);
    }
}

struct Case
{
    std::wstring repository;
    std::wstring version;
    std::vector<std::wstring> targets;
    std::string consumer;
    bool graphical = false;
};

struct Driver
{
    const Case* test = nullptr;
    bool fresh = true;
    std::set<std::wstring> completed;
    std::map<std::wstring, std::pair<HWND, ULONGLONG>> submitted;
    std::string failure;
};

Driver* active = nullptr;

BOOL CALLBACK drive(HWND window, LPARAM)
{
    if (!active || !active->failure.empty())
    {
        return TRUE;
    }
    wchar_t title_buffer[256]{};
    GetWindowTextW(window, title_buffer, 256);
    const std::wstring title = title_buffer;
    if (active->completed.contains(title))
    {
        const auto previous = active->submitted.at(title);
        if (previous.first != window || GetTickCount64() - previous.second > 5000)
        {
            active->failure = "Library form reopened or did not advance after approval; inspect "
                              "the actual API/validation output";
            PostMessageW(window, WM_COMMAND, IDCANCEL, 0);
        }
        return TRUE;
    }
    try
    {
        if (title == L"GitHub / GitLab Repository")
        {
            require(GetWindowTextLengthW(GetDlgItem(window, 2000)) == 0,
                "Fresh Add reused the previous repository URL");
            SetWindowTextW(GetDlgItem(window, 2000), active->test->repository.c_str());
        }
        else if (title == L"Library Version")
        {
            SetWindowTextW(GetDlgItem(window, 2001), active->test->version.c_str());
        }
        else if (title == L"Approve dependency verification")
        {
            // Trusted public repositories are explicitly approved by this opt-in suite.
            // Disable examples/tests consistently; leave public module defaults intact.
            SetWindowTextW(GetDlgItem(window, 2003),
                L"BUILD_EXAMPLES=OFF\nBUILD_TESTING=OFF\nBUILD_SHARED_LIBS=OFF");
        }
        else if (title == L"Select Library Targets")
        {
            const auto list = GetDlgItem(window, 2000);
            require(!active->fresh || SendMessageW(list, LB_GETSELCOUNT, 0, 0) == 0,
                "Fresh Add silently selected a target");
            SendMessageW(list, LB_SETSEL, FALSE, -1);
            SendMessageW(GetDlgItem(window, 2003), LB_SETSEL, FALSE, -1);
            for (const auto& target : active->test->targets)
            {
                auto selected_list = list;
                auto index = SendMessageW(list,
                    LB_FINDSTRINGEXACT,
                    static_cast<WPARAM>(-1),
                    reinterpret_cast<LPARAM>(target.c_str()));
                if (index == LB_ERR && target == L"raylib")
                {
                    // A backing target is intentionally under Advanced when the
                    // actual graph supplies a preferred public alias.
                    selected_list = GetDlgItem(window, 2003);
                    index = SendMessageW(selected_list,
                        LB_FINDSTRINGEXACT,
                        static_cast<WPARAM>(-1),
                        reinterpret_cast<LPARAM>(target.c_str()));
                }
                require(
                    index != LB_ERR, "Required target was not discovered in the appropriate list");
                SendMessageW(selected_list, LB_SETSEL, TRUE, index);
            }
        }
        else
        {
            return TRUE;
        }
        if (IsWindowEnabled(GetDlgItem(window, IDOK)))
        {
            active->completed.insert(title);
            active->submitted[title] = {window, GetTickCount64()};
            PostMessageW(window, WM_COMMAND, IDOK, 0);
        }
    }
    catch (const std::exception& error)
    {
        active->failure = error.what();
        PostMessageW(window, WM_COMMAND, IDCANCEL, 0);
    }
    return TRUE;
}

void CALLBACK timer(HWND, UINT, UINT_PTR, DWORD)
{
    EnumThreadWindows(GetCurrentThreadId(), drive, 0);
}

BOOL CALLBACK visible_program(HWND window, LPARAM parameter)
{
    auto* process = reinterpret_cast<std::pair<DWORD, bool>*>(parameter);
    DWORD owner = 0;
    GetWindowThreadProcessId(window, &owner);
    if (owner == process->first && IsWindowVisible(window))
    {
        process->second = true;
    }
    return TRUE;
}

void run_consumer(const fs::path& project, bool graphical)
{
    ProjectOptions options;
    options.target = L"app";
    const auto targets = executable_targets(project, L"Debug", options);
    require(targets.size() == 1 && fs::is_regular_file(targets.at(0).artifact),
        "Generated consumer artifact was not discovered");
    const auto directory = managed_build_directory(project, L"Debug", options);
    ConsoleSession session;
    session.start_program(targets.at(0).artifact, {}, project, runtime_environment({}, directory));
    std::pair<DWORD, bool> window{session.process_id(), false};
    std::string output;
    const auto deadline = GetTickCount64() + 30000;
    while (session.running() && GetTickCount64() < deadline)
    {
        EnumWindows(visible_program, reinterpret_cast<LPARAM>(&window));
        for (const auto& chunk : session.poll_output())
        {
            output += chunk.bytes;
        }
        Sleep(10);
    }
    require(!session.running(), "Consumer timed out");
    for (const auto& chunk : session.poll_output())
    {
        output += chunk.bytes;
    }
    require(session.exit_code() == 0,
        "Consumer exited with " + std::to_string(session.exit_code()) + ": " + output);
    require(!graphical || window.second, "Graphical consumer did not create a visible window");
    require(output.find("CPPLIVELAB_CONSUMER_OK") != output.npos,
        "Consumer functionality marker missing");
}

LibraryRecord install(HWND owner,
    const fs::path& cli,
    const Case& test,
    const std::optional<LibraryRecord>& existing = {},
    LibraryEdit operation = LibraryEdit::Add)
{
    Driver driver{&test, !existing.has_value()};
    active = &driver;
    const auto timer_id = SetTimer(nullptr, 0, 100, timer);
    std::optional<std::string> id;
    try
    {
        id = github_library_dialog(
            owner,
            ThemeChoice::Dark,
            cli,
            [](const std::wstring& output)
            {
                std::wcout << output << std::flush;
            },
            existing,
            operation);
    }
    catch (...)
    {
        KillTimer(nullptr, timer_id);
        active = nullptr;
        throw;
    }
    KillTimer(nullptr, timer_id);
    active = nullptr;
    require(driver.failure.empty(), driver.failure);
    require(id.has_value(), "Actual Add/Manage Library verification workflow failed");
    require(driver.completed.size() == (!existing                                    ? 4
                                           : operation == LibraryEdit::UpdateVersion ? 3
                                                                                     : 2),
        "Expected actual library forms were not all exercised");
    const auto record = known_library(*id);
    require(record.has_value() && record->revision.size() == 40,
        "Library workflow did not record an immutable commit");
    return *record;
}

void write_consumer(const fs::path& project, const std::string& source)
{
    require(static_cast<bool>(std::ofstream(project / L"src/main.cpp", std::ios::binary) << source),
        "Cannot write isolated consumer fixture");
}

bool integrate(
    const fs::path& project, const std::vector<LibraryRecord>& records, bool build = true)
{
    const auto change = preview_library_change(project, records, "app");
    ProjectOptions options;
    options.target = L"app";
    options.allow_configure_writes = true;
    options.library_transaction = change.token;
    return apply_library_change(change,
        true,
        [&]
        {
            return !build || recover_managed_build(project, L"Debug", options, true) == 0;
        });
}

void multiple(const fs::path& root,
    HWND owner,
    const fs::path& cli,
    const std::vector<LibraryRecord>& records,
    const std::vector<Case>& tests)
{
    const auto suffix = L"-" + std::to_wstring(GetTickCount64());
    const auto project = root / (L"three-libraries" + suffix);
    create_managed_project(project, "empty");
    write_consumer(project, tests[2].consumer);
    require(integrate(project, {records[2]}), "First fmt addition failed");
    const std::string combined = R"cpp(#include <fmt/format.h>
#include <nlohmann/json.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <iostream>
int main()
{
    const auto value = nlohmann::json::parse("{\"answer\":42}");
    if (fmt::format("{}", value.at("answer").get<int>()) != "42" || !glfwGetVersionString())
    {
        return 1;
    }
    std::cout << "CPPLIVELAB_CONSUMER_OK\n";
}
)cpp";
    auto two = tests[3].consumer;
    two.insert(0, "#include <fmt/format.h>\n");
    write_consumer(project, two);
    require(integrate(project, {records[2], records[3]}), "fmt followed by JSON failed");
    write_consumer(project, combined);
    require(integrate(project, {records[2], records[3], records[4]}),
        "Three compatible actual libraries failed");
    run_consumer(project, false);
    ProjectOptions options;
    options.target = L"app";
    options.allow_configure_writes = true;
    const auto artifact = executable_targets(project, L"Debug", options).at(0).artifact;
    const auto timestamp = fs::last_write_time(artifact);
    require(managed_build(project, L"Debug", options, false, true) == 0 &&
                fs::last_write_time(artifact) == timestamp,
        "Unchanged project was unnecessarily relinked");
    require(integrate(project, {records[2], records[4]}, false), "Removing only JSON failed");
    require(managed_build(project, L"Debug", options, false, false) != 0,
        "Consumer using a removed dependency falsely built successfully");
    require(integrate(project, {records[2], records[3], records[4]}), "Re-adding JSON failed");
    run_consumer(project, false);
    auto newer_case = tests[3];
    newer_case.version = L"v3.12.0";
    const auto updated = install(owner, cli, newer_case, records[3], LibraryEdit::UpdateVersion);
    require(updated.revision != records[3].revision, "Version update retained the old commit");
    require(
        integrate(project, {records[2], updated, records[4]}), "Actual JSON version update failed");
    require(project_libraries(root / L"consumer-3").at(0).revision == records[3].revision,
        "Version update silently changed a different project's pin");
    run_consumer(project, false);
    for (const bool reverse : {false, true})
    {
        const auto ordered =
            root / ((reverse ? L"sfml-then-raylib" : L"raylib-then-sfml") + suffix);
        create_managed_project(ordered, "empty");
        const auto first = reverse ? 1 : 0;
        const auto second = reverse ? 0 : 1;
        write_consumer(ordered, tests[first].consumer);
        require(integrate(ordered, {records[first]}), "First graphical library addition failed");
        auto source = tests[0].consumer;
        source.insert(0, "#include <SFML/Network.hpp>\n");
        source.insert(source.find("    std::cout"), "    sf::Packet packet;\n    packet << 42;\n");
        write_consumer(ordered, source);
        require(integrate(ordered, {records[first], records[second]}),
            "Raylib/SFML repeated additions failed or duplicated dependency targets");
        run_consumer(ordered, true);
    }
    auto target_case = tests[1];
    target_case.targets = {L"SFML::System"};
    const auto changed = install(owner, cli, target_case, records[1], LibraryEdit::ChangeTargets);
    require(!integrate(root / L"consumer-1", {changed}) &&
                project_libraries(root / L"consumer-1").at(0).targets == records[1].targets,
        "Failed actual SFML target change lost working Graphics/Audio links");
    run_consumer(root / L"consumer-1", true);
    // Project-specific pins survive deletion of the catalog recipe.
    delete_known_library(updated.id, true);
    require(project_libraries(project).at(1).revision == updated.revision,
        "Catalog removal broke project-specific updated metadata");
    require(managed_build(root / L"consumer-2", L"Debug", options, false, true) == 0,
        "Switching/reopening the original fmt project reused incompatible dependency state");
    std::cout << "LIVE_MANAGEMENT repeated additions, both graphical orders, three libraries, "
                 "no-work build, removal/re-add, version update, independent pins, failed-target "
                 "rollback, "
                 "catalog removal and project reopening passed\n";
}

std::string consumer_source(const char* name)
{
    const auto path = fs::path(CPPLIVELAB_TEST_SOURCE_DIR) / L"tests/consumers" / name;
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "Consumer fixture source is unavailable");
    return {std::istreambuf_iterator<char>(input), {}};
}

std::vector<Case> cases()
{
    return {{L"https://github.com/raysan5/raylib",
                L"6.2",
                {L"raylib"},
                consumer_source("raylib.cpp"),
                true},
        {L"https://github.com/SFML/SFML",
            L"3.1.0",
            {L"SFML::Graphics", L"SFML::Window", L"SFML::System", L"SFML::Audio", L"SFML::Network"},
            consumer_source("sfml.cpp"),
            true},
        {L"https://github.com/fmtlib/fmt", L"11.0.2", {L"fmt::fmt"}, consumer_source("fmt.cpp")},
        {L"https://github.com/nlohmann/json",
            L"v3.11.3",
            {L"nlohmann_json::nlohmann_json"},
            consumer_source("json.cpp")},
        {L"https://github.com/glfw/glfw", L"3.4", {L"glfw"}, consumer_source("glfw.cpp"), true},
        {L"https://github.com/libsdl-org/SDL",
            L"release-3.4.18",
            {L"SDL3::SDL3"},
            consumer_source("sdl3.cpp"),
            true}};
}

std::string read_file(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

std::string nonempty_dependency_lines(const std::string& bytes)
{
    // Replacing owned blocks preserves surrounding blank lines. They do not
    // invalidate a verified consumer; every nonempty CMake line must still match.
    std::istringstream input(bytes);
    std::string result;
    for (std::string line; std::getline(input, line);)
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (!line.empty())
        {
            result += line + '\n';
        }
    }
    return result;
}

struct RetainFailedFixture
{
    workspace_test::Fixture& fixture;
    int exceptions = std::uncaught_exceptions();

    ~RetainFailedFixture()
    {
        if (std::uncaught_exceptions() > exceptions)
        {
            std::cout << "LIVE_FIXTURE_RETAINED " << fixture.root << '\n' << std::flush;
            // Only the test harness retains failed fixtures. This is separate
            // from application restart and adds no project checkpoint feature.
            fixture.root.clear();
        }
    }
};

std::optional<LibraryRecord> previously_verified(const fs::path& project,
    const Case& test,
    const std::set<std::pair<std::string, std::string>>& completed)
{
    if (!fs::is_regular_file(project / L"CMakeLists.txt"))
    {
        return {};
    }
    const auto records = project_libraries(project);
    if (records.size() != 1 || !completed.contains({records[0].repository, records[0].revision}))
    {
        return {};
    }
    auto repository = test.repository.substr(std::wstring_view(L"https://github.com/").size());
    std::transform(repository.begin(), repository.end(), repository.begin(), std::towlower);
    const std::string expected_repository(repository.begin(), repository.end());
    if (records[0].repository != expected_repository ||
        read_file(project / L"src/main.cpp") != test.consumer ||
        nonempty_dependency_lines(read_file(project / L"cmake/Dependencies.cmake")) !=
            nonempty_dependency_lines(managed_dependency_file(records, "app")))
    {
        return {};
    }
    ProjectOptions options;
    options.target = L"app";
    const auto targets = executable_targets(project, L"Debug", options);
    if (targets.size() != 1 || !fs::is_regular_file(targets[0].artifact))
    {
        return {};
    }
    return records[0];
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    try
    {
        const bool resuming = argc == 4 && std::wstring_view(argv[2]) == L"--resume";
        require(argc == 2 || argc == 3 || resuming,
            "Expected CLI path, optional library filter, or --resume <owned failed fixture>");
        workspace_test::Fixture fixture;
        if (resuming)
        {
            const auto resume = fs::weakly_canonical(argv[3]);
            require(resume.parent_path() == fs::weakly_canonical(fs::temp_directory_path()) &&
                        resume.filename().wstring().starts_with(L"cll5-") &&
                        read_file(resume / L".cpplivelab-live-fixture")
                                .find("CppLiveLab live test fixture") != std::string::npos,
                "Resume requires a previously owned marked temporary live-test fixture");
            require(fs::remove(fixture.root), "Cannot release unused empty fixture reservation");
            fixture.root = resume;
            SetEnvironmentVariableW(L"CPPLIVELAB_STATE_HOME", (resume / L"manager state").c_str());
        }
        RetainFailedFixture retain{fixture};
        std::ofstream(fixture.root / L".cpplivelab-live-fixture")
            << "CppLiveLab live test fixture\n";
        std::set<std::pair<std::string, std::string>> completed;
        std::ifstream progress(fixture.root / L"verified-consumers.txt");
        for (std::string repository, revision; progress >> repository >> revision;)
        {
            completed.emplace(repository, revision);
        }
        InitCommonControls();
        const auto owner = CreateWindowExW(0,
            L"STATIC",
            L"CppLiveLab live test owner",
            WS_OVERLAPPED,
            0,
            0,
            100,
            100,
            nullptr,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);
        require(owner != nullptr, "Cannot create test dialog owner");
        unsigned index = 0;
        const auto tests = cases();
        std::vector<LibraryRecord> records;
        for (const auto& test : tests)
        {
            const auto project = fixture.root / (L"consumer-" + std::to_wstring(index++));
            if (argc == 3 && test.repository.find(argv[2]) == std::wstring::npos)
            {
                continue;
            }
            if (resuming)
            {
                if (const auto verified = previously_verified(project, test, completed))
                {
                    records.push_back(*verified);
                    std::cout << "LIVE_RESULT_RESUMED " << verified->repository << " "
                              << verified->version << " " << verified->revision
                              << " prior configure/compile/link/run PASS; matching "
                                 "source/pins/artifact retained\n";
                    continue;
                }
            }
            if (!fs::exists(project))
            {
                create_managed_project(project, "empty");
            }
            const auto record = install(owner, argv[1], test);
            records.push_back(record);
            write_consumer(project, test.consumer);
            require(integrate(project, {record}),
                "Generated real consumer failed configuration/compile/link");
            run_consumer(project, test.graphical);
            require(static_cast<bool>(
                        std::ofstream(fixture.root / L"verified-consumers.txt", std::ios::app)
                        << record.repository << ' ' << record.revision << '\n'),
                "Cannot record verified consumer result");
            std::cout << "LIVE_RESULT " << record.repository << " " << record.version << " "
                      << record.revision << " configure=PASS compile/link=PASS run=PASS window="
                      << (test.graphical ? "PASS" : "N/A") << '\n'
                      << std::flush;
        }
        require(!records.empty(), "Library filter did not select any real consumer test");
        if (argc == 2 || resuming)
        {
            multiple(fixture.root, owner, argv[1], records, tests);
        }
        DestroyWindow(owner);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Live library failure: " << error.what() << '\n';
        return 1;
    }
}
