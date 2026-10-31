# CppLiveLab UI design standards

These are permanent standards for every future UI change, including later stages.
Read this file together with AGENTS.md and CODING_STYLE.md before editing UI code.

## Priorities

No duplicates. Simplicity. Efficiency. Consistency. Preserve approved behavior.
Adapt illustrations to the actual native Win32 application; do not copy mockups
rigidly or replace the product with a different framework.

## Main workspace and navigation

- Keep frequent actions visible: active project, Build/Run/Stop/Open Editor,
  configuration/target, Auto Build, status, console tabs and program input.
- Give remaining space to the consoles. Resizing primarily changes their area.
- Use one themed navigation row: Project and Tools on the left, Settings aligned
  to the right edge. Its native buttons
  occupy a roughly 48-logical-pixel band, scaled with DPI.
- Keep exactly one Settings entry. Never add a competing title-bar gear.
- Project owns New/Open/Recent/Project Settings/Manage Libraries/Copy Project Path.
- Build/Run/Stop stay in the workspace; do not restore a redundant Build menu.
- Tools owns editor launch, summary/detailed diagnostics and Run CppLiveLab Tests.
  Stream real tool/test output into Build & System without clearing Program Console.
- Settings owns Appearance, Preferred Editor and global Build Preferences.
- Never restore permanent folder, appearance or debounce fields to the workspace.
- Show the project path once, with path ellipsis and Copy Project Path available.
- Retired control IDs remain reserved. Do not reuse them for unrelated actions.

## Shared appearance

- Use `UiTheme`, `theme_palette` and `ProjectFormWindow`; do not create independent
  dialog palettes or hardcoded copies of user-customized colors.
- Support Dark, Light and Follow Windows, including retained console recoloring.
- Preserve semantic identity: red errors, green success, blue information, orange
  warnings. Ordinary stdout is neutral. Keep textual severity labels.
- Normal console text must meet at least 4.5:1 contrast, using unrounded linear-sRGB
  relative luminance. Do not lower accessibility checks to accommodate a new color.
- Use Segoe UI for interface text and the established console font for output.
- Use consistent 16/20-pixel margins, 8/12-pixel gaps, roughly 32–36-pixel buttons,
  readable labels and the shared rounded-control treatment. Values are logical pixels.
- Preserve hover, focus, selected and disabled states. Keep native keyboard behavior.
- OS-owned folder/file selection dialogs retain their native Windows presentation.

## Forms and interactions

- New application forms go through the shared native form helper. It owns fonts,
  brushes through UiTheme, control creation, DPI layout, scrolling and modal cleanup.
- Use short labels and friendly display names; preserve stable underlying identifiers.
- Use native Browse for folders and executables; allow typed paths too.
- Put infrequent overrides under Advanced Options. Custom editor selection reveals
  necessary advanced fields automatically. Never discard hidden override values.
- Add information buttons/tooltips only for unfamiliar settings. Help must work via
  keyboard (Tab/Space or F1), with practical examples and accessible names.
- Provide inline validation/errors without closing the form or losing entered values.
- Restore focus after modal windows. Scroll keyboard-focused fields into view when
  a form exceeds the available monitor area. Do not force-scroll on every mouse event.
- Recent projects show name and path with direct Open/Remove actions. Forgetting a
  recent entry must never remove files. Missing locations need actionable diagnostics.
- Library selections are a growing list: one selected-library row and Remove action
  per entry, followed by Add Library. Exclude duplicates and unsupported integrations.
- Explain each catalog entry's purpose, download requirement and actual support.
  Selection/configuration/download/build success are different statuses.
- Existing-project library previews are labelled previews. Do not imply installation
  or silently change project-owned CMake/source/assets. Require authorization for writes.

## Architecture and reliability

- Keep one source of truth for preferences, editor settings, active project, target,
  build configuration and Auto Build state. Dialogs edit values; controllers persist them.
- Reuse existing action handlers. Popup menus return one command and suppress duplicate
  WM_COMMAND notification. Toolbar shortcuts and menus must not execute an action twice.
- Keep scheduler, watcher, build freshness and process ownership separate from styling.
  Do not introduce a new scheduler or editor launcher for a new UI entry point.
- Pause inappropriate automatic dispatch while editing project identity/settings;
  continue draining an already-owned operation when a global settings form is open.
- Follow Allman braces, four-space indentation and resource ownership rules. Destroy
  controls before their fonts/theme resources and clear owned accessibility annotations.
- No heavy rendering framework or unrelated dependencies for interface work.

## Checklist for every future UI feature

1. Read these guidelines and inspect existing helpers/actions first.
2. Place the feature in the appropriate existing menu or workspace section.
3. Check for duplicate controls, state, logic and settings destinations.
4. Reuse the current theme automatically; preserve customized palettes.
5. Add Browse/help/validation where needed and test keyboard access.
6. Check long names/paths, small/maximized/restored windows, DPI and both themes.
7. Add meaningful native-control/regression tests without weakening existing coverage.
8. Run Quick, Full, formatting and Git validation. Protect real projects with hashes.
9. Provide a complete manual acceptance guide. Do not claim visual or mixed-DPI
   verification unless it was actually performed; state what remains manual.

These standards do not authorize implementing future stages or modifying user projects.

## Welcome, dependency approval and owned tools

- No project is a normal state. Show New/Open/recognizable recents at startup;
  never implicitly reopen a project. Use informative prerequisite messages.
- Advanced layouts are planned before moving controls. Suppress intermediate redraw,
  reset rounded regions, and erase/invalidate parent borders after positioning.
  Handle resize/DPI/scroll changes without recursive layout; test repeated toggles.
- Help uses a small glyph inside a keyboard-accessible hit area and themed tooltip.
- Passive GitHub discovery may read API/archive metadata, never execute CMake.
  Show the repository and exact pinned commit before approval to execute dependency
  code. Discover actual CMake targets; label configuration, link probe and real
  project build separately. Never promise arbitrary repository compatibility.
- Existing-project dependency changes require an exact preview and separate approval.
  Preserve source/assets/presets, keep a backup and restore CMake on failed/cancelled
  verification. Refuse stale previews and duplicate managed blocks.
- Tests use existing scripts/CTest, source-checkout discovery and a separate build
  directory. Test cancellation owns only its test tree, never the user program.
  Prevent overlapping sessions and recursive suite execution. Report actual exit,
  output and elapsed time; never infer success from a download or process launch.
- Diagnostics show discovery summaries before optional original details. Found,
  terminal PATH, developer setup and successful compilation are separate evidence.

## Rendering regressions to retain

Measure meaningful action captions instead of clipping approval labels. Keep combo
text alignment identical during selection and repaint. Read-only Edit controls still
need opaque input painting under WM_CTLCOLORSTATIC; static explanations preserve
literal ampersands. Paint native edit outlines inside the child, preserving text,
selection and scrollbars. Progress modals must have a visible keyboard default,
monitor-safe placement, and restored owner focus after destruction. Retain pixel,
long-text, scroll/repaint and production-modal regression tests for these fixes.

## Current product direction: Cpp

Use Cpp as the displayed name and Make Your Life Easier as the tagline. Preserve
internal identifiers and customized palettes. Keep Add Library and Manage Libraries
prominent in the workspace beside Build/Run/Stop/program Restart and Auto Run.
Manager restart is a separate Tools action; Settings remains a single destination.
Auto Run uses the existing watcher/scheduler and CMake pipeline with isolated
replacement outputs. Never stop the old application before successful compilation.
Window placement restoration does not preserve application memory or Snap Groups.

