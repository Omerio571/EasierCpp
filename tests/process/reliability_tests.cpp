#include "cpplivelab/session.hpp"
#include "cpplivelab/process.hpp"
#include "cpplivelab/projects.hpp"
#include "build/auto_build.hpp"
#include "build/build_lease.hpp"
#include "build/build_freshness.hpp"
#include "cpplivelab/doctor.hpp"
#include "process/windows_support.hpp"
#include <iostream>
#include <stdexcept>
#include <fstream>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::string finish(cpplivelab::ConsoleSession& session, ULONGLONG timeout = 15000)
{
    const auto started = GetTickCount64();
    std::string output;
    while (session.running() || session.output_pending())
    {
        output += session.poll();
        if (GetTickCount64() - started >= timeout)
        {
            std::string state = "Operation did not reach a terminal state. Root PID " +
                                std::to_string(session.process_id()) +
                                (session.root_exited() ? " has exited; owned PIDs:"
                                                       : " is still alive; owned PIDs:");
            for (const auto id : session.active_process_ids())
            {
                state += " " + std::to_string(id);
            }
            throw std::runtime_error(state + "\nCaptured output:\n" + output);
        }
        Sleep(5);
    }
    return output;
}

struct TestProject
{
    std::filesystem::path container =
        std::filesystem::temp_directory_path() /
        (L"CppLiveLab-reliability-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::path root = container / L"Project";

    TestProject()
    {
        require(!std::filesystem::exists(container), "Temporary test root already exists");
        cpplivelab::create_project(root, true);
    }

    ~TestProject()
    {
        std::error_code error;
        std::filesystem::remove_all(container, error);
    }
};

std::string contents(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}

void queue(const std::filesystem::path& fixture)
{
    TestProject project;
    const auto cli = fixture.parent_path() / L"EasierCpp.exe";
    cpplivelab::ConsoleSession build;
    const auto compile = [&]
    {
        build.start_worker(cli, {L"build", project.root.wstring(), L"--incremental"}, project.root);
        const auto output = finish(build, 60000);
        require(build.exit_code() == 0, output.c_str());
    };
    compile();
    const auto cmake = project.root / L"CMakeLists.txt";
    const auto original = contents(cmake);
    const auto executable = project.root / L".easiercpp/build-Debug/app.exe";
    const auto working_program = contents(executable);
    std::ofstream(cmake) << original
                         << "\nadd_custom_target(cancellation_hold ALL\n"
                            " COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/holding.flag\n"
                            " COMMAND ${CMAKE_COMMAND} -E sleep 30 VERBATIM)\n";
    build.start_worker(cli, {L"build", project.root.wstring(), L"--incremental"}, project.root);
    const auto started = GetTickCount64();
    while (!std::filesystem::exists(project.root / L".easiercpp/build-Debug/holding.flag"))
    {
        build.poll();
        require(build.running() && GetTickCount64() - started < 30000,
            "Real build did not reach the cancellation point");
        Sleep(10);
    }
    cpplivelab::AutoBuildScheduler scheduler;
    scheduler.enable(true);
    for (int save = 0; save < 1000; ++save)
    {
        scheduler.notify(save);
        scheduler.confirm(true, false);
        require(!scheduler.ready(save, true, true), "Queued changes overlapped a running build");
    }
    std::ofstream(project.root / L"src/calculator.cpp", std::ios::app)
        << "\n// latest pending edit\n";
    build.stop();
    finish(build);
    require(build.exit_code() != 0, "Cancelled compiler operation reported success");
    scheduler.cancel();
    require(!scheduler.pending(), "Cancellation retained automatic queue ownership");
    require(contents(executable) == working_program, "Cancellation destroyed a working executable");
    std::ofstream(cmake) << original;
    compile();
    std::ofstream(project.root / L"src/calculator.cpp")
        << "#error Executable is running or not writable - deliberate compiler diagnostic\n";
    build.start_worker(cli, {L"build", project.root.wstring(), L"--incremental"}, project.root);
    const auto diagnostic = finish(build, 60000);
    require(build.exit_code() != 0 && build.exit_code() != cpplivelab::build_deferred_exit_code &&
                diagnostic.find("error C1189") != std::string::npos,
        "A compiler diagnostic was confused with an explicit Deferred result");
    std::ofstream(project.root / L"src/calculator.cpp")
        << "#include \"calculator.hpp\"\ndouble add(double a, double b) { return a + b; }\n";
    compile();
}

void freshness_test(const std::filesystem::path& fixture)
{
    namespace fs = std::filesystem;
    TestProject project;
    const auto cli = fixture.parent_path() / L"EasierCpp.exe";
    const auto external_header = project.container / L"external.hpp";
    std::ofstream(external_header) << "constexpr double extra = 0;\n";
    std::ofstream(project.root / L"CMakeLists.txt", std::ios::app)
        << "\ntarget_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)\n";
    std::ofstream(project.root / L"src/calculator.cpp")
        << "#include \"calculator.hpp\"\n#include \"external.hpp\"\n"
           "double add(double a, double b) { return a + b + extra; }\n";
    cpplivelab::ConsoleSession worker;
    cpplivelab::BuildFreshness freshness;
    const cpplivelab::BuildIdentity debug{
        project.root, L"Debug", project.root / L".easiercpp/build-Debug/app.exe"};
    const cpplivelab::BuildIdentity release{
        project.root, L"Release", project.root / L".easiercpp/build-Release/app.exe"};
    const auto compile = [&](const cpplivelab::BuildIdentity& identity)
    {
        const auto before = freshness.begin(identity);
        worker.start_worker(cli,
            {L"build",
                project.root.wstring(),
                L"--config",
                identity.configuration,
                L"--incremental"},
            project.root);
        const auto output = finish(worker, 60000);
        require(worker.exit_code() == 0, output.c_str());
        require(
            freshness.succeeded(identity, before), "Successful unchanged inputs were not recorded");
    };
    require(!freshness.can_probe(debug), "Restart trusted an unobserved build");
    compile(debug);
    require(freshness.can_probe(debug) && !freshness.can_probe(release),
        "Debug success incorrectly marked Release current");
    const auto tools = cpplivelab::discover_toolchain(cpplivelab::system_discovery_inputs());
    fs::path ninja;
    for (const auto& tool : tools.tools)
    {
        if (tool.name == "ninja")
        {
            ninja = tool.installed_path;
        }
    }
    const auto environment = cpplivelab::developer_environment(tools.developer_command);
    const auto is_current = [&]
    {
        worker.start_program(ninja, {L"-n"}, debug.directory(), environment);
        const auto output = finish(worker);
        return cpplivelab::ninja_reports_current(output, worker.exit_code());
    };
    const auto timestamp = fs::last_write_time(debug.executable);
    require(is_current() && is_current(), "Unchanged successful build was not a Ninja no-op");
    require(fs::last_write_time(debug.executable) == timestamp,
        "Freshness probes rebuilt the executable");
    std::ofstream(external_header) << "constexpr double extra = 1;\n";
    require(freshness.can_probe(debug) && !is_current(),
        "Ninja did not detect a dependency outside the watched project");
    compile(debug);
    require(is_current(), "Dependency rebuild did not become current");
    std::ofstream(project.root / L"src/calculator.hpp", std::ios::app) << "// header change\n";
    require(!freshness.can_probe(debug), "Header content change did not invalidate freshness");
    compile(debug);
    compile(release);
    require(freshness.can_probe(debug) && freshness.can_probe(release),
        "Configuration observations were not independent");
    fs::last_write_time(debug.executable, timestamp);
    require(!freshness.can_probe(debug) && freshness.can_probe(release),
        "External artifact modification was trusted");
    fs::remove(debug.executable);
    require(!freshness.can_probe(debug), "Missing executable was considered current");
    compile(debug);
    const auto configured = contents(project.root / L"CMakeLists.txt");
    std::ofstream(project.root / L"src/unattached.cpp") << "int unattached() { return 42; }\n";
    require(!freshness.can_probe(debug), "Added source did not invalidate observation");
    compile(debug);
    require(is_current() && contents(project.root / L"CMakeLists.txt") == configured,
        "Unattached source was silently added to CMake");
    freshness.invalidate(debug);
    require(!freshness.can_probe(debug), "Failure/cancellation retained a successful observation");
    const auto before = freshness.begin(debug);
    std::ofstream(project.root / L"src/unattached.cpp", std::ios::app)
        << "// changed during build\n";
    require(!freshness.succeeded(debug, before), "Changes during a build were marked current");
    std::ofstream(project.root / L"CMakeLists.txt", std::ios::app)
        << "\nfile(GLOB extra_sources CONFIGURE_DEPENDS src/extra/*.cpp)\n"
           "target_sources(app PRIVATE ${extra_sources})\n";
    fs::create_directories(project.root / L"src/extra");
    std::ofstream(project.root / L"src/extra/one.cpp") << "int one() { return 1; }\n";
    compile(debug);
    require(!is_current(), "Configure-dependent globs incorrectly skipped Ninja regeneration");
    require(!cpplivelab::ninja_reports_current("echo ninja: no work to do.\n", 0) &&
                !cpplivelab::ninja_reports_current("ninja: no work to do.\n", 1),
        "Ambiguous or failed dependency probe was trusted");
}

void inheritance(const std::filesystem::path& fixture)
{
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    cpplivelab::windows_support::UniqueHandle unrelated_event;
    unrelated_event.reset(CreateEventW(&security, TRUE, FALSE, nullptr));
    require(unrelated_event.value != nullptr, "Event fixture unavailable");
    const auto number = std::to_wstring(reinterpret_cast<std::uintptr_t>(unrelated_event.value));
    cpplivelab::ConsoleSession session;
    session.start(fixture, {L"signal-handle", number}, fixture.parent_path());
    finish(session);
    require(WaitForSingleObject(unrelated_event.value, 0) == WAIT_TIMEOUT,
        "Confirmed: session child inherited an unrelated parent handle");
    require(cpplivelab::run_process(
                fixture, {L"signal-handle", number}, fixture.parent_path(), {}) == 1,
        "Synchronous launcher inherited an unrelated parent handle");

    cpplivelab::windows_support::UniqueHandle reader, writer;
    require(
        CreatePipe(&reader.value, &writer.value, &security, 0) != 0, "Pipe fixture unavailable");
    SetHandleInformation(reader.value, HANDLE_FLAG_INHERIT, 0);
    session.start(fixture, {L"hold"}, fixture.parent_path());
    writer.reset();
    DWORD available = 0;
    require(!PeekNamedPipe(reader.value, nullptr, 0, nullptr, &available, nullptr) &&
                GetLastError() == ERROR_BROKEN_PIPE,
        "An unrelated running child retained a foreign pipe writer and prevented EOF");
    session.stop();
    finish(session);
}

void lifecycle(const std::filesystem::path& fixture)
{
    cpplivelab::ConsoleSession build;
    for (int iteration = 0; iteration < 20; ++iteration)
    {
        // This fixture intentionally leaves a live child after its root exits.
        build.start(fixture, {L"orphan"}, fixture.parent_path());
        const auto started = GetTickCount64();
        while (!build.root_exited())
        {
            build.poll();
            require(GetTickCount64() - started < 5000, "Worker failed to exit");
            Sleep(5);
        }
        require(build.running() && !build.active_process_ids().empty(),
            "Lingering helper fixture was not active");
        require(build.exit_code() == 0, "Worker exit was confused with helper state");
        build.finish_build();
        finish(build);
        require(build.exit_code() == 0, "Helper cleanup changed successful worker exit");
        build.release_completed();
        require(!build.running() && build.process_id() == 0, "Completed build retained ownership");
        build.start(fixture, {L"hold"}, fixture.parent_path());
        build.stop();
        finish(build);
        build.release_completed();
    }
    const auto setup = std::filesystem::temp_directory_path() /
                       (L"CppLiveLab-setup-" + std::to_wstring(GetCurrentProcessId()) + L".bat");
    build.start_worker(fixture, {L"orphan"}, fixture.parent_path());
    finish(build);
    require(build.exit_code() == 0 && !build.running(),
        "Completed worker remained busy because of an owned helper");
    build.release_completed();
    require(!std::filesystem::exists(setup), "Setup fixture already exists");
    // Simulate a vendor setup script that leaves a background process holding
    // stdout open. The fixture session's job owns that process and removes it.
    std::ofstream(setup) << "@echo off\nstart \"\" /b \"" << fixture.string()
                         << "\" silent-hold\nexit /b 0\n";
    build.start(fixture, {L"capture-environment", setup.wstring()}, fixture.parent_path());
    const auto started = GetTickCount64();
    std::string output;
    while (!build.root_exited())
    {
        output += build.poll();
        require(GetTickCount64() - started < 5000, "Setup capture waited for a helper's pipe EOF");
        Sleep(5);
    }
    build.finish_build();
    output += finish(build);
    std::filesystem::remove(setup);
    require(output.find("ENVREADY") != std::string::npos && build.exit_code() == 0,
        "Setup environment was not captured after root exit");
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    try
    {
        require(argc == 3, "Expected test mode and fixture");
        if (std::wstring(argv[1]) == L"inheritance")
        {
            inheritance(argv[2]);
        }
        else if (std::wstring(argv[1]) == L"lifecycle")
        {
            lifecycle(argv[2]);
        }
        else if (std::wstring(argv[1]) == L"queue")
        {
            queue(argv[2]);
        }
        else if (std::wstring(argv[1]) == L"freshness")
        {
            freshness_test(argv[2]);
        }
        else
        {
            throw std::runtime_error("Unknown reliability test");
        }
        std::cout << "Reliability test passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
