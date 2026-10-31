# Limited-budget Auto Run maintenance

Scope: the user's corrected 39% five-hour estimate and approximately 5% weekly
estimate. Live allowance is not visible to this agent. One small change was selected;
no window-management rewrite, dependency installation, staging architecture, branding
change, commit or push was attempted.

## Completed

When a successful replacement became obsolete because of a newer save, the GUI
unconditionally called `auto_build.confirm(true, true)`. That promoted ordinary
source changes to CMake reconfiguration requests. The follow-up now compares the
existing source fingerprints and requests configuration only for changed/added/
removed CMake inputs or presets. Already queued configuration requests remain set
by the existing scheduler. The same filename classification is shared with the
watcher's existing change handling.

Modified files:

- `src/auto_build.hpp`, `src/auto_build.cpp`: configuration-input comparison and shared
  classification; no additional filesystem traversal or background service.
- `src/gui.cpp`: preserve incremental mode for superseded source-only replacements.
- `tests/stage4_tests.cpp`: source-only versus CMake/preset addition/change/removal
  regression cases in the existing scheduler test.
- This report.

## First window restoration: not resolved

Inspection confirmed that placement is captured immediately before requesting shutdown,
and each program launch clears restored-window handles and starts its retry interval.
The existing native GUI regression already moves/resizes a manually launched window,
then checks its first automatic replacement and subsequent maximized replacements.

The reported first-only failure has not been reproduced in the affected real project,
and its root cause is not established. Window tracking/timing is therefore unchanged.
One possibility to investigate is application-side repositioning after the manager's
first successful restoration; this is a hypothesis, not a diagnosed cause.

## Build performance limits

The two build directories protect the currently running executable and its DLLs from
being overwritten. Both caches are retained and reused. Each cold slot still needs its
own configured build tree and compiled dependencies. Removing that first-use cost or
sharing compiled Raylib/GLFW outputs safely requires separate design/validation and was
deferred. No real-project rebuild, live dependency download or speed benchmark was run.
This fix avoids one unnecessary explicit reconfiguration path; it does not promise a
particular number of seconds saved.

## Verification

Formatting was applied and checked: clang-format 19.1.5, 83 maintained C++ files.
The initial build could not link `build/reliability/cpplivelab-gui.exe` because that
GUI was running (LNK1168). No user process was closed. Verification moved to an
isolated `build/optimization` directory.

Isolated build succeeded; script exit code **0**. Focused result: **6/6 passed**,
**97.38 seconds**, including the native GUI flow (97.16 seconds). The six tests were:
`stage4_scheduler`, `stage4_watcher`, `stage4_auto_build`, `redirection_windows`,
`reliability_freshness`, and `reliability_gui_flow`. This deliberately avoids exhaustive
or live-network suites, as requested for this budget-limited task.
`git diff --check` also passed. No related test failure remained after the isolated
build; the earlier link failure was caused by the running executable, not compilation.

Logs: `build/reliability-work/optimization-isolated-tests.log` and
`build/optimization/Testing/Temporary/LastTest.log`.

## Manual acceptance and next steps

1. Close the older manager normally before using the updated one; do not run both
   against the same user settings/project. Launch the executable below.
2. In your existing test project, Run manually, move/resize its graphical window,
   enable Auto Run and save a small source edit. Check the **first** replacement,
   then two subsequent replacements. Report the project and which transition fails.
3. After both automatic slots are warm, save again during a replacement build.
   Source-only follow-up work should use incremental building; CMake/Ninja may still
   regenerate if their real inputs require it. No timing improvement is claimed yet.
4. Save a syntax error while the application runs: it should retain the old process.
   Fix/save, then confirm a successful replacement. Check Stop and interactive input.

If first placement still fails, the next small task is a narrowly scoped diagnostic
trace of capture bounds, matched replacement window, restoration time and subsequent
bounds in the affected project. Use it to distinguish incorrect capture from late
application repositioning before choosing a fix. Do not redesign the lifecycle first.

Successfully built updated executable:

```text
C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab\build\optimization\cpplivelab-gui.exe
```

Focused repeat command from the repository root (close that output's GUI first):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -BuildDirectory .\build\optimization -TestMode Full -TestFilter '^(stage4_(scheduler|watcher|auto_build)|redirection_windows|reliability_gui_flow|reliability_freshness)$' -Jobs 4
```
