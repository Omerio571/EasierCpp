# Latest automated reliability retest — October 28, 2026

Final executed command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Doctor
```

**Exit 0. All 28 tests passed; 0 failed; total CTest time 373.93 seconds.**
The final invocation verified the built binaries (`ninja: no work to do`). An earlier
28-test run passed in 210.23 seconds, before the additional diagnostic-classification
regression was added and fixed. The 373.93-second run includes that final fix.

## Failures found, fixed, and retested

1. **Completed CLI builds waiting on telemetry helpers.** The first full run failed
   reliability_queue, reliability_freshness, stage4_auto_build and
   maintenance_unicode_project after 60-second operation timeouts. Captured logs
   showed successful compilation/linking, and the Unicode program even printed its
   expected result. Process inspection identified MSVC's background `vctip.exe`.
   CLI/build sessions now explicitly use `start_worker`: root exit plus drained output
   ends the worker and removes only remaining processes in its owned job. Interactive
   program sessions keep their original full-tree policy. No timeout was raised to
   hide this failure. The seven affected/related tests passed in 125.29 seconds.
2. **Run after End Input kept stdin disabled.** A new hidden-controller test failed
   with `Fast Run after End Input kept stdin disabled from the previous process`.
   Every program launch now resets the input-closed flag. Retested with real stdin.
3. **Stop left Run disabled.** The next controller run exposed
   `Completed Stop left Run disabled until another Build`. Completion now clears
   operation-local cancellation state. Run becomes available without reopening.
4. **Minimize corrupted control geometry.** The controller test reproduced
   `Minimizing applied a zero-size layout and corrupted input/control geometry`.
   Zero/minimized sizes now preserve the last layout. Control moves and rounded
   regions finish before a single full redraw, with sibling clipping and no stale
   native edit edge. Eighty resize cycles verify non-overlap, rounded-region geometry
   and bounded GDI resources. Actual visual appearance remains a manual check.
5. **Compiler errors colored as deferred warnings.** A new diagnostic test failed
   when a real MSVC fatal error quoted `Executable is running or not writable`.
   Classification now recognizes the controller's explicit deferred prefix rather
   than guessing from message substrings. The console-model and real queue/error/
   cancellation tests passed together in 25.62 seconds after the fix.

Clear Program also now provides a neutral input-availability status while the running
process waits. The controller test verifies that clearing keeps the same process,
enabled Send, working input, and independent build/program histories.

## Final coverage

| Test group | Executed coverage | Final result |
| --- | --- | --- |
| reliability_gui_flow | Real controller in a hidden, test-owned native window; repeated Run without rebuild; EOF/restart; clear/input; Stop/restart; 80 resizes; rounded regions/GDI bounds; real Debug and Release build/run; shutdown | Passed, 23.72 s |
| reliability_inheritance | Unrelated event/pipe handles excluded from children; foreign writers cannot hold EOF | Passed |
| reliability_lifecycle | Twenty helper/termination cycles, explicit worker completion, setup capture with helper-held stdout | Passed |
| reliability_queue | Real cancellation with queued edits, preserved working executable, successful subsequent build, 1,000 coalesced scheduler requests, true error versus Deferred | Passed |
| reliability_freshness | Actual Ninja dry-runs, external header dependency, source/header changes, deleted/modified executable, independent configurations, failed/changed observations, unattached source and CMake glob fallback | Passed |
| Original seven Stage 4 tests | Native console, watcher, scheduler, real automatic-build integration, console model, separated channels, appearance/contrast/preferences/DPI calculations | All passed |
| Original sixteen Stage 1–3/maintenance regressions | Tool discovery, empty PATH, CLI behavior, single/multi-file builds, Debug/Release, input/EOF, safety locks, process trees, cached settings/presets, Unicode, argument/handle/output/path regressions | All passed |

No original assertion or regression was removed. CLI launches in the integration
tests now declare the worker contract; direct interactive/orphan process-tree tests
remain unchanged and passed. The GUI-controller host uses an isolated test registry
key and temporary calculator, not the user's project or preferences. Its window is
not shown and it does not inject desktop input. Enter-key behavior, actual themes,
Windows minimize/maximize visuals and physical mixed-DPI displays remain manual.

Additional executed checks:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\format.ps1 -Check
.\build\debug\cpplivelab.exe --version
.\build\debug\cpplivelab.exe doctor
git diff --check
git ls-files -ci --exclude-standard
git diff --cached --name-only
```

