# Unified Auto Run acceptance guide

Use a new disposable project first. Automated tests did not edit FarmGame or another
existing user project. Close any older manager normally before using the updated build.

## 1. Launch and inspect

In ordinary PowerShell:

```powershell
Set-Location 'C:\Users\omer2\OneDrive\Desktop\Projects\EasierCpp'
& '.\build\fast-auto-run\cpplivelab-gui.exe'
```

Expected: the existing interface/themes, Project/Tools/Settings, console panes and manual
Build/Run/Stop/Restart remain. There is one Auto Run control and no Auto Build control.

## 2. Launch from idle

1. Create a new project outside the manager source/build directories. Open it in your
   preferred editor using Open Editor. Leave the program stopped.
2. Turn Auto Run ON. Verify that enabling it alone launches nothing.
3. Change a source message, save, and wait for the initial build/toolchain setup.

Expected: a successful relevant build selects the single executable target and runs it,
even though you had not manually run it. If there are several executable targets, use
manual Build/configure and choose the intended target first. A short-lived program may
exit immediately; its output remains in Program Console. Cold builds are slower.

## 3. Interactive console and ordinary controls

Use your disposable calculator or an existing input-waiting test program.

1. Build and Run manually. Answer its prompts with Send/Enter.
2. With Auto Run ON, save a source edit while the program waits for input.
3. Answer prompts again after replacement. Click Restart, then Stop.

Expected: working input/output after automatic and manual restart. Restart reuses the
last successful executable. Stop closes only the managed process tree and pauses
automatic launching. Auto Run shows **ON (paused)**; saves may still compile but cannot
launch. Run, Restart, or an OFF/ON toggle rearms automatic launching.

## 4. OFF and explicit manual requests

1. Turn Auto Run OFF. Save a source change. Wait at least one second.
2. Use manual Build and Run while OFF.
3. Turn ON, trigger an automatic build, then turn OFF before it finishes.
4. When an automatic build is active, explicitly queue Run, then turn OFF.

Expected: OFF starts no new automatic builds/restarts and cancels automatic workers
without killing the working program. Manual Build/Run still work. An explicit queued
manual Run is preserved when switching automation OFF.

## 5. Failures, rapid edits and no-op saves

1. Run a program that stays open. With Auto Run ON, add a syntax error and save.
2. Verify that errors appear and the old application remains usable.
3. Fix/save, then save several distinct edits rapidly during compilation.
4. Save identical contents again after things settle.

Expected: failed compilation does not replace the working application. The latest
successful source version is launched; obsolete results are not launched. Only one
automatic build runs at a time; edits can require one follow-up build. No-op saves do
not rebuild/restart. A program that exits or crashes does not relaunch in a loop;
a later relevant save can launch it again while Auto Run remains armed.

## 6. Graphical window restoration

For a dependency-free sample, copy `tests/consumers/native_window.cpp` into the generated
source of a NEW disposable project and add `target_link_libraries(app PRIVATE user32)`
to its CMakeLists.txt. Use the actual target name if different.

1. Run manually, move/resize its window, enable Auto Run and save a comment.
2. Check the first replacement and several subsequent replacements.
3. Maximize the window and repeat. Move it yourself after restoration and wait.
4. If available, repeat on a secondary/mixed-DPI monitor and test disconnected-monitor
   fallback. Keep unrelated application windows open to check that they remain untouched.

Expected: normal bounds or maximized state return. Successful restoration stops moving
the window afterward. Actual hardware-specific monitor/DPI behavior still needs manual
acceptance. This feature restores placement, not application memory or game state.

## 7. Project configuration and migration

1. Switch between Debug/Release while idle; Build/Run each configuration.
2. Check your target, arguments, working directory, assets and runtime DLL behavior.
3. Turn Auto Run OFF, close/reopen the manager/project and verify that OFF remains.
4. Check your previously configured debounce in Settings -> Build Preferences.

Expected: per-configuration options/builds remain independent. Explicit project ON/OFF
is preserved. Only a project without an Auto Run settings file imports the old global
Auto Build default. Corrupt project settings fail closed. Old global data is read-only
migration input; it is not another active mode. Existing debounce values are retained;
new profiles default to 150 ms. Stop's launch pause is session-local; opening a project
rearms an enabled mode. Enabling/opening only watches future meaningful saves.

## 8. Repeat automated checks

Close this build's manager normally before rebuilding its executable:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -BuildDirectory .\build\fast-auto-run -TestMode Quick -Jobs 4
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -BuildDirectory .\build\fast-auto-run -TestMode Full -TestFilter '^(library_compatibility_|redirection_|reliability_|stage5_(restart|library_|unified_(unit|projects)|ui_|github_versions|unit$|projects$|presets$)|stage[234]_|doctor_|maintenance_|cli_|test_selection)' -Jobs 4 -Doctor
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\benchmark-auto-run.ps1 -Executable .\build\fast-auto-run\cpplivelab.exe -ReportDirectory .\build\fast-auto-run-work\manual-benchmark
```

The filtered Full selection runs the 51 permitted local tests. It is not an unfiltered
Full/network-library approval run. The benchmark creates and removes only its own
temporary fixture and keeps CSV/log results under the requested report directory.
