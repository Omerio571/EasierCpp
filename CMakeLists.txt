cmake_minimum_required(VERSION 3.20)
project(CppLiveLab VERSION 0.1.0 LANGUAGES CXX)

add_library(cpplivelab_core src/doctor.cpp)
target_include_directories(cpplivelab_core PUBLIC include)
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

include(CTest)
if(BUILD_TESTING)
  add_executable(doctor_tests tests/doctor_tests.cpp)
  target_link_libraries(doctor_tests PRIVATE cpplivelab_core)
  add_test(NAME doctor_unit COMMAND doctor_tests)
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
