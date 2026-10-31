# Cpp — manual acceptance guide

Use the separate `build/redirection` executable for this change. Existing executable
names, manager state paths and project ownership markers intentionally remain stable.
The previous GUI in build/debug was left open and was not overwritten.

## Start and identify the new application

1. Close the existing CppLiveLab manager normally. Leave Visual Studio open if wanted.
   The manager has one instance per state root; otherwise the older instance will
   receive focus instead of launching the new build.
2. In an ordinary PowerShell window:

   ```powershell
   Set-Location 'C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab'
   .\build\redirection\cpplivelab.exe --version
   .\build\redirection\cpplivelab.exe doctor
   Start-Process .\build\redirection\cpplivelab-gui.exe
   ```

   Expected: version `Cpp 0.1.0`, discovered development tools, and a welcome screen
   labelled **Cpp — Make Your Life Easier**. GCC is optional with working MSVC.
3. Open a disposable normal CMake project. Expected: project name/path, Add Library,
   Manage Libraries, Build, Run, Stop, Restart and Auto Run. There is one Settings
   destination. Open Editor still launches the configured editor/Visual Studio 2022.

## Offline native window demo

4. Create a NEW external demo folder using the tested native sample. The guard below
   deliberately refuses to overwrite an existing folder:

   ```powershell
   Set-Location 'C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab'
   $demo = Join-Path $env:USERPROFILE 'CppDemoProjects\AutoRunWindow'
   if (Test-Path -LiteralPath $demo) { throw 'Choose a NEW demo folder; existing files are protected.' }
   .\build\redirection\cpplivelab.exe new $demo --template empty
   if ($LASTEXITCODE -ne 0) { throw 'Demo creation failed; stop here.' }
   Copy-Item -LiteralPath .\tests\consumers\native_window.cpp -Destination (Join-Path $demo 'src\main.cpp')
   Add-Content -LiteralPath (Join-Path $demo 'CMakeLists.txt') -Value 'target_link_libraries(app PRIVATE user32)'
   ```

   Expected: only this newly created demo is changed; manager and user build trees
   remain separate. No graphics library or download is required.
5. Open `$demo` in Cpp. Select Debug, Build, select `app`, then Run. Expected: the
   native window appears. Its title is `Cpp Auto Run Window Demo`.
6. Leave Auto Run OFF. Edit the window title string in `src/main.cpp` and save.
   Expected: the existing application keeps running without automatic restart.
7. Enable Auto Run, then change the title again and save. Expected: Build & System
   shows a replacement build while the old window stays usable. After success it
   closes gracefully and the new window opens with the new title.
8. Move and resize the application to a distinctive rectangle. Save another harmless
   source change. Expected: the replacement returns to that rectangle. Application
   memory resets; this is a fresh process, not state-preserving hot reload.
9. Add `intentional syntax error` to the source and save. Expected: compiler errors
   and **Build failed — previous application is still running.** The old window stays
   open. Remove the error and save: the next successful build restarts it.
10. Save several changes quickly, including another save while Building. Expected:
    one build at a time, later saves coalesced, and no obsolete replacement launched.
    Save identical content again: no new automatic build/restart.
11. Press Restart. Expected: a new process/window using the last successful artifact;
    no compilation. Try this with unsaved/invalid source: Restart still uses that
    successful artifact. Run performs the normal freshness/build checks instead.
12. Press Stop. Expected: graceful window closure, no immediate restart. A later save
    may build, but does not launch a stopped program. Explicit Run/Restart launches
    it. Closing/crashing a program does not create a restart loop.
13. Switch to Release after stopping, then Build/Run and repeat a save. Expected:
    Release uses its own build directories and the selected target. Debug remains
    separate. Repeat with a project containing multiple executables: choose the
    intended executable target before enabling Auto Run.

## Monitor/window acceptance (requires your hardware)

14. With Auto Run enabled and the demo running, test each placement independently,
    saving a comment after placing the old window:

    | Placement | Expected after restart |
    |---|---|
    | Normal custom position/size | Same outer rectangle |
    | Maximized primary monitor | Maximized primary monitor |
    | Maximized secondary monitor | Same monitor, maximized |
    | Maximized vertically arranged upper monitor | Upper monitor, maximized |
    | Right half / left half | Approximately the same rectangle |
    | Another monitor with different DPI | Same monitor and usable rectangle; inspect scaling |
    | Minimized | Visible normal placement, not an apparently missing program |
    | Old display disconnected/rearranged | Visible placement on an available display |

    Do not change the Windows display arrangement solely for automated tests.
    Mixed-DPI and physical multi-monitor cases require manual acceptance.
