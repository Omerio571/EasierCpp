# CppLiveLab — Stage 4

A standalone Windows C++20 development manager, with a small native graphical interface.
Stage 1 provides version/help and toolchain discovery. Stage 2 adds project creation,
single-file and multi-file CMake builds, and console execution with interactive input.
Stage 3 makes CppLiveLab the main application, with Visual Studio 2022 used for editing
and debugging. Stage 4 adds optional automatic compilation, two console tabs, and
Dark, Light, and Follow Windows appearance. The CLI retains its commands and version.

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
including its CMake tools and Windows SDK. Visual Studio 2022 IDE is required for the
Edit button; Build Tools alone can still build and run projects. VS Code is optional.

From PowerShell in this folder:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Doctor
.\build\debug\cpplivelab.exe --version
.\build\debug\cpplivelab.exe --help
.\build\debug\cpplivelab.exe doctor
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

If using VS Code, run the named `CppLiveLab: build and test` task. It loads the developer
environment automatically. Existing editor tasks are preserved.

## Start the graphical application

```powershell
.\build\debug\cpplivelab-gui.exe
```

Double-clicking `build/debug/cpplivelab-gui.exe` also works. Keep the companion
`cpplivelab.exe` in the same directory. No Raylib, SDL, GUI framework, or graphics-library
installation is needed; the interface uses Windows controls and system libraries.

Paste a CMake project folder and click **Open Project**, or use **Browse...**.
The active project's folder name and full path appear above the buttons. **Edit in
Visual Studio 2022** discovers the installed 17.x IDE using vswhere and opens that
folder directly. It does not generate a solution or change project settings.
Visual Studio's [Open Folder workflow](https://devblogs.microsoft.com/cppblog/bring-your-c-codebase-to-visual-studio-with-open-folder/)
supports this direct `devenv.exe <folder>` launch.

Choose Debug or Release, then **Build** or **Run**. GUI Run reuses a successful build
only after checking the selected project/configuration/executable, source fingerprints,
build artifacts and a Ninja dependency dry-run. Unknown or changed inputs build first;
observations are never trusted across restarts. Program output appears in
**Program Console**. Type into the input field and press **Send** or Enter
for each line. **End Input** closes stdin. For custom executable names, set the
Executable field relative to the chosen build directory, such as `src/myprogram.exe`.

**Stop** terminates only the operation's owned Windows Job Object (the CLI, compiler,
program and descendants). It is a forced stop, so the program does not get a cleanup
callback. Build and project selection stay disabled until that process tree has
exited; Run can queue behind a build. A completed build worker releases owned
background tool helpers rather than waiting indefinitely for them. Closing the
window while busy also stops its owned processes. Visual Studio
is launched separately and is never included in that job.

CppLiveLab holds a per-project/configuration build lock through program execution,
blocking simultaneous builds/runs from another instance. It also checks existing build
executables for write access before configuring, to catch programs launched externally.
The OS remains responsible for enforcing executable locks if an external program is
started after that check.

User builds remain in their own `.cpplivelab/build-Debug` and `build-Release` directories,
separate from this manager's `build/debug` and Visual Studio's usual `out/build` tree.
CppLiveLab does not write CMake presets, CMakeSettings, or Visual Studio configurations.
New build directories receive the initial defaults; subsequent configurations retain
cached compiler choices and custom cache variables. Existing project sources are untouched.

A fresh addition demo for GUI testing is in `../CppLiveLab-UserProjects/Stage3Calculator`.
Your existing CalculatorDemo edits are preserved. See [STAGE3_MANUAL_TESTS.md](STAGE3_MANUAL_TESTS.md)
for numbered steps and expected results.

## Create, build and run user programs

Keep user projects outside this repository. For example, from this folder:

```powershell
.\build\debug\cpplivelab.exe create ..\CppLiveLab-UserProjects\HelloProject
.\build\debug\cpplivelab.exe build ..\CppLiveLab-UserProjects\HelloProject --config Debug
.\build\debug\cpplivelab.exe run ..\CppLiveLab-UserProjects\HelloProject --config Release
```

Create accepts a new or empty folder and never overwrites an existing project.
Use `create <new-folder> --demo calculator` for the multi-file calculator template.
A fresh addition demo is in the sibling `CppLiveLab-UserProjects/Stage3Calculator` folder.

