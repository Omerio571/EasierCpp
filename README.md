# EasierCpp — Make Your Life Easier

For the v0.1.0 portable testing package, see the concise
[release user guide](docs/release/README.md) and
[release notes](docs/release/RELEASE_NOTES.md). Build/package locally with
`powershell -NoProfile -ExecutionPolicy Bypass -File scripts/release.ps1`.
Publication remains a separate owner-approved step. The historical development
notes below are retained for context.

EasierCpp is a lightweight Windows C++ development manager focused on libraries,
standard CMake projects, and automatic rebuild/restart with window placement
restoration. This is process restart: gameplay, memory and application state are
not preserved. No universal hot-reloading or checkpoint feature is planned here.

Start with [the current testing guide](docs/migration/MANUAL_TESTS.md) and
[migration report](docs/migration/FINAL_REPORT.md). The executables are now
`EasierCpp.exe` / `EasierCpp-Gui.exe`. Legacy public interfaces and metadata readers
remain available for compatibility. Earlier stage
reports below describe historical acceptance; their counts are not current results.

The controlled identity migration is tracked in [the naming inventory](docs/migration/RENAMING.md).
Supported legacy metadata is copied conservatively into `%LOCALAPPDATA%\EasierCpp`;
originals remain untouched. Preferences use `HKCU\Software\EasierCpp\Preferences`.
Existing new data wins conflicts. Build/download caches are not transplanted.
The public repository is [Omerio571/EasierCpp](https://github.com/Omerio571/EasierCpp).

The earlier unified-generation refinement is in [STAGE5_UNIFIED_REPORT.md](STAGE5_UNIFIED_REPORT.md),
with [numbered manual acceptance steps](STAGE5_UNIFIED_MANUAL_TESTS.md). Executed
regressions: Quick 28/28, Full 41/41 plus Doctor. Remaining human acceptance
coverage is explicitly listed; Stage 5 still awaits your approval.

# Earlier Stage 5 acceptance

See [STAGE5_CONSOLIDATED_REPORT.md](STAGE5_CONSOLIDATED_REPORT.md) and the complete
[numbered A–X manual guide](STAGE5_CONSOLIDATED_MANUAL_TESTS.md). Startup now shows
New/Open/Recent rather than automatically reopening a project. Project/Tools sit
left; Settings sits right. Build actions remain in the workspace. All forms share
Dark/Light/Follow Windows styling. Tools > Run EasierCpp Tests uses a separate
owned process and isolated test-build directory.

# EasierCpp — Stage 5

A standalone Windows C++20 development manager, with a small native graphical interface.
Stage 1 provides version/help and toolchain discovery. Stage 2 adds project creation,
single-file and multi-file CMake builds, and console execution with interactive input.
Stage 3 makes EasierCpp the main application, with Visual Studio 2022 used for editing
and debugging. Stage 4 adds optional automatic compilation, two console tabs, and
Dark, Light, and Follow Windows appearance. The CLI retains its commands and version.
Stage 5 adds read-only project import, external managed builds, executable targets,
new empty/multi-file projects, pinned libraries, editor selection and recents,
plus library/cache management and safe application restart.
The current UI adds one Project/Build/Tools/Settings navigation row and shared themed
forms, plus multiple optional library selections (fmt and JSON for Modern C++).
Start with the [complete Stage 5 testing guide](STAGE5_UI_MANUAL_TESTS.md); current
scope/files/limits are in [STAGE5_UI_REPORT.md](STAGE5_UI_REPORT.md).
Persistent standards are in [UI_DESIGN_GUIDELINES.md](UI_DESIGN_GUIDELINES.md).
FarmGame has been inspected read-only;
its real build/run acceptance remains separate and requires configure-write approval.

## Maintenance conventions

The post-Stage 3 maintenance pass adds a readable Allman C++20 style, permanent
development instructions in [AGENTS.md](AGENTS.md), and a concise [coding guide](CODING_STYLE.md).
Apply or verify the formatter from ordinary PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\format.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\format.ps1 -Check
```

Only maintained `src/`, `include/` and `tests/` C++ files are selected; generated,
third-party and external user projects are excluded. The configuration was validated
against clang-format 19.1.5, discovered in Visual Studio's LLVM tools.

For independent testing and Git review, see [MAINTENANCE_MANUAL_TESTS.md](MAINTENANCE_MANUAL_TESTS.md),
[MAINTENANCE_REVIEW.md](MAINTENANCE_REVIEW.md), and [GIT_CHECKPOINT.md](GIT_CHECKPOINT.md).
No maintenance commit or push is performed automatically.

## Build and test on Windows

Install Visual Studio 2022 (or Build Tools) with **Desktop development with C++**,
including its CMake tools and Windows SDK. The IDE is only required when selected as
the editor. Build Tools can compile while VS Code, CLion or a custom editor is used.

From PowerShell in this folder:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Doctor
.\build\debug\EasierCpp.exe --version
.\build\debug\EasierCpp.exe --help
.\build\debug\EasierCpp.exe doctor
```

The script discovers Visual Studio with vswhere, loads its x64 developer environment,
configures with Ninja, builds, and runs CTest. `-Doctor` also runs doctor in that environment.
It changes only the script process environment, never the system or user PATH.
No manual developer-shell setup is required for this build command or the VS Code task.

`doctor` discovers Visual Studio C++ installations with `vswhere`, reads the default
MSVC toolset version, and checks bundled CMake, Ninja, and Clang. It also searches PATH,
standard Program Files and per-user install folders, WinGet links, Scoop shims, and
Chocolatey. Each tool shows its executable path and separate **Installed** and
**Available in current terminal** statuses. An installed MSVC compiler is sufficient;
GCC is optional. The discovery result retains the MSVC developer-environment command
so child builds can initialize INCLUDE, LIB, SDK and PATH without persistent changes.

If CMake, Ninja, and a C++20 compiler are already on PATH, use:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

If using VS Code, run the named `EasierCpp: build and test` task. It loads the developer
environment automatically. Existing editor tasks are preserved.

## Start the graphical application

```powershell
.\build\debug\EasierCpp-Gui.exe
```

Double-clicking `build/debug/EasierCpp-Gui.exe` also works. Keep the companion
`EasierCpp.exe` in the same directory. No Raylib, SDL, GUI framework, or graphics-library
installation is needed; the interface uses Windows controls and system libraries.

Choose **Project > Open Existing Project** and select the CMake source folder.
The active project's folder name and full path appear above the buttons. Set
**Preferred Editor**, then click **Edit in Preferred Editor** to open that folder.
Project > New Project, Recent Projects and Project Settings provide creation,
reopening, preset/working-directory and optional vcpkg choices without a custom editor.
Appearance, Preferred Editor and Build Preferences are under the single Settings entry.
The manager does not generate a solution or rewrite imported project settings.
Visual Studio's [Open Folder workflow](https://devblogs.microsoft.com/cppblog/bring-your-c-codebase-to-visual-studio-with-open-folder/)
supports this direct `devenv.exe <folder>` launch.

Choose Debug or Release, then **Build** or **Run**. GUI Run reuses a successful build
only after checking the selected project/configuration/executable, source fingerprints,
build artifacts and a Ninja dependency dry-run. Unknown or changed inputs build first;
observations are never trusted across restarts. Program output appears in
**Program Console**. Type into the input field and press **Send** or Enter
for each line. **End Input** closes stdin. Build discovers actual executable targets
through CMake's File API; choose the program in the target dropdown. Multiple targets
are not guessed. VS-generated projects perform an incremental build before Run.

**Stop** terminates only the operation's owned Windows Job Object (the CLI, compiler,
program and descendants). It is a forced stop, so the program does not get a cleanup
callback. Build and project selection stay disabled until that process tree has
exited; Run can queue behind a build. A completed build worker releases owned
background tool helpers rather than waiting indefinitely for them. Closing the
window while busy also stops its owned processes. Visual Studio
is launched separately and is never included in that job.

