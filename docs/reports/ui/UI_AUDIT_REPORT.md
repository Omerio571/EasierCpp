# Stage 5 UI audit — October 29, 2026

## Scope and evidence

**26 UI surfaces/workflows reviewed; 10 defect categories fixed.** No Stage 6,
commit, push, global display/theme change or personal-project modification.
The real Win32 controller was used in a visible audit host with a disposable
calculator, isolated manager state and an isolated registry preference key.
The original manager was closed normally only after confirming Stop was disabled
and status was Build Successful; Visual Studio was left open.

Evidence distinctions below:

- **V:** actual desktop screenshot or native test-window render inspected.
- **A:** automated native geometry, rendering, focus or state assertions.
- **F:** functional regression, not a visual claim.
- **Pending:** not actually exercised visually; acceptance instructions below.

Production modal audit opens the real New/Recent/Project Settings/Editor/Libraries/
Build Preferences/Appearance/Test Runner handlers in Dark, Light and Follow Windows.
It checks visible direct-child rectangles (excluding intentionally separate dropdown
popups), horizontal bounds, keyboard reachability after scrolling, repeated Advanced
toggles, 550/760/624-width dialog sizes and 100/125/150% **logical** DPI layouts.
Workspace sizes include 780x600, 1080x800 and 1600x950, then restoration.
These are resize assertions, not a claim that physical mixed-monitor DPI changes or
every OS maximize/minimize transition were manually tested.

## Audit table

