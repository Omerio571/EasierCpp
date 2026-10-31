# Phase 3 manual acceptance

The shipped manager optimization is prepared-toolchain discovery reuse. PCH is a
separate, measured opt-in project experiment; your original FarmGame was not changed.
Start with a disposable project or the automated copied-game trial below.

1. **Launch the new application.** Close an older manager normally, then run in
   ordinary PowerShell:

   ```powershell
   Set-Location 'C:\Users\omer2\OneDrive\Desktop\Projects\EasierCpp'
   & '.\build\phase3\cpplivelab-gui.exe'
   ```

   Expected: the existing layout/colors, one Auto Run control and the usual manual
   Build/Run/Stop/Restart and editor commands. No separate Auto Build control.

2. **Check FarmGame manually.** Open
   `C:\Users\omer2\OneDrive\Desktop\Projects\FarmGame`, select Debug and its executable
   target, Build and Run. Let initial configuration/compilation finish. Check textures,
   gameplay/input and your normal working directory/arguments. Open Editor should still
   open your existing VS2022 workflow. Do not reorganize the project.

3. **Check the first automatic replacement.** Move/resize the game window. Turn Auto Run
   ON: enabling alone must not stop/relaunch it. Make a small intentional source edit
   and save. Expected: old game stays usable while compiling; a successful replacement
   returns to the same bounds. Repeat at least ten times, including maximization. Record
   slow samples as well as typical ones. Real OneDrive/antivirus effects can differ from
   temporary-copy measurements. Cold builds are slower than warm edits.

4. **Check reliability.** Save an intentional syntax error while the game is running.
   Expected: compiler errors appear and the old process remains usable. Fix/save, then
   save several distinct edits rapidly during compilation. Expected: the latest saved
   version runs, without overlapping workers or stale launch. Save identical contents:
   no new automatic build/restart. Turn OFF and save: no automatic work.

5. **Check manual controls and configurations.** Build/Run while OFF, then Restart and
   Stop. Expected: Restart reuses the last successful executable. Stop closes the owned
   process and pauses automatic launching while ON. Toggle OFF/ON or use Run/Restart to
   rearm; enabling alone still does nothing. Check Debug/Release independently using
   dependencies built for each configuration. Check your VS debugging normally; a
   successful PCH header smoke is not a substitute for a real Release dependency build.

6. **Check console programs.** Create a new calculator outside EasierCpp:

   ```powershell
   $project = Join-Path (Split-Path (Get-Location) -Parent) ('Phase3Calculator-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
   & '.\build\phase3\cpplivelab.exe' create $project --demo calculator
   & '.\build\phase3\cpplivelab-gui.exe' $project
   ```

   Build/Run and answer `Ada`, `2`, `3` with Enter/Send. Expected: sum 5. With ON, change
   a source message while it waits for input; the updated program should launch and
   accept input again. Repeat error/fix, OFF, manual Restart and Stop checks.

7. **Try the measured PCH configuration safely first.** Close other benchmarks/builds
   and run:

   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\benchmark-farm-auto-run.ps1 -ExecutableDirectory .\build\phase3 -FarmGameDirectory ..\FarmGame -ReportDirectory .\build\phase3-manual-pch -Samples 10 -PrecompiledHeaders
   ```

   Expected: actual game windows from a marked temporary copy, ten warm replacements
   plus a header change, placement assertions, CSV statistics and exit 0. The script
   verifies original input hashes and deletes only its owned copy/state. It requires
   the existing Debug raylib binary/headers at the documented FarmGame out/build path;
   it downloads nothing. Compare without `-PrecompiledHeaders` in another report directory.

8. **Review PCH before editing the real project's configuration.** The optional fragment
   is `docs/experiments/auto-run/FarmGamePch.cmake` in EasierCpp. To opt in after review,
   back up your current FarmGame CMakeLists.txt, copy the fragment into a new/non-conflicting
   `FarmGame/cmake/FarmGamePch.cmake`, and add this line after the executable/raylib setup:

   ```cmake
   include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FarmGamePch.cmake")
   ```

   This is an explicit project-owner edit, not something the manager already applied.
   The fragment defaults its `FARMGAME_USE_PCH` option to ON when included and applies
   only under MSVC. It excludes project headers, preserves debug flags, and costs about
   68 MB per build tree. Initial builds must generate PCH; warm source/header edits
   should benefit. Validate gameplay, Debug/Release dependencies and VS debugging before
   keeping it. To roll back, remove the include and rebuild/reconfigure; remove the copied
   fragment if no longer wanted. Avoid overwriting later unrelated CMake edits with a backup.

The previous intermittent first-restoration issue did not reproduce in these automated
copy/native checks. Test secondary/mixed-DPI monitors and disconnected-monitor fallback
on your hardware. Placement restoration does not preserve game memory or game state.

## Exact automated verification commands

Close the Phase 3 manager before rebuilding its executable. In the EasierCpp root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -BuildDirectory .\build\phase3 -TestMode Quick -Jobs 4
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -BuildDirectory .\build\phase3 -TestMode Full -TestFilter '^(library_compatibility_|redirection_|reliability_|stage5_(restart|library_|unified_(unit|projects)|ui_|github_versions|unit$|projects$|presets$)|stage[234]_|doctor_|maintenance_|cli_|test_selection)' -Jobs 4 -Doctor
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\benchmark-auto-run.ps1 -Executable .\build\phase3\cpplivelab.exe -ReportDirectory .\build\phase3-manual-small -ProfileBuild -Samples 10
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\benchmark-farm-auto-run.ps1 -ExecutableDirectory .\build\phase3 -FarmGameDirectory ..\FarmGame -ReportDirectory .\build\phase3-manual-farm -Samples 10
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\benchmark-asset-copy.ps1 -AssetsDirectory ..\FarmGame\Assets -ReportDirectory .\build\phase3-manual-assets -Samples 10
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\benchmark-build-slots.ps1 -Executable .\build\phase3\cpplivelab.exe -ReportDirectory .\build\phase3-manual-slots -Samples 8
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\check-pch-configurations.ps1 -Executable .\build\phase3\cpplivelab.exe -RaylibHeaders ..\FarmGame\out\build\x64-Debug\_deps\raylib-build\raylib\include -ReportDirectory .\build\phase3-manual-configurations
```

Run timing commands sequentially, not during tests/builds. Read `timings.csv`,
`profiles.csv`, `summary.csv` and raw `gui.log` for FarmGame. A/B warm rows are labelled
with their actual slot. The header row is excluded from warm statistics. P95 is nearest
rank, so ten-sample P95 equals the maximum. Benchmark scripts are experimental tools;
the shared-tree prototype does not change the application's production architecture.

The explicit Full filter selects 51 local tests and excludes the previously deferred
network/third-party tests. It is not unfiltered Full approval. Repository-wide format
check currently fails on the unchanged pre-existing performance_timer.hpp; changed C++
files pass scoped formatting. No compiler-cache installation is required or performed.
