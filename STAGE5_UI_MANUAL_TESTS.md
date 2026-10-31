# Complete Stage 5 manual acceptance guide

Stage 5 has not been manually approved. This guide covers the original project-management
features AND the UI refinements. Use new disposable projects for all edits, failures,
libraries and switching tests. Do not change FarmGame during UI acceptance.

## 1. Build, automated tests and launch

Close CppLiveLab first. Open ordinary PowerShell:

```powershell
Set-Location 'C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab'
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Quick -Jobs 4
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Full -Jobs 4 -Doctor
$testParent = Join-Path (Split-Path (Get-Location)) ('CppLiveLab-UserProjects\UIAcceptance-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $testParent
$testParent
.\build\debug\cpplivelab-gui.exe
```

Expected: Quick **25/25**, Full **35/35**, doctor succeeds. Full contains every existing
Stage 1–5 regression; Quick is not a substitute for approval. Keep this PowerShell
session and the printed test-parent path.

## 2. Workspace, navigation and keyboard

1. Expect exactly Project / Build / Tools / Settings in one themed top row. No title-bar
   gear and no separate competing Settings control.
2. Expect no permanent project-path input, Browse/Open buttons, appearance dropdown or
   debounce field. Frequent Build/Run/Stop/Open Editor/configuration/target/Auto Build,
   console tabs, input/Send/End Input and status remain available.
3. Press F10 to focus navigation, or Alt+P / Alt+B / Alt+T / Alt+S to open a section.
   Use arrows/Enter/Escape in menus. Tab/Shift+Tab should reach the normal controls.
4. Settings has Appearance, Preferred Editor and Build Preferences. Project Settings
   and Manage Libraries occur only under Project. Build menu actions use the same
   implementation/state as toolbar actions.

## 3. New empty project, location Browse and validation

1. Project > New Project. Name `EmptyDemo`. Browse to the printed `$testParent`, or
   paste/type it into Parent folder. The new folder will be inside that parent.
2. Choose **Empty C++ Project**. Use the template/location information buttons or
   focus the relevant field and press F1. Expected: short explanations; hover help
   is also available. Blank optional libraries means no dependencies/download.
3. Create. Expected: the project opens and its name/path update once in the workspace.
   Build, then Run: expect `Hello from C++20!` and target `app` discovered.
4. Reopen New Project and attempt the same name/location. Expected: refusal, no overwrite.
   Also try `CON`, a name containing `?`, and a path instead of a single folder name.
   Expected: rejection before files are created. Cancel should create nothing.
5. Open a project with a long path/name if available. Expect path ellipsis, no overlap.
   Project > Copy Project Path should copy the full path; paste it into PowerShell
   or Notepad to inspect it. This action does not modify the project.

## 4. Preferred editor and Advanced Options

1. Settings > Preferred Editor. Choose an installed VS2022/VS Code/CLion editor.
   Ordinary choices use automatic detection; no override is required when located.
2. Expand Advanced Options. Expect Executable override with executable Browse, and
   custom arguments. Help explains each override and `{project}`. Collapse again.
3. Choose Custom. Expect necessary advanced fields automatically visible. Pick a real
   editor .exe. Arguments are one per line; `{project}` becomes the whole folder as
   one safely quoted argument. Leave arguments blank for ordinary folder opening.
4. Save, then Open Editor. Expected: selected folder opens; compiler/build settings
   do not change. Reopen settings/restart the manager: preference remains.
5. Set a nonexistent custom .exe, save and Open Editor. Expect a useful error, no
   project changes. Restore a valid editor. Nonstandard/Toolbox installations can
   use an explicit .exe path. Real editor UI/debugging acceptance remains manual.

## 5. Multi-file project and normal editing/building

1. Project > New Project: `MultiDemo`, same test parent, **Multi-file C++ Project**,
   no libraries. Build/Run: expect `Hello from two C++ files!`.
2. Open Editor, change the returned message in src/greeting.cpp, save and Build/Run.
   Expect the new message. include/greeting.hpp remains a separately included header.
3. Select Release, Build/Run; return to Debug. Expect separate build directories.
   The GUI prints manager-owned output paths, not an IDE's existing binary directory.

## 6. Multiple libraries: growing rows and actual integration

1. New Project: `TwoLibraries`, same parent, either template.
2. Click Add Library. Choose **fmt**, read its formatting/download/support information,
   then Add. Expected: `Added Library: fmt`, a Remove action, then Add Library below.
3. Add **JSON for Modern C++**. Expected: both rows remain; the form grows, and Add
   Library remains underneath. It becomes disabled when all currently supported
   catalog entries are already selected. Duplicates are not offered.
4. Remove either selection. Expected: only that row disappears and Add becomes usable.
   Add it back, then Create with both selected.
5. Build with internet access, then Run. Expected: pinned/checksummed downloads,
   successful compile/link, and output containing `fmt dependency verified:` and a
   JSON object with `answer` equal to `42`.
