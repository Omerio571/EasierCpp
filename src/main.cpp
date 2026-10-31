#include "cpplivelab/doctor.hpp"
#include "cpplivelab/projects.hpp"
#include "build/build_lease.hpp"
#include "build/build_profile.hpp"
#include "cli/workspace_cli.hpp"
#include <iostream>
#include <string_view>
#include <stdexcept>

int wmain(int argc, wchar_t** argv)
{
    cpplivelab::BuildProfile::worker_start();
    try
    {
        if (argc == 1 || (argc == 2 && std::wstring_view(argv[1]) == L"--help"))
        {
            std::cout
                << "Cpp — Make Your Life Easier\nUsage: cpplivelab [--help | --version | doctor]\n"
                   "  cpplivelab create <folder> [--demo calculator]\n"
                   "  cpplivelab build <file.cpp|project-folder> [--config Debug|Release] "
                   "[--incremental]\n"
                   "  cpplivelab run <file.cpp|project-folder> [--config Debug|Release] "
                   "[--exe relative-path]\n"
                   "Run builds first, then launches with console input/output. Default "
                   "config: Debug.\n"
                   "Stage 5 isolated workflows:\n"
                   "  inspect <folder> | targets <folder> | edit <folder>\n"
                   "  new <folder> [--template empty|multi] [--library fmt,nlohmann_json]\n"
                   "  manage-build|manage-run <folder> [--config Debug|Release]\n"
                   "  manage-rebuild <folder> --approve-recovery [--config Debug|Release]\n"
                   "    [--preset name|-] [--target name] [--working-dir folder]\n"
                   "    [--vcpkg root] [--allow-configure-writes] [--incremental]\n";
            return 0;
        }
        const std::wstring_view command(argv[1]);
        if (const auto result = workspace_command(argc, argv))
        {
            return *result;
        }
        if (argc == 2 && command == L"--version")
        {
            std::cout << "Cpp " << CPPLIVELAB_VERSION << '\n';
            return 0;
        }
        if (argc == 2 && command == L"doctor")
        {
            return cpplivelab::run_doctor(
                std::cout, cpplivelab::discover_toolchain(cpplivelab::system_discovery_inputs()));
        }
        if (command == L"create" &&
            (argc == 3 || (argc == 5 && std::wstring_view(argv[3]) == L"--demo" &&
                              std::wstring_view(argv[4]) == L"calculator")))
        {
            cpplivelab::create_project(argv[2], argc == 5);
            return 0;
        }
        if ((command == L"build" || command == L"run") && argc >= 3)
        {
            std::string configuration = "Debug";
            std::filesystem::path executable;
            bool has_config = false, has_exe = false;
            bool incremental = false;
            for (int i = 3; i < argc;)
            {
                const std::wstring_view option(argv[i]);
                if (option == L"--incremental" && command == L"build" && !incremental)
                {
                    incremental = true;
                    ++i;
                    continue;
                }
                if (i + 1 >= argc)
                {
                    throw std::invalid_argument("Option requires a value.");
                }
                const std::wstring_view value(argv[i + 1]);
                if (option == L"--config" && !has_config)
                {
                    if (value != L"Debug" && value != L"Release")
                    {
                        throw std::invalid_argument("Configuration must be Debug or Release.");
                    }
                    configuration = value == L"Debug" ? "Debug" : "Release";
                    has_config = true;
                }
                else if (option == L"--exe" && command == L"run" && !has_exe)
                {
                    executable = argv[i + 1];
                    has_exe = true;
                }
                else
                {
                    throw std::invalid_argument("Unknown or duplicate option.");
                }
                i += 2;
            }
            return cpplivelab::build_project(
                argv[2], configuration, command == L"run", executable, incremental);
        }
        std::cerr << "Invalid arguments. Use cpplivelab --help.\n";
        return 2;
    }
    catch (const cpplivelab::BuildDeferredError& error)
    {
        std::cerr << "CppLiveLab build deferred: " << error.what() << '\n';
        return cpplivelab::build_deferred_exit_code;
    }
    catch (const std::invalid_argument& error)
    {
        std::cerr << "Invalid arguments: " << error.what() << '\n';
        return 2;
    }
    catch (const std::exception& error)
    {
        std::cerr << "CppLiveLab error: " << error.what() << '\n';
        return 1;
    }
}
