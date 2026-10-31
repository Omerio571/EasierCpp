# Optional, measured FarmGame experiment. Not installed into the real project.
# After reviewing the report, copy this file into your project's cmake/ directory
# and include it AFTER the executable and raylib include paths have been defined.
# CMake creates independent PCH artifacts for each target/configuration/build tree.
option(FARMGAME_USE_PCH "Precompile stable standard-library and raylib headers" ON)

if(MSVC AND FARMGAME_USE_PCH)
    target_precompile_headers(${PROJECT_NAME_SAFE} PRIVATE
        <string>
        <vector>
        <iostream>
        <cmath>
        <raylib.h>
        <raymath.h>
    )
endif()