- clang-format 19.1.5 verified **37** maintained C++ files.
- Version remains `CppLiveLab 0.1.0`; ordinary-terminal doctor returned 0, finding
  installed CMake/Ninja/MSVC/Clang while reporting current-terminal availability No.
  GCC remains absent and optional. No persistent PATH changes were made.
- Git whitespace and generated-file ignore checks passed. Required new sources are
  not ignored. Nothing was staged, committed or pushed during this pass.
- Checksum comparison found **21 existing external user-project files unchanged**.

Manual handoff: [STAGE4_REPAIR_MANUAL_TESTS.md](STAGE4_REPAIR_MANUAL_TESTS.md).
Current code/files/limits: [STAGE4_REVIEW.md](STAGE4_REVIEW.md).
Structured/friendly diagnostics, the Simple/Detailed log presentation and further
requested Light palette distinction are still unfinished from the earlier request.
Passing these tests does not claim those missing features are implemented or approve
Stage 4. No Stage 5 work was performed.

---

# Earlier Stage 4 completion — executed October 28, 2026

Final command, started from ordinary PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Doctor
```

Result: **exit 0; 23/23 CTest tests passed; 0 failures; 145.56 seconds.**
The manager and native GUI were built with MSVC/Ninja before this final run. The
last invocation reported `ninja: no work to do` against those verified binaries.
An earlier complete run also passed 23/23 in 170.01 seconds; a subsequent real-GUI
viewport check prompted the final correction and another full run.

| Tests executed | Coverage | Final result |
| --- | --- | --- |
| stage4_console_view | Real Rich Edit prompts, hidden-tab visibility, caret and actual viewport follow, selection, scroll preservation, separate Clear, color, bounded history, recoloring | Passed |
| stage4_watcher | Real Windows notifications; sources/headers/CMake; add/delete/rename; generated/duplicate suppression; project switch; cancellation/handles; atomic save; Unicode filename; locked-file retry | Passed (57 checks) |
| stage4_scheduler | Debounce, disabled mode, busy build, deferred executable, coalescing and cancellation | Passed (15 checks) |
| stage4_auto_build | Real Debug/Release compilation, compile-error recovery, retained successful executable, input-waiting program preservation, source/config saves, rapid saves during a real build and one follow-up | Passed |
| stage4_console_model | Independent histories, unread/error flags, classification and bounds | Passed (11 checks) |
| stage4_channels | Real interactive stdin with separate stdout/stderr, no stream leakage, exit code | Passed (8 checks) |
| stage4_appearance | Dark/Light contrast, Follow Windows resolution, isolated preference round-trip, DPI math | Passed (21 checks) |
| Original 16 tests | doctor_unit; maintenance_paths/output/handles/arguments/unicode_project; stage3_session/tree/projects; stage2_integration; doctor_without_path; cli_version/help/unknown/extra/doctor | All passed |

Stage 2 integration executed its original 36 checks. Stage 3 session/tree/projects
executed 13/5/20 checks. No previous regression was removed or disabled.

Failures discovered and addressed:

- A hidden Rich Edit could become visible on WM_SETREDRAW(TRUE); hidden tabs now
  retain their visibility during output/theme updates (fixed earlier in Stage 4).
- The background-follow test initially compared incompatible CRLF/caret lengths.
  It now queries Rich Edit character counts. Empty histories explicitly follow tail.
- A real window check then found that moving the caret did not always move the
  viewport after hidden/redraw-suspended updates. Explicit bottom scrolling after
  show/redraw now fixes this. The test uses an unactivated off-screen host and
  verifies the actual scrollbar position, not just the caret.
- The added locked-file test initially used a nonexistent UniqueHandle constructor;
  corrected to the existing reset/ownership API before the final successful build.

Actual GUI checks used only a fresh temporary calculator, not existing demos:

- A source save triggered Automatic Debug build and Build Successful.
- A CMake save reconfigured and emitted 40 status lines; opening the background
  Build & System tab showed its latest success at the bottom.
- Build output stayed out of Program Console; inactive tabs indicated new messages.
- Enter submitted name/number input; Send submitted the final number. The running
  multiplication executable printed `Hello, Ada! Sum: 9.375` after addition had been
  saved. The running process stayed intact, showed Build Deferred, then automatically
  built the pending edit on exit without launching another program.
- Stop terminated an input-waiting owned calculator; its deferred build then succeeded.
- Auto Build OFF prevented a real source save from rebuilding (executable timestamp
  unchanged after two seconds); manual Build still completed successfully while off.
- Light and Dark themes retained output, and maximize/restore remained usable.
  Light/Auto Build ON preferences were restored on launch from the earlier session.

Additional commands executed:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\format.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\format.ps1 -Check
.\build\debug\cpplivelab.exe --version
.\build\debug\cpplivelab.exe doctor
git diff --check
git ls-files -ci --exclude-standard
git diff --cached --name-only
```