`build <path>` accepts a `.cpp`, `.cc`, `.cxx` source file or an existing CMake project
folder. `run <path>` builds first and launches only on success. Debug is the default;
`--config Release` selects a separate optimized build. Compiler and CMake diagnostics
appear directly in the terminal. The program inherits stdin, stdout, stderr and its
project working directory; its exit code becomes CppLiveLab's exit code.

New projects use C++20 and target `app`. Existing CMake projects receive C++20 defaults;
their own explicit target settings remain authoritative. Use `run <folder> --exe
<relative-path>` for an existing project whose executable is not `app.exe`.
The executable path is relative to the selected build directory; absolute paths work too.

Tool discovery supplies absolute CMake, Ninja and compiler paths. When MSVC is selected,
CppLiveLab captures `VsDevCmd.bat`'s environment and supplies it to child processes,
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

1. Run `--version`: expect `CppLiveLab 0.1.0` and exit 0.
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
compilers have not been compile-tested. Existing CMake projects must support Ninja and
provide their own dependencies/options; executable selection is explicit with `--exe`.
Source paths containing semicolons or newlines are unsupported by the single-file wrapper.
Concurrent builds of the same project/configuration are refused. GUI input is line-based,
with a 1,000-character field limit; output is plain text with bounded history, not a
terminal emulator. Full-screen terminal applications and ANSI cursor control are unsupported.
There is no program-argument forwarding, project history, or persistent execution state.
The GUI opens CMake project folders; single-source files remain supported by the CLI.
No custom code editor, library integration, hot reloading, checkpoints, or state
restoration is implemented.

## Automatic builds, consoles, and appearance

The current automated reliability retest and manual handoff are documented in
[STAGE4_REPAIR_MANUAL_TESTS.md](STAGE4_REPAIR_MANUAL_TESTS.md). The earlier requested
Simple/Detailed diagnostic presentation and additional Light palette improvements
remain open; passing automated tests is not approval of those unfinished requests.

Open a project, enable **Auto Build: ON**, edit in Visual Studio, and save. The default
debounce is 400 ms; the field accepts 100–5000 ms and applies when focus leaves it.
Turning Auto Build on establishes a fresh baseline; it does not build immediately or
retroactively build changes made while off. Manual Build and Run remain available.
Only the active project is watched. Source saves use incremental compilation;
CMake changes request configuration. Builds never restart the application.

The watcher uses overlapped `ReadDirectoryChangesW` with one event, no watcher thread,
and nonblocking event checks on the existing GUI timer. Content fingerprints suppress
duplicate saves. Directory changes/notification overflow reconcile a snapshot. Rapid
saves coalesce; changes during a build leave one pending follow-up. Generated folders
(`.git`, `.vs`, `.cpplivelab`, `build`, `out`, `CMakeFiles`, common build prefixes and
custom directories containing CMakeCache.txt) are excluded. A running program keeps
its build lease: changes show **Build Deferred**, then build after exit or Stop.
Watchers cancel and release their Windows handles on project switch and close.

**Program Console** contains stdout, red `[stderr]` output, and input echoes. Ordinary
stdout stays neutral. **Build & System** contains full compiler diagnostics and
labelled build/system/success/warning/error messages. Inactive tabs show `(new)` or
`(!)` without changing focus. Each tab has its own Clear button, selection/copy and
scroll position. Output follows only when already at the bottom without a selection.
Each history retains at most 400,000 characters/3,000 entries, discarding oldest
entries first. Separate pipes preserve each stream's order, but simultaneous stdout
and stderr cannot have a guaranteed combined order. Program buffering still applies;
programs should flush prompts normally (`std::cin` does so for tied `std::cout`).

**Appearance** offers Dark, Light, and Follow Windows, with rounded native controls,
themed console colors and per-monitor DPI handling. Follow Windows updates when the
OS broadcasts a theme change. Only Theme, Auto Build and debounce are persisted as
DWORD preferences under `HKCU\Software\CppLiveLab`; no project or execution state is
restored. Native scrollbar/title-bar details may vary with Windows version.

See [STAGE4_MANUAL_TESTS.md](STAGE4_MANUAL_TESTS.md) for numbered acceptance tests and
[STAGE4_REVIEW.md](STAGE4_REVIEW.md) for files, implementation decisions and limitations.
