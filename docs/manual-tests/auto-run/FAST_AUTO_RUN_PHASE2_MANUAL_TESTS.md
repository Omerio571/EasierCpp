# Fast Auto Run Phase 2 acceptance

Use the new build below. Close an older manager normally first. These checks use a
new disposable project, without changing FarmGame or other existing projects.

1. **Create and open a calculator.** Run in ordinary PowerShell:

   ```powershell
   Set-Location 'C:\Users\omer2\OneDrive\Desktop\Projects\EasierCpp'
   $testProject = Join-Path (Split-Path (Get-Location) -Parent) ('EasierCppAcceptance-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
   & '.\build\fast-auto-run-phase2\cpplivelab.exe' create $testProject --demo calculator
   & '.\build\fast-auto-run-phase2\cpplivelab-gui.exe' $testProject
   ```

   Expected: one **Auto Run** control, no **Auto Build** button/menu, and the usual
   Project/Tools/Settings navigation, target/configuration and console controls.

2. **Check manual operation while OFF.** Set Auto Run OFF, Build, then Run. Answer
   the calculator prompts with `Ada`, `2`, `3`, using Enter/Send after each.
   Expected: sum `5`. Restart should run it again; Stop should stop its owned process.
   Repeat Build/Run in Release, then switch back to Debug.

3. **Check OFF and arming.** Open the project in your editor. While the calculator
   waits for input, save a comment in `src/main.cpp` with Auto Run OFF. Wait a second.
   Expected: no automatic build/restart. Turn ON and wait again: enabling alone must
   not stop/relaunch the program. Save a different comment: it should build and restart.

4. **Check failure and latest edits.** Introduce a syntax error and save with ON.
   Expected: compiler errors in Build & System, with the previous calculator still
   usable. Fix/save, then make several quick distinct edits while compilation runs.
   Expected: the final saved version runs; no overlapping builds or stale launch.
   Save identical contents again: no automatic build/restart.

5. **Check idle launch and Stop.** With ON, Stop the calculator. Expected: ON (paused);
   subsequent saves can build but must not relaunch. Toggle OFF/ON to rearm: enabling
   alone does nothing, but the next changed save should build and launch from idle.

6. **Check CMake regeneration.** In this disposable project's CMakeLists.txt, append
   `set_target_properties(app PROPERTIES OUTPUT_NAME calculator_phase2)` and save.
   Expected: CMake regenerates and the newly named executable runs with working input.
   A subsequent source-only save should reuse that target and configuration.

7. **Check graphical placement separately.** Follow section 6 of
   [the existing acceptance guide](FAST_AUTO_RUN_MANUAL_TESTS.md#6-graphical-window-restoration)
   using another disposable project, launched with the Phase 2 GUI above. Verify the
   first manual-Run-to-Auto-Run replacement, repeated replacements, moved/resized and
   maximized windows. Multi-monitor/mixed-DPI behavior needs your hardware check.

8. **Check your real project after the disposable checks.** Open FarmGame, verify its
   intended target, configuration, arguments and working directory, then Run and test
   a small authorized edit. Expected: assets/DLLs and window placement behave as before.
   No source, CMake, assets or directory restructuring is required by this optimization.

Cold toolchain setup and first compilation in each automatic build slot are slower.
Both slots retain independent object files and dependencies. Sub-second warm builds
are a goal, not a promise; compilation/linking remain the largest cost on the fixture.

## Repeat the automated local checks

Close this manager build normally before rebuilding its executable:

```powershell
Set-Location 'C:\Users\omer2\OneDrive\Desktop\Projects\EasierCpp'
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -BuildDirectory .\build\fast-auto-run-phase2 -TestMode Quick -Jobs 4
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -BuildDirectory .\build\fast-auto-run-phase2 -TestMode Full -TestFilter '^(library_compatibility_|redirection_|reliability_|stage5_(restart|library_|unified_(unit|projects)|ui_|github_versions|unit$|projects$|presets$)|stage[234]_|doctor_|maintenance_|cli_|test_selection)' -Jobs 4 -Doctor
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\benchmark-auto-run.ps1 -Executable .\build\fast-auto-run-phase2\cpplivelab.exe -ReportDirectory .\build\phase2-manual-benchmark -ProfileBuild
```

The filtered Full command selects the 51 permitted local tests, excluding the previously
deferred network/third-party library tests. It is not unfiltered Full approval.
The benchmark creates/removes only its own temporary project. Read `timings.csv`,
`profiles.csv` and the individual logs in the report directory. Run it without other
builds/tests for useful latency measurements. Omit `-ProfileBuild` for ordinary output.
