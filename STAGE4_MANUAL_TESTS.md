# Stage 4 manual acceptance

Use ordinary PowerShell. Close CppLiveLab before rebuilding its executable. Leave
your existing CalculatorDemo and Stage3Calculator unchanged: these tests create a
new sibling project. No commit or push is part of these steps.

## Setup

1. Open PowerShell and run:

   ```powershell
   Set-Location 'C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab'
   powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Doctor
   .\build\debug\cpplivelab.exe --version
   .\build\debug\cpplivelab.exe doctor
   $demo = Join-Path (Split-Path (Get-Location)) 'CppLiveLab-UserProjects\Stage4Acceptance'
   .\build\debug\cpplivelab.exe create $demo --demo calculator
   $demo
   .\build\debug\cpplivelab-gui.exe
   ```

   Expected: all 30 Full CTest tests pass; version is `CppLiveLab 0.1.0`; doctor finds the
   installed toolchain without requiring a developer shell. A new calculator is
   created outside the manager. If Stage4Acceptance already exists, choose a new
   name rather than overwriting it.

2. Paste the printed `$demo` folder in **Project folder**, click **Open Project**,
   select Debug, leave Executable as `app.exe`, and click **Edit in Visual Studio
   2022**. Expected: the correct name/path appear and VS opens that CMake folder.
   Do not open or edit your old demo for these acceptance checks.

## A — Automatic compilation and result

1. Set **Auto Build: ON**, debounce **400**, then click elsewhere to apply it.
   Expected: `Watching`; enabling alone does not launch a build.
2. In Visual Studio open `src/calculator.cpp`. Keep the `add` function name and
   parameters; change only `return left + right;` to `return left * right;`, then
   press Ctrl+S. If your editor uses different parameter names, keep those names.
3. Select **Build & System**. Expected: Changes Detected, Building (Auto), then a
   green `[SUCCESS] Build Successful (exit 0)`. The first build takes longer while
   CMake detects the compiler; later source saves build incrementally.
4. Click **Run**. Wait for `Your name:` in **Program Console**. Enter `Ada` and
   press Enter; enter `2.5` and press Enter; enter `3.75` and click **Send**.
   Expected: `Hello, Ada!` and `Sum: 9.375`. The template still says "Sum" because
   we changed the calculation, not its display label. System/build logs stay out
   of this tab. Run is always explicit; saving does not start the application.

## B — Compile error and recovery

1. Leave Auto Build on. Add `#error Stage4 manual failure` to calculator.cpp and
   save. Remain on Program Console. Expected: Build & System shows `(!)` and the
   status becomes Build Failed; input focus and existing program output stay put.
2. Select Build & System. Expected: red `[ERROR]` compiler diagnostics include the
   source path, line and failure message. It does not retry endlessly without a
   further change. The last successfully linked executable is retained.
3. Remove the `#error`, save, and wait. Expected: another automatic build succeeds.

## C — Auto Build off, coalescing, configuration, and project switch

1. Set **Auto Build: OFF**. Restore addition in calculator.cpp and save. Wait two
   seconds. Expected: no automatic build. Click **Build**; expect success. Click
   Run and send Ada/2.5/3.75; expect `Sum: 6.25`.
2. Turn Auto Build on. Make several small comment edits and saves quickly (within
   400 ms if possible). Expected: rapid saves combine into one build. If another
   save lands during compilation, one follow-up build may occur; builds never
   overlap. Saving identical content does not request another build.
3. Add a harmless comment to `src/calculator.hpp` and save. Expected: an
   automatic build. Add a comment to CMakeLists.txt and save. Expected: CMake
   configuration and a successful build; your other settings remain intact.
4. Edit/create a `notes.txt` file, or a harmless temporary file inside `.vs`,
   `.git`, or `.cpplivelab`. Expected: no automatic build. Do not edit Git internals.
5. For a separate project-switch test, in PowerShell run:

   ```powershell
   $other = Join-Path (Split-Path (Get-Location)) 'CppLiveLab-UserProjects\Stage4Other'
   .\build\debug\cpplivelab.exe create $other
   $other
   ```

   Open that printed folder in the GUI. Save a comment in the previous calculator:
   expect no build. Save a comment in the new project's src/main.cpp: expect a build
   for Stage4Other only. Reopen Stage4Acceptance for the remaining checks. Project
   switching is disabled while an operation runs; Stop or finish it first.

## D — Separate consoles, input, severity, and scrolling

1. Run the calculator. Send one line with Enter and another with Send. Expected:
   prompts and neutral program output appear immediately; input echoes remain in
   Program Console. Build & System contains build/start/exit messages only.
2. Run again and click **End Input** at the name prompt. Expected: stdin closes and
   the calculator exits rather than leaving Send enabled indefinitely.
3. To test stderr, temporarily add `std::cerr << "Stage4 stderr test\n";` inside
   main, save, wait for success, and Run. Expected: red `[stderr] Stage4 stderr test` in Program
   Console. It is distinct from the neutral stdout. Stop, then remove that line.
