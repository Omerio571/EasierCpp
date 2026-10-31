
cmake_minimum_required(VERSION 3.20)


# =============================================================================
# Validate required test configuration
# =============================================================================

# The calling CMakeLists.txt supplies the CTest executable, build directory,
# and build script. The configuration is supplied for multi-config generators
# such as Visual Studio.

foreach(required_variable IN ITEMS
    CTEST
    BUILD_DIR
    SCRIPT
)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR
            "Missing required variable: ${required_variable}"
        )
    endif()
endforeach()

if(NOT DEFINED TEST_CONFIG OR TEST_CONFIG STREQUAL "")
    set(TEST_CONFIG "Debug")
endif()


# =============================================================================
# Read registered CTest tests
# =============================================================================

# Query the configured build directory directly. A temporary directory
# containing only CTestTestfile.cmake may not reproduce the original build
# tree's registration context.
#
# CTest's JSON output includes test names and properties, allowing all
# selection rules to be validated without running the test executables.

execute_process(
    COMMAND
        "${CTEST}"
        --test-dir "${BUILD_DIR}"
        -C "${TEST_CONFIG}"
        --show-only=json-v1
    WORKING_DIRECTORY
        "${BUILD_DIR}"
    RESULT_VARIABLE
        listing_result
    OUTPUT_VARIABLE
        selection_json
    ERROR_VARIABLE
        listing_error
)

if(NOT "${listing_result}" STREQUAL "0")
    message(FATAL_ERROR
        "CTest listing failed:\n${listing_error}"
    )
endif()

string(
    JSON registered_test_count
    ERROR_VARIABLE json_error
    LENGTH "${selection_json}" tests
)

if(json_error)
    message(FATAL_ERROR
        "Could not parse CTest JSON output: ${json_error}"
    )
endif()

if(registered_test_count EQUAL 0)
    message(FATAL_ERROR
        "CTest registration is empty in: ${BUILD_DIR}"
    )
endif()


# =============================================================================
# Collect registered test names and labels
# =============================================================================

set(all_tests)
set(full_tests)
set(quick_tests)
set(library_tests)

math(EXPR last_test_index "${registered_test_count} - 1")

foreach(test_index RANGE 0 ${last_test_index})

    string(
        JSON test_name
        GET "${selection_json}" tests ${test_index} name
    )

    list(APPEND all_tests "${test_name}")

    string(
        JSON property_count
        LENGTH "${selection_json}" tests ${test_index} properties
    )

    if(property_count EQUAL 0)
        continue()
    endif()

    math(EXPR last_property_index "${property_count} - 1")

    foreach(property_index RANGE 0 ${last_property_index})

        string(
            JSON property_name
            GET "${selection_json}"
                tests ${test_index} properties ${property_index} name
        )

        if(property_name STREQUAL "DISABLED")

            string(
                JSON is_disabled
                GET "${selection_json}"
                    tests ${test_index} properties ${property_index} value
            )

            if(is_disabled)
                message(FATAL_ERROR
                    "A registered regression was disabled: ${test_name}"
                )
            endif()

        elseif(property_name STREQUAL "LABELS")

            string(
                JSON label_count
                LENGTH "${selection_json}"
                    tests ${test_index} properties ${property_index} value
            )

            if(label_count EQUAL 0)
                continue()
            endif()

            math(EXPR last_label_index "${label_count} - 1")

            foreach(label_index RANGE 0 ${last_label_index})

                string(
                    JSON label_name
                    GET "${selection_json}"
                        tests ${test_index}
                        properties ${property_index}
                        value ${label_index}
                )

                if(label_name STREQUAL "full")
                    list(APPEND full_tests "${test_name}")

                elseif(label_name STREQUAL "quick")
                    list(APPEND quick_tests "${test_name}")

                elseif(label_name STREQUAL "libraries")
                    list(APPEND library_tests "${test_name}")
                endif()

            endforeach()

        endif()

    endforeach()

endforeach()


# =============================================================================
# Verify complete test classification
# =============================================================================

list(SORT all_tests)
list(SORT full_tests)
list(SORT quick_tests)

# Every registered test must belong to the Full regression suite or the
# separately approved live-library suite.

set(classified_tests
    ${full_tests}
    ${library_tests}
)

list(REMOVE_DUPLICATES classified_tests)
list(SORT classified_tests)

if(NOT "${all_tests}" STREQUAL "${classified_tests}")
    message(FATAL_ERROR
        "Full and Libraries labels do not cover every registered test."
    )
endif()

# Live-library tests can download and execute third-party dependency code.
# They must remain excluded from normal Quick and Full regression runs.

foreach(test_name IN LISTS library_tests)

    if(test_name IN_LIST quick_tests OR test_name IN_LIST full_tests)
        message(FATAL_ERROR
            "Live-library test leaked into regular selection: ${test_name}"
        )
    endif()

endforeach()


# =============================================================================
# Define expected Quick regression tests
# =============================================================================

# This list is intentional. Changes to Quick selection must be reviewed,
# rather than silently adding expensive integration tests.

set(expected_quick_tests
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


# =============================================================================
# Define integration regression tests
# =============================================================================

# These tests cover workflows that may configure, compile, or execute projects.

set(integration_tests
    stage5_unified_projects
    stage5_unified_raylib
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
)


# =============================================================================
# Verify Quick and Full selection rules
# =============================================================================

list(SORT expected_quick_tests)

if(NOT "${quick_tests}" STREQUAL "${expected_quick_tests}")
    message(FATAL_ERROR
        "Quick selection changed unexpectedly.\n"
        "Expected: ${expected_quick_tests}\n"
        "Actual: ${quick_tests}"
    )
endif()

foreach(test_name IN LISTS
    expected_quick_tests
    integration_tests
)
    if(NOT test_name IN_LIST full_tests)
        message(FATAL_ERROR
            "Regression missing from Full: ${test_name}"
        )
    endif()
endforeach()

foreach(test_name IN LISTS integration_tests)

    if(test_name IN_LIST quick_tests)
        message(FATAL_ERROR
            "Expensive integration test selected by Quick: ${test_name}"
        )
    endif()

endforeach()


# =============================================================================
# Verify build-script default
# =============================================================================

# Full must remain the default approval suite. Quick is an explicit,
# faster selection and must not silently replace Full.

file(READ "${SCRIPT}" build_script_content)

string(
    FIND
    "${build_script_content}"
    "[string]$TestMode = 'Full'"
    default_full_position
)

if(default_full_position LESS 0)
    message(FATAL_ERROR
        "Build script must default to Full, not Quick."
    )
endif()


# =============================================================================
# Report successful validation
# =============================================================================

list(LENGTH quick_tests quick_test_count)
list(LENGTH full_tests full_test_count)

message(STATUS
    "Selection verified: "
    "Quick=${quick_test_count}; "
    "Full=${full_test_count}; "
    "default=Full"
)
