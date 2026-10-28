# Stage 2: independent manual tests

Use an ordinary PowerShell terminal. No developer shell or PATH setup is needed.
These commands keep every user project outside the CppLiveLab source folder.

## 1. Build CppLiveLab and execute all automated tests

```powershell
cd C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Doctor
$LASTEXITCODE
```

Expect exit 0 and all registered CTest tests to pass, including the seven Stage 1 tests.
The integration test creates temporary projects, clears each child process's PATH,
builds with MSVC, exercises the programs, and removes its temporary files.

For the remaining checks, set these two variables in your PowerShell window:

```powershell
$lab = 'C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab\build\debug\cpplivelab.exe'
$projects = 'C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab-UserProjects'
```

## 2. Stage 1 regression checks

```powershell
& $lab --version
& $lab --help
& $lab doctor
$LASTEXITCODE
```

Expect version `CppLiveLab 0.1.0`, help including the new commands, and doctor exit 0.
Installed tools can correctly report terminal availability No.

## 3. Create a standard C++20 project in your chosen folder

Choose a new or empty folder. If `HelloProject` already exists, choose a different name.

```powershell
& $lab create "$projects\HelloProject"
Get-Content "$projects\HelloProject\CMakeLists.txt"
Get-Content "$projects\HelloProject\src\main.cpp"
```

Expect C++20 CMake settings and a hello-world program. Repeating the create command
must fail without overwriting the project.

## 4. Compile only; compare Debug and Release

```powershell
& $lab build "$projects\HelloProject" --config Debug
$LASTEXITCODE
& $lab build "$projects\HelloProject" --config Release
$LASTEXITCODE
Get-Item "$projects\HelloProject\.cpplivelab\build-Debug\app.exe"
Get-Item "$projects\HelloProject\.cpplivelab\build-Release\app.exe"
```

Expect two successful builds, exit 0, and separate executables. `build` does not run them.

```powershell
& $lab run "$projects\HelloProject" --config Debug
& $lab run "$projects\HelloProject" --config Release
```

Each run builds first, then prints `Hello from C++20!`. Debug is the default when
`--config` is omitted.

## 5. Calculator demo: interactive console and multiple source files

A calculator project is already provided at the path below. To create another one,
use `create <new-folder> --demo calculator`.

```powershell
& $lab run "$projects\CalculatorDemo" --config Debug
```

At the prompts enter `Ada Lovelace`, then `2.5`, then `3.75`, pressing Enter each time.
Expect `Hello, Ada Lovelace! Sum: 6.25` and exit 0. Prompts must appear before input.
Repeat with `--config Release` to test the optimized version. Enter a nonnumeric value
at a number prompt to see `Invalid number.` and exit 1.

The demo is a multi-file CMake project: `src/main.cpp`, `src/calculator.cpp`,
and `src/calculator.hpp`.

## 6. Compile and execute one C++20 source file

```powershell
@'
#include <iostream>
#include <string>
#include <span>
int main() {
    int values[] = {10, 20};
    std::span<int> numbers(values);
    std::string name;
    std::cout << "Name: ";
    std::getline(std::cin, name);
    std::cout << name << ": " << numbers[0] + numbers[1] << '\n';
}
'@ | Set-Content -Encoding utf8 "$projects\single.cpp"
& $lab build "$projects\single.cpp" --config Debug
& $lab run "$projects\single.cpp" --config Debug
```

Enter `Grace Hopper`; expect `Grace Hopper: 30`. The use of `std::span` verifies C++20.

```powershell
& $lab run "$projects\single.cpp" --config Release
```

Enter another name. Its output should again end with `: 30`.
The original source remains where you created it; generated CMake/build files go in
`$projects\.cpplivelab\single.cpp\`.

## 7. Existing multi-file CMake project with a custom executable name

This is independent of the generated templates. Use a new folder for this fixture.

```powershell
$existing = "$projects\ExistingCMake"
New-Item -ItemType Directory "$existing\src" -Force | Out-Null
@'
cmake_minimum_required(VERSION 3.20)
project(Existing LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(myprogram src/main.cpp src/sum.cpp)
'@ | Set-Content -Encoding utf8 "$existing\CMakeLists.txt"
@'
#include <iostream>
int sum(int, int);
int main() { std::cout << "Sum: " << sum(20, 22) << '\n'; }
'@ | Set-Content -Encoding utf8 "$existing\src\main.cpp"
'int sum(int a, int b) { return a + b; }' | Set-Content -Encoding utf8 "$existing\src\sum.cpp"
& $lab build $existing --config Debug
& $lab run $existing --config Debug --exe myprogram.exe
```

Expect `Sum: 42`. `--exe` is relative to `.cpplivelab\build-Debug` (or the selected
configuration's build directory). For example, a CMake subdirectory target might use
`--exe src\myprogram.exe`. Absolute executable paths are also accepted. Without this
option, `run` expects `app.exe`, the name used by generated projects.

## 8. Clear compilation errors and no execution after failure

```powershell
'int main() { THIS_IS_A_COMPILER_ERROR }' | Set-Content "$projects\broken.cpp"
& $lab run "$projects\broken.cpp"
$LASTEXITCODE
```

Expect compiler diagnostics with the source filename and line, `Compilation failed`,
and a nonzero exit code. There must be no `Running:` line.

## 9. Confirm automatic setup and unchanged terminal PATH

This test intentionally clears only the current PowerShell process's PATH temporarily.
The `finally` block restores it, even if a command fails.

```powershell
$savedPath = $env:PATH
try {
    $env:PATH = ''
    & $lab doctor
    & $lab run "$projects\CalculatorDemo" --config Debug
    $LASTEXITCODE
} finally {
    $env:PATH = $savedPath
}
$env:PATH -ceq $savedPath
```

Enter a name and two numbers. Expect successful compilation/execution, exit 0,
and `True` from the final comparison. No system-wide or user PATH is changed.

## 10. Project separation

```powershell
& $lab create 'C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab\ForbiddenProject'
$LASTEXITCODE
```

Expect rejection and exit 1. The folder must not be created. All demo and test projects
above are in the sibling `CppLiveLab-UserProjects` folder.
