# Stage 5 UI refinement report

Stage 5 still awaits the user's manual acceptance. The mockup guided the arrangement;
the implementation keeps the existing native Win32 controls, customized palettes,
console behavior and build/process architecture. No Stage 6/7 work or automatic commit.

## Workspace and navigation

The main window now has one themed Project / Build / Tools / Settings navigation row.
There is exactly one Settings entry and no competing title-bar gear. Native popup
menus use themed owner drawing and native arrow/Enter/Escape behavior; Alt+P/B/T/S
opens sections, and F10 focuses navigation. Tab/Space still work on native buttons.

Permanent folder/Open/Browse, appearance and debounce controls were removed from the
workspace; their IDs remain reserved. Project > Open Existing owns folder selection.
Settings owns Appearance, Preferred Editor and Build Preferences. Project Settings
and Manage Libraries are separate project-specific destinations.

The active project name and one ellipsized path remain visible, with Copy Project Path
under Project for full-path access. Build/Run/Stop/Open Editor, configuration/target,
Auto Build, status and console/input controls remain immediately accessible. A compact
target selector is retained for discoverability and safe multiple-target selection.
The console area expands with the window; the supported minimum is 780×600 logical pixels.

Build/Run/Stop menu items route to the existing toolbar command IDs. The popup uses
RETURNCMD/NONOTIFY to dispatch once. Tools diagnostics uses Build & System output,
not another diagnostic window. No second scheduler, editor launcher or preference store.

## Shared dialogs and interactions

`ProjectFormWindow` is the common native form implementation. New Project, Recent
Projects, Preferred Editor, Project Settings, Appearance, Build Preferences, Add Library,
Manage Libraries and configure-write permission use it. Windows file/folder pickers
remain OS-owned. No heavy interface library was added.

Shared behavior includes:

- The current UiTheme/theme_palette, custom semantic colors and DWM title-bar hint.
- Segoe UI, rounded buttons/inputs, shared panel/border painting and dropdown styling.
- DPI scaling, monitor-height constraints, native vertical scrolling and keyboard
  focus scrolling for forms taller than the available work area.
- Native Browse for parent/working/vcpkg directories and editor executables.
- Information buttons, hover tooltips, F1 field help and Tab/Space access. Information
  buttons receive descriptive Windows accessibility names when services are available.
- Inline validation/errors without closing the form, focus restoration and native
  default/cancel actions. Configure-write permission defaults to Cancel.
- Collapsible Advanced Options; selecting Custom editor reveals required overrides.
  Hidden fields retain their values. Project Settings puts working directory first,
  with presets/vcpkg under Advanced Options.

Recent Projects now has a compact two-line name/path list with direct Open and Remove
from Recent actions. Missing entries show a useful error and remain removable. Removal
only updates metadata; it never deletes the project. Project creation explains the
new-folder location, adds Browse and uses friendly template names while retaining
`empty`/`multi` internally.

Moving debounce uncovered an old widget read in watcher startup. It now reads the
existing shared preference directly, so switching projects does not reset a saved
value to 400 ms. A native controller regression verifies 675 ms survives restart of
the watcher. Scheduler/watcher/process modules themselves were not changed.

## Multiple libraries and honest status

New Project has a growing selection section. Each chosen integration gets its own
Added Library row and Remove button; Add Library follows the rows. Duplicate and
unsupported choices are excluded. Add is disabled when every current catalog entry
is selected, and becomes usable after removing a row. No-library creation remains valid.

The two validated optional integrations are:

| Integration | Purpose | Pinned version | Download |
|---|---|---|---|
| fmt | C++ text formatting | 11.0.2 | Required |
| JSON for Modern C++ | Read/write JSON data | 3.11.3 | Required |

They are dependencies of newly generated user projects, not the manager. Both use
checksummed upstream release archives. JSON archive SHA-256 was measured as
`0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406`.
A real project selecting BOTH was configured, compiled, linked and executed, verifying
fmt output containing JSON with answer 42. The existing fmt-only case remains tested.
CLI accepts `--library fmt,nlohmann_json`; the original single-library forms still work.

