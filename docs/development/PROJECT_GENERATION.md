# Generated project source discovery

New standard projects created by New Project, the managed CLI, and the calculator
generator discover .cpp, .cc, .cxx and .c files recursively under src/. CMake's
CONFIGURE_DEPENDS updates the build after additions, renames and removals. Headers
are included normally and are never compiled independently.

The shared templates provide include/, assets/, .gitignore and a project README.
The executable remains app. Dependencies keep their existing modular configuration
and approval requirements. Single-file wrappers explicitly compile their selected
file. Imported and previously generated projects retain their own configuration.

## Manual management

For advanced projects, configure with -DEASIERCPP_AUTO_SOURCES=OFF and maintain
the generated APP_SOURCES list in CMakeLists.txt. Switch back with ON. An existing
configure preset can supply this cache variable when building through EasierCpp.
The README provides standalone Ninja commands. This option adds no Workspace control.

## Limits and performance

Keep generated code, build outputs and dependency sources outside src/. Reserved
subdirectories build, build-*, out, dist, _deps, CMakeFiles, cmake-build-*, debug,
release, external, vendor and third_party are excluded without case sensitivity.
Git, Visual Studio and manager metadata directories are excluded as well.
Directory symlinks are not followed by GLOB_RECURSE.
Use manual source management for layouts outside this convention.

CMake documents generator limitations and a file-list check on every build.
Source-list changes regenerate each independent build tree when next used.
Ordinary source edits continue to reuse object files, dependency builds and caches.
Ninja's dry-run output for CONFIGURE_DEPENDS cannot prove freshness by itself.
EasierCpp keeps its conservative check and may run a no-op build on a manual Run.
It never skips a required dependency check or runs a stale failed build.

## Manual acceptance

1. Create a new multi-file project outside the EasierCpp folder. Check README.md,
   .gitignore, src/, include/ and assets/. Build and Run Debug, then Release.
2. Create include/Game.hpp declaring int game(). Add src/Game.cpp implementing
   it and call game() from main.cpp. Build without editing CMakeLists.txt.
3. Enable Auto Run with a program that stays open. Move Game.cpp into src/game/,
   rename it, and save. Expect a successful replacement.
4. Add a temporary implementation, then remove it. Expect subsequent builds to
   update their source lists. Introduce an error, then fix it. The working
   application should survive the failed build and update after recovery.
5. Build again without edits. Expect no compilation or linking. Build outputs,
   external/, include/ and assets/ implementation files should not be collected.
6. In a developer PowerShell, configure a separate tree with
   cmake -S . -B build/manual -G Ninja -DEASIERCPP_AUTO_SOURCES=OFF.
   Add the new implementation to APP_SOURCES yourself and build that tree.
7. Open an existing imported project and inspect its CMake and .gitignore.
   Opening or building it should preserve its chosen source-management policy.

See [CMake's file documentation](https://cmake.org/cmake/help/latest/command/file.html)
for CONFIGURE_DEPENDS and generator limitations.
