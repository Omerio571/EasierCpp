#include "cli/workspace_cli.hpp"
#include "cpplivelab/workspace.hpp"
#include "projects/project_paths.hpp"
#include <iostream>
#include <map>

std::optional<int> workspace_command(int argc, wchar_t** argv)
{
    using namespace cpplivelab;
    if (argc < 3)
    {
        return {};
    }
    const std::wstring command = argv[1];
    if (command != L"inspect" && command != L"manage-build" && command != L"manage-run" &&
        command != L"new" && command != L"targets" && command != L"edit" &&
        command != L"manage-rebuild" && command != L"pch-config" && command != L"manage-probe")
    {
        return {};
    }
    std::map<std::wstring, std::wstring> arguments;
    bool incremental = false, allow_writes = false;
    bool require_target = false;
    bool approved_recovery = false;
    for (int index = 3; index < argc; ++index)
    {
        const std::wstring option = argv[index];
        if (option == L"--require-target")
        {
            require_target = true;
            continue;
        }
        if (option == L"--approve-recovery")
        {
            approved_recovery = true;
            continue;
        }
        if (option == L"--incremental" || option == L"--allow-configure-writes")
        {
            (option == L"--incremental" ? incremental : allow_writes) = true;
            continue;
        }
        if (++index >= argc || arguments.contains(option))
        {
            throw std::invalid_argument("Missing option value or duplicate option.");
        }
        if (option != L"--recipe" && option != L"--config" && option != L"--preset" &&
            option != L"--target" && option != L"--working-dir" && option != L"--vcpkg" &&
            option != L"--template" && option != L"--library" && option != L"--transaction" &&
            option != L"--build-slot")
        {
            throw std::invalid_argument("Unknown Stage 5 option.");
        }
        arguments.emplace(option, argv[index]);
    }
    const auto root = std::filesystem::path(argv[2]);
    if (arguments.contains(L"--recipe") && command != L"pch-config")
    {
        throw std::invalid_argument("--recipe is only supported by pch-config.");
    }
    const auto configuration = arguments.contains(L"--config") ? arguments[L"--config"] : L"Debug";
    if (command == L"new")
    {
        create_managed_project(root,
            arguments.contains(L"--template") ? path_support::path_utf8(arguments[L"--template"])
                                              : "empty",
            arguments.contains(L"--library") ? path_support::path_utf8(arguments[L"--library"])
                                             : "none");
        return 0;
    }
    const auto inspected = inspect_project(root);
    if (command == L"pch-config")
    {
        if (!arguments.contains(L"--recipe"))
        {
            throw std::invalid_argument("Use --recipe <trusted-file.cmake|off>.");
        }
        configure_project_pch(inspected.root,
            configuration,
            arguments[L"--recipe"] == L"off" ? std::filesystem::path{}
                                             : std::filesystem::absolute(arguments[L"--recipe"]));
        return 0;
    }
    if (command == L"inspect")
    {
        std::cout << "Read-only CMake project: " << path_support::path_utf8(inspected.root)
                  << "\nSources/headers: " << inspected.sources.size()
                  << "\nCMakePresets: " << inspected.presets
                  << "; UserPresets: " << inspected.user_presets
                  << "\nConfigure file-writing commands: " << inspected.configure_writes << '\n';
        for (const auto& path : inspected.assets)
        {
            std::cout << "Asset directory: " << path_support::path_utf8(path) << '\n';
        }
        for (const auto& evidence : inspected.dependency_evidence)
        {
            std::cout << "Dependency evidence (not verified installed): " << evidence << '\n';
        }
        std::cout << "Compiler/generator/targets are resolved by CMake only on explicit Build.\n";
        return 0;
    }
    if (command == L"edit")
    {
        launch_editor(load_editor(), inspected.root);
        return 0;
    }
    auto options = load_project_options(inspected.root, configuration);
    if (arguments.contains(L"--preset"))
    {
        options.preset = arguments[L"--preset"];
    }
    if (arguments.contains(L"--target"))
    {
        options.target = arguments[L"--target"];
    }
    if (arguments.contains(L"--working-dir"))
    {
        options.working_directory = std::filesystem::absolute(arguments[L"--working-dir"]);
    }
    if (arguments.contains(L"--vcpkg"))
    {
        options.vcpkg = arguments[L"--vcpkg"];
    }
    options.allow_configure_writes = allow_writes;
    if (arguments.contains(L"--build-slot"))
    {
        options.recovery_slot = arguments[L"--build-slot"];
    }
    if (arguments.contains(L"--transaction"))
    {
        options.library_transaction = path_support::path_utf8(arguments[L"--transaction"]);
    }
    if (command == L"targets")
    {
        const auto targets = executable_targets(inspected.root, configuration, options);
        for (const auto& target : targets)
        {
            std::cout << path_support::path_utf8(target.name) << " => "
                      << path_support::path_utf8(target.artifact) << '\n';
        }
        if (targets.empty())
        {
            std::cout << "No configured executable targets; Build first.\n";
        }
        return 0;
    }
    if (command == L"manage-rebuild")
    {
        return recover_managed_build(inspected.root, configuration, options, approved_recovery);
    }
    if (command == L"manage-probe")
    {
        return probe_managed_launch(inspected.root, configuration, options);
    }
    return managed_build(inspected.root,
        configuration,
        options,
        command == L"manage-run",
        incremental,
        require_target);
}