EasierCpp holds a per-project/configuration build lock through program execution,
blocking simultaneous builds/runs from another instance. It also checks existing build
executables for write access before configuring, to catch programs launched externally.
The OS remains responsible for enforcing executable locks if an external program is
started after that check.

GUI managed builds use `%LOCALAPPDATA%\EasierCpp\projects`, outside the source folder,
separate from this manager's `build/debug` and an IDE's existing build tree.
EasierCpp does not write CMake presets, CMakeSettings, or Visual Studio configurations.
Configure presets and project compiler flags are retained. CMake file-writing commands
trigger a permission warning; arbitrary CMake code is not a read-only sandbox.

A fresh addition demo for GUI testing is in `../EasierCpp-UserProjects/Stage3Calculator`.
Your existing CalculatorDemo edits are preserved. See [STAGE3_MANUAL_TESTS.md](STAGE3_MANUAL_TESTS.md)
for numbered steps and expected results.

## Legacy CLI commands retained from Stages 1–4

For the Stage 5 isolated workflow use `new`, `inspect`, `manage-build`, `manage-run`
and `targets`; see the Stage 5 manual. The legacy commands below retain their existing
generated `.cpplivelab` directories and `--exe` behavior for compatibility.

Keep user projects outside this repository. For example, from this folder:

