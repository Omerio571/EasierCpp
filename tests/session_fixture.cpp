#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>

int wmain(int argc, wchar_t** argv)
{
    // Exercise actual Windows argv reconstruction, including cases a shell-style
    // quoting shortcut would corrupt. This is used by the maintenance regressions.
    if (argc >= 2 && std::wstring(argv[1]) == L"arguments")
    {
        const std::vector<std::wstring> expected_arguments{
            L"", L"path with spaces", L"tail\\", L"quoted\"text", L"C:\\path with spaces\\"};

        if (argc != static_cast<int>(expected_arguments.size()) + 2)
        {
            return 4;
        }

        for (size_t argument_index = 0; argument_index < expected_arguments.size();
            ++argument_index)
        {
            if (argv[argument_index + 2] != expected_arguments[argument_index])
            {
                return 5;
            }
        }

        std::cout << "Argument round-trip passed\n";
        return 0;
    }

    if (argc != 2)
    {
        return 2;
    }
    const std::wstring fixture_mode(argv[1]);
    if (fixture_mode == L"hold")
    {
        std::cout << "HOLD " << GetCurrentProcessId() << '\n' << std::flush;
        Sleep(INFINITE);
    }
    if (fixture_mode == L"tree" || fixture_mode == L"orphan")
    {
        std::vector<wchar_t> module_path_buffer(32768);
        GetModuleFileNameW(
            nullptr, module_path_buffer.data(), static_cast<DWORD>(module_path_buffer.size()));
        auto child_command_line = L"\"" + std::wstring(module_path_buffer.data()) + L"\" hold";
        STARTUPINFOW startup_information{};
        startup_information.cb = sizeof(startup_information);
        startup_information.dwFlags = STARTF_USESTDHANDLES;
        startup_information.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        startup_information.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        startup_information.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        PROCESS_INFORMATION process_information{};
        if (!CreateProcessW(module_path_buffer.data(),
                child_command_line.data(),
                nullptr,
                nullptr,
                TRUE,
                0,
                nullptr,
                nullptr,
                &startup_information,
                &process_information))
        {
            return 3;
        }
        CloseHandle(process_information.hThread);
        CloseHandle(process_information.hProcess);
        std::cout << "CHILD " << process_information.dwProcessId << '\n' << std::flush;
        if (fixture_mode == L"orphan")
        {
            return 0;
        }
        Sleep(INFINITE);
    }
    if (fixture_mode == L"flood")
    {
        std::cout << std::string(200000, 'x');
        return 0;
    }
    if (fixture_mode == L"eof")
    {
        std::string input_line;
        while (std::getline(std::cin, input_line))
        {
        }
        std::cout << "EOF received\n";
        return 0;
    }
    if (fixture_mode == L"interactive")
    {
        std::string user_name;
        double first_number = 0, second_number = 0;
        std::cout << "Name: " << std::flush;
        if (!std::getline(std::cin, user_name))
        {
            return 1;
        }
        std::cout << "First: " << std::flush;
        if (!(std::cin >> first_number))
        {
            return 1;
        }
        std::cout << "Second: " << std::flush;
        if (!(std::cin >> second_number))
        {
            return 1;
        }
        std::cout << user_name << " sum " << first_number + second_number << '\n';
        std::cerr << "stderr visible\n";
        return 7;
    }
    return 2;
}
