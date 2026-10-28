#include "cpplivelab/session.hpp"
#include "cpplivelab/projects.hpp"
#include "cpplivelab/doctor.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
namespace fs = std::filesystem;
int checks = 0;

void check(bool ok, const char* message)
{
    if (!ok)
    {
        throw std::runtime_error(message);
    }
    ++checks;
}

std::string contents(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}

void write(const fs::path& path, const std::string& value)
{
    std::ofstream(path, std::ios::binary) << value;
}

void wait_for_output(
    cpplivelab::ConsoleSession& session, std::string& captured_output, const std::string& token)
{
    const auto start = GetTickCount64();
    while (captured_output.find(token) == std::string::npos)
    {
        captured_output += session.poll();
        if (GetTickCount64() - start > 60000 || !session.running())
        {
            captured_output += session.poll();
            if (captured_output.find(token) == std::string::npos)
            {
                throw std::runtime_error(
                    "Expected output missing: " + token + "\n" + captured_output);
            }
        }
        Sleep(10);
    }
}

std::string wait_for_exit(cpplivelab::ConsoleSession& session)
{
    const auto start = GetTickCount64();
    std::string captured_output;
    while (session.running())
    {
        captured_output += session.poll();
        if (GetTickCount64() - start > 60000)
        {
            throw std::runtime_error("Process did not finish\n" + captured_output);
        }
        Sleep(10);
    }
    for (;;)
    {
        const auto part = session.poll();
        if (part.empty())
        {
            break;
        }
        captured_output += part;
    }
    return captured_output;
}

DWORD parse_child_process_id(const std::string& captured_output)
{
    return static_cast<DWORD>(
        std::stoul(captured_output.substr(captured_output.find("CHILD ") + 6)));
}

void verify_child_stopped(HANDLE child)
{
    check(child && WaitForSingleObject(child, 5000) == WAIT_OBJECT_0,
        "Owned descendant survived Stop/destruction");
    CloseHandle(child);
}

void test_console_io(const fs::path& fixture_executable)
{
    const auto test_directory = fixture_executable.parent_path();

    cpplivelab::ConsoleSession session;
    std::string captured_output;
    session.start(fixture_executable, {L"interactive"}, test_directory);
    wait_for_output(session, captured_output, "Name: ");
    check(session.send_line("Ada Lovelace"), "Name input failed");
    wait_for_output(session, captured_output, "First: ");
    check(session.send_line("2.5"), "First numeric input failed");
    wait_for_output(session, captured_output, "Second: ");
    check(session.send_line("3.75"), "Second numeric input failed");
    captured_output += wait_for_exit(session);
    check(captured_output.find("Ada Lovelace sum 6.25") != captured_output.npos,
        "Interactive output incorrect");
    check(captured_output.find("stderr visible") != captured_output.npos, "stderr not captured");
    check(session.exit_code() == 7, "Exit code lost");
    session.start(fixture_executable, {L"eof"}, test_directory);
    session.close_input();
    check(wait_for_exit(session).find("EOF received") != std::string::npos,
        "End Input did not deliver EOF");
    session.start(fixture_executable, {L"flood"}, test_directory);
    check(wait_for_exit(session).size() == 200000, "Output flood lost data or deadlocked");
    check(!session.output_pending(), "Output remained buffered after draining");
    session.start(fixture_executable, {L"hold"}, test_directory);
    captured_output.clear();
    wait_for_output(session, captured_output, "HOLD ");
    bool refused = false;
    try
    {
        session.start(fixture_executable, {L"flood"}, test_directory);
    }
    catch (...)
    {
        refused = true;
    }
    check(refused, "Second operation started while busy");
    int sent = 0;
    while (sent < 100 && session.send_line(std::string(4000, 'a')))
    {
        ++sent;
    }
    check(sent > 0 && sent < 100, "Input backpressure failed");
    session.stop();
    wait_for_exit(session);
    check(!session.running(), "Stop did not finish");
    session.start(fixture_executable, {L"eof"}, test_directory);
    session.close_input();
    wait_for_exit(session);
    check(session.exit_code() == 0, "Restart after Stop failed");
}

void test_process_tree_cleanup(const fs::path& fixture_executable)
{
    const auto test_directory = fixture_executable.parent_path();

    for (const auto& kind : {L"tree", L"orphan"})
    {
        cpplivelab::ConsoleSession session;
        std::string captured_output;
        session.start(fixture_executable, {kind}, test_directory);
        wait_for_output(session, captured_output, "HOLD ");
        HANDLE child = OpenProcess(SYNCHRONIZE, FALSE, parse_child_process_id(captured_output));
        check(session.running(), "Descendant tree incorrectly considered idle");
        session.stop();
        wait_for_exit(session);
        verify_child_stopped(child);
    }
    HANDLE child = nullptr;
    {
        cpplivelab::ConsoleSession session;
        std::string captured_output;
        session.start(fixture_executable, {L"tree"}, test_directory);
        wait_for_output(session, captured_output, "HOLD ");
        child = OpenProcess(SYNCHRONIZE, FALSE, parse_child_process_id(captured_output));
    }
    verify_child_stopped(child);
}