```powershell
.\build\debug\EasierCpp.exe create ..\EasierCpp-UserProjects\HelloProject
.\build\debug\EasierCpp.exe build ..\EasierCpp-UserProjects\HelloProject --config Debug
.\build\debug\EasierCpp.exe run ..\EasierCpp-UserProjects\HelloProject --config Release
```

Create accepts a new or empty folder and never overwrites an existing project.
Use `create <new-folder> --demo calculator` for the multi-file calculator template.
A fresh addition demo is in the sibling `EasierCpp-UserProjects/Stage3Calculator` folder.

`build <path>` accepts a `.cpp`, `.cc`, `.cxx` source file or an existing CMake project
folder. `run <path>` builds first and launches only on success. Debug is the default;
`--config Release` selects a separate optimized build. Compiler and CMake diagnostics
appear directly in the terminal. The program inherits stdin, stdout, stderr and its
project working directory; its exit code becomes EasierCpp's exit code.

New projects use C++20 and target `app`. Existing CMake projects receive C++20 defaults;
their own explicit target settings remain authoritative. Use `run <folder> --exe
<relative-path>` for an existing project whose executable is not `app.exe`.
The executable path is relative to the selected build directory; absolute paths work too.

Tool discovery supplies absolute CMake, Ninja and compiler paths. When MSVC is selected,
EasierCpp captures `VsDevCmd.bat`'s environment and supplies it to child processes,
including SDK tools and include/library paths. It never modifies the parent terminal
or persistent PATH. MSVC from the discovered VS installation is preferred, with
Clang/GCC fallback when no MSVC is found.

Generated build files live in the user project's reserved `.cpplivelab` directory.
Multi-file builds use `.cpplivelab/build-Debug` or `build-Release`. Single-file builds
use `.cpplivelab/<source-filename>/source` and `build-Debug` or `build-Release` beside
the source file. These files are disposable build artifacts, not development checkpoints.

See [STAGE2_MANUAL_TESTS.md](STAGE2_MANUAL_TESTS.md) for exact, independent verification
commands covering every Stage 2 feature, including console input and compilation errors.

## Manual checks

1. Run `--version`: expect `EasierCpp 0.1.0` and exit 0.
2. Run with no arguments or `--help`: expect usage and exit 0.
3. Run `doctor` from ordinary PowerShell: expect CMake, Ninja, MSVC and Clang to show
   `Installed: Yes` on this machine, even when `Available in current terminal: No`.
   Expect exit 0. GCC may show `Installed: No`.
4. Run `unknown` or `--version extra`: expect an error and exit 2.
5. Check `$LASTEXITCODE` immediately after each command.

## Tests and structure

