# Stage 5 unified workflow: manual acceptance

Use NEW disposable projects for edits, errors, source deletion and recovery.
Keep FarmGame and existing personal projects read-only during acceptance.

## 1. Build and launch from ordinary PowerShell

Close CppLiveLab first; leave Visual Studio open if desired.

```powershell
Set-Location 'C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab'
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Quick -Jobs 4
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Full -Jobs 4 -Doctor
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\format.ps1 -Check
.\build\debug\cpplivelab.exe --version
.\build\debug\cpplivelab.exe doctor
Start-Process .\build\debug\cpplivelab-gui.exe
```

Expected: successful builds/tests, discovered tools from ordinary PowerShell, and
a welcome screen with one Settings destination. Full needs Internet and a Windows
desktop/OpenGL driver; its disposable Raylib windows briefly open and close.

## 2. Console generation, naming and compilation

1. Project > New Project. Select an external parent, e.g.
   `C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab-UserProjects`.
2. Name it `Unified Console Test`, choose Multi-file, select no libraries, Create.
3. Open Editor. Inspect CMakeLists.txt and src/main.cpp: project name is
   `Unified_Console_Test`, C/CXX and C++20 are declared, braces are Allman and
   indentation is four spaces. There is no unnecessary Dependencies.cmake.
4. Build Debug, then Run. Expected: greeting from the two C++ files; correct target
   is found without manually entering an executable path.
5. Build again without edits. Expected: no work to do/no recompilation.
6. Change the greeting text and save. With Auto Build ON, expect one automatic
   build, then Run shows the updated greeting. Save repeatedly during a build:
   it finishes normally and queues one follow-up for the newest changes.
7. Select Release, Build, Run. Expected: a separate Release artifact and same output.
8. Inspect the manager build path in Build & System. It is outside both the project
   and CppLiveLab's own build directory.
9. Optional source-list check: add another .cpp and explicitly add it to the CMake
   source list, then build. Delete it and remove the entry, then build again.
   Configuring does not create missing source files for you.

## 3. Empty GitHub input and multiple libraries

1. Open New Project or Project > Manage Libraries, then Add Library > GitHub Repository.
2. Expected: empty URL input; optional example text is a placeholder only; Retrieve
   Versions is disabled. There is no previously selected version or error.
3. Enter an invalid URL: retrieval stays disabled. Enter
   `https://github.com/raysan5/raylib`: retrieval becomes available.
4. Cancel, reopen Add GitHub Repository. Expected: empty again.
5. In a disposable New Project, add fmt and JSON for Modern C++. Expected: two
   added-library rows, Remove controls and another Add Library action; duplicate
   selection is refused. Create, Build, Run: output includes answer 42.
6. Inspect cmake/Dependencies.cmake: pinned dependencies and correct link targets;
   root CMake includes that file. Existing unrelated source is not reformatted.

## 4. Actual generated Raylib 6.2 project

1. New Project, external parent, name `Unified Raylib Test`.
2. Add GitHub Repository, enter `https://github.com/raysan5/raylib`, retrieve versions,
   explicitly select/enter `6.2`, resolve and review the exact commit. Approve only
   the intended Raylib verification. Choose its actual `raylib` target, verify link.
3. Create with that selected integration. Inspect root CMake, Dependencies.cmake
   and readable src/main.cpp. Full commit must be
   `02566453880e06bec97f5b8160fa6fe1e98397ca`; a short content-directory key is expected.
4. Build Debug. Expected: configure/download/compile/link completes; no 100-pass
   Ninja regeneration loop or filesystem iterator exception.
5. Run. Expected: an 800x600 Raylib window with Hello from CppLiveLab. Close it
   normally. Build unchanged: no recompilation.
6. Edit the displayed text, save with Auto Build ON, wait for success and Run again.
   Expected: updated text. Stop should terminate only the owned program tree.
7. Repeat Build/Run in Release. Inspect assets beside the actual artifact after
   adding an asset and causing a rebuild; imported asset conventions are unaffected.

## 5. Compiler errors and preferred editor

1. In a disposable project append `intentional compilation error` to main.cpp. Save.
2. Expected: Build Failed, no stale program launch, raw file/line compiler output
   and a concise next-action message in Build & System.
3. Double-click the file error. Expected: preferred editor opens the actual source;
   VS/CLion navigate to the line and Code supports line/column.
4. Keyboard alternative: focus the error line and Enter, or Tools > Review Build
   Errors > select diagnostic > Open in Editor > Perform Action.
5. With VS already open, verify file navigation reuses an existing instance. If
   several VS instances exist, VS chooses the receiving instance; verify it manually.
6. Repair and save. Expected: successful rebuild and updated program runs.
7. Settings > Preferred Editor supports Custom arguments, one per line, for example:
   `--goto` then `{file}:{line}:{column}`. Choose a real compatible editor executable.
   Project-only launch continues to support `{project}`.

## 6. Approved fresh build recovery

1. On a disposable project, Tools > Clean & Rebuild (Fresh Directory).
2. Read the confirmation, then Cancel. Expected: no build or active-directory change.
3. Open again and approve Build Fresh Directory. Expected: configure/build succeeds
   in a new manager-owned slot; only then it becomes active. Prior outputs and logs
   remain. Project source, CMake, assets and imported IDE build directories stay intact.
4. Introduce a source error and approve recovery. Expected: failure; the previous
   active directory stays selected. Repair source, build normally; no automatic
   cleanup/retry loop occurs.
5. While the program is running, recovery must ask you to stop the active operation;
   it must not overwrite a running executable or kill Visual Studio.
6. A real cache/generator/source mismatch can be reviewed via Tools > Review Build
   Errors; use fresh recovery only after reviewing the diagnostic. Do not delete
   arbitrary build/out folders. Automated fixtures cover deliberate cache corruption.

## 7. Existing projects and remaining regressions

1. Open an existing project without approving changes. Inspect name, path, target
   and project settings. Expected: no rewritten CMake, source, assets or organization.
2. FarmGame acceptance: select its existing preset/target and working directory;
   only build/run if you approve executing its existing CMake. Expected: its normal
   window/assets and separate manager outputs. Do not reorganize it.
3. Repeat calculator name/two-number input, End Input, Stop and project switching.
4. Check Dark, Light and Follow Windows, resize forms and Advanced Options. Check
   Tools > Review Build Errors and Clean & Rebuild have readable, unclipped controls.
5. Run integrated Quick Tests and toolchain diagnostics. Test cancellation must
   affect only the owned operation. Keep the existing full Stage 5 guide for other
   approved import/preset/library/editor scenarios.

Record your results before approving. Stage 6/7 and Git commit/push remain separate.
