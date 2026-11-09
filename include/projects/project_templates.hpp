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
    bool assets = false,
    bool automatic_sources = false)
{
    std::string result = "cmake_minimum_required(VERSION 3.24)\n\nproject(" +
                         project_identifier(name) + " LANGUAGES C CXX)\n\n";
    result += "set(CMAKE_CXX_STANDARD 20)\n"
              "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
              "set(CMAKE_CXX_EXTENSIONS OFF)\n"
              "# This starter uses headers, not implicit C++ module imports.\n"
              "set(CMAKE_CXX_SCAN_FOR_MODULES OFF)\n\n";
    if (automatic_sources)
    {
        result += R"cmake(# EasierCpp starter: sources under src/ are discovered automatically.
option(EASIERCPP_AUTO_SOURCES "Discover implementation files under src/" ON)
if(EASIERCPP_AUTO_SOURCES)
    file(GLOB_RECURSE APP_SOURCE_CANDIDATES CONFIGURE_DEPENDS
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    )
    # Keep build outputs and vendored code out of the application target.
    set(APP_EXCLUDED_DIRECTORIES "build|build-[^/]*|out|dist|_deps|cmakefiles|cmake-build-[^/]*")
    string(APPEND APP_EXCLUDED_DIRECTORIES "|debug|release|external|vendor|third_party")
    string(APPEND APP_EXCLUDED_DIRECTORIES "|[.]git|[.]vs|[.]easiercpp|[.]cpplivelab")
    set(APP_SOURCES)
    foreach(APP_SOURCE IN LISTS APP_SOURCE_CANDIDATES)
        string(TOLOWER "${APP_SOURCE}" APP_SOURCE_LOWER)
        if(APP_SOURCE_LOWER MATCHES "/(${APP_EXCLUDED_DIRECTORIES})/")
            continue()
        endif()
        list(APPEND APP_SOURCES "${APP_SOURCE}")
    endforeach()
    list(SORT APP_SOURCES)
else()
    # Advanced manual mode: maintain this list when adding or removing files.
    set(APP_SOURCES
)cmake";
    }
    else
    {
        result += "# EasierCpp starter: explicit sources; add new files here.\n"
                  "add_executable(app\n";
    }
    for (const auto& source : sources)
    {
        result += std::string(automatic_sources ? "        \"" : "    \"") + source + "\"\n";
    }
    result += automatic_sources ? "    )\nendif()\n\nadd_executable(app ${APP_SOURCES})\n" : ")\n";
    result += "\ntarget_compile_features(app PRIVATE cxx_std_20)\n"
              "set_target_properties(app PROPERTIES CXX_EXTENSIONS OFF)\n"
              "target_include_directories(app PRIVATE \"${CMAKE_CURRENT_SOURCE_DIR}/include\")\n"
              "set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT app)\n";
    if (dependencies)
    {
        result += "\n# EasierCpp-managed dependency declarations.\n"
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

inline constexpr std::string_view gitignore = R"ignore(# EasierCpp state
/.easiercpp/
/.cpplivelab/

# Build directories
/build/
/out/
/dist/
/cmake-build-*/

# CMake and Ninja
CMakeFiles/
CMakeCache.txt
cmake_install.cmake
compile_commands.json
build.ninja
.ninja_deps
.ninja_log
_deps/
CMakeUserPresets.json

# Visual Studio
.vs/
*.user
*.suo

# Compiled outputs
*.obj
*.o
*.exe
*.dll
*.lib
*.exp
*.pdb
*.ilk
*.pch
*.a
*.so
*.dylib
*.idb
*.tlog/
)ignore";

inline std::string readme(std::string_view name)
{
    return "# " + std::string(name) + R"readme(

A C++20 CMake project created with EasierCpp.

## Layout

- src/ contains implementation files, including nested directories.
- include/ contains shared headers.
- assets/ contains runtime assets copied beside the executable after linking.
- cmake/Dependencies.cmake contains selected library integrations when present.

## Build and run

Open this folder in EasierCpp, select Debug or Release, then Build and Run.
Auto Run builds saved changes and replaces the application after a successful build.
Stop pauses automatic launch. Your preferred editor handles source editing.

With CMake, Ninja and a configured compiler environment, build independently:

```powershell
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
.\build\debug\app.exe
```

For Release, use build/release and -DCMAKE_BUILD_TYPE=Release.
With the Visual Studio generator use --config Debug or --config Release.
Installed tools and selected dependencies must be available for a build to succeed.

## Adding source files

New .cpp, .cc, .cxx and .c files under src/ are discovered on the next build.
Include headers normally and add their implementations under src/.
CMake CONFIGURE_DEPENDS checks the file list each build. File additions, renames,
and removals regenerate the build system. Ordinary edits remain incremental.
Ninja and Visual Studio are tested. Other generators may have limitations.
Keep build trees, generated build files, and dependencies outside src/.
Build/dependency subdirectories are excluded, including build, build-*, out, dist,
_deps, CMakeFiles, cmake-build-*, debug, release, external, vendor and third_party.
Names are checked without case sensitivity. Directory symlinks are not traversed.

## Advanced manual source management

Configure with -DEASIERCPP_AUTO_SOURCES=OFF to use the explicit APP_SOURCES list
in CMakeLists.txt. Add or remove entries there yourself in manual mode.
Switch back with -DEASIERCPP_AUTO_SOURCES=ON. In EasierCpp, an existing CMake
configure preset can set the same cache variable. Existing imported projects
retain their own configuration.

The .gitignore excludes build outputs and personal settings while retaining
sources, headers, assets, CMakeLists.txt and shared CMakePresets.json.
Git initialization and remote configuration are left to you.
)readme";
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

    InitWindow(screen_width, screen_height, "EasierCpp");
    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("Hello from EasierCpp!", 20, 20, 24, WHITE);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
)cpp";
} // namespace cpplivelab::project_templates
