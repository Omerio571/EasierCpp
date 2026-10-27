# CppLiveLab — Stage 1

A standalone C++20 command-line foundation for a development manager used alongside VS Code.
Stage 1 contains only version/help commands and an environment doctor.

## Build and test on Windows

Install Visual Studio 2022 (or Build Tools) with **Desktop development with C++**,
including its CMake tools. VS Code extensions: Microsoft C/C++ and CMake Tools.

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

Open this folder in VS Code with `code .`. Press Ctrl+Shift+B to build and test.
The default task loads the developer environment automatically.

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
installed Visual Studio toolchain.
`include/` exposes the doctor interface; `src/` holds the core and CLI;
`tests/` holds tests; `scripts/` holds the Windows build bootstrap.

## Limitations

Doctor checks executable files, not version compatibility, SDK availability, or a compile
probe. A real build validates the selected compiler. Custom standalone install folders
must be on PATH if they are outside the locations searched above. Visual Studio discovery
requires its bundled vswhere at the standard installer location.
The build script requires Visual Studio C++ tools; the preset can also use an already
configured compiler. Windows is the supported target. No graphical interface, project
management, library integration, automatic rebuilding, live updates, checkpoints, or
state restoration are implemented.
