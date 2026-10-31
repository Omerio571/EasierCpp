# Final Stage 4 refinements

This report records the original refinement pass. The later customized palettes and
their accessibility fixes are documented in [THEME_ACCESSIBILITY.md](THEME_ACCESSIBILITY.md).

This pass changes console clearing, semantic palettes and test tooling only. It does
not change the working scheduler, watcher, intelligent Run, tool discovery, build
configurations, process ownership or cancellation. No Stage 5 work, commit or push.

## Files changed in this pass

- `src/console_model.hpp/.cpp`: track a reliable observed unfinished stdout line.
- `src/console_view.hpp/.cpp`: clear completed history while retaining that line.
- `src/gui.cpp`: use preservation only for a currently running program; retain the
  neutral input-available status. No queue or Run changes.
- `src/appearance.hpp/.cpp`: separate semantic palettes and testable Follow Windows
  palette resolution; existing OS change notifications remain unchanged.
- `tests/stage4_tests.cpp`, `tests/console_view_tests.cpp`, `tests/gui_flow_tests.cpp`:
  prompt, native rendered colors, theme resolution and continued calculator input tests.
- `tests/session_fixture.cpp`, new `tests/performance_probe.cpp`: opt-in timing probe
  and no-op process fixture; no production pipeline changes.
- `CMakeLists.txt`, `scripts/build.ps1`, new `tests/test_selection.cmake`: explicit
  Quick/Full labels, optional worker count/timing and selection regression.
- New `TESTING.md` and this report; updated README, TEST_RESULTS and existing Stage 4
  review/manual documents to point to current behavior.

Before/after hashes confirmed all 11 protected scheduler/build/process/toolchain source
files unchanged, all 27 sampled existing user-project source/configuration files
unchanged, and `src/ui_theme.cpp` unchanged (rounded controls/layout renderer).

## Console behavior

Clear Program removes completed history and retains the actual unfinished stdout line
while the program is running. Writes `Na`, `me`, `: ` become `Name: `; a trailing
newline completes the line. Input echoes complete the answered prompt, so a later
`Second number: ` is preserved instead. Clear never sends input, restarts the process
or resets its variables. Build & System clearing remains independent.

The line is not a guessed formal prompt. Mixed partial stderr/input, unsupported
control sequences, backspaces or an over-limit line cause a neutral fallback rather
than invented text. A subsequent newline restores tracking. Clearing a stopped
program clears its history completely. Only already received output can be retained;
normal application flushing still applies. This is not a terminal emulator.

## Semantic colors

| Meaning | Dark | Light |
|---|---|---|
| Error / red | `#F87171` | `#B91C1C` |
| Success / green | `#4ADE80` | `#15803D` |
| Information / blue | `#60A5FA` | `#1D4ED8` |
| Warning / orange | `#FBBF24` | `#A16207` |

Ordinary stdout stays neutral. Textual severity labels remain. Tests check contrast
of at least 4.5:1 on each console background, semantic channel dominance, pairwise
hue separation, both Follow Windows resolutions, and actual Rich Edit text colors
under both explicit palettes. OS appearance-change broadcasts and visual readability
on the user's display remain manual checks.

## Timing findings

The opt-in probe was executed against fresh identical small CMake projects in local
TEMP and OneDrive Projects. Values below are medians, seconds unless stated otherwise.
Project operations have two samples per location; process launches have 20 interleaved
samples per location; SDK setup has three samples. Raw data is in the ignored local
artifact `build/stage4-timing.csv` and summary in `build/stage4-timing-summary.csv`.

| Operation | Local TEMP | OneDrive Projects |
|---|---:|---:|
| Cold CMake configuration | 1.789 | 1.702 |
| Warm configuration | 0.190 | 0.230 |
| Cold compilation | 1.628 | 1.715 |
| Incremental compilation | 0.573 | 0.745 |
| Already configured no-op build | 0.103 | 0.112 |
| Process creation | 13.135 ms | 13.460 ms |
| No-op process lifetime/exit observation | 50.569 ms | 50.391 ms |
| Watcher notification | 1.754 ms | 1.147 ms |
| Content fingerprint | 0.643 ms | 0.427 ms |
| Initial watcher snapshot | 2.080 ms | 3.723 ms |
| Default debounce observed | 0.401 | 0.398 |

Tool discovery took 89.223 ms. **Fresh Visual Studio developer-environment setup took
5.419 seconds median**, range 5.078–6.137 seconds. A real warm incremental CLI build
took 6.055 seconds locally and 5.017 in OneDrive; about 98% elapsed before its
configuration marker (CLI startup, discovery and SDK setup). Reusing an environment
inside the measurement probe reduces an already configured no-op operation to about
0.1 seconds, but production environment caching was deliberately not introduced.

Repeated SDK setup is therefore a major measured cost. The integrations deliberately
start fresh CLI commands: roughly 10 build/run operations in stage2, eight in auto
build, seven builds plus one environment preparation in freshness, five in queue,
two CLI builds plus GUI preparation in GUI flow, and two builds in stage3_projects.
Cold compiler probes and real compilation add further cost. These operations verify
approved behavior; removing them or sharing production state would change coverage.