| Surface | States checked | Problem / severity | Root cause and focused fix | Automated evidence | Visual/manual evidence |
|---|---|---|---|---|---|
| 1. Welcome | Startup, recent selection, diagnostics | Clear button overlaps recents; High. Garbled separator; Medium | Reposition Clear beside Open, below list; replace damaged Unicode literal with ASCII | A: new fast `stage5_ui_layout` reproduced controls 122/152 intersecting before fix | V: overlap repair and readable recent entry seen in live rebuilt app |
| 2. Workspace/navigation | Idle/build/running, several sizes, console tabs | No additional overlap found | Existing action routing preserved | A/F: geometry and full native controller regression | V: live Dark/Light workspace and rendered narrow workspace inspected |
| 3. Project menu | Enabled actions, Light/Dark | Menu font smaller than UI; Medium | Popup DC did not inherit control font; select UI font and restore DC | F: existing single-dispatch routing; native renders | V: actual popup inspected before/after font correction |
| 4. Tools menu | No project, disabled editor/cancel, diagnostics | Same menu font issue | Shared menu fix | F: diagnostics and runner regressions | V: settled popup, not just opening animation, inspected |
| 5. Settings menu | Right-edge destination, popup | Same menu font issue | Shared menu fix; no second Settings destination | A/F: existing navigation tests | V: actual popup inspected |
| 6. New Project | Default template, Browse/help, selected library row | Edit corner outlines incomplete; Medium | Parent border clipped by child; native edit now paints its own hollow outline after text | A: corner-pixel and combo-alignment checks; templates/library tests | V: actual dialog and selected fmt row; final rounded renders in both palettes |
| 7. Open/folder workflow | Native folder picker open/cancel | No confirmed defect | Native Windows presentation retained | F: read-only import/project tests | V: actual Browse picker opened and cancelled; no folder changed |
| 8. Recent Projects | Name/path, Open/Remove, missing entry | Welcome encoding issue above; no new modal defect | No duplicate project implementation | A/F: native recent actions preserve files | V: all three theme renders inspected; live Welcome Open succeeded |
| 9. Project Settings Basic | Blank working directory, Browse, help | Border issue above | Shared edit-outline fix | A: production form geometry and help tests | V: actual Light form and picker cancellation inspected |
| 10. Advanced Options | Expanded/collapsed, repeated toggles/resizes | No renewed overlapping/duplicate OK/Cancel found | Existing planned/batched layout preserved | A: repeated real-form toggles, action reachability, GDI counts | V: actual expanded Light form; Dark/Light/Windows renders inspected |
| 11. Preferred Editor | Ordinary and expanded overrides | Border issue above; no additional overlap | Shared outline fix; editor/compiler semantics unchanged | A/F: production form, Custom reveal/persistence regressions | V: normal/expanded renders in all three themes; IDE launch not manually repeated |
| 12. Library Manager | Read-only configuration, add/cancel | Read-only text can ghost; High | CTLCOLORSTATIC misclassified read-only edits as transparent labels; now opaque input background | A: color-mode/background and scroll regression | V: actual manager; final read-only fixture in both palettes |
| 13. Known/Add Library | Source choice, known selection, growing row | Combo text shifted left/center on selection; Medium | Owner draw centered text while normal repaint left-aligned; now consistently left | A: pixel test verifies text in left text area | V: actual nested dialogs and successful fmt row addition without project creation |
| 14. GitHub repository | URL, Retrieve Versions, cancel | Action label truncated; High for approval readability | Fixed 128-pixel action width; now measured from actual label/font | A: Retrieve/Approve action labels fit with padding | V: truncation reproduced in real dialog; corrected sizing asserted |
| 15. GitHub versions | Actual raylib API release/tag list | Overpainted read-only version lines; High | Transparent read-only background; opaque input classification fixes repaint | A: 26-line read-only scroll fixture and opaque background check; offline API cases | V: real defect reproduced; corrected Dark/Light version fixture inspected |
| 16. Dependency approval/target selection | Source and shared form review | Long approval label could truncate; High | Same measured action-width fix | A/F: long-label tests, actual configure/link and rollback regressions retained | Pending: final approval/target dialogs were not manually accepted; no new third-party execution in this desktop audit |
| 17. Test Runner | Quick/Full choices, workers field, modal cancel | Garbled worker range; Medium | Damaged literal replaced with `1-16` | A: actual production form audited; F: owned runner output/cancel regression | V: all three theme renders inspected; complete GUI-launched suite was not replayed by hand here |
| 18. Build Preferences | Debounce, help, resize | Border issue above | Shared native outline fix | A/F: modal geometry and debounce validation | V: all three theme renders inspected |
| 19. Appearance | Dark/Light/Follow Windows, keyboard arrows, apply | Combo alignment issue above | Shared draw alignment | A/F: palettes and retained-console contrast regressions unchanged | V: live keyboard theme switch and Light apply, plus all theme renders |
| 20. Toolchain Diagnostics | Welcome and project summary | Welcome overlap above | Presentation/discovery unchanged except layout | A/F: summary formatting and tool discovery tests | V: real Found/PATH-independent summary displayed; detailed report covered functionally |
| 21. Program Console | Build/Run, stdin, prompts, result | No new defect found | Interactive process path preserved | F: full console/controller/lifecycle suite | V: live Audit/2.5/3.75 produced sum 6.25 and exit 0 |
| 22. Build & System | Build output, diagnostics, inactive badge | No new text contrast defect | Customized palette unchanged | A/F: Rich Edit color and contrast tests retained | V: live compilation/status output, readable blue/green messages in Light |
| 23. Information/help | Small glyph, click, long explanation | Long help clipped; Medium. Literal ampersand missing; Low | Fixed help height replaced by measured wrapped height; static notes use SS_NOPREFIX | A: >200-pixel help, action separation, literal-prefix style assertions | V: actual working-directory explanation inspected; long-message fixture tested. Tooltip placement remains manual |
| 24. Confirmation/warning/error | Validation infrastructure, exact actions | Truncated long action labels and help clipping | Shared measured action/help layouts | A/F: label fit, rollback, stale/duplicate preview and validation checks | Pending: every destructive/configure-write confirmation path was not manually accepted |
| 25. Progress | Actual passive discovery, controlled cancellation | Keyboard focus/default and screen placement risk; High | Activate modal, clamp with shared layout, focus Cancel, default Cancel, restore owner after destruction | A: actual owned task cancelled, owner focus restored, unique reachable action | V: actual passive GitHub progress window showed a focused Cancel; no project edit approved |
| 26. Resize/DPI/resources | Repeated narrow/normal/tall, Advanced, themes | No further bound/action/GDI defect found | Existing guarded layout retained | A: production modal stress at logical 96/120/144 DPI; GDI bounds | Pending: physical mixed-monitor DPI and exhaustive OS maximize/restore remain manual |

