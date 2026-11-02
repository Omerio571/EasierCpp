# Explicit, reusable PCH

PCH is off by default. The manager never guesses library headers or edits imported
source/CMake/preset files. Use this for stable headers that are expensive to parse;
measure warm builds before keeping it. Small targets may cost more to build cold.

## Configure a project

Stop its program/build and close the manager first. Create a **trusted, self-contained
CMake recipe outside the imported project**, replacing these names with actual targets:

```cmake
easiercpp_target_pch(my_app HEADERS <vector> <string> <iostream>)
easiercpp_target_pch(my_library HEADERS include/stable_library.hpp)
```

Headers belong to each target's include/definition/compiler context. Relative file
headers resolve against that target's SOURCE_DIR, including subdirectories. Angle
headers use its normal transitive include directories, so this works with different
libraries without a Raylib-specific list. Prefer stable standard/library headers;
avoid frequently edited project headers or headers with source-specific macro requirements.

Run in ordinary PowerShell (use your actual project/recipe paths):

```powershell
$cli = 'C:\Users\omer2\OneDrive\Desktop\Projects\EasierCpp\build\universal-pch\EasierCpp.exe'
& $cli pch-config 'C:\Projects\MyProject' --recipe 'C:\Recipes\my-project.cmake' --config Debug
& $cli manage-build 'C:\Projects\MyProject' --config Debug
& 'C:\Users\omer2\OneDrive\Desktop\Projects\EasierCpp\build\universal-pch\EasierCpp-Gui.exe' 'C:\Projects\MyProject'
```

The recipe is executable CMake configuration: review it before opting in. It is copied
to immutable manager-owned state, not included in place. Changing the original recipe
has no effect until you reapply `pch-config`; identical contents reuse the same snapshot.
Changed recipe bytes select a fresh isolated profile, even for comments, rather than
trying to infer the meaning of arbitrary CMake code. Existing outputs are retained.
Do not rely on relative `include()` paths from the recipe's original location.

Reopen the project after changing the setting. Its normal Build and Auto Run workers
load the saved setting. No separate automatic mode or new build scheduler exists.
Look for `EasierCpp PCH: enabled for ...` in Build & System during configuration.

Debug and Release are independent; opt in separately if wanted. Disable the manager
overlay with:

```powershell
& $cli pch-config 'C:\Projects\MyProject' --recipe off --config Debug
```

This returns to the non-overlay profile; it does not disable a project's own native PCH.
Reopen and Build normally. No source changes or destructive cache cleanup are required.
Settings changes refuse active build/program leases across the current profile's slots.

## Compatibility and fallback

- CMake 3.24+ manager workflow, Windows 11, MSVC and Clang (clang-cl tested). No tools
  are installed. Existing C++ standard, debug symbols, optimization/link flags and
  dependency settings remain authoritative.
- The default Ninja profile receives an external top-level hook. A deferred recipe runs
  after root/subdirectory targets exist. Only named concrete C++ targets are opted in;
  headers are PRIVATE and CXX-only, so C sources in mixed targets are not forced to use them.
- Imported/alias/interface targets, C-only/unknown-source targets, unsupported compilers,
  disabled PCH, existing target PCH/REUSE_FROM, and source-specific flags/definitions are
  conservatively skipped with diagnostics. Missing relative headers skip that target.
- An invalid angle header or incompatible opted-in C++ header can still fail compilation.
  Errors are shown normally; no hidden retry discards compiler errors. The existing
  Auto Run failure policy retains the working process. Fix the header/recipe or disable.
- **Explicit presets conservatively skip the automatic overlay**, preserving their
  existing top-level/package-manager hooks. To use the reusable module with such a
  preset, the project owner can explicitly compose it into an existing approved hook
  or include `cmake/EasierCppPch.cmake` and call its function after target definitions.
  Existing native CMake PCH with VS2022/Ninja Multi-Config continues to work. This release
  does not automatically rewrite/merge arbitrary preset inheritance or toolchain hooks.
- Each target/configuration/A/B slot owns its own PCH. There is no cross-slot binary
  copying or REUSE_FROM shortcut. Once both slots are warm, source-only saves retain
  unchanged PCH; included header/compile-definition changes rebuild it through CMake/Ninja.
- Project Settings preserves the saved PCH field. Existing settings without the new
  optional trailing field still load with PCH off; existing non-PCH directory keys stay unchanged.

## Manual acceptance

1. Build your library-dependent project with PCH off; record cold and several alternating
   warm source edits. Keep the same compiler/configuration and a quiet machine.
2. Apply a reviewed recipe, reopen, and Build. Expect one initial PCH build per target
   per slot; those cold builds may be slower. Repeat the warm edits and compare medians.
3. Save another source-only edit. Build output should compile the changed source and
   link, without a `cmake_pch` compilation. A no-op save should not restart at all.
4. Change a header included in PCH: expect its PCH and dependent objects to rebuild.
   A non-PCH project header should rebuild its consumers without rebuilding unrelated PCH.
5. While a valid application runs, introduce a source/PCH header error and save. Expect
   diagnostics and the same old process; fix/save and verify successful replacement.
6. Check runtime input/assets/DLLs/window placement and Debug/Release separately.
7. Disable if warm performance regresses, diagnostics reveal incompatibility, or cold
   build/disk costs outweigh the benefit. No PCH is enabled automatically on other projects.

Automated coverage: `stage5_pch` and `reliability_gui_flow`; exact outcomes and samples
are in the integration report. Primary API reference:
[CMake target_precompile_headers](https://cmake.org/cmake/help/latest/command/target_precompile_headers.html).