clang-format 19.1.5 verified all 32 maintained C++ files. Version remained
`CppLiveLab 0.1.0`. Ordinary-terminal doctor found installed CMake, Ninja, MSVC and
Clang even though each reported Available in current terminal: No. GCC was absent
and optional. No persistent PATH change was made. Git whitespace validation passed,
no tracked ignored artifacts were found, and nothing was staged/committed/pushed
during this completion pass. Hashes for all 12 existing external user-project source
and configuration files matched before/after testing.

Still awaiting user acceptance: the full A–G manual walkthrough, VS editing/debugging
workflow on the user's chosen project, physical mixed-DPI displays and actual Windows
app-color changes. These are not claimed as executed. Console cross-pipe ordering,
history limits, large-tree/network watcher limits and unsupported future features
are documented in STAGE4_REVIEW.md. No Stage 5 work was performed.

---

# Stage 1 discovery fix — executed results

Executed October 27, 2026 on Windows x64.
MSVC 19.44.35228.0; CMake 3.31.6-msvc6; Ninja 1.12.1.

Build/test command (started from ordinary PowerShell, no developer shell):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Doctor
```

Final run: exit 0; Ninja built the core library and both executables.

```text
doctor_unit          Passed  0.25 sec
doctor_without_path  Passed  0.25 sec
cli_version          Passed  0.08 sec
cli_help             Passed  0.09 sec
cli_unknown          Passed  0.09 sec
cli_extra            Passed  0.08 sec
cli_doctor           Passed  0.16 sec
100% tests passed, 0 tests failed out of 7
Total Test time (real) = 1.02 sec
```

The unit test uses real temporary filesystem fixtures, independent of the installed
machine tools. It covers VS toolset discovery with empty PATH, standalone installations,
terminal PATH discovery, paths with spaces, missing required tools, optional GCC, and
multiple VS installations. The integration test clears PATH before running the real
executable and requires installed CMake, Ninja, and MSVC to be found with terminal
availability reported as No.

Direct ordinary-PowerShell verification:

```text
cmake: Installed: Yes | Available in current terminal: No
ninja: Installed: Yes | Available in current terminal: No
cl: Installed: Yes | Available in current terminal: No
clang++: Installed: Yes | Available in current terminal: No
g++: Installed: No | Available in current terminal: No
Build tools found.
doctor exit: 0
Terminal PATH unchanged: True
CppLiveLab 0.1.0
version exit: 0
```

All four installed tools resolved to the Visual Studio Community 2022 installation
under `C:\Program Files\Microsoft Visual Studio\2022\Community`. MSVC resolved to
`VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe`. Doctor also returned the
`Common7\Tools\VsDevCmd.bat` command needed to initialize child builds.
In the build script's initialized environment all four tools reported terminal
availability Yes. No system/user PATH changes were made.

The first rebuild exposed an ambiguous C++ helper name, which was corrected before
this successful run. Build/tests ran outside the sandbox because the prior Stage 1
sandbox run encountered an MSVC program database error. Only MSVC was compile-tested;
Clang was discovered, not compile-tested. No Stage 2 functionality was added.

# Stage 2 — final executed results

The final build and complete CTest suite ran successfully on Windows x64 using
MSVC 19.44.35228.0, CMake 3.31.6-msvc6 and Ninja 1.12.1.

Command, from an ordinary PowerShell terminal in the CppLiveLab folder:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Doctor
```

Exit code: 0. Exact final CTest results:

```text
doctor_unit          Passed   0.27 sec
stage2_integration   Passed  59.97 sec
doctor_without_path  Passed   0.17 sec
cli_version          Passed   0.08 sec
cli_help             Passed   0.07 sec
cli_unknown          Passed   0.07 sec
cli_extra            Passed   0.07 sec
cli_doctor           Passed   0.14 sec
100% tests passed, 0 tests failed out of 8
Total Test time (real) = 60.88 sec
```

Stage 2 integration completed 36 assertions. Each CppLiveLab child started with an
empty PATH. Real compiled programs covered:

- New C++20 projects, nonempty-folder overwrite refusal, and manager/project separation.
- Separate Debug and Release configurations for generated projects and single-file programs.
- C++20 `std::span` compilation, names containing spaces, and redirected stdin/stdout.
- Calculator prompts and sum: Ada Lovelace, 2.5, 3.75 produced 6.25.
- Invalid numeric input produced exit 1.
- Existing CMake project with a subdirectory, custom executable and two compilation units.
- Program exit code 7 propagated unchanged.
- Compilation failure exposed MSVC diagnostics and did not run a previously built executable.
- Missing inputs, invalid/missing options, unchanged parent PATH, and automatic SDK setup.

All seven Stage 1 regression tests remain in the suite and passed.

A separate live-console check was executed (not just piped input):

```powershell
.\build\debug\cpplivelab.exe create ..\CppLiveLab-UserProjects\CalculatorDemo --demo calculator
.\build\debug\cpplivelab.exe run ..\CppLiveLab-UserProjects\CalculatorDemo --config Debug
```

Create exited 0. The running program displayed each prompt before receiving the next
answer. Entering `Ada Lovelace`, `2.5`, then `3.75` produced:

```text
Your name: Ada Lovelace
First number: 2.5
Second number: 3.75
Hello, Ada Lovelace! Sum: 6.25
```

Program/Cli exit code: 0. The demo's source files remain in the separate sibling
`CppLiveLab-UserProjects/CalculatorDemo` folder for manual testing.

An initial integration run exposed missing SDK tools with an empty PATH. This was
fixed by supplying System32 to the setup child before invoking VsDevCmd. The complete
suite passed afterward, and passed again after the final C++20-default change.
Build/tests ran outside the sandbox, as required by this machine's MSVC PDB tooling.
Only MSVC was compile-tested. No Stage 3 functionality was implemented.

# Stage 3 — final executed results

The native Windows GUI and the preserved CLI were built with MSVC 19.44.35228.0,
CMake 3.31.6-msvc6 and Ninja 1.12.1. The manager uses C++20 and Win32 system libraries;
no Raylib, SDL or third-party GUI framework is required.