6. Existing supported integrations are fmt 11.0.2 and JSON for Modern C++ 3.11.3.
   Raylib/RayMath names in the illustration describe row behavior, not additional
   validated installers. RayMath is associated with Raylib; no separate automatic
   RayMath installer is claimed. No search UI is needed for two catalog entries.

CLI equivalent for a different NEW folder:

```powershell
$both = Join-Path $testParent 'TwoLibrariesCli'
.\build\debug\cpplivelab.exe new $both --template multi --library 'fmt,nlohmann_json'
.\build\debug\cpplivelab.exe manage-run $both --target app
$rejected = Join-Path $testParent 'RejectedLibrary'
.\build\debug\cpplivelab.exe new $rejected --library 'https://invalid.invalid/library.git'
Test-Path -LiteralPath $rejected
```

Expected: both valid libraries build/run together. Unknown Git integration is rejected
and Test-Path is False. Merely selecting/creating is never called installed successfully.

## 7. Manage Libraries: existing projects stay read-only

1. Open a disposable project, then Project > Manage Libraries. Expect project
   declarations labelled as evidence and configured CMake targets when available.
2. Add both catalog entries. Expect independent rows, descriptions/download information
   in the picker and per-row Remove. Removing a row only changes this selection.
3. Save Preview. Expected: Build & System reports a manager-owned
   `library-integration-preview.cmake` path. It contains the selected integrations,
   but no original CMake/source/assets were modified and nothing was installed.
4. Review the preview externally if desired. Applying it to an existing project is
   a separate, explicit user edit; automatic imported-project rewriting/removal is
   intentionally unavailable. Do not apply it to FarmGame during UI testing.

## 8. Project Settings, presets and working directories

1. Project > Project Settings. Working directory is the basic setting with Browse/help.
   Blank means project root. A full override is useful for relative asset lookups.
2. Advanced Options exposes configure preset and vcpkg root, with explanations.
   Leave defaults blank unless needed. Cancel must preserve saved values.
3. Set a valid working directory in one disposable project, save, switch projects,
   then return. Expect its value/configuration retained independently.
4. For a disposable project with existing presets, enter an existing configure preset
   name. Build should preserve inherited options/compiler/toolchain and use a separate
   manager-owned binary directory. Missing/incompatible choices should fail clearly.
   `-` explicitly chooses the no-preset Ninja profile. No preset files are rewritten.
5. A vcpkg root must contain scripts/buildsystems/vcpkg.cmake. Invalid roots fail.
   Real vcpkg acquisition requires an installed/configured vcpkg and remains unverified
   on this machine; do not override a preset's own toolchain.

## 9. Multiple executable targets and assets

Create a disposable custom-layout fixture from the same PowerShell session:

```powershell
$targets = Join-Path $testParent 'Multiple Programs'
New-Item -ItemType Directory -Path $targets
New-Item -ItemType Directory -Path (Join-Path $targets 'Odd Sources')
New-Item -ItemType Directory -Path (Join-Path $targets 'Resources')
@'
cmake_minimum_required(VERSION 3.24)
project(Multiple LANGUAGES CXX)
add_executable(game "Odd Sources/entry.cpp")
add_executable(tool "Odd Sources/tool.cpp")
set_target_properties(game PROPERTIES OUTPUT_NAME actual_game RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
'@ | Set-Content -LiteralPath (Join-Path $targets 'CMakeLists.txt') -Encoding utf8
@'
#include <fstream>
#include <iostream>
int main()
{
    std::ifstream input("value.txt");
    std::string value;
    input >> value;
    std::cout << "GAME " << value << '\n';
    return value == "assets-ok" ? 0 : 1;
}
'@ | Set-Content -LiteralPath (Join-Path $targets 'Odd Sources\entry.cpp') -Encoding utf8
@'
#include <iostream>
int main()
{
    std::cout << "TOOL\n";
}
'@ | Set-Content -LiteralPath (Join-Path $targets 'Odd Sources\tool.cpp') -Encoding utf8
'assets-ok' | Set-Content -LiteralPath (Join-Path $targets 'Resources\value.txt') -Encoding ascii
$targets
```

1. Project > Open Existing, select this folder and Build. Expect `game` and `tool`.
2. Project Settings: Browse to its Resources working directory. Select game and Run:
   expect `GAME assets-ok`, even though the artifact is named actual_game.exe.
3. Select tool, Build/Run: expect `TOOL`. Reopen the project and check selection.
4. Repeat in Release, then return to Debug. Expect independent binaries/settings.
5. Select an invalid target through CLI to check refusal:

   ```powershell
   .\build\debug\cpplivelab.exe targets $targets
   .\build\debug\cpplivelab.exe manage-run $targets --target nonexistent
   ```

   Expected: useful nonzero failure, no wrong/stale executable launch.

## 10. Recent Projects and safe switching

1. Project > Recent Projects. Expect scrollable rows with project name AND path,
   direct Open and Remove from Recent buttons, not a second action dropdown.