4. Build with Program Console selected. Expected: a `(new)` indicator appears on
   Build & System; the selected tab/focus do not change. Switch tabs to read it.
5. Select some text and press Ctrl+C; paste into a scratch editor. Expected: normal
   readable text. Scroll up in a long build log, then Build again: it stays where
   you are reading. Return to the bottom and clear the selection: new output follows.
6. Click **Clear Build & System**. Expected: only that history clears. Select Program
   Console: its output remains. **Clear Program** clears only that history, preserving
   a reliable unfinished stdout line while running. See STAGE4_FINAL_REFINEMENTS.md
   for the active-prompt/input checks.

## E — Themes, persistence, and resizing

1. Select **Dark**, then **Light**, then **Follow Windows**. Expected: background,
   inputs, buttons, dropdowns, tabs and console colors change together; severity
   labels stay readable and stdout stays neutral. Existing output is retained.
2. Hover, press, Tab through controls, and run a program to inspect disabled controls.
   Expected: distinct hover/pressed/focus/disabled states and rounded corners.
3. Resize down to the minimum size, maximize and restore. Expected: no control
   overlap; consoles and input remain usable. If another monitor is available,
   move between different display scaling levels and inspect readability.
4. With Follow Windows selected, change Windows' app color mode normally in Settings
   (optional), then return. Expected: CppLiveLab follows the changed app color mode.
   This is a manual OS/display check, not claimed by the automated suite.
5. Select Light and set debounce to 450. Close and reopen CppLiveLab. Expected:
   Light, the Auto Build choice and 450 persist. No project/program/session is
   automatically restored. Reopen your test folder. Restore 400, Follow Windows
   and Auto Build OFF when finished if those are your preferred defaults.

## F — Saving while an executable is running

1. Restore addition and build successfully. Enable Auto Build, then Run and leave
   the calculator waiting at `Your name:`.
2. Change addition to multiplication and save. Expected: **Build Deferred - running
   program preserved**, not Build Failed. The existing prompt and process remain.
3. Send Ada/2.5/3.75. Expected: this already-running addition executable still gives
   `Sum: 6.25`. After exit, the pending build completes automatically. No new
   program starts automatically. Run explicitly: now expect `Sum: 9.375`.
4. Run again, leave it waiting, restore addition and save. Expected: deferred again.
   Click **Stop**. Expected: only the owned process tree stops, then the pending
   build succeeds. Run explicitly and expect `Sum: 6.25`.
5. Run and close the manager while waiting. Expected: its owned program terminates
   safely; VS and unrelated applications remain open. Reopen and build normally.

## G — Previous features and final regression

1. With the restored calculator, manually Build and Run in **Debug**, then **Release**.
   Expected: both succeed, interactive input works, and separate app.exe files exist:

   ```powershell
   Get-Item "$demo\.cpplivelab\build-Debug\app.exe"
   Get-Item "$demo\.cpplivelab\build-Release\app.exe"
   ```

2. Open the test project again and use Edit in Visual Studio 2022. Expected: the
   correct folder opens without rewriting CMakeLists.txt or your IDE configuration.
3. Verify the CLI still runs your test calculator:

   ```powershell
   .\build\debug\cpplivelab.exe build $demo --config Debug
   .\build\debug\cpplivelab.exe run $demo --config Release
   ```

   Send Ada, 2.5 and 3.75 on separate lines; expect `Sum: 6.25` after restoration.
   Single-file creation/build/run and other Stage 1–3 paths are also covered by
   the unchanged regression suite and earlier manual guides.
4. Close the GUI, then rerun all tests and formatting validation:

   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Doctor
   powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\format.ps1 -Check
   git diff --check
   git status --short
   ```

   Expected: 30/30 Full tests, formatter passes, no whitespace errors. Review source/docs
   changes only; generated artifacts are ignored. Do not commit/push until your
   acceptance testing is complete. Report the failed test letter, exact action,
   status and copied diagnostics if anything differs. Stage 5 awaits your approval.

## If saving seems to do nothing

1. Confirm the GUI displays the correct **active project** name/path. Pasting a
   folder is not enough: click **Open Project**. Verify Visual Studio is editing a
   file under that same project, not an older calculator copy.
2. Confirm **Auto Build: ON**, then make a real content edit and save. Enabling Auto
   Build establishes a baseline; unchanged-content saves intentionally do not build.
3. Check **Build & System** and the status. **Build Deferred** means the old program
   is still running or its output is locked. Finish or Stop that program; the queued
   build will then run. It will never automatically restart the application.
4. Check that the edited file is a source/header/CMake file outside generated folders.
   Unsaved editor text and ignored build/.vs/.git/.cpplivelab files do not trigger builds.
5. If the watcher reports a filesystem error, correct the reported path/access problem,
   then turn Auto Build on again. Copy the exact diagnostics if it still fails.