After observed root exit, direct CMake output drain/completion typically took
0.03–0.08 ms; CLI drains were 0.05–0.18 ms. This measures time after exit is observed,
not kernel exit latency. Launch/lifetime timings include polling. No exit stall was
observed in this probe. Existing 5–10 ms polling loops and watcher negative-observation
windows remain. Auto-build's two intentional CMake sleeps total about four seconds;
the cancellation fixture's 30-second command is stopped after its start signal,
rather than deliberately waited out. The probe's own requested 20 ms quiet period
measured about 29–30 ms and is reported separately from event delivery. The 400 ms
scheduler debounce was not changed.

OneDrive-location compile differences were small in this sample and do not explain
minutes of delay. OneDrive was not running when subsequently checked; active syncing
was not measured throughout. Location, cache/order and machine load are confounded,
so these measurements do not establish a synchronization penalty or guarantee that
sync can never affect another run. The user's earlier 454-second run cannot be
reconstructed exactly from one later measurement.

Safe optimizations: Quick avoids seven repeated-build integrations; optional CTest
workers overlap independent fixtures; selection checks use one metadata query instead
of three (measured 6.49 to 2.23 seconds before log isolation). All original stress
assertions and necessary waits remain. Full remains the default and approval mode.

## Executed results

After the MSVC build, final Quick passed **23/23 in 8.09 seconds**. Full passed
**30/30 in 125.28 seconds with two workers**, then **30/30 in 221.40 seconds with
the default one worker**. All durations are CTest real time; manager SDK/configure/
build setup is additional (about 6–7 seconds on these warm manager builds).
Two workers saved 96.12 seconds, about 43%, in this measured pair. That is an
observation on this machine, not a universal timing guarantee or a change to the
application's scheduler. The original 28 tests and their assertions remain.

The final serial run's six integration timings corresponding to the user's list:

| Test | Seconds |
|---|---:|
| stage2_integration | 58.66 |
| stage4_auto_build | 48.21 |
| reliability_freshness | 42.17 |
| reliability_queue | 26.16 |
| reliability_gui_flow | 20.46 |
| stage3_projects | 9.84 |

The first metadata-selection implementation was found to truncate its enclosing
CTest log. It now includes the real generated registrations from an isolated listing
directory; final logs retain every passing test record (23 Quick, 30 Full). Formatting
validation passed for 38 files; Git whitespace/ignore/index checks passed. Exact
executed commands and the historical reliability results are in TEST_RESULTS.md.

## Manual checks

1. Close CppLiveLab. In ordinary PowerShell:

   ```powershell
   Set-Location 'C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab'
   powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Quick
   powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Full -Jobs 2 -Doctor
   $demo = Join-Path (Split-Path (Get-Location)) ('CppLiveLab-UserProjects\FinalStage4-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
   .\build\debug\cpplivelab.exe create $demo --demo calculator
   $demo
   .\build\debug\cpplivelab-gui.exe
   ```

   Expect Quick 23/23, Full 30/30, and installed tools detected. A new sibling demo
   keeps your existing projects unchanged. Do not overwrite an existing demo folder.

2. Paste `$demo` into Project folder, Open Project, choose Debug and `app.exe`.
   Turn Auto Build OFF, Build, then Run. At `Your name: ` click Clear Program twice.
   Expect that exact prompt to remain, Send enabled, and no program restart.

3. Enter `Ada`, then `2.5`, each followed by Enter. At `Second number: ` click Clear
   Program. Expect only that prompt, with earlier completed output removed. Enter
   `3.75`. Expect `Sum: 6.25`, proving earlier input and variables survived clearing.
   When the program has exited, Clear Program should leave a blank history.

4. Run and finish the calculator again to leave output, then Build again to create
   Build & System output. Clear that tab. Expect only its
   history removed; Program Console's existing output remains. Repeat in reverse.

5. Choose Dark, then Light. Successful builds should have green `[SUCCESS]` and
   system information blue. To inspect red errors, use Edit in Visual Studio 2022,
   remove the semicolon from `return left + right;` in this new demo only, save and
   Build. Expect red compiler errors and a failed build. Restore the semicolon and
   rebuild successfully. Run, then Stop while waiting for input to see warning color.
   Ordinary name/number prompts and results should remain neutral in both themes.

6. Choose Follow Windows. Change Windows Settings > Personalization > Colors >
   application mode between Light and Dark. Expect the matching palette to update,
   with severity labels and retained console text intact. Restore your OS preference.
   Resize the manager; rounded controls, spacing and input area should remain intact.

7. For final regression confidence use the wider numbered scenarios in
   `STAGE4_REPAIR_MANUAL_TESTS.md` (queue, Run, Stop, Debug/Release and VS). The approved
   scheduler semantics have not been redesigned. Stop here and review before approval.

Full regression results and final validation are recorded in `TEST_RESULTS.md`.
Manual visual verification, real mixed-DPI inspection and active-sync performance
remain the user's checks. Earlier structured diagnostic summaries/Simple-Detailed
presentation are outside this refinement request and remain unfinished.