2. Open EmptyDemo/MultiDemo. Expect their own target/configuration/settings.
3. Remove a recent entry. Expect its files remain and Open Existing still works.
4. A missing test-project location should report an actionable message and permit
   removing its stale entry. Do not rename/delete real projects for this test.
5. During an active build/program, attempt switching. Expect switching blocked until
   completion/Stop. Pending old-project saves/Run requests must not attach to the next
   project. Edit an inactive disposable project: only the active project is watched.

## 11. Auto Build, Intelligent Run, failures and consoles

1. Settings > Build Preferences: set debounce to **675**, OK. Reopen to verify it.
   Try invalid input such as `abc` or `20`: expect inline validation without closing
   the form. Cancel keeps the previous value. Restore 400 when finished if preferred.
2. Turn Auto Build ON. Enabling establishes a baseline. Make several quick saves
   in MultiDemo; expect one debounced build. Edit while a build is active; it finishes
   and coalesces requests into one follow-up rather than repeatedly cancelling.
3. Switch/reopen projects and confirm the chosen global debounce remains applied.
4. Run unchanged Ninja projects repeatedly. Expect dependency checks instead of
   unnecessary compilation. VS-generated projects perform an incremental build.
5. In a disposable project remove a semicolon and Build/Run. Expect visible compiler
   errors, failed status and no older executable launch. Restore it and rebuild.
6. Create a fresh interactive calculator with the preserved CLI:

   ```powershell
   $calculator = Join-Path $testParent 'Calculator'
   .\build\debug\cpplivelab.exe create $calculator --demo calculator
   $calculator
   ```

   Open it, Build/Run, Clear Program at the name prompt, enter Ada then 2.5, clear at
   Second number, then enter 3.75. Expect preserved prompts and `Sum: 6.25`, proving
   process/input/state survive Clear. Verify independent tab clearing and scroll/selection.
7. End Input/Stop, then Run again. Expect input enabled. Stop must affect only the
   owned operation and leave the external editor open. Saving never restarts a program.
8. Tools > Toolchain Diagnostics writes to Build & System; no redundant diagnostic
   dialog. Build menu and toolbar must execute each requested operation once.

## 12. Theme, help, resize and DPI

1. Settings > Appearance: preview Dark/Light, cancel and verify no persisted change;
   repeat with OK, then restart and verify persistence. Follow Windows should track
   OS application mode. Your customized semantic colors must remain unchanged.
2. Open New/Recent/Preferred Editor/Project Settings/Manage Libraries/Build Preferences.
   Expect matching palettes, fonts, rounded controls, input borders and spacing.
3. Help: hover information buttons, Tab/Space to activate them, or focus a relevant
   field and press F1. Expect an explanation readable by keyboard; advanced controls
   must not remain in the Tab order when hidden.
4. Resize the workspace smaller, maximize/restore, minimize/restore. Expect no toolbar,
   target, tab or input overlap; console area takes the size change.
5. If available, move between monitors/scaling levels. Forms should scale and scroll
   when taller than available space; Tab should bring focused controls into view.
   Closing/cancelling a form should restore focus to the main application.
6. Native OS folder/executable pickers may keep Windows styling. Native title-bar/
   popup chrome can vary by OS. Real mixed-monitor and screen-reader checks remain
   manual; off-screen snapshot review does not prove those environments.

## 13. FarmGame protection and later real-project acceptance

For this UI pass, inspect/open only with Auto Build OFF and cancel configure permission.
Do NOT edit FarmGame or authorize configuration simply to test its dialog.

```powershell
$farm = 'C:\Users\omer2\OneDrive\Desktop\Projects\FarmGame'
$farmBefore = @(Get-ChildItem -LiteralPath $farm -Recurse -File -Force | ForEach-Object {
    [pscustomobject]@{Path=$_.FullName;Hash=(Get-FileHash -LiteralPath $_.FullName).Hash}
})
$farmDirsBefore = @(Get-ChildItem -LiteralPath $farm -Recurse -Directory -Force | ForEach-Object FullName)
.\build\debug\cpplivelab.exe inspect $farm
```

Open it through Project > Open Existing. Expect its identity, Assets and Raylib evidence.
If testing Build's permission prompt, choose Cancel; Cancel is the default. Then:

```powershell
$farmBefore | Where-Object {
    -not (Test-Path -LiteralPath $_.Path) -or (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Hash
}
Compare-Object @($farmBefore.Path) @(Get-ChildItem -LiteralPath $farm -Recurse -File -Force | ForEach-Object FullName)
Compare-Object $farmDirsBefore @(Get-ChildItem -LiteralPath $farm -Recurse -Directory -Force | ForEach-Object FullName)
```

Expected: no differences. FarmGame's existing CMake can create folders and conditionally
write Source/main.cpp. Real build/run is a separate explicitly authorized acceptance
step, documented in STAGE5_MANUAL_TESTS.md. It has NOT been verified against the original
FarmGame automatically. Its Assets/Sprites lookups match a project-root working directory.

Record failures/expected results for all scenarios before Stage 5 approval. No automatic
commit/push or Stage 6/7 work is included.
