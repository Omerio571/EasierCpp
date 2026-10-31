# Stage 5 manual acceptance

For the CURRENT refined interface and all original Stage 5 features, use
[STAGE5_UI_MANUAL_TESTS.md](STAGE5_UI_MANUAL_TESTS.md). The original handoff below is
historical; its Yes/No permission choices now read Authorize/Cancel, template display
names are friendly, and recent projects use direct actions. Current suites are 25/35.

Use disposable new projects for A/B/D/F/G/H. Keep FarmGame unchanged during the
read-only import check. Close CppLiveLab before rebuilding the manager executable.
Do not commit/push as part of these steps.

## Setup and automated verification

1. Open ordinary PowerShell:

   ```powershell
   Set-Location 'C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab'
   powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Quick -Jobs 4
   powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Full -Jobs 4 -Doctor
   $homeForTests = Join-Path (Split-Path (Get-Location)) ('CppLiveLab-UserProjects\Stage5-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
   New-Item -ItemType Directory -Path $homeForTests
   $homeForTests
   .\build\debug\cpplivelab-gui.exe
   ```

   Expected: Quick 24/24, Full 34/34, installed tools detected. The new test parent
   is outside CppLiveLab. Keep the printed path and this PowerShell session.

## A — New empty C++20 project and Auto Build

1. In the GUI choose **Project > New Project**. Name `EmptyDemo`, paste the printed
   test parent as location, choose `empty`, library `none`, and click OK.
   Expected: it opens automatically and identifies the new source folder. Existing
   destinations are rejected rather than overwritten.
2. Click Build. Expected: success and executable target `app` selected. Click Run.
   Expected Program Console: `Hello from C++20!`. No download is required.
3. Set Preferred Editor to an installed editor (see E), then click Edit in Preferred
   Editor. Change the message in src/main.cpp to `Stage 5 changed`, save, and enable
   Auto Build. Enabling alone establishes a baseline; make another harmless comment
   edit/save. Expected: one debounced automatic build; no automatic program launch.
4. Click Run. Expected: `Stage 5 changed`. Run again unchanged: Ninja reports the
   build current and avoids an extra compilation. Settings/build paths printed in
   Build & System are under the manager's external state directory.

Independent CLI check for this new project:

```powershell
$empty = Join-Path $homeForTests 'EmptyDemo'
.\build\debug\cpplivelab.exe inspect $empty
.\build\debug\cpplivelab.exe manage-run $empty --target app --config Debug
```

## B — New multi-file project

1. New Project: name `MultiDemo`, same parent, template `multi`, library `none`.
2. Build and Run. Expected: `Hello from two C++ files!`.
3. Edit include/greeting.hpp and src/greeting.cpp using the external editor. Change
   only the returned message in greeting.cpp, save/build/run. Expected: the new message.
   No files in other projects are moved or edited.
4. Select Release, Build and Run. Switch back to Debug and Build/Run. Expected:
   separate output directories and appropriate target/configuration selections.

## C — Read-only import, including FarmGame

1. In the same PowerShell session, record FarmGame files/directories:

   ```powershell
   $farm = 'C:\Users\omer2\OneDrive\Desktop\Projects\FarmGame'
   $farmBefore = @(Get-ChildItem -LiteralPath $farm -Recurse -File -Force | ForEach-Object {
       [pscustomobject]@{ Path=$_.FullName; Hash=(Get-FileHash -LiteralPath $_.FullName).Hash }
   })
   $farmDirsBefore = @(Get-ChildItem -LiteralPath $farm -Recurse -Directory -Force | ForEach-Object FullName)
   .\build\debug\cpplivelab.exe inspect $farm
   ```

   Expected: source/header count, Assets directories, Raylib dependency evidence,
   and a configure-file-writing warning. Inspection does not fetch Raylib or run CMake.
2. In the GUI, turn Auto Build OFF. Choose Project > Open Existing Project and select
   FarmGame itself (the folder containing CMakeLists.txt). Expected: project name/path,
   isolated manager storage path and dependency/write warnings. No configure/build.
