# Stage 5 consolidated manual acceptance

These are acceptance checks for the real desktop UI. Automated off-screen captures
and process tests do not replace your visual, keyboard, mixed-DPI or FarmGame checks.
Nothing in this guide requires approval of Stage 6. Do not commit until satisfied.

## Start

Close CppLiveLab before rebuilding its normal executable. In ordinary PowerShell:

```powershell
Set-Location 'C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab'
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Quick -Jobs 4
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Full -Jobs 4 -Doctor
.\build\debug\cpplivelab-gui.exe
```

Use disposable projects for dependency-edit approval checks. FarmGame can be opened,
built and run with its existing configuration; do not approve dependency modifications
to it during acceptance. Opening a folder does not rewrite its files or presets.

## A–D: Welcome and projects

1. **A — Welcome:** launch without command-line project arguments. Expect New Project,
   Open Existing Project and recent names/paths; no previous project automatically
   opens. Project/Tools are left; the only Settings entry is right. There is no Build menu.
   Use Project > Manage Libraries without a project: expect a helpful prerequisite
   status, not “Error — see output”.
2. **B — Create:** New Project > Browse; select an existing parent outside the manager.
   Enter `Stage5 Test`, choose Empty, leave libraries empty, Create. Expect a NEW folder
   with ordinary C++20/CMake files, followed by the development workspace. Repeat with
   a Unicode name and Multi-file template. An existing destination must be rejected.
3. **C — Open:** Project > Open Existing Project; select a disposable existing CMake
   root. Expect its recognizable name and one path. Build outputs belong to manager
   metadata outside its source. A long path is truncated visually; Project > Copy
   Project Path copies the full path. Existing presets and source organization remain.
4. **D — Recent:** Project > Recent Projects. Select/Open a recent project. Remove an
   entry and verify its directory still exists. Rename a disposable recent folder,
   then try opening it: expect a useful missing-folder explanation. On next startup,
   direct welcome recent selection > Open Selected Recent should also work.

## E–I: Libraries and explicit approval

5. **E — Known:** New Project > Add Library > Known Library > fmt. Add again and select
   JSON. Expect two Added Library rows, individual Remove buttons, and Add Library
   underneath. A duplicate is not offered. Create, Build, Run; expect the generated
   formatting/JSON example to work after real dependency download/build.
6. **F — GitHub:** in a disposable New Project, Add Library > GitHub Repository.
   Enter `https://github.com/raysan5/raylib`, Retrieve Versions. Expect actual release
   and tag alternatives. Choose Latest Stable, Validate Version. Read the exact
   repository/commit approval before selecting Approve Verification. Select the
   actual configured `raylib` target, Verify Link. Expect detailed Build & System
   output and a pinned Added Library row only after the consumer link succeeds.
   Create the project and Build it: probe success alone does not prove project success.
7. **G — Older:** repeat GitHub selection with `5.5`. Expect the exact resolved commit
   `c1ab645ca298a2801097931d1079b10ff7eb9df8`, not a silent substitution with latest.
   Tags/commits may be typed; lists show up to 100 releases and 100 tags. No automatic
   dependency updating occurs. For header-only integration, choose that method,
   supply a real relative include directory and header; expect an actual include compile.
8. **H — Invalid:** use an invented tag such as `does-not-exist-cpplivelab`. Expect a
   version/access error with alternatives retained in the dialog. Test disconnected
   networking separately: it must say network/API failure, not assert the version is
   absent. A rate-limit refusal must identify that possibility and permit retry later.
   A repository without a root CMake library or suitable headers must not claim success.
9. **I — Cancel/safe edits:** cancel before verification approval, during a dependency
   task, and before creating the new project. Existing project files must be unchanged.
   In a disposable existing project, Build first and select its executable target;
   Project > Manage Libraries > select library > Save Preview. Inspect the exact
   proposed CMake addition. Cancel the separate Approve & Build dialog: no source
   change. Approve only when ready to test an actual edit. Expect a manager backup;
   only root CMakeLists.txt receives the displayed block. Failed/cancelled verification
   restores its original bytes. Reapplying a managed block is refused to prevent duplicates.
   Existing vcpkg roots/presets remain configured under Project Settings; this does
   not install packages or create a second vcpkg manager.

## J–O: Dialogs, help, appearance and resize

10. **J — Advanced:** Project > Project Settings. Initially only Basic working directory
    is visible. Expand/collapse Advanced Options at least ten times. Expect preset and
    vcpkg fields only while expanded; entered values survive toggles. Cancel must not save.
11. **K — Artifacts:** after each toggle, check input/Browse boundaries, labels, borders
    and the SINGLE OK/Cancel pair. Cover/uncover the form, minimize/restore the main
    window, and move the form. Expect no ghosts, overlap, clipped actions or stale borders.
