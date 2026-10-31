# Stage 5 UI audit acceptance

Close an older manager before rebuilding. From ordinary PowerShell:

```powershell
Set-Location 'C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab'
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Quick -Jobs 4
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Full -Jobs 4 -Doctor
.\build\debug\cpplivelab-gui.exe
```

1. On Welcome, Tools > Toolchain Diagnostics. Clear Build & System must be below
   the recent list, beside Open Selected Recent, not covering a recent item.
   Recent names must have a normal separator; worker range must read `1-16`.
2. In each theme, inspect Project/Tools/Settings menus after opening animations settle.
   Labels must be readable and use the UI font. Tab/Space/arrow keys should work.
3. New Project > Add Library: inspect source choice and known-library dropdown.
   Use keyboard arrows, then focus another field: selected text stays left-aligned.
   Add fmt and JSON in a disposable project: each gets its own row and Remove button.
4. Project Settings: expand/collapse ten times, resize narrow/tall/short, cover/uncover
   the form. Expect one OK/Cancel pair, no overlap, and complete rounded input borders.
   Click help, then Tab to help and press Space or F1 from its field. Long messages
   must move confirmation actions down and remain readable by scrolling.
5. Browse and cancel: old working directory remains unchanged. Confirm read-only
   previews/lists do not smear text after scrolling, resizing, theme changes or uncovering.
6. In a disposable project, Add Library > GitHub Repository >
   `https://github.com/raysan5/raylib` > Retrieve Versions. The ENTIRE action caption
   must fit. Inspect actual version alternatives, scroll to the bottom and back.
   No overlapping old tag names should remain. Cancel before approving execution;
   existing source/CMake/presets/assets must remain untouched.
7. Inspect long actions such as Approve Verification and Approve & Build only in
   disposable fixtures. They must fit in full; do not approve personal-project edits
   merely to test appearance. Approval/target-selection layouts still need manual acceptance.
8. Tools > Run CppLiveLab Tests: inspect Quick/Full, worker validation and Run/Cancel.
   Run Quick, then cancel a Full run via Tools > Cancel Running Tests while a calculator
   waits for input. Only tests stop; calculator PID/input and Program Console remain.
   Current selections: Quick 27; Full 38. Real output and exit/duration must be shown.
9. Build/Run calculator, enter a name, 2.5 and 3.75. Expect sum 6.25, exit 0. Test
   Clear Program while waiting, End Input, Stop/restart, Debug/Release, targets and
   Auto Build using the existing complete Stage 5 guide.
10. Test maximum, restored and minimum main-window sizes. Repeat all resizable forms.
    If available, move windows between physical 100%, 125% and 150% monitors.
    Do not change global display settings just for this test. Check every tooltip
    near screen edges and keyboard focus after cancelling progress dialogs.
11. Open Preferred Editor and Custom, inspect overrides/Browse/arguments; cancel
    unless intentionally changing preferences. Test the installed editor launch.
    Test FarmGame graphics/assets separately without reorganizing or editing it.

Optional isolated audit host and screenshot reproduction (does not use personal projects):

```powershell
.\build\debug\ui_audit_tests.exe --interactive
.\build\debug\ui_audit_tests.exe .\build\ui-audit-reproduction
.\build\debug\ui_forms_tests.exe .\build\ui-form-reproduction
```

The automated production-form stress run can move its own modal windows/focus while
working; wait for it to finish before typing in another app. Report step, theme,
scaling, expected/actual behavior and relevant output before approving this audit.
No commit/push or Stage 6 work is authorized by these checks.