3. Click Build, then choose **No** at the configure-write permission dialog.
   Expected: no configuration, source writes or Raylib download.
4. Verify the read-only acceptance check:

   ```powershell
   $changed = @($farmBefore | Where-Object {
       -not (Test-Path -LiteralPath $_.Path) -or (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Hash
   })
   $farmAfter = @(Get-ChildItem -LiteralPath $farm -Recurse -File -Force | ForEach-Object FullName)
   $farmDirsAfter = @(Get-ChildItem -LiteralPath $farm -Recurse -Directory -Force | ForEach-Object FullName)
   $changed
   Compare-Object @($farmBefore.Path) $farmAfter
   Compare-Object $farmDirsBefore $farmDirsAfter
   ```

   Expected: no output from all comparisons. New/deleted directories and files are
   checked as well as modified file hashes.

### FarmGame build/run: separate explicit authorization

Do this only if you authorize executing FarmGame's existing CMake commands. That file
can create missing Source/Include/Assets subdirectories and conditionally write
Source/main.cpp. CppLiveLab has not changed it, and this is not a read-only action.

1. Review FarmGame/CMakeLists.txt. Leave its original layout and configuration intact.
   Check that Source/main.cpp and the intended existing source/assets are present.
2. If you authorize those CMake actions, click Build and choose Yes. Expected:
   discovered SDK tools initialize automatically, Raylib is fetched/built by the
   project's own CMake, and the executable target is discovered. Network/compiler
   failures remain visible; a failed build never launches an older binary.
3. Select the discovered FarmGame target if not selected automatically. In Project
   Settings leave working directory blank or set it to the FarmGame root. Its code
   loads `Assets/Sprites/...`, which matches that root.
4. Click Run. Expected: the external FarmGame window opens with its assets. Use Stop
   or close that window to finish; Stop must not close your editor. Build while it is
   running is disabled/deferred. Switch Debug/Release only after it stops.

FarmGame itself was **not built or run automatically** during implementation. This
is the real-project acceptance milestone; record any compiler/dependency/runtime
diagnostics before approval. Do not reorganize it to accommodate the manager.

## D — Multiple executable targets and working directories

Create a disposable custom-layout fixture in PowerShell:

```powershell
$targets = Join-Path $homeForTests 'Multiple Programs'
New-Item -ItemType Directory -Path $targets
New-Item -ItemType Directory -Path (Join-Path $targets 'Unusual Sources')
New-Item -ItemType Directory -Path (Join-Path $targets 'Resources')
@'
cmake_minimum_required(VERSION 3.24)
project(Multiple LANGUAGES CXX)
add_executable(game "Unusual Sources/entry.cpp")
add_executable(tool "Unusual Sources/tool.cpp")
set_target_properties(game PROPERTIES OUTPUT_NAME actual_game RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
'@ | Set-Content -LiteralPath (Join-Path $targets 'CMakeLists.txt') -Encoding utf8
@'
#include <fstream>
#include <iostream>
int main()
{
    std::ifstream file("value.txt");
    std::string value;
    file >> value;
    std::cout << "GAME " << value << '\n';
    return value == "assets-ok" ? 0 : 1;
}
'@ | Set-Content -LiteralPath (Join-Path $targets 'Unusual Sources\entry.cpp') -Encoding utf8
@'
#include <iostream>
int main()
{
    std::cout << "TOOL\n";
}
'@ | Set-Content -LiteralPath (Join-Path $targets 'Unusual Sources\tool.cpp') -Encoding utf8
'assets-ok' | Set-Content -LiteralPath (Join-Path $targets 'Resources\value.txt') -Encoding ascii
$targets
```

1. Open the printed project folder. Build once to discover targets. Expected dropdown:
   `game` and `tool`; no guessed filename based on the project folder.
2. Project Settings: set working directory to the fixture's Resources folder. Select
   `game` and Run. Expected: `GAME assets-ok` from the renamed actual_game.exe.
3. Select `tool`, Build and Run. Expected: `TOOL`. Reopen the project: expect its
   selected target retained. Configure Release separately and verify independence.