12. **L — Help:** hover the small `i`, then Tab to it and press Space. Expect the practical
    explanation and themed tooltip. F1 from a field with help also works. The small glyph
    must remain easy to click without obscuring a label or field.
13. **M — Themes:** Settings > Appearance: test Dark, Light, Follow Windows. Reopen New,
    Recent, Project Settings, Preferred Editor, Add Library and Test Runner. Expect
    shared customized colors, readable console severities, focus and disabled states.
14. **N — Rendering:** inspect rounded buttons/inputs, expanded/collapsed settings and
    multiple library rows. No duplicate Settings gear or competing navigation is present.
    Compare both themes. Operating-system folder/file pickers may use Windows styling.
15. **O — Resize/DPI:** resize forms narrower/wider/shorter and resize the main window.
    Use scrollbar/Tab when necessary. If available, move between 100%, 150% and 200%
    monitors. Expect visible focus, reachable actions, usable dropdowns and no overlaps.
    Settings remains at the right edge; consoles take available workspace space.

## P–T: Diagnostics and integrated testing

16. **P — Diagnostics:** Tools > Toolchain Diagnostics. Expect concise Found/Not installed
    lines and developer-environment availability before technical details. Compatibility
    is explicitly unverified by discovery. Tools > Detailed Toolchain Diagnostics shows
    original copyable paths and terminal availability. Missing terminal PATH is not
    treated as failure if discovery found tools. These actions also work on Welcome.
17. **Q — Quick:** Tools > Run CppLiveLab Tests > Quick, workers 4, Run Tests. No visible
    PowerShell window should open. Expect live actual output, passed/failed counts,
    CTest summary, exit code and elapsed seconds in Build & System. Program Console
    history is preserved. Current expected Quick count: 26.
18. **R — Full:** repeat with Full. Expect all 36 registered tests and Doctor. Tests run
    from CppLiveLab's source checkout using `build/integrated-tests`, independently of
    the active user project. The GUI executable you are running is not overwritten.
19. **S — Workers:** repeat Quick with 1, then 4. Expect the selected worker count in output.
    Enter 0, 17 or text: validation must keep the form open. Launch another session
    while tests run: expect an informative already-running message, no replacement job.
20. **T — Cancel:** start an interactive calculator and leave it waiting for input.
    Run Full Tests, then Tools > Cancel Running Tests. Expect the test tree to stop,
    cancellation/actual exit reporting, responsive GUI and the SAME calculator still
    accepting input. Toolbar Stop belongs to the program/build, not the test session.
    Repeat after a failed test if one occurs; failures must remain visible.

## U–X: Development regressions and FarmGame

21. **U — Console:** Build/Run the calculator demo; enter a name, 2.5 and 3.75, expect
    sum 6.25. Clear Program while waiting: current unfinished prompt/input still works.
    End Input, Run again, Stop, Run again. Run unchanged code: Intelligent Run must
    avoid needless Ninja rebuilding. Save rapidly with Auto Build ON: current build
    finishes, saves coalesce, one follow-up build handles the latest changes. A running
    application is not unexpectedly restarted. Introduce/fix a compile error; output
    must show the real error, then a successful rebuild after correction. Test Debug
    and Release separately and select each executable in a multi-target fixture.
22. **V — Graphical/FarmGame:** Project > Open Existing >
    `C:\Users\omer2\OneDrive\Desktop\Projects\FarmGame`. Select its EXISTING preset if
    required; set only manager-owned working-directory options when assets need it.
    Build, Run, verify its own window/assets, Stop. Do not reorganize source/assets or
    approve dependency rewrites. Confirm the game executable is stopped before rebuild.
    Actual FarmGame graphical/runtime acceptance remains YOUR manual test.
23. **W — Switching:** stop an owned operation, switch between two disposable projects.
    Expect correct identity, targets, working-directory/configuration choices and
    separate managed build directories. Recents removal must not delete either folder.
    Test Runner must continue referring to CppLiveLab, never either selected project.
24. **X — Previous stages/editor:** set VS2022, Open Editor and verify the existing
    CMake root opens. Repeat with installed VS Code/CLion or a custom executable using
    `{project}`; changing editor must not change compiler. Reopen settings/restart and
    verify persistence. From ordinary PowerShell run `--version`, `doctor`, and existing
    CLI build/run commands documented in README/earlier guides. Keep previous stage
    acceptance guides available for their detailed cases.

Record failed step, expected/actual behavior, theme/DPI, exact project/preset/target and
relevant Build & System text. Report bugs before approval; no automatic commit/push.
