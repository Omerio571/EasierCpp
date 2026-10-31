# EasierCPP V1 — proposed implementation order

This plan does not authorize later phases. Preserve Windows 11, C++20, native
Win32/Rich Edit/shared forms, standard CMake, VS2022 compatibility, internal names,
state paths and project layouts. Do not add Qt/frameworks or a competing build system.

The five requested workstreams map to project Phases 2–6. Existing Auto Run and
restoration have working bounded paths; complete and qualify them rather than
reimplementing from scratch.

## Phase 2 / 1. Interface and branding

**Reuse:** Application actions, UiTheme, ProjectFormWindow, ConsoleView, customized
palettes, ProjectOptions and File API discovery.

**Work:** Centralize displayed EasierCPP/Make Your Life Easier text. Present public
V1 separately from the current internal 0.1.0; do not silently announce 1.0.0.
Preserve cpplivelab executable/repository/namespace/registry/storage names and CMake
ownership markers. Explain no-project/unconfigured/configuring/configure-failed/
no-executable/choose-target/stale-selection/metadata-error states. Correct the proven
failed-compilation refresh using validated current metadata, not stale data from an
unsuccessful reconfiguration. Do not run unapproved CMake automatically on Open.
Extract only the small product-text/target-state helpers needed.

**Proposed files:** `src/gui.cpp`, `main.cpp`, `doctor.cpp`, current README/docs;
small product-info/target-state helpers if useful; `workspace.hpp/cpp` and
`scripts/file_api.cmake` only for factual current-generation state. Reuse
`project_dialogs.*`, `ui_theme.*`, `appearance.*`. Extend `gui_flow_tests.cpp`,
`ui_audit_tests.cpp`, `cli.cmake`, focused target-state tests and registrations.

**Risks:** duplicate widget/model state, stale configuration, implicit project
writes, palette regression, renamed paths breaking saved data or scripts.

**Tests:** fresh/imported/library-only/multiple-target/compile-failed/configure-failed/
corrupt/stale-choice states; Debug/Release/preset switches; native resize/keyboard/
both themes/contrast, welcome/editor/output regressions. Preserve all registrations.

**Depends on:** Phase 1 parser fix/evidence. No dependency/lifecycle redesign needed.

## Phase 3 / 2. Complete Library Manager

**Reuse:** LibraryRecord schemas 1–3, transaction/cache backend, manager/workflow
forms, pinned GitHub verification, managed-build CLI.

**Work:** Clearly distinguish catalog, managed, manual, shared and transitive
evidence; requested versus resolved versions/status; supported Add/Edit/Update/
Remove/Repair paths and runtime DLL requirements. Preserve exact previews, separate
approval, backups, rollback and pins. Do not adopt arbitrary CMake as owned data or
claim universal compatibility. Keep one dependency operation backend.

**Files:** `libraries.hpp/cpp`, `library_manager.*`, `library_workflow.*`,
`workspace_creation.cpp`, existing dependency tests/current docs.

**Risks:** manual CMake adoption, duplicated linkage, cross-project pin changes,
stale previews, shared cache deletion and version/API incompatibility.

**Tests:** offline fixtures, missing paths/packages, multiple consumers, duplicates,
manual/transitive cases, update/repair/rollback and interrupted recovery. Obtain exact
execution approval before live Raylib/SFML/SDL3/GLFW/fmt/JSON tests; real-project edits
require separate approval.

**Depends on:** Phase 2 project/target/profile clarity and existing CMake execution.

## Phase 4 / 3. Auto Run reliability

**Reuse:** ProjectWatcher, AutoBuildScheduler, BuildFreshness, alternate slots,
ConsoleSession/jobs, existing CLI and GUI/native regression fixtures.

**Work:** Extract one small lifecycle controller from GUI rendering. Snapshot each
operation identity. Retain old application until current replacement succeeds,
coalesce saves and report exit/crash during compilation truthfully. Make cancellation,
activation and output-draining states consistent. Preserve Stop/no-loop policy and
latest-successful Restart. Measure VS Run reuse/large scans before optimizing.

**Files:** `gui.cpp`, `auto_build.*`, `build_freshness.*`, `session.*`, workspace
execution/lease helpers; new small controller only where needed; lifecycle tests.

**Risks:** overlapping workers, stale launch, mutable selection during work,
activation failure, orphan helpers, EOF/timeout handling and UI-thread stalls.

**Tests:** exit/crash while build succeeds/fails, save during shutdown, rapid/identical
saves, cancellation per stage, failed-build PID retention, Debug/Release/targets,
children retaining pipes, uncooperative timeout with force OFF/ON, console input,
idle/Stop/close behavior.

**Depends on:** Phase 2 identity/state boundaries. Phase 3 operations must use the
same locks and cannot race builds/restarts.

## Phase 5 / 4. Windows window restoration

**Reuse:** application_windows capture/close/restore, job PIDs, native fixture and
timer-driven delayed restoration/geometry tests.

**Work:** Separate window APIs/restoration coordination from settings/widgets. Define
physical versus logical rectangle/DPI policy. Validate monitor identity, negative
coordinates, taskbar/frame offsets and disconnected fallback. Qualify multiple-window
matching, readiness and recreation. Keep borderless/exclusive fullscreen and Snap
Groups best-effort/cooperative; never imply memory restoration or move unrelated apps.

**Files:** `application_windows.*`, lifecycle/controller integration, preference
helpers and native GUI/restoration tests/manual guide.

**Risks:** DPI virtualization, display changes, ambiguous identity, HWND reuse,
late windows, application-enforced bounds and exclusive graphics modes.

**Tests:** normal/custom/maximized/minimized, delayed/multiple/recreated windows;
primary/secondary/upper vertical monitors, left/right half, mixed DPI and display
disconnection/rearrangement. Hardware/fullscreen acceptance remains explicitly manual
where it cannot be simulated honestly.

**Depends on:** Phase 4 capture/shutdown/launch boundaries. No new graphics framework.

## Phase 6 / 5. Regression, polish and V1 acceptance

**Reuse:** CTest/PowerShell runner and labels, native controller/UI captures, theme
accessibility and source-preservation fixtures.

**Work:** Run focused then permitted complete regressions; record approved live
dependency and hardware tests. Measure responsiveness with representative real
projects, verify editor/build compatibility and consolidate current docs. Review
working tree with the user; commit/push only if separately authorized.

**Risks:** skipped-as-passed claims, outdated docs, hidden source-tree packaging
assumptions, unsafe cleanup and premature production-readiness claims.

**Tests:** all local and approved live suites; real-project source/assets/preset
hashes; repeated build/run/restart, unrelated-process safety, persistence/reopen,
packaged launch without checkout where supported, Debug/Release, discovery failure
and non-destructive recovery.

**Depends on:** prior workstreams, explicit dependency/project-write approval and
manual acceptance. Each report must state exactly what ran and what remains untested.

## Recommended immediate approval boundary

Review the Phase 1 audit, then approve **Phase 2 only**: public branding, native
interface clarity and the demonstrated target selector state/refresh problem.
Keep broader library/lifecycle/window work in separate approved increments.
