cmake_minimum_required(VERSION 3.20)
project(CppLiveLab VERSION 0.1.0 LANGUAGES CXX)

add_library(cpplivelab_core
  src/doctor.cpp
  src/process.cpp
  src/projects.cpp
  src/session.cpp
  src/auto_build.cpp
  src/build_freshness.cpp
  src/console_model.cpp
  src/appearance.cpp
  src/output_decoder.cpp)
target_compile_definitions(cpplivelab_core PRIVATE CPPLIVELAB_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(cpplivelab_core PUBLIC include)
target_link_libraries(cpplivelab_core PUBLIC advapi32)
target_compile_features(cpplivelab_core PUBLIC cxx_std_20)
set_target_properties(cpplivelab_core PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
  target_compile_options(cpplivelab_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(cpplivelab_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(cpplivelab src/main.cpp)
target_link_libraries(cpplivelab PRIVATE cpplivelab_core)
target_compile_definitions(cpplivelab PRIVATE CPPLIVELAB_VERSION="${PROJECT_VERSION}")
add_executable(cpplivelab-gui WIN32 src/gui.cpp src/console_view.cpp src/ui_theme.cpp)
target_compile_definitions(cpplivelab-gui PRIVATE UNICODE _UNICODE)
target_link_libraries(cpplivelab-gui PRIVATE cpplivelab_core user32 gdi32 shell32 ole32 uuid comctl32 uxtheme dwmapi)
add_dependencies(cpplivelab-gui cpplivelab)

include(CTest)
if(BUILD_TESTING)
  add_executable(gui_flow_tests tests/gui_flow_tests.cpp src/console_view.cpp src/ui_theme.cpp)
  target_compile_definitions(gui_flow_tests PRIVATE UNICODE _UNICODE)
  target_link_libraries(gui_flow_tests PRIVATE cpplivelab_core user32 gdi32 shell32 ole32 uuid comctl32 uxtheme dwmapi)
  add_dependencies(gui_flow_tests cpplivelab)
  add_test(NAME reliability_gui_flow COMMAND gui_flow_tests)
  set_tests_properties(reliability_gui_flow PROPERTIES TIMEOUT 180)
  add_executable(reliability_tests tests/reliability_tests.cpp)
  target_link_libraries(reliability_tests PRIVATE cpplivelab_core)
  add_test(NAME reliability_inheritance COMMAND reliability_tests inheritance $<TARGET_FILE:session_fixture>)
  set_tests_properties(reliability_inheritance PROPERTIES TIMEOUT 30)
  add_test(NAME reliability_lifecycle COMMAND reliability_tests lifecycle $<TARGET_FILE:session_fixture>)
  set_tests_properties(reliability_lifecycle PROPERTIES TIMEOUT 45)
  add_test(NAME reliability_queue COMMAND reliability_tests queue $<TARGET_FILE:session_fixture>)
  set_tests_properties(reliability_queue PROPERTIES TIMEOUT 180)
  add_test(NAME reliability_freshness COMMAND reliability_tests freshness $<TARGET_FILE:session_fixture>)
  set_tests_properties(reliability_freshness PROPERTIES TIMEOUT 240)
  add_executable(console_view_tests tests/console_view_tests.cpp src/console_view.cpp)
  target_compile_definitions(console_view_tests PRIVATE UNICODE _UNICODE)
  target_link_libraries(console_view_tests PRIVATE cpplivelab_core user32 gdi32)
  add_test(NAME stage4_console_view COMMAND console_view_tests)
  add_executable(stage4_tests tests/stage4_tests.cpp)
  target_link_libraries(stage4_tests PRIVATE cpplivelab_core)
  foreach(case IN ITEMS watcher scheduler auto_build console_model channels appearance)
    add_test(NAME stage4_${case} COMMAND stage4_tests ${case} $<TARGET_FILE:cpplivelab>)
    set_tests_properties(stage4_${case} PROPERTIES TIMEOUT 180)
  endforeach()
  add_executable(doctor_tests tests/doctor_tests.cpp)
  target_link_libraries(doctor_tests PRIVATE cpplivelab_core)
  add_test(NAME doctor_unit COMMAND doctor_tests)
  add_executable(maintenance_tests tests/maintenance_tests.cpp)
  target_link_libraries(maintenance_tests PRIVATE cpplivelab_core)
  foreach(case IN ITEMS paths output handles arguments unicode_project)
    add_test(NAME maintenance_${case} COMMAND maintenance_tests ${case}
      $<TARGET_FILE:session_fixture> $<TARGET_FILE:cpplivelab>)
    set_tests_properties(maintenance_${case} PROPERTIES TIMEOUT 120)
  endforeach()
  add_executable(session_fixture tests/session_fixture.cpp)
  target_link_libraries(session_fixture PRIVATE cpplivelab_core)
  target_compile_features(session_fixture PRIVATE cxx_std_20)
  add_executable(session_tests tests/session_tests.cpp)
  target_link_libraries(session_tests PRIVATE cpplivelab_core)
  foreach(case IN ITEMS session tree projects)
    add_test(NAME stage3_${case} COMMAND session_tests
      $<TARGET_FILE:session_fixture> $<TARGET_FILE:cpplivelab> ${case})
    set_tests_properties(stage3_${case} PROPERTIES TIMEOUT 180)
  endforeach()
  add_test(NAME stage2_integration COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
    -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/stage2.ps1 -App $<TARGET_FILE:cpplivelab>)
  set_tests_properties(stage2_integration PROPERTIES TIMEOUT 300)
  if(WIN32)
    # Integration test requires the installed VS C++ tools used to build this project.
    add_test(NAME doctor_without_path COMMAND ${CMAKE_COMMAND}
      -DAPP=$<TARGET_FILE:cpplivelab>
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/doctor_no_path.cmake)
  endif()
  foreach(case IN ITEMS version help unknown extra doctor)
    add_test(NAME cli_${case} COMMAND ${CMAKE_COMMAND}
      -DAPP=$<TARGET_FILE:cpplivelab> -DCASE=${case}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli.cmake)
  endforeach()
endif()