void test_project_builds(const fs::path& cli_executable, const fs::path& test_directory)
{

    SetEnvironmentVariableW(L"PATH", L"");
    const auto temporary_directory =
        fs::temp_directory_path() /
        (L"CppLiveLab Stage3 " + std::to_wstring(GetCurrentProcessId()));
    check(!fs::exists(temporary_directory), "Temporary test path already exists");

    struct Cleanup
    {
        fs::path temporary_directory;

        ~Cleanup()
        {
            std::error_code ec;
            fs::remove_all(temporary_directory, ec);
        }
    } cleanup{temporary_directory};

    const auto project_directory = temporary_directory / "User Project";
    cpplivelab::create_project(project_directory, true);
    check(cpplivelab::validate_gui_project(project_directory) ==
              fs::weakly_canonical(project_directory),
        "Project validation failed");
    bool rejected = false;
    try
    {
        cpplivelab::validate_gui_project(temporary_directory);
    }
    catch (...)
    {
        rejected = true;
    }
    check(rejected, "Invalid project folder accepted");
    rejected = false;
    try
    {
        cpplivelab::validate_gui_project(test_directory);
    }
    catch (...)
    {
        rejected = true;
    }
    check(rejected, "Manager tree accepted as user project");
    const auto cmake_before = contents(project_directory / "CMakeLists.txt");
    const std::string presets = "{\"version\":3}\n";
    write(project_directory / "CMakePresets.json", presets);
    write(project_directory / "CMakeUserPresets.json", presets);
    write(project_directory / "CMakeSettings.json", "{\"configurations\": []}\n");
    cpplivelab::ConsoleSession session, second_session;
    session.start(cli_executable, {L"run", project_directory.wstring()}, project_directory);
    std::string captured_output;
    wait_for_output(session, captured_output, "Your name: ");
    const auto program = project_directory / ".cpplivelab/build-Debug/app.exe";
    const auto original_time = fs::last_write_time(program);
    second_session.start(
        cli_executable, {L"build", project_directory.wstring()}, project_directory);
    auto denied = wait_for_exit(second_session);
    check(second_session.exit_code() != 0 &&
              denied.find("already building or running") != denied.npos,
        "Concurrent build ignored lock");
    check(fs::last_write_time(program) == original_time, "Running executable was rebuilt");
    session.stop();
    wait_for_exit(session);
    session.start(program, {}, project_directory);
    captured_output.clear();
    wait_for_output(session, captured_output, "Your name: ");
    second_session.start(
        cli_executable, {L"build", project_directory.wstring()}, project_directory);
    denied = wait_for_exit(second_session);
    check(second_session.exit_code() != 0 &&
              denied.find("Executable is running or not writable") != denied.npos,
        "External running executable not detected");
    session.stop();
    wait_for_exit(session);
    const auto cache_path = project_directory / ".cpplivelab/build-Debug/CMakeCache.txt";
    auto cache = contents(cache_path);
    const std::string entry = "CMAKE_CXX_STANDARD:UNINITIALIZED=20";
    const auto at = cache.find(entry);
    check(at != cache.npos, "C++20 cache entry missing");
    cache.replace(at, entry.size(), "CMAKE_CXX_STANDARD:UNINITIALIZED=23");
    write(cache_path, cache + "\nSTAGE3_KEEP:STRING=original\n");
    session.start(cli_executable, {L"run", project_directory.wstring()}, project_directory);
    captured_output.clear();
    wait_for_output(session, captured_output, "Your name: ");
    check(session.send_line("Grace Hopper"), "Calculator name send failed");
    wait_for_output(session, captured_output, "First number: ");
    check(session.send_line("10"), "Calculator first number send failed");
    wait_for_output(session, captured_output, "Second number: ");
    check(session.send_line("20"), "Calculator second number send failed");
    captured_output += wait_for_exit(session);
    check(session.exit_code() == 0 &&
              captured_output.find("Grace Hopper! Sum: 30") != captured_output.npos,
        "Rebuild/run after Stop failed");
    check(contents(cache_path).find("CMAKE_CXX_STANDARD:UNINITIALIZED=23") != std::string::npos,
        "Cached compiler choice overwritten");
    check(contents(cache_path).find("STAGE3_KEEP:STRING=original") != std::string::npos,
        "Custom cache option lost");
    check(contents(project_directory / "CMakeLists.txt") == cmake_before,
        "Project configuration overwritten");
    check(
        contents(project_directory / "CMakePresets.json") == presets, "CMake presets overwritten");
    check(contents(project_directory / "CMakeUserPresets.json") == presets,
        "User presets overwritten");
    check(contents(project_directory / "CMakeSettings.json") == "{\"configurations\": []}\n",
        "VS settings overwritten");
    check(fs::is_regular_file(cpplivelab::find_visual_studio_2022()), "VS2022 discovery failed");
    rejected = false;
    try
    {
        cpplivelab::open_in_visual_studio({}, project_directory);
    }
    catch (...)
    {
        rejected = true;
    }
    check(rejected, "Missing IDE not reported");
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    if (argc != 4)
    {
        return 2;
    }
    const fs::path fixture_executable = argv[1], cli_executable = argv[2];
    const std::wstring test_case = argv[3];
    const auto test_directory = fixture_executable.parent_path();
    try
    {
        if (test_case == L"session")
        {
            test_console_io(fixture_executable);
        }
        else if (test_case == L"tree")
        {
            test_process_tree_cleanup(fixture_executable);
        }
        else if (test_case == L"projects")
        {
            test_project_builds(cli_executable, test_directory);
        }
        else
        {
            return 2;
        }

        std::wcout << L"Stage 3 " << test_case << L": " << checks << L" checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Stage 3 failure: " << error.what() << '\n';
        return 1;
    }
}
