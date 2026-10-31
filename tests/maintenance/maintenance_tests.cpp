#include "cpplivelab/process.hpp"
#include "cpplivelab/projects.hpp"
#include "cpplivelab/session.hpp"
#include "process/output_decoder.hpp"
#include "projects/project_paths.hpp"
#include "process/windows_support.hpp"
#include "../workspace_test_support.hpp"
#include <fstream>
#include <algorithm>

#include <iostream>
#include <stdexcept>

namespace
{
namespace fs = std::filesystem;
int completed_checks = 0;

void require_condition(bool condition, const char* description)
{
    if (!condition)
    {
        throw std::runtime_error(description);
    }

    ++completed_checks;
}

std::string wait_for_session(cpplivelab::ConsoleSession& session)
{
    const auto start_time = GetTickCount64();
    std::string captured_output;

    while (session.running() || session.output_pending())
    {
        captured_output += session.poll();

        if (GetTickCount64() - start_time > 60000)
        {
            throw std::runtime_error("Maintenance session timed out\n" + captured_output);
        }

        Sleep(10);
    }

    captured_output += session.poll();
    return captured_output;
}

void test_directory_boundaries()
{
    using cpplivelab::path_support::is_within_directory;
    const fs::path manager_directory = L"C:\\Projects\\CppLiveLab";

    require_condition(is_within_directory(manager_directory, manager_directory),
        "Manager itself must be rejected");
    require_condition(is_within_directory(L"c:\\projects\\CPPLIVELAB\\src", manager_directory),
        "Case-insensitive child check failed");
    require_condition(
        is_within_directory(L"C:\\Projects\\CppLiveLab\\src\\..\\build", manager_directory),
        "Normalized child escaped guard");
    require_condition(
        is_within_directory(L"C:\\Projects\\CppLiveLab\\src", L"C:\\Projects\\CppLiveLab\\"),
        "Trailing separator changed the boundary");
    require_condition(
        !is_within_directory(L"C:\\Projects\\CppLiveLab-UserProjects", manager_directory),
        "Sibling confused with child");
    require_condition(
        !is_within_directory(L"C:\\Projects", manager_directory), "Parent is not a child");
    require_condition(!is_within_directory(L"D:\\UserProjects\\Calculator", manager_directory),
        "Other-drive project wrongly rejected");
    require_condition(!is_within_directory(L"\\\\server\\share\\Calculator", manager_directory),
        "UNC project wrongly rejected");

    const auto encoded_path =
        cpplivelab::path_support::path_utf8(fs::path(L"C:\\Projects\\\u6771\u4EAC"));
    require_condition(encoded_path.find("\xE6\x9D\xB1\xE4\xBA\xAC") != std::string::npos,
        "Diagnostic path lost Unicode characters");
}

void test_output_boundaries()
{
    const std::string encoded_text =
        "Price \xE2\x82\xAC / \xE6\x9D\xB1\xE4\xBA\xAC / \xF0\x9F\x9A\x80\n";
    const std::wstring expected_text = L"Price \u20AC / \u6771\u4EAC / \U0001F680\n";

    // Reproduce every possible split, not just splits between complete characters.
    for (size_t split_position = 0; split_position <= encoded_text.size(); ++split_position)
    {
        cpplivelab::ConsoleOutputDecoder decoder;
        auto decoded_text =
            decoder.append(std::string_view(encoded_text).substr(0, split_position));
        decoded_text += decoder.append(std::string_view(encoded_text).substr(split_position), true);
        require_condition(decoded_text == expected_text, "Split UTF-8 character was corrupted");
    }

    cpplivelab::ConsoleOutputDecoder decoder;
    std::wstring decoded_text;

    for (char byte : encoded_text)
    {
        decoded_text += decoder.append(std::string_view(&byte, 1));
    }

    decoded_text += decoder.append({}, true);
    require_condition(decoded_text == expected_text, "Byte-at-a-time UTF-8 output failed");
    require_condition(
        decoder.append("\xE2").empty(), "Incomplete suffix must wait for another read");
    require_condition(
        !decoder.append({}, true).empty(), "EOF silently discarded an unfinished character");
    decoder.append("\xE2");
    decoder.reset();
    require_condition(
        decoder.append("new run", true) == L"new run", "Old output leaked into another run");
    require_condition(cpplivelab::decode_console_text("ASCII diagnostics") == L"ASCII diagnostics",
        "ASCII output changed");
}

void test_handle_ownership()
{
    DWORD flags = 0;
    HANDLE released_handle = nullptr;

    {
        cpplivelab::windows_support::UniqueHandle event_handle;
        event_handle.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        require_condition(event_handle.value != nullptr, "Cannot create test event");
        require_condition(GetHandleInformation(event_handle.value, &flags) != FALSE,
            "RAII did not retain ownership");
        released_handle = event_handle.value;
    }

    require_condition(
        GetHandleInformation(released_handle, &flags) == FALSE, "RAII destructor leaked a handle");

    struct ExpectedUnwind
    {
    };

    try
    {
        cpplivelab::windows_support::UniqueHandle event_handle;
        event_handle.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        require_condition(event_handle.value != nullptr, "Cannot create exception test event");
        released_handle = event_handle.value;
        throw ExpectedUnwind{};
    }
    catch (const ExpectedUnwind&)
    {
        require_condition(GetHandleInformation(released_handle, &flags) == FALSE,
            "Exception path leaked a handle");
    }

    cpplivelab::windows_support::UniqueHandle empty_handle;
    empty_handle.reset(INVALID_HANDLE_VALUE);
    empty_handle.reset();
    require_condition(empty_handle.value == nullptr, "Empty/failed ownership reset incorrectly");
}

void test_argument_round_trip(const fs::path& fixture_executable)
{
    const std::vector<std::wstring> arguments{L"arguments",
        L"",
        L"path with spaces",
        L"tail\\",
        L"quoted\"text",
        L"C:\\path with spaces\\"};
    const auto working_directory = fixture_executable.parent_path();

    require_condition(
        cpplivelab::run_process(fixture_executable, arguments, working_directory) == 0,
        "CLI argument quoting changed");

    cpplivelab::ConsoleSession session;
    session.start(fixture_executable, arguments, working_directory);
    const auto captured_output = wait_for_session(session);
    require_condition(session.exit_code() == 0, "GUI worker argument quoting changed");
    require_condition(captured_output.find("Argument round-trip passed") != std::string::npos,
        "Argument fixture output not captured");
    workspace_test::Fixture owned;
    const auto script = owned.root / L"VsDevCmd.bat";
    std::ofstream(script) << "@echo off\n"
                             "echo setup>>\"%~dp0counter.txt\"\n"
                             "set VSCMD_ARG_TGT_ARCH=x64\n"
                             "set VSCMD_ARG_HOST_ARCH=x64\n"
                             "set INCLUDE=owned-fixture\n"
                             "set LIB=owned-fixture\n";
    auto prepared = cpplivelab::developer_environment(script);
    const auto check_preparation = [&](size_t expected)
    {
        cpplivelab::ConsoleSession worker;
        worker.start_worker(
            fixture_executable, {L"prepared-environment", script.wstring()}, owned.root, prepared);
        wait_for_session(worker);
        require_condition(worker.exit_code() == 0, "Prepared SDK worker failed");
        std::ifstream counter(owned.root / L"counter.txt");
        // cmd /u also writes redirected ECHO as UTF-16LE. Count newline bytes,
        // avoiding a phantom final line from the terminating UTF-16 NUL byte.
        const std::string bytes{std::istreambuf_iterator<char>(counter), {}};
        const auto lines = static_cast<size_t>(std::count(bytes.begin(), bytes.end(), '\n'));
        require_condition(lines == expected, "Prepared SDK reuse/fallback ran setup incorrectly");
    };
    check_preparation(1);
    const std::wstring contents(prepared.begin(), prepared.end());
    const auto architecture = contents.find(L"VSCMD_ARG_TGT_ARCH=x64");
    require_condition(architecture != contents.npos, "Owned SDK fixture lost its architecture");
    prepared[architecture + std::wstring_view(L"VSCMD_ARG_TGT_ARCH=x").size()] = L'8';
    prepared[architecture + std::wstring_view(L"VSCMD_ARG_TGT_ARCH=x8").size()] = L'6';
    check_preparation(2);
    prepared = cpplivelab::developer_environment(script);
    const std::wstring corrected(prepared.begin(), prepared.end());
    const auto marker = corrected.find(L"CPPLIVELAB_PREPARED_VS=");
    require_condition(marker != corrected.npos, "Owned SDK block lost its setup identity");
    prepared[marker + std::wstring_view(L"CPPLIVELAB_PREPARED_VS=").size()] = L'#';
    check_preparation(4);
}

void test_unicode_project(const fs::path& cli_executable)
{
    const auto temporary_directory =
        fs::temp_directory_path() /
        (L"CppLiveLab Maintenance " + std::to_wstring(GetCurrentProcessId()));
    require_condition(!fs::exists(temporary_directory), "Unique test directory already exists");

    // Cleanup is limited to the previously nonexistent, explicitly created test root.
    struct TestDirectoryCleanup
    {
        fs::path directory;

        ~TestDirectoryCleanup()
        {
            std::error_code ignored_error;
            fs::remove_all(directory, ignored_error);
        }
    } cleanup{temporary_directory};

    const auto project_directory = temporary_directory / L"\u6771\u4EAC";
    cpplivelab::create_project(project_directory, false);
    require_condition(cpplivelab::validate_gui_project(project_directory) ==
                          fs::weakly_canonical(project_directory),
        "Unicode project could not be selected");

    cpplivelab::ConsoleSession session;
    session.start_worker(cli_executable, {L"run", project_directory.wstring()}, project_directory);
    const auto captured_output = wait_for_session(session);
    require_condition(session.exit_code() == 0, "Unicode project build/run failed");
    require_condition(captured_output.find("Hello from C++20!") != std::string::npos,
        "Unicode project did not execute");
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    if (argc != 4)
    {
        return 2;
    }

    try
    {
        const std::wstring test_case = argv[1];

        if (test_case == L"paths")
        {
            test_directory_boundaries();
        }
        else if (test_case == L"output")
        {
            test_output_boundaries();
        }
        else if (test_case == L"handles")
        {
            test_handle_ownership();
        }
        else if (test_case == L"arguments")
        {
            test_argument_round_trip(argv[2]);
        }
        else if (test_case == L"unicode_project")
        {
            test_unicode_project(argv[3]);
        }
        else
        {
            return 2;
        }

        std::wcout << L"Maintenance " << test_case << L": " << completed_checks
                   << L" checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Maintenance regression failed: " << error.what() << '\n';
        return 1;
    }
}