One catalog supplies names/descriptions and one dependency_cmake generator supplies
both new-project configuration and integration previews. Context7 was consulted for
Win32 references and the upstream [JSON CMake integration](https://github.com/nlohmann/json/blob/v3.11.3/docs/mkdocs/docs/integration/cmake.md).

Manage Libraries shows declarative evidence and configured CMake targets when available.
Its Add workflow can select multiple integrations, then **Save Preview** writes a
manager-owned CMake fragment outside the project. This is explicitly a preview, not
installation or automatic editing. Applying/removing configuration from an existing
project remains a separate authorized user edit. No FarmGame files are rewritten.
Raylib/RayMath in the illustration are row examples, not newly advertised installers.

## Permanent standards and files

`UI_DESIGN_GUIDELINES.md` exists and covers layout, styling, interactions, accessibility,
shared state/actions, resource ownership and the checklist for every future stage.
`AGENTS.md` explicitly requires reading/following it before all future UI work.

Files for this refinement (earlier uncommitted work is preserved):

- `src/gui.cpp`: reorganized workspace/navigation and dialogs; shared preference routing.
- `src/project_dialogs.hpp/.cpp`: reusable themed native form/control/layout helpers.
- `src/ui_theme.cpp`: themed native popup-menu rendering, reusing the existing palette.
- `include/cpplivelab/workspace.hpp`, `src/workspace_creation.cpp`: curated multi-library
  selection and shared pinned dependency configuration/preview generation.
- `src/main.cpp`: additive CLI help for comma-separated libraries.
- New `tests/ui_forms_tests.cpp`, `tests/ui_capture.hpp`; extended GUI controller and
  Stage 5 library tests. CMake/test-selection registration includes the new fast test.
- New design guide, this report and `STAGE5_UI_MANUAL_TESTS.md`; updated AGENTS, README,
  TESTING, TEST_RESULTS and historical Stage 5 handoff links.

## Verification and limits

The complete current manual guide is STAGE5_UI_MANUAL_TESTS.md. It includes all original
Stage 5 scenarios plus the UI changes, expected results and exact PowerShell commands.
Automated final counts are Quick 25 and Full 35; actual final timings/results are in
TEST_RESULTS.md. Full retains all previous Stage 1–5 tests.

Final requested commands were executed after compilation:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Quick -Jobs 4
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Full -Jobs 4 -Doctor
```

**Quick: 25/25 passed in 3.26 seconds. Full: 35/35 passed in 110.98 seconds. Doctor
passed.** Durations are CTest real time; manager setup/build time is additional.
clang-format 19.1.5 validated 51 maintained files, and Git whitespace checks passed.
Eight protected palette/scheduler/watcher/process/freshness files matched their
before-refinement hashes; all 24 FarmGame file hashes and file count were unchanged.
No files were staged, committed or pushed.

Targeted native tests cover form palettes, dynamic rows, duplicate refusal, Browse/help
presence/actions, editor advanced visibility, inline validation, recent Open/Remove,
high-DPI scrolling, routing, resize and persistence. Real multi-library compilation
and all previous program/console/build lifecycle checks remain. Off-screen test-owned
Dark/Light workspace and New Project snapshots were rendered for visual inspection;
they do not capture or control the user's desktop.
Final regenerated Dark/Light workspace and New Project snapshots were inspected.
They are local ignored QA artifacts under `build/ui-snapshots`; real monitor, picker,
screen-reader and editor acceptance is not claimed by those snapshots.

Visual inspection found a target-label/toolbar overlap and missing shared form input
border painting; both were corrected. Compile integration errors in the optional
snapshot/accessibility support were fixed before testing. Real mixed-monitor behavior,
screen-reader operation, native picker interaction and external-editor UIs remain
manual. OS title-bar/popup chrome may vary. Unsupported graphical templates, arbitrary
Git integration, automatic imported-library rewriting/removal and unverified vcpkg
acquisition remain outside the implemented scope.

FarmGame is protected by before/after file hashes. It was not configured, built, run,
edited or reorganized during this refinement. Manual acceptance of the original
FarmGame remains separate and requires explicit authorization for its own CMake writes.
