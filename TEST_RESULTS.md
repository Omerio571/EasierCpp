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
