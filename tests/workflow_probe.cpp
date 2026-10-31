#include "tool_workflows.hpp"
#include "cpplivelab/session.hpp"
#include "workspace_test_support.hpp"
#include <windows.h>
#include <fstream>
#include <iostream>

// Explicit, non-CTest integration driver. Keeping real suite invocation outside
// CTest prevents recursion; normal regressions use bounded controlled workers.
int wmain(int argc, wchar_t** argv)
{
    try
    {
        if (argc < 2)
        {
            throw std::runtime_error("Choose Quick, Full, or dependency <CLI> <source> <target>.");
        }
        cpplivelab::TestCommand command;
        std::unique_ptr<workspace_test::Fixture> fixture;
        if (std::wstring(argv[1]) == L"dependency")
        {
            if (argc != 5)
            {
                throw std::runtime_error(
                    "dependency requires CLI, downloaded source, actual target.");
            }
            fixture = std::make_unique<workspace_test::Fixture>();
            const auto project = fixture->root / L"actual dependency probe";
            std::filesystem::create_directory(project);
            std::ofstream(project / L"main.cpp") << "int main() { return 0; }\n";
            auto setup = cpplivelab::cmake_dependency_probe(argv[3]);
            // The same discovery/configuration helper as the real library dialog.
            std::ofstream(project / L"CMakeLists.txt") << setup;
            command = {argv[2],
                project,
                {L"manage-build",
                    project.wstring(),
                    L"--target",
                    L"cpplivelab_probe",
                    L"--allow-configure-writes"}};
            const auto run = [&](const cpplivelab::TestCommand& task)
            {
                cpplivelab::ConsoleSession session;
                session.start_worker(task.executable, task.arguments, task.repository);
                const auto start = GetTickCount64();
                while (session.running() || session.output_pending())
                {
                    std::cout << session.poll() << std::flush;
                    if (session.root_exited())
                    {
                        session.finish_build();
                    }
                    if (GetTickCount64() - start > 240000)
                    {
                        session.stop();
                        throw std::runtime_error("Dependency probe timed out.");
                    }
                    Sleep(10);
                }
                return session.exit_code();
            };
            if (const auto code = run(command))
            {
                return code;
            }
            std::wifstream targets(project / L"targets.txt");
            bool found = false;
            for (std::wstring target; std::getline(targets, target);)
            {
                if (target == argv[4])
                {
                    found = true;
                }
            }
            if (!found)
            {
                throw std::runtime_error("Requested target was not actually discovered by CMake.");
            }
            const auto target = std::filesystem::path(argv[4]).string();
            std::ofstream(project / L"CMakeLists.txt")
                << setup << "\ntarget_link_libraries(cpplivelab_probe PRIVATE " << target << ")\n";
            return run(command);
        }
        command = cpplivelab::test_command(argv[1], 4);
        cpplivelab::ConsoleSession session;
        session.start_worker(command.executable, command.arguments, command.repository);
        const auto started = GetTickCount64();
        while (session.running() || session.output_pending())
        {
            std::cout << session.poll() << std::flush;
            if (session.root_exited())
            {
                session.finish_build();
            }
            if (GetTickCount64() - started > 600000)
            {
                session.stop();
                throw std::runtime_error("Real suite runner timed out.");
            }
            Sleep(10);
        }
        std::cout << "Owned test runner exit: " << session.exit_code()
                  << "; seconds: " << (GetTickCount64() - started) / 1000.0 << '\n';
        return session.exit_code();
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
