# Stage 4 reliability retest — manual handoff

These steps test the current reliability fixes. Use a new calculator so your existing
Stage4Acceptance and CalculatorDemo stay unchanged. Visual inspection is intentionally
left to you; the new GUI-controller test runs in a hidden test-owned window.

Still open from the earlier request: structured/friendly diagnostic summaries, a
Simple/Detailed log toggle. The final refinements now improve Light/Dark palettes
and preserve unfinished prompts; see STAGE4_FINAL_REFINEMENTS.md. Raw
compiler messages remain available in Build & System. Do not treat this automated
retest as completion/approval of those unfinished presentation requests.

## Setup

1. Close CppLiveLab, open ordinary PowerShell, and run:

   ```powershell
   Set-Location 'C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab'
   powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Doctor
   $demo = Join-Path (Split-Path (Get-Location)) ('CppLiveLab-UserProjects\Stage4Retest-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
   .\build\debug\cpplivelab.exe create $demo --demo calculator
   $demo
   .\build\debug\cpplivelab-gui.exe
   ```

   Expect 30/30 Full automated tests. Keep this PowerShell window open: `$demo` belongs
   to this session. In another window, set `$demo` to the printed absolute folder.

2. Paste the printed folder into **Project folder**, click **Open Project**, choose
   **Debug**, and leave Executable as `app.exe`. Click **Edit in Visual Studio 2022**.
   Expect the active name/path and VS folder to refer to this new test calculator.

## A — Intelligent Run and restarting input

1. Set Auto Build OFF and click **Build**. Expect Build Successful and a unique
   `Build #<manager PID>-<number>` in Build & System.
2. Click **Run**. Expect `Build is up to date - starting application.` with no new
   build ID. Enter `Ada`, `2.5`, and `3.75` as separate lines using Enter/Send.
   Expect `Hello, Ada! Sum: 6.25`.
3. Run again without edits. Expect no additional build. At the name prompt, click
   **End Input**. The calculator exits with code 1 because its template treats EOF
   before a name as an unsuccessful input; this is not a compiler failure.
4. Run again. Expect Send/End Input enabled and working. Click **Stop**, then verify
   Run becomes available immediately, without having to Build or reopen the project.
5. With the program stopped and Auto Build OFF, change the return expression in
   `src/calculator.cpp` from `left + right` to `left * right` and save. Click Run.
   Expect a build before launch; Ada/2.5/3.75 should now produce `Sum: 9.375`.
   Restore addition and save/build when done.

## B — Automatic failure and recovery

1. Finish or Stop the program, then enable Auto Build. Remove the semicolon from
   `return left + right;` in calculator.cpp and save.
2. Expect **Build Failed**, not Deferred. Build & System should contain the original
   MSVC diagnostic with calculator.cpp, its line number and diagnostic code/message.
   The friendly summary/toggle requested earlier is still unfinished.
3. Click Run while the source is still invalid. Expect another failed build and no
   silent launch of the old executable. Restore the semicolon, save, and expect
   automatic recovery. Run explicitly; saving never automatically starts a program.

## C — Rapid saves and pending Run

1. With Auto Build ON, make several harmless comment edits/saves quickly.
2. Save another edit while a build is running. Click Run during that build if time
   permits. Expect one active build, at most one coalesced follow-up for the latest
   edits, and Run waiting rather than starting an overlapping build.
3. Once edits stop, expect a terminal result and usable controls. Build IDs and worker
   PID/exit messages provide a trace. Saving unchanged content or notes.txt must not
   start another build. Headers live in `src/calculator.hpp` in this template.

## D — Cancellation without reopening

1. Turn Auto Build OFF. In this new test project's CMakeLists.txt only, temporarily add:

   ```cmake
   add_custom_target(manual_wait ALL
       COMMAND "${CMAKE_COMMAND}" -E sleep 8
       VERBATIM)
   ```

2. Save, click Build, and click Stop while it is Building. Expect **Build Cancelled**
   and a warning containing `Build cancelled by user`.
3. Cancellation discards that operation's pending automatic/Run requests. Source
   edits remain on disk. Resume with a new save or an explicit Build/Run.
4. Remove the temporary manual_wait block, save, and click Build. Expect success
   without reopening. Run should also be available. Do not expect transactional
   executable restoration if a linker itself was interrupted; rebuild before Run.

## E — Clear display history while input is active

1. Run and leave the calculator at the name prompt. Click Clear Program.
2. Expect completed history to clear and the observed unfinished name prompt to
   remain. The status says `Program running - input remains available (earlier output cleared)`;
   Send stays enabled. No formal prompt text is guessed or invented.
3. Send Ada/2.5/3.75. Expect the usual result, proving the application was not reset.
4. Clear Build & System. Expect Program Console to retain its own output.

## F — Theme and window inspection

1. Select Light, Dark, and Follow Windows. Check semantic colors and labels side by
   side after a success, error, and cancellation. Expect red errors, green success,
   blue system information and orange warnings, with neutral ordinary stdout.
2. Resize repeatedly, minimize/restore, maximize/restore, then return to a small
   supported size. Expect controls and rounded corners to remain complete, with no
   overlap or clipped input area. Report any visual artifacts and display scaling.
3. If available, move between monitors with different scaling. Real mixed-DPI and
   Windows color-mode changes still require your visual/manual verification.

## G — Debug and Release locations

1. Stop the program. Select Debug, Build, and Run the calculator; finish its input.
2. Select Release, Build, and Run again. Expect the same correct program behavior.
3. In the original PowerShell window run:

   ```powershell
   Get-Item "$demo\.cpplivelab\build-Debug\app.exe"
   Get-Item "$demo\.cpplivelab\build-Release\app.exe"
   .\build\debug\cpplivelab.exe run $demo --config Release
   ```

   Expect both files at those separate paths and interactive CLI execution. A Debug
   success alone does not make Release current. The hidden-controller test now builds
   and executes both configurations independently, including those exact locations.

## H — Running program and deferred save

1. Restore addition, build, enable Auto Build, and Run. Leave it at a prompt.
2. Save multiplication in calculator.cpp. Expect Build Deferred and the current
   process to remain alive. Complete it with Ada/2.5/3.75: it should still print 6.25.
3. After exit, expect the pending build to finish. Run explicitly: now expect 9.375.
4. Repeat with another saved edit, but Stop instead of completing input. Expect the
   pending build to proceed after the owned program exits. Restore addition afterward.

## I — Final automated checks

Close the manager and run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Doctor
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\format.ps1 -Check
git diff --check
git status --short
```

Expect 30/30 Full tests, formatting success, and no whitespace errors. Review changes before
any commit or push; none is automatic. Report the test letter, exact action, status,
configuration/path and copied diagnostics for any mismatch. Stage 5 has not started.