15. Repeat with your own disposable graphical demo that starts slowly, recreates its
    window, or has multiple top-level windows. Restoration waits up to 15 seconds and
    matches title/class, then class/enumeration order. Different classes/titles are
    more reliable than several indistinguishable windows. An unmatched/constraint-
    limited window reports partial restoration. Native Snap Group membership and
    exclusive fullscreen are not guaranteed; borderless fullscreen is best-effort.
16. Settings > Build Preferences: turn window restoration OFF and repeat. Expected:
    the application chooses its own startup placement. Set graceful timeout and
    optional force-stop. Force-stop is OFF by default. An application that ignores
    WM_CLOSE/EOF is retained after timeout; enable force-stop only if you want its
    owned process tree terminated after that timeout. EOF cannot be reopened for a
    retained console process; restarting that process is necessary to regain stdin.

## Library Manager (offline checks first)

17. On a disposable project, stop running operations. Press Add Library. Select local
    header-only directory, enter a name and an absolute include directory. Preview the
    exact CMake changes. Cancel once: all project files must remain unchanged. Repeat
    and explicitly approve: the consumer build verifies the integration.
18. Add a second library using Local CMake source and its ACTUAL exported target(s).
    Expected: the first remains in the project, both appear in Manage Libraries,
    and a consumer using both builds/runs. Upstream/local library source is preserved.
19. Test Installed CMake package with a configured package prefix/toolchain and actual
    targets. Precompiled libraries need an imported CMake target; a raw DLL alone is
    insufficient. Example targets to verify against the installed version:
    `raylib`, `SFML::Graphics`, `SDL3::SDL3`, `glfw`, `fmt::fmt`,
    `nlohmann_json::nlohmann_json`. These are not claims that those packages are
    installed or were exercised live during this local-only run.
20. Manage Libraries > Inspect: check version, method, path/package, targets, ownership
    and recorded status. Update availability is explicitly not checked automatically.
    Edit Integration changes supported target/options/path settings. FetchContent
    versions use Update Version's existing repository/commit approval workflow.
21. Remove one managed library using the preview/confirmation. Expected: only its
    owned integration is removed; the other library, manual CMake, source/assets and
    shared caches survive. Source that still uses the removed library may fail to
    build until you edit it. Close/reopen: remaining metadata persists.
22. Move a disposable local include/source directory to simulate a broken integration.
    Edit Integration to point at its new absolute path, approve and verify. Use
    Reinstall / Repair to revalidate the recorded recipe. Missing packages/configure
    errors must fail clearly and restore the previous configuration. Do not move
    real shared dependency directories for this test.
23. Try adding the same managed library twice or an already manually linked target.
    Expected: duplicate/overlap refusal with no changes. Cpp never assumes ownership
    of manual or transitive configuration. Review ambiguous/manual CMake yourself.
24. Network-dependent Raylib/SFML/SDL3/GLFW/fmt/JSON live tests remain deferred by your
    local-only instruction. Do not approve third-party execution in a library dialog
    unless you intend to authorize that exact repository/pin and project edit.

## Console and regression acceptance

25. Create/open the existing calculator demo, Run, enter a name and two numbers.
    Expected: prompts and sum; stdout stays neutral, stderr retains its theme color.
    Repeat after Auto Run and manual Restart. Clear each tab independently; input
    stays associated with the correct running process. End Input sends EOF.
26. Check Dark, Light and Follow Windows, resize, and use keyboard navigation. Expected:
    existing customized palettes/semantic contrast and one Settings entry. Program
    Console and Build & System stay separate and preserve compiler navigation.
27. Open FarmGame read-only and inspect project/target/editor controls. Do not approve
    library edits or configure-time writes unless you intend to authorize them.
    Building/running real projects is separate acceptance; no real-project mutation
    was performed for these automated tests.

## Repeat automated checks (ordinary PowerShell)

```powershell
Set-Location 'C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab'
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\format.ps1 -Check
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -BuildDirectory .\build\redirection -TestMode Quick -Jobs 4
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -BuildDirectory .\build\redirection -TestMode Full -TestFilter '^(redirection_|reliability_|stage5_(restart|library_|unified_(unit|projects)|ui_|github_versions|unit$|projects$|presets$)|stage[234]_|doctor_|maintenance_|cli_|test_selection)' -Jobs 4 -Doctor
```

The last command is deliberately a FILTERED local run, not unfiltered Full. It
excludes `stage5_unified_raylib` and `stage5_libraries`; the opt-in
`stage5_live_libraries` suite is also not selected. No regression was disabled or
removed. Close the GUI from build/redirection before rebuilding that executable.