## The ten fixes

1. Welcome diagnostic Clear overlaps recent list.
2. Corrupt recent separator and worker-count range.
3. Fixed-height help clips long explanations.
4. Progress modality/focus/default/placement/owner restoration.
5. Popup menus fail to select the established UI font.
6. Combo text alignment changes between selection and repaint.
7. Edit corner outlines clipped by parent/child painting.
8. Long action captions ellipsized by fixed confirmation-button width.
9. Static explanations consume literal ampersands.
10. Read-only multiline fields repaint transparently and ghost old text.

No palette RGB changes. The original Advanced overlap was stressed rather than
rewritten again. Native opening/closing animations can appear translucent in an
immediate capture; settled screenshots were checked before calling that a bug.

## Screenshots actually reviewed

- 37 native captures in `build/ui-audit-captures`, covering all eight production
  forms, expanded editor/settings, welcome/system output, workspace in three themes,
  and controlled progress. All were inspected through ten labelled review sheets
  in `build/ui-audit-review-sheets` (not a claim of 37 manual desktop interactions).
  These first-pass captures retain some defects that were subsequently fixed.
- Final focused renders in `build/ui-audit-rounded`: expanded settings in both
  themes, plus `versions-dark.png` / `versions-light.png`, were inspected individually
  after outline/read-only fixes. Earlier New Project renders were also inspected.
- Computer-use screenshots were actually inspected for live Welcome, menus,
  New/known-library selection, Appearance keyboard switching, workspace/console,
  Project Settings/Advanced/help/Browse, Library Manager, GitHub repository,
  progress and actual version selection. Those tool screenshots are inline session
  evidence, not saved desktop image files in the repository.
- Win32 PrintWindow sometimes includes black nonclient/shadow margins. Such capture
  margins were not falsely counted as application client-area bugs.

## Tests and files

New `tests/ui_audit_tests.cpp` exercises the real controller and modal handlers,
with an optional `--interactive` isolated host and optional screenshot directory.
`stage5_ui_layout` is in Quick; longer `stage5_ui_audit` is in Full. Existing tests
were retained. `ui_forms_tests` adds pixel checks for combo placement and rounded
corners, long help/action checks, and read-only repaint/prefix assertions.
Hidden child controls reject PrintWindow; the corner test sends the supported
native WM_PRINT request directly and still asserts actual themed border pixels.

Changed for this audit: `src/gui.cpp`, `src/project_dialogs.cpp`,
`src/library_workflow.cpp`, `src/ui_theme.cpp`, `tests/ui_forms_tests.cpp`,
`tests/ui_audit_tests.cpp`, `CMakeLists.txt`, `tests/test_selection.cmake`, and
audit/testing documentation. Other existing uncommitted work was preserved.

Final requested commands executed on the normal build:

- Quick: **27/27 passed**, zero failures, **12.30 seconds** CTest real time.
- Full: **38/38 passed**, zero failures, **227.24 seconds**; **Doctor passed**.
- The new exhaustive production-modal audit passed in Full in **34.18 seconds**.
- Formatting check: **57 maintained C++ files verified**, clang-format 19.1.5.
- Git whitespace validation passed; status was inspected without committing.
- Protected palette/scheduler/process/freshness files: **8/8 SHA256 matches**.
- FarmGame: **24/24 original files unchanged, still 24 total files**.

The actual logs are `build/ui-audit-final-quick.log` and
`build/ui-audit-final-full.log`. Earlier targeted failures were investigated and
retested; no existing regression/accessibility test was disabled.
See [UI_AUDIT_MANUAL_TESTS.md](UI_AUDIT_MANUAL_TESTS.md) for focused acceptance and
the earlier full Stage 5 guide for broader project/dependency workflows.

## Remaining limits

This is not an unconditional complete visual pass. Physical multi-monitor DPI,
every tooltip location, actual graphical-game runtime, final dependency approval
paths, editor launching and every maximize/restore permutation were not all
manually exercised. The relevant source and shared layouts were reviewed, and
functional regressions retained, but those are different forms of evidence.
FarmGame was not opened/configured/built/edited during the audit. No source or
configuration changes were approved in a personal project.