CTest runs discovery tests against temporary Visual Studio and standalone installation
fixtures (empty PATH, paths with spaces, missing tools, optional GCC, and multiple VS
instances), plus CLI tests for version, help, invalid commands, extra arguments, and doctor.
An integration test runs the real doctor with PATH emptied against this machine's
installed Visual Studio toolchain. Stage 2 integration builds real temporary projects
with empty child PATH, checks Debug and Release, single/multi-file programs, console I/O,
custom executable paths, errors, exit codes, overwrite protection and project separation.
The full suite requires the installed Visual Studio C++ toolchain and Windows SDK.
Stage 3 tests exercise the same process supervisor used by the GUI: incremental console
I/O, EOF, output flooding, nonblocking input backpressure, exit codes, process-tree Stop,
destruction cleanup, prevention of concurrent builds and external-executable overwrites,
rebuild after Stop, cached-option/preset preservation, and VS2022 IDE discovery.
Maintenance regressions additionally cover cross-drive/UNC directory boundaries,
Unicode diagnostic paths and a real Unicode-folder build/run, UTF-8 characters split
across reads, RAII handle lifetime, and argument round-trips through both launch paths.
`include/` exposes the core interfaces; `src/` holds the core, CLI and native GUI;
`tests/` holds tests; `scripts/` holds the Windows build bootstrap.

## Limitations

Doctor checks executable files, not version compatibility, SDK availability, or a compile
probe. A real build validates the selected compiler. Custom standalone install folders
must be on PATH if they are outside the locations searched above. Visual Studio discovery
requires its bundled vswhere at the standard installer location.
The build script requires Visual Studio C++ tools; the preset can also use an already
configured compiler. MSVC x64 on Windows is the verified build/run toolchain; fallback
compilers have not been compile-tested. Managed CMake projects support Ninja, Ninja
Multi-Config and VS2022 generators; dependencies and custom options remain project-owned.
Source paths containing semicolons or newlines are unsupported by the single-file wrapper.
Concurrent builds of the same project/configuration are refused. GUI input is line-based,
with a 1,000-character field limit; output is plain text with bounded history, not a
terminal emulator. Full-screen terminal applications and ANSI cursor control are unsupported.
There is no program-argument forwarding or persistent execution state. Recent project
paths and per-project build settings are manager-owned metadata.
The GUI opens CMake project folders; single-source files remain supported by the CLI.
No custom code editor, memory hot reloading, checkpoints or application-state restoration is implemented. OS window placement restoration is available with Auto Run.
Known fmt/JSON and verified pinned GitHub integrations can be selected for new projects.
Manage Libraries first saves an external preview. A SEPARATE exact-change approval
can append a dependency block to an existing root CMakeLists.txt, back it up, build,
and restore the original bytes on failure/cancellation. Source/assets/presets are
not edited by the manager. Unsupported dependency configurations need manual work.

## Automatic builds, consoles, and appearance

The latest prompt-clearing, Dark/Light palette and test-performance refinements are
documented in [STAGE4_FINAL_REFINEMENTS.md](STAGE4_FINAL_REFINEMENTS.md), including
numbered manual checks. [TESTING.md](TESTING.md) provides exact Quick (28 tests) and
Full (41 tests) selections and commands. Full remains the default for stage approval.
Tools offers concise discovery diagnostics and a separate detailed, copyable report.

Open a project, enable **Auto Run: ON**, press Run, and save relevant source changes.
The existing watcher debounces saves (default 400 ms), fingerprints content and
ignores generated directories. CMake/Ninja builds a replacement in the alternate
manager-owned build slot while the current application remains running. Compiler
errors retain the old application; successful, current builds request a graceful
restart. Window placement restoration is attempted for the managed process tree.

Restart launches the latest successful artifact without building. Stop cancels
queued automatic work; only a later relevant save schedules new work, and a stopped
application is not automatically launched. Force-stop after the graceful timeout
is OFF by default and can be enabled in Settings > Build Preferences. Project Auto
Run/window/shutdown preferences live in external manager metadata. See
[the current acceptance guide](PROJECT_REDIRECTION_MANUAL_TESTS.md) for limitations.

