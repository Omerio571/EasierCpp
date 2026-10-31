#pragma once

#include <string_view>
#include <string>
#include <vector>

namespace cpplivelab::project_templates
{
// Kept as readable source text rather than densely escaped C++ string fragments.
// These templates are written only when creating a new or empty user project.
// Names are identifiers, never executable CMake text. Keep Unicode in directory
// names; normalize unsupported identifier bytes deterministically for CMake.
inline std::string project_identifier(std::string_view name)
{
    std::string result;
    for (const unsigned char character : name)
    {
        result += (character >= 'a' && character <= 'z') ||
                          (character >= 'A' && character <= 'Z') ||
                          (character >= '0' && character <= '9') || character == '_'
                      ? static_cast<char>(character)
                      : '_';
    }
    if (result.empty() || (result.front() >= '0' && result.front() <= '9'))
    {
        result.insert(0, "Project_");
    }
    return result;
}

inline std::string cmake_project(std::string_view name,
    const std::vector<std::string>& sources,
    bool dependencies = false,
    bool assets = false)
{
    std::string result = "cmake_minimum_required(VERSION 3.24)\n\nproject(" +
                         project_identifier(name) + " LANGUAGES C CXX)\n\n";
    result += "set(CMAKE_CXX_STANDARD 20)\n"
              "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
              "set(CMAKE_CXX_EXTENSIONS OFF)\n"
              "# This starter uses headers, not implicit C++ module imports.\n"
              "set(CMAKE_CXX_SCAN_FOR_MODULES OFF)\n\n";
    result += "# CppLiveLab starter: explicit sources; add new files here.\n"
              "add_executable(app\n";
    for (const auto& source : sources)
    {
        result += "    \"" + source + "\"\n";
    }
    result += ")\n\ntarget_compile_features(app PRIVATE cxx_std_20)\n"
              "set_target_properties(app PROPERTIES CXX_EXTENSIONS OFF)\n"
              "target_include_directories(app PRIVATE \"${CMAKE_CURRENT_SOURCE_DIR}/include\")\n"
              "set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT app)\n";
    if (dependencies)
    {
        result += "\n# CppLiveLab-managed dependency declarations.\n"
                  "include(\"${CMAKE_CURRENT_SOURCE_DIR}/cmake/Dependencies.cmake\")\n";
    }
    if (assets)
    {
        result += R"cmake(
# Keep assets beside the actual Debug/Release executable.
add_custom_command(TARGET app POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:app>/assets"
    VERBATIM
)
)cmake";
    }
    return result;
}

inline constexpr std::string_view calculator_header = R"cpp(#pragma once

double add(double left, double right);
)cpp";

inline constexpr std::string_view calculator_implementation = R"cpp(#include "calculator.hpp"

double add(double left, double right)
{
    return left + right;
}
)cpp";

inline constexpr std::string_view calculator_main = R"cpp(#include "calculator.hpp"

#include <iostream>
#include <string>

int main()
{
    std::string user_name;
    double first_number = 0;
    double second_number = 0;

    std::cout << "Your name: ";

    if (!std::getline(std::cin, user_name))
    {
        return 1;
    }

    std::cout << "First number: ";

    if (!(std::cin >> first_number))
    {
        std::cerr << "Invalid number.\n";
        return 1;
    }

    std::cout << "Second number: ";

    if (!(std::cin >> second_number))
    {
        std::cerr << "Invalid number.\n";
        return 1;
    }

    std::cout << "Hello, " << user_name << "! Sum: " << add(first_number, second_number) << '\n';
}
)cpp";

inline constexpr std::string_view hello_main = R"cpp(#include <iostream>

int main()
{
    std::cout << "Hello from C++20!\n";
}
)cpp";

inline constexpr std::string_view raylib_main = R"cpp(#include <raylib.h>

int main()
{
    const int screen_width = 800;
    const int screen_height = 600;

    InitWindow(screen_width, screen_height, "CppLiveLab");
    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("Hello from CppLiveLab!", 20, 20, 24, WHITE);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
)cpp";
} // namespace cpplivelab::project_templates
