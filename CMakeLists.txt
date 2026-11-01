
cmake_minimum_required(VERSION 3.20)

project(
    CppLiveLab
    VERSION 0.1.0
    LANGUAGES CXX
)


# =============================================================================
# Core library
# =============================================================================

# Shared functionality used by the CLI, GUI, and test executables.

set(CPPLIVELAB_CORE_SOURCES
    src/build/build_workers.cpp
    src/libraries/library_compatibility.cpp
    src/process/application_windows.cpp
    src/libraries/libraries.cpp
    src/process/restart.cpp
    src/tools/tool_workflows.cpp
    src/projects/workspace.cpp
    src/projects/workspace_creation.cpp
    src/tools/editors.cpp
    src/cli/doctor.cpp
    src/process/process.cpp
    src/projects/projects.cpp
    src/process/session.cpp
    src/build/auto_build.cpp
    src/build/build_freshness.cpp
    src/process/console_model.cpp
    src/appearance/appearance.cpp
    src/process/output_decoder.cpp
)

add_library(
    cpplivelab_core
    ${CPPLIVELAB_CORE_SOURCES}
)

target_compile_definitions(
    cpplivelab_core
    PRIVATE
        CPPLIVELAB_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_include_directories(
    cpplivelab_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    cpplivelab_core
    PUBLIC
        advapi32
        user32
)

target_compile_features(
    cpplivelab_core
    PUBLIC
        cxx_std_20
)

set_target_properties(
    cpplivelab_core
    PROPERTIES
        CXX_EXTENSIONS OFF
)


# =============================================================================
# Compiler configuration
# =============================================================================

if(MSVC)
    # Interpret source files and product text as UTF-8 on Windows.
    target_compile_options(
        cpplivelab_core
        PUBLIC
            /utf-8
    )

    # Enable strict compiler warnings for maintained core code.
    target_compile_options(
        cpplivelab_core
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        cpplivelab_core
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()


# =============================================================================
# Command-line application
# =============================================================================

add_executable(
    cpplivelab
    src/main.cpp
    src/cli/workspace_cli.cpp
)

target_link_libraries(
    cpplivelab
    PRIVATE
        cpplivelab_core
)

target_compile_definitions(
    cpplivelab
    PRIVATE
        CPPLIVELAB_VERSION="${PROJECT_VERSION}"
)


# =============================================================================
# Windows graphical application
# =============================================================================

set(CPPLIVELAB_GUI_SOURCES
    src/gui/gui.cpp
    src/gui/console_view.cpp
    src/appearance/ui_theme.cpp
    src/gui/project_dialogs.cpp
    src/gui/library_workflow.cpp
    src/gui/library_manager.cpp
)

add_executable(
    cpplivelab-gui
    WIN32
    ${CPPLIVELAB_GUI_SOURCES}
)

target_compile_definitions(
    cpplivelab-gui
    PRIVATE
        UNICODE
        _UNICODE
)

target_link_libraries(
    cpplivelab-gui
    PRIVATE
        cpplivelab_core
        user32
        gdi32
        shell32
        ole32
        oleacc
        uuid
        comctl32
        uxtheme
        dwmapi
)

# The GUI supervises the CLI and requires its executable to be built.
add_dependencies(
    cpplivelab-gui
    cpplivelab
)


# =============================================================================
# Testing configuration
# =============================================================================

include(CTest)

if(BUILD_TESTING)

    # =========================================================================
    # Library compatibility tests
    # =========================================================================

    add_executable(
        library_compatibility_tests
        tests/libraries/library_compatibility_tests.cpp
    )

    target_link_libraries(
        library_compatibility_tests
        PRIVATE
            cpplivelab_core
    )

    add_test(
        NAME library_compatibility_unit
        COMMAND library_compatibility_tests
    )

    add_test(
        NAME library_compatibility_configured
        COMMAND library_compatibility_tests configured
    )

    set_tests_properties(
        library_compatibility_configured
        PROPERTIES
            TIMEOUT 180
    )


    # =========================================================================
    # Process redirection tests
    # =========================================================================

    add_executable(
        redirection_tests
        tests/process/redirection_tests.cpp
    )

    target_link_libraries(
        redirection_tests
        PRIVATE
            cpplivelab_core
    )

    add_test(
        NAME redirection_unit
        COMMAND redirection_tests
    )

    add_test(
        NAME redirection_libraries
        COMMAND redirection_tests libraries
    )

    add_test(
        NAME redirection_windows
        COMMAND redirection_tests windows
    )

    set_tests_properties(
        redirection_windows
        PROPERTIES
            TIMEOUT 30
    )

    set_tests_properties(
        redirection_libraries
        PROPERTIES
            TIMEOUT 300
    )


    # =========================================================================
    # Restart tests
    # =========================================================================

    add_executable(
        restart_gui_tests
        tests/gui/restart_gui_tests.cpp
    )

    target_link_libraries(
        restart_gui_tests
        PRIVATE
            cpplivelab_core
            user32
            advapi32
    )

    add_dependencies(
        restart_gui_tests
        cpplivelab-gui
    )

    add_test(
        NAME stage5_restart_gui
        COMMAND restart_gui_tests
            $<TARGET_FILE:cpplivelab-gui>
    )

    set_tests_properties(
        stage5_restart_gui
        PROPERTIES
            TIMEOUT 150
            RUN_SERIAL TRUE
    )

    add_executable(
        restart_tests
        tests/process/restart_tests.cpp
    )

    target_link_libraries(
        restart_tests
        PRIVATE
            cpplivelab_core
    )

    add_test(
        NAME stage5_restart
        COMMAND restart_tests
    )

    set_tests_properties(
        stage5_restart
        PROPERTIES
            TIMEOUT 90
    )


    # =========================================================================
    # Library management tests
    # =========================================================================

    add_executable(
        library_management_tests
        tests/libraries/library_management_tests.cpp
    )

    target_link_libraries(
        library_management_tests
        PRIVATE
            cpplivelab_core
    )

    foreach(case IN ITEMS
        metadata
        transactions
        caches
        discovery
        consumers
    )
        add_test(
            NAME stage5_library_${case}
            COMMAND library_management_tests
                ${case}
        )

        set_tests_properties(
            stage5_library_${case}
            PROPERTIES
                TIMEOUT 300
        )
    endforeach()


    # =========================================================================
    # Unified project tests
    # =========================================================================

    add_executable(
        unified_tests
        tests/projects/unified_tests.cpp
    )

    target_link_libraries(
        unified_tests
        PRIVATE
            cpplivelab_core
    )

    target_compile_definitions(
        unified_tests
        PRIVATE
            CPPLIVELAB_TEST_SOURCE="${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME stage5_unified_unit
        COMMAND unified_tests unit
    )

    add_test(
        NAME stage5_unified_projects
        COMMAND unified_tests projects
    )

    add_test(
        NAME stage5_unified_raylib
        COMMAND unified_tests
            raylib
            $<TARGET_FILE:cpplivelab>
    )

    set_tests_properties(
        stage5_unified_projects
        PROPERTIES
            TIMEOUT 240
    )

    set_tests_properties(
        stage5_unified_raylib
        PROPERTIES
            TIMEOUT 900
            LABELS integration
    )


    # =========================================================================
    # GUI audit tests
    # =========================================================================

    add_executable(
        ui_audit_tests
        tests/gui/ui_audit_tests.cpp
        src/gui/console_view.cpp
        src/appearance/ui_theme.cpp
        src/gui/project_dialogs.cpp
        src/gui/library_workflow.cpp
        src/gui/library_manager.cpp
    )

    target_compile_definitions(
        ui_audit_tests
        PRIVATE
            UNICODE
            _UNICODE
    )

    target_link_libraries(
        ui_audit_tests
        PRIVATE
            cpplivelab_core
            user32
            gdi32
            shell32
            ole32
            oleacc
            uuid
            comctl32
            uxtheme
            dwmapi
            gdiplus
    )

    add_dependencies(
        ui_audit_tests
        cpplivelab
    )

    add_test(
        NAME stage5_ui_audit
        COMMAND ui_audit_tests
    )

    add_test(
        NAME stage5_ui_layout
        COMMAND ui_audit_tests --quick
    )

    set_tests_properties(
        stage5_ui_audit
        PROPERTIES
            TIMEOUT 90
    )


    # =========================================================================
    # Workflow and GitHub library tests
    # =========================================================================

    add_executable(
        workflow_probe
        tests/tools/workflow_probe.cpp
    )

    target_link_libraries(
        workflow_probe
        PRIVATE
            cpplivelab_core
    )

    add_test(
        NAME stage5_github_versions
        COMMAND powershell.exe
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_SOURCE_DIR}/tests/github_library.ps1
            -Script ${CMAKE_SOURCE_DIR}/scripts/github-library.ps1
    )


    # =========================================================================
    # GUI forms tests
    # =========================================================================

    add_executable(
        ui_forms_tests
        tests/gui/ui_forms_tests.cpp
        src/gui/project_dialogs.cpp
        src/appearance/ui_theme.cpp
    )

    target_compile_definitions(
        ui_forms_tests
        PRIVATE
            UNICODE
            _UNICODE
    )

    target_link_libraries(
        ui_forms_tests
        PRIVATE
            cpplivelab_core
            user32
            gdi32
            shell32
            ole32
            oleacc
            uuid
            comctl32
            uxtheme
            dwmapi
            gdiplus
    )

    add_test(
        NAME stage5_ui_forms
        COMMAND ui_forms_tests
    )


    # =========================================================================
    # Stage 5 integration tests
    # =========================================================================

    add_executable(
        stage5_tests
        tests/projects/stage5_tests.cpp
    )

    target_link_libraries(
        stage5_tests
        PRIVATE
            cpplivelab_core
    )

    foreach(case IN ITEMS
        unit
        projects
        presets
        libraries
        pch
    )
        add_test(
            NAME stage5_${case}
            COMMAND stage5_tests
                ${case}
                $<TARGET_FILE:cpplivelab>
                $<TARGET_FILE:session_fixture>
        )

        set_tests_properties(
            stage5_${case}
            PROPERTIES
                TIMEOUT 600
        )
    endforeach()


    # =========================================================================
    # Performance measurement
    # =========================================================================

    # Optional measurement tool rather than an expensive approval test.

    add_executable(
        performance_probe
        tests/build/performance_probe.cpp
    )

    target_link_libraries(
        performance_probe
        PRIVATE
            cpplivelab_core
    )

    add_dependencies(
        performance_probe
        session_fixture
        cpplivelab
    )


    # =========================================================================
    # GUI workflow reliability tests
    # =========================================================================

    add_executable(
        gui_flow_tests
        tests/gui/gui_flow_tests.cpp
        src/gui/console_view.cpp
        src/appearance/ui_theme.cpp
        src/gui/project_dialogs.cpp
        src/gui/library_workflow.cpp
        src/gui/library_manager.cpp
    )

    target_compile_definitions(
        gui_flow_tests
        PRIVATE
            UNICODE
            _UNICODE
    )

    target_link_libraries(
        gui_flow_tests
        PRIVATE
            cpplivelab_core
            user32
            gdi32
            shell32
            ole32
            oleacc
            uuid
            comctl32
            uxtheme
            dwmapi
            gdiplus
    )

    add_dependencies(
        gui_flow_tests
        cpplivelab
    )

    add_test(
        NAME reliability_gui_flow
        COMMAND gui_flow_tests
            "${CMAKE_BINARY_DIR}/redirection-ui"
    )

    set_tests_properties(
        reliability_gui_flow
        PROPERTIES
            TIMEOUT 180
    )

    add_test(NAME window_management COMMAND gui_flow_tests --window-management)
    set_tests_properties(window_management PROPERTIES TIMEOUT 240)
    add_test(NAME compilation_workers COMMAND gui_flow_tests --compilation-workers)
    set_tests_properties(compilation_workers PROPERTIES TIMEOUT 600 RUN_SERIAL TRUE)


    # =========================================================================
    # Process and build reliability tests
    # =========================================================================

    add_executable(
        reliability_tests
        tests/process/reliability_tests.cpp
    )

    target_link_libraries(
        reliability_tests
        PRIVATE
            cpplivelab_core
    )

    add_test(
        NAME reliability_inheritance
        COMMAND reliability_tests
            inheritance
            $<TARGET_FILE:session_fixture>
    )

    set_tests_properties(
        reliability_inheritance
        PROPERTIES
            TIMEOUT 30
    )

    add_test(
        NAME reliability_lifecycle
        COMMAND reliability_tests
            lifecycle
            $<TARGET_FILE:session_fixture>
    )

    set_tests_properties(
        reliability_lifecycle
        PROPERTIES
            TIMEOUT 45
    )

    add_test(
        NAME reliability_queue
        COMMAND reliability_tests
            queue
            $<TARGET_FILE:session_fixture>
    )

    set_tests_properties(
        reliability_queue
        PROPERTIES
            TIMEOUT 180
    )

    add_test(
        NAME reliability_freshness
        COMMAND reliability_tests
            freshness
            $<TARGET_FILE:session_fixture>
    )

    set_tests_properties(
        reliability_freshness
        PROPERTIES
            TIMEOUT 240
    )


    # =========================================================================
    # Console view tests
    # =========================================================================

    add_executable(
        console_view_tests
        tests/gui/console_view_tests.cpp
        src/gui/console_view.cpp
    )

    target_compile_definitions(
        console_view_tests
        PRIVATE
            UNICODE
            _UNICODE
    )

    target_link_libraries(
        console_view_tests
        PRIVATE
            cpplivelab_core
            user32
            gdi32
    )

    add_test(
        NAME stage4_console_view
        COMMAND console_view_tests
    )


    # =========================================================================
    # Stage 4 build and console tests
    # =========================================================================

    add_executable(
        stage4_tests
        tests/build/stage4_tests.cpp
    )

    target_link_libraries(
        stage4_tests
        PRIVATE
            cpplivelab_core
    )

    foreach(case IN ITEMS
        watcher
        scheduler
        auto_build
        console_model
        channels
        appearance
        prompts
    )
        add_test(
            NAME stage4_${case}
            COMMAND stage4_tests
                ${case}
                $<TARGET_FILE:cpplivelab>
        )

        set_tests_properties(
            stage4_${case}
            PROPERTIES
                TIMEOUT 180
        )
    endforeach()


    # =========================================================================
    # Development environment diagnostics
    # =========================================================================

    add_executable(
        doctor_tests
        tests/cli/doctor_tests.cpp
    )

    target_link_libraries(
        doctor_tests
        PRIVATE
            cpplivelab_core
    )

    add_test(
        NAME doctor_unit
        COMMAND doctor_tests
    )


    # =========================================================================
    # Maintenance regression tests
    # =========================================================================

    add_executable(
        maintenance_tests
        tests/maintenance/maintenance_tests.cpp
    )

    target_link_libraries(
        maintenance_tests
        PRIVATE
            cpplivelab_core
    )

    foreach(case IN ITEMS
        paths
        output
        handles
        arguments
        unicode_project
    )
        add_test(
            NAME maintenance_${case}
            COMMAND maintenance_tests
                ${case}
                $<TARGET_FILE:session_fixture>
                $<TARGET_FILE:cpplivelab>
        )

        set_tests_properties(
            maintenance_${case}
            PROPERTIES
                TIMEOUT 120
        )
    endforeach()


    # =========================================================================
    # Session test fixture
    # =========================================================================

    # Child executable used by session, process, and maintenance tests.

    add_executable(
        session_fixture
        tests/process/session_fixture.cpp
    )

    target_link_libraries(
        session_fixture
        PRIVATE
            cpplivelab_core
    )

    target_compile_features(
        session_fixture
        PRIVATE
            cxx_std_20
    )


    # =========================================================================
    # Stage 3 session tests
    # =========================================================================

    add_executable(
        session_tests
        tests/process/session_tests.cpp
    )

    target_link_libraries(
        session_tests
        PRIVATE
            cpplivelab_core
    )

    foreach(case IN ITEMS
        session
        tree
        projects
    )
        add_test(
            NAME stage3_${case}
            COMMAND session_tests
                $<TARGET_FILE:session_fixture>
                $<TARGET_FILE:cpplivelab>
                ${case}
        )

        set_tests_properties(
            stage3_${case}
            PROPERTIES
                TIMEOUT 180
        )
    endforeach()


    # =========================================================================
    # Stage 2 integration test
    # =========================================================================

    add_test(
        NAME stage2_integration
        COMMAND powershell.exe
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/stage2.ps1
            -App $<TARGET_FILE:cpplivelab>
    )

    set_tests_properties(
        stage2_integration
        PROPERTIES
            TIMEOUT 300
    )


    # =========================================================================
    # Windows development environment discovery
    # =========================================================================

    if(WIN32)
        # Requires the installed Visual Studio C++ build tools.
        # Verifies discovery without relying on the user's PATH.

        add_test(
            NAME doctor_without_path
            COMMAND ${CMAKE_COMMAND}
                -DAPP=$<TARGET_FILE:cpplivelab>
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/doctor_no_path.cmake
        )
    endif()


    # =========================================================================
    # CLI contract tests
    # =========================================================================

    foreach(case IN ITEMS
        version
        help
        unknown
        extra
        doctor
    )
        add_test(
            NAME cli_${case}
            COMMAND ${CMAKE_COMMAND}
                -DAPP=$<TARGET_FILE:cpplivelab>
                -DCASE=${case}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli.cmake
        )
    endforeach()


    # =========================================================================
    # Test selection validation
    # =========================================================================

    add_test(
    NAME test_selection
    COMMAND ${CMAKE_COMMAND}
        "-DCTEST=${CMAKE_CTEST_COMMAND}"
        "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DSCRIPT=${CMAKE_SOURCE_DIR}/scripts/build.ps1"
        "-DTEST_CONFIG=$<CONFIG>"
        -P "${CMAKE_SOURCE_DIR}/tests/test_selection.cmake"
    )


    # =========================================================================
    # Full regression suite
    # =========================================================================

    # Label every currently registered regression as part of the full suite.
    # The optional live-library suite is deliberately registered afterward.

    get_property(
        all_tests
        DIRECTORY
        PROPERTY TESTS
    )

    set_property(
        TEST ${all_tests}
        APPEND PROPERTY
            LABELS full
    )


    # =========================================================================
    # Optional live library integration tests
    # =========================================================================

    # Explicitly opt-in because these tests download and execute trusted
    # third-party dependency CMake configuration and build scripts.

    add_executable(
        library_live_tests
        tests/libraries/library_live_tests.cpp
        src/gui/project_dialogs.cpp
        src/appearance/ui_theme.cpp
        src/gui/library_workflow.cpp
    )

    target_compile_definitions(
        library_live_tests
        PRIVATE
            UNICODE
            _UNICODE
            CPPLIVELAB_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )

    target_link_libraries(
        library_live_tests
        PRIVATE
            cpplivelab_core
            user32
            gdi32
            shell32
            ole32
            oleacc
            uuid
            comctl32
            uxtheme
            dwmapi
    )

    add_dependencies(
        library_live_tests
        cpplivelab
    )

    add_test(
        NAME stage5_live_libraries
        COMMAND library_live_tests
            $<TARGET_FILE:cpplivelab>
    )

    set_tests_properties(
        stage5_live_libraries
        PROPERTIES
            LABELS libraries
            TIMEOUT 3600
    )


    # =========================================================================
    # Quick regression suite
    # =========================================================================

    # Fast tests intended for frequent local validation.
    # Excludes expensive tests that repeatedly configure or build projects.

    set(quick_tests
        library_compatibility_unit
        redirection_unit
        redirection_windows
        stage5_restart
        stage5_library_metadata
        stage5_library_transactions
        stage5_library_caches
        stage5_unified_unit
        reliability_inheritance
        reliability_lifecycle
        stage4_console_view
        stage4_watcher
        stage4_scheduler
        stage4_console_model
        stage4_channels
        stage4_appearance
        stage4_prompts
        doctor_unit
        maintenance_paths
        maintenance_output
        maintenance_handles
        maintenance_arguments
        stage3_session
        stage3_tree
        doctor_without_path
        cli_version
        cli_help
        cli_unknown
        cli_extra
        cli_doctor
        test_selection
        stage5_unit
        stage5_ui_forms
        stage5_github_versions
        stage5_ui_layout
    )

    set_property(
        TEST ${quick_tests}
        APPEND PROPERTY
            LABELS quick
    )


    # =========================================================================
    # Integration regression suite
    # =========================================================================

    # Tests covering complete workflows, GUI interaction, project creation,
    # build freshness, and interactions between subsystems.

    set_property(
        TEST
            reliability_gui_flow
            reliability_queue
            reliability_freshness
            stage4_auto_build
            maintenance_unicode_project
            stage3_projects
            stage2_integration
            stage5_projects
            stage5_presets
            stage5_libraries
            stage5_ui_audit
        APPEND PROPERTY
            LABELS integration
    )

endif()
