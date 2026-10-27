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