**Program Console** contains stdout, red `[stderr]` output, and input echoes. Ordinary
stdout stays neutral. **Build & System** contains full compiler diagnostics and
labelled build/system/success/warning/error messages. Inactive tabs show `(new)` or
`(!)` without changing focus. Each tab has its own Clear button, selection/copy and
scroll position. Output follows only when already at the bottom without a selection.
Clear Program preserves a reliably observed unfinished stdout line while the program
is running, including prompts split across writes, without resetting input or state.
When no reliable line exists, the neutral running/input status remains available.
Each history retains at most 400,000 characters/3,000 entries, discarding oldest
entries first. Separate pipes preserve each stream's order, but simultaneous stdout
and stderr cannot have a guaranteed combined order. Program buffering still applies;
programs should flush prompts normally (`std::cin` does so for tied `std::cout`).

**Appearance** offers Dark, Light, and Follow Windows, with rounded native controls,
themed console colors and per-monitor DPI handling. Follow Windows updates when the
OS broadcasts a theme change. Theme and debounce are persisted as
DWORD preferences under `HKCU\Software\EasierCpp\Preferences`. Auto Run is a single per-project
preference stored outside the source tree. An explicit project ON/OFF wins over the
read-only legacy global Auto Build migration input. New profiles use a 150 ms debounce;
existing saved values are retained. Ordinary launch shows the
welcome screen; Restart restores the active project and persistent preferences.
It does not restore a running program or checkpoint. Native scrollbar/title-bar details may vary with Windows version.

Auto Run ON builds relevant saved changes and launches the selected current target,
including from idle. Enabling it alone does not launch or stop a program. Stop pauses
automatic launching; Run, Restart, or an OFF/ON toggle rearms it. OFF cancels automatic
workers and pending automatic launches while preserving explicit manual Run requests.
Manual Build/Run/Stop/Restart remain available. See the
[unified Auto Run acceptance guide](docs/manual-tests/auto-run/FAST_AUTO_RUN_MANUAL_TESTS.md).
Customized palette RGB values, measured contrast and theme-testing details are in
[THEME_ACCESSIBILITY.md](THEME_ACCESSIBILITY.md).

See [STAGE4_MANUAL_TESTS.md](STAGE4_MANUAL_TESTS.md) for numbered acceptance tests and
[STAGE4_REVIEW.md](STAGE4_REVIEW.md) for files, implementation decisions and limitations.

## Unified Stage 5 project workflow

See [generated project standards](GENERATED_PROJECT_STANDARDS.md) and the
[numbered manual acceptance guide](STAGE5_UNIFIED_MANUAL_TESTS.md) for shared CMake
generation, modular dependencies, empty GitHub Add input, source diagnostics and
approved fresh-directory recovery. Existing imported project files are preserved.
Current registration: Quick 32 tests; Full 48 tests, including the real generated
Raylib 6.2 Debug/Release/window regression. Full requires network and a desktop.

## Stage 5 final libraries and Restart

Use **Project > Manage Libraries** for version updates, target changes, removal,
catalog visibility/deletion, registered cache inspection/deletion and exact-pin
repair. Add Library retains its simple growing list. Existing project edits need
an exact preview and approval; failed validation restores the prior configuration.
Project pins survive catalog deletion. Legacy/unregistered or linked paths are
never guessed as safe cleanup targets.

**Restart** is beside the single Settings button. It prepares a replacement process
and waits for startup acknowledgment before closing the current app. Active work
requires confirmation; critical filesystem work finishes at a safe boundary.
Project/editor/build preferences return, while temporary dialog/scheduler state resets.

See [the complete manual guide](STAGE5_FINAL_MANUAL_TESTS.md),
[implementation/testing report](STAGE5_FINAL_REPORT.md), and [suite details](TESTING.md).
The additional six-library live workflow suite is explicit:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Libraries -Jobs 1
```

It is separate from deterministic Quick and the original Full regressions.