Final build/test command (outside the sandbox for this machine's MSVC PDB tooling):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Doctor
```

Exit code: 0. Exact final results, after the output-drain adjustment:

```text
doctor_unit          Passed   0.27 sec
stage3_session       Passed   0.66 sec
stage3_tree          Passed   0.40 sec
stage3_projects      Passed  13.69 sec
stage2_integration   Passed  67.46 sec
doctor_without_path  Passed   0.30 sec
cli_version          Passed   0.09 sec
cli_help             Passed   0.09 sec
cli_unknown          Passed   0.09 sec
cli_extra            Passed   0.10 sec
cli_doctor           Passed   0.18 sec
100% tests passed, 0 tests failed out of 11
Total Test time (real) = 83.37 sec
```

The three Stage 3 tests executed 38 checks (13 session, 5 tree, 20 project checks).
The Stage 2 integration test also passed its 36 checks. All seven Stage 1 tests passed.
The Stage 3 tests exercise the actual GUI process-supervision backend and real compiled
projects, not a simulated build result:

- Incremental interactive stdin, stdout/stderr capture, EOF and exit-code preservation.
- 200,000 output bytes drained without loss; input backpressure without blocking.
- Refusing a second operation while busy; Stop and restart.
- Terminating root/child processes, descendants left after a parent exits, and cleanup
  when the supervisor is destroyed.
- Building and running a real CMake calculator from empty PATH using discovered tools.
- Refusing a simultaneous build while the same configuration is running, without
  changing the executable timestamp.
- Detecting an externally launched executable before configuring or linking over it.
- Rebuilding and interactive execution after Stop.
- Preserving CMakeLists.txt, CMakePresets.json, CMakeUserPresets.json, CMakeSettings.json,
  a modified C++ standard cache entry and a custom cache variable byte-for-byte/as set.
- Validating project separation, rejecting invalid projects, discovering the actual
  Visual Studio 2022 IDE and reporting a missing IDE.

Live native-window checks were also executed using the computer-use tools:

1. Opened an existing CMake project through the folder-path field and Open Project.
   Its folder name and full location appeared; Build/Run/Edit were enabled.
2. Opened and cancelled the native Browse folder picker without changing the project.
3. Built the existing calculator; the pane showed real CMake/Ninja output and exit 0.
4. Ran it with line-by-line input using both Enter and Send. Your existing calculator
   now multiplies, so Ada Lovelace / 2.5 / 3.75 correctly produced its current result
   9.375. Those source edits and the existing VS Code tasks were preserved.
5. Clicked Edit in Visual Studio 2022. The discovered IDE opened the selected folder;
   its window title became CalculatorDemo - CMake Overview Pages - Microsoft Visual Studio.
6. Created a separate Stage3Calculator addition demo. Run reached the name prompt;
   Build and Run were disabled while it was active. Stop restored the controls and
   displayed Stopped. Restarting rebuilt successfully.
7. Built an isolated project with an intentional #error. The pane displayed the source
   filename/line, MSVC fatal error C1189, Compilation failed, Program was not run,
   and Finished (exit 1). This temporary test project was removed afterward.
8. Closed the GUI while the calculator waited for input. Process inspection afterward
   found no remaining calculator/CLI workers for that project. Visual Studio remained open.
9. Reopened the final GUI and ran Stage3Calculator. Entering Ada Lovelace, 2.5 and 3.75
   produced Hello, Ada Lovelace! Sum: 6.25, followed by Finished (exit 0). The GUI was
   left open with that project selected and no program running.

Initial errors were corrected before this final successful run: Unicode definitions
for Windows resource constants, the test helper's C++20 setting, an input-pipe check
that could block behind std::cin (replaced with asynchronous writes), and a test-fixture
line-ending mismatch. A label overlap was corrected after visual inspection.

Limits: the GUI uses line-based plain-text console I/O, not a terminal emulator.
Stop is a forced termination of the owned job, so it does not run user cleanup code.
Keep the companion cpplivelab.exe next to the GUI. GUI project selection is not saved.
No automatic rebuilding, hot reload, library management or checkpoint restoration was added.

# Post-Stage 3 maintenance — final executed results

This pass preserves Stage 1–3 and does not implement Stage 4. An initial full rebuild
compiled the changed CLI/core/GUI and all test executables; its complete 16-test run
passed in 76.33 seconds. After final test-only organization/exception-check changes,
the complete suite was rebuilt and executed again using:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Doctor
```

Final command exit: 0. Exact final results:

```text
doctor_unit                 Passed   0.08 sec
maintenance_paths           Passed   0.32 sec
maintenance_output          Passed   0.04 sec
maintenance_handles         Passed   0.03 sec
maintenance_arguments       Passed   0.15 sec
maintenance_unicode_project Passed   8.39 sec
stage3_session              Passed   0.57 sec
stage3_tree                 Passed   0.31 sec
stage3_projects             Passed   9.75 sec
stage2_integration          Passed  55.14 sec
doctor_without_path         Passed   0.14 sec
cli_version                 Passed   0.08 sec
cli_help                    Passed   0.09 sec
cli_unknown                 Passed   0.08 sec
cli_extra                   Passed   0.08 sec
cli_doctor                  Passed   0.15 sec
100% tests passed, 0 tests failed out of 16
Total Test time (real) = 75.45 sec
```

MSVC 19.44.35228.0, CMake 3.31.6-msvc6, Ninja 1.12.1. As in the previous stages,
compilation/tests ran outside the sandbox for this machine's MSVC PDB tooling.
All original 11 CTest cases remain present and passed. No pre-existing failing test
was observed, and no completed CTest run failed during this maintenance pass.

New targeted regressions cover: same-directory/descendant/sibling path boundaries;
case and trailing-separator handling; deterministic different-drive/UNC paths; UTF-8
path diagnostics; every split position and byte-at-a-time UTF-8 output; EOF/reset;
RAII handle release on scope exit and exceptions; real Windows argument reconstruction
through both process launchers; and a real Unicode-folder project creation/build/run.
A physical second-drive or remote-share build was not performed.

Formatter commands executed:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\format.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\format.ps1 -Check
```

clang-format 19.1.5 parsed the configuration successfully, applied it, and verified
19 maintained C++ files. No generated/third-party files or external user projects
were selected. No indentation tabs were found in maintained C++ source/headers.

Direct ordinary-terminal commands executed:

```text
cpplivelab --version -> CppLiveLab 0.1.0; exit 0
cpplivelab doctor -> installed CMake/Ninja/MSVC/Clang found; exit 0
```

Git validation executed: status, ls-files .vs, ls-files -ci --exclude-standard,
check-ignore -v for VS/build/cache artifacts, required-source ignore checks,
diff --check, and the staged-file list. No tracked generated files were found,
no required source/configuration was ignored, and no files were staged. Credential
signature scanning of review candidates found zero matches. Existing VS Code task
content matched the pre-maintenance baseline byte-for-byte. The .vs directory and
unrelated processes were left intact. No commit or push was created.

The complete GUI walkthrough was not repeated during this maintenance pass;
independent maintenance manual testing is intentionally the next approval gate.
See MAINTENANCE_MANUAL_TESTS.md, MAINTENANCE_REVIEW.md and GIT_CHECKPOINT.md.
