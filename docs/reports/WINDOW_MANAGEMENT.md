# Window management implementation — 2026-10-31

## Scope and reuse

Completed the three requested checkpoints using the existing Win32 forms, theme,
project workspace, A/B builds, PCH profiles, process jobs, interactive pipes, graceful
shutdown and monitor/DPI restoration. No dependency, Library Manager, source-project,
system PATH, commit or push changes were made for this task. Earlier uncommitted work
was retained.

Preferences are per project; placement records are additionally isolated by executable
target and configuration. Safe mode remains the default. Legacy restoration ON migrates
to Session, OFF to Off; new settings default to persistent memory. Preference changes
do not immediately restart a program. See [behavior and manual acceptance](../development/WINDOW_MANAGEMENT.md).

## Changed components

- `include/process/application_windows.hpp`, `src/process/application_windows.cpp`:
  preference enums/migration, conservative PE-subsystem readiness detection, bounded
  Unicode placement storage and session memory, minimized-capture identification.
- `include/cpplivelab/session.hpp`, `src/process/session.cpp`: exchange whole heap-owned
  sessions without moving overlapped I/O buffers; optional non-activation startup hint.
- `src/gui/gui.cpp`: Preferences form, independent candidate readiness/retirement,
  retained leases during asynchronous cleanup, manual Restart guard, periodic stable
  placement sampling and launch/Stop/completion integration.
- `tests/process/redirection_tests.cpp`: all six preference combinations, migration,
  persistent/session/Off, project/target/configuration isolation, Unicode, maximization,
  negative coordinates, ignored minimized/destroyed captures, corrupt/oversized data.
- `tests/gui/gui_flow_tests.cpp`, `CMakeLists.txt`: isolated `window_management` workflow
  exercising slow startup, repeated handoff, compiler/launch/startup failures, exclusive
  instances, Safe fallback, readiness timeout, rapid saves, Stop during startup,
  console I/O, preference combinations and manual/persistent placement relaunches.
- `tests/gui/ui_audit_tests.cpp`: Preferences added to existing native-control/theme audit.
- `docs/development/ARCHITECTURE_MAP.md`, `docs/development/WINDOW_MANAGEMENT.md`, this report.

## Executed verification

- Checkpoint 1: `redirection_unit`, 1/1 passed (0.37 s).
- Checkpoint 2: `redirection_unit` + `window_management`, 2/2 passed (25.65 s).
- Initial checkpoint 3 exposed a genuine console-readiness bug: historical GUI placement
  incorrectly required a window from a console replacement. Fixed readiness to depend
  on the current running window/selected executable, not historical records. Retest:
  `redirection_unit`, `redirection_windows`, `window_management`, 3/3 passed (67.70 s).
- Local Full selection: 53/53 passed (287.21 s), including all 35 Quick tests; Doctor
  passed. Network-dependent `stage5_unified_raylib` and `stage5_libraries` were excluded
  according to the existing restriction. Optional live-library tests were not run.
- Final scoped verification after cleanup/Restart hardening and expanded combination
  coverage: **6/6 passed (94.02 s)**, Doctor passed. Selection: `redirection_unit`,
  `redirection_windows`, `window_management`, `reliability_gui_flow`, `stage5_ui_audit`,
  `stage5_pch`. Log: `build/window-management-logs/final-regressions.log`.
- Changed maintained C++ passed discovered clang-format `--dry-run --Werror`;
  `git diff --check` passed (only existing line-ending normalization notices).

The first Full run precedes the final localized cleanup/Restart hardening; the final
scoped run rebuilds and tests the final executable. Full logs and the exact local Full
selection are retained under `build/window-management-logs/`.

Only one display (1920×1080) was available. Existing synthetic disconnected/negative
monitor geometry checks passed. Physical multi-monitor, mixed DPI, fullscreen/Snap
Groups and real-game acceptance remain manual. No separate performance benchmark was
performed; the new readiness interval is deliberate overlap, not a compilation-speed
claim. See the guide for readiness limits and exclusive-instance compatibility.

Updated executable: `build/window-management/cpplivelab-gui.exe`.