4. CLI equivalents:

   ```powershell
   .\build\debug\cpplivelab.exe targets $targets
   .\build\debug\cpplivelab.exe manage-run $targets --target game --working-dir (Join-Path $targets 'Resources')
   .\build\debug\cpplivelab.exe manage-run $targets --target tool --config Release
   .\build\debug\cpplivelab.exe manage-run $targets --target nonexistent
   ```

   Expected last command: useful error, nonzero exit and no wrong/stale program launch.

## E — Editor independence

1. Choose Preferred Editor > Visual Studio 2022 with blank executable override. Edit
   the selected test project. Expected: VS opens its folder; debugging stays in VS.
2. Repeat with VS Code or CLion only if installed. If detection fails for a nonstandard
   installation, set its actual .exe path. Do not use a .cmd shell wrapper.
3. Custom: set the actual editor executable, arguments one per line. `{project}` is
   replaced with the entire folder as one quoted argument; without it, the folder is
   appended. Close/reopen CppLiveLab: expect editor preference retained.
4. Set a nonexistent custom .exe path, then click Edit. Expected: useful launch error;
   project remains usable. Restore a valid editor. Build/Run should still use the
   same compiler and manager-owned output directory.

## F — Real optional library and failure handling

1. New Project: name `FmtDemo`, template `empty` or `multi`, optional library `fmt`.
2. Build and Run with internet access. Expected: pinned/checksummed fmt 11.0.2 fetch,
   actual compile/link success, then `fmt dependency verified: 42`. Build & System
   lists the configured fmt library target after success. Merely creating a project
   is not reported as a successful dependency installation.
3. CLI alternative for a different new project:

   ```powershell
   $fmt = Join-Path $homeForTests 'FmtCli'
   .\build\debug\cpplivelab.exe new $fmt --template empty --library fmt
   .\build\debug\cpplivelab.exe manage-run $fmt --target app
   $invalid = Join-Path $homeForTests 'RejectedLibrary'
   .\build\debug\cpplivelab.exe new $invalid --library 'https://invalid.invalid/library.git'
   Test-Path -LiteralPath $invalid
   ```

   Expected: the valid project builds/runs; unknown Git integration is rejected;
   Test-Path returns False. Existing projects are unaffected. Full tests also execute
   an unreachable-download fixture and verify no source change/program launch.
4. Imported-library addition/removal is not automated. Use the external editor and
   explicitly review any proposed CMake/source changes; the manager never guesses them.
   A real vcpkg installation can be selected in Project Settings or supplied as
   `--vcpkg <absolute-root>`; use an existing preset's toolchain instead when present.
   vcpkg package installation itself was not verified on this machine.

## G — Recent projects and safe switching

1. With no program/build active, use Project > Recent Projects to reopen EmptyDemo
   and MultiDemo. Expected: independent selected target, configuration and settings.
2. Remove one from the recent list using its Action selection. Expected: list entry
   gone, original files still present and reopenable through Open Existing Project.
3. Enable Auto Build, save several changes quickly and try switching during a build.
   Expected: active operation retains its project; finish or Stop before switching.
   A queued request must never compile the newly selected project's source as if it
   belonged to the old project. Once idle, switching cancels old pending/watch work.
4. Edit the inactive test project. Expected: only the active project's watcher can
   schedule builds. Reopening another project establishes its own fresh baseline.

## H — Stage 4 regressions

1. Run your existing calculator and enter a name/two numbers. Clear Program at a
   partial prompt: expect prompt preserved and calculation/input state retained.
2. Check independent Program/Build & System clearing, Dark/Light/custom colors and
   Follow Windows. Ordinary stdout remains neutral; diagnostic labels stay visible.
3. Save rapidly during an active build. Expect it to finish and coalesce changes
   into one follow-up; no unnecessary cancellation or automatic program restart.
4. Run unchanged Ninja projects again: expect dependency checking instead of extra
   compilation. Stop/restart and EOF/restart must keep input usable. For VS-generated
   projects an actual incremental build is performed before Run.

Stop after these tests and report results before approving Stage 5. No Stage 6/7
implementation or automatic Git commit/push is included.
