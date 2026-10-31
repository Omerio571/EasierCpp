# Auto Run, native window restoration and library compatibility

This maintenance task preserves the current Win32 interface, branding, palettes,
CLI, C++20/CMake architecture and VS2022 workflow. No new dependencies, permanent
environment changes, commits or pushes were introduced. Real user projects were not
used as writeable test fixtures.

## Implementation

Auto Build and Auto Run now have separate adjacent controls. They reuse one existing
watcher/debouncer/scheduler. Auto Build retains safe build deferral while an application
holds the current output. Auto Run can compile into the existing alternate manager build
slot while that application remains alive. Only a successful, current replacement is
eligible for restart; errors retain the old process. OFF/cancellation is checked again
before relaunch. Rapid saves retain one follow-up request instead of starting duplicate
builds. Manual Restart continues to reuse a successful artifact.

The existing owned-process job lifecycle, graceful shutdown timeout/force-stop policy,
build leases, separate build slots and runtime environment are reused. Project options
now persist argument vectors as optional trailing data, preserving older four-field
settings. GUI and CLI launches pass these arguments through the existing argument
quoting code, along with the selected working directory and runtime environment.

Window capture/restoration is restricted to visible top-level windows of managed job
processes. Existing delayed-window retries, class/title matching and restored-handle
tracking remain. Capture/restore calls now use a scoped per-monitor-aware physical
coordinate context; this does not change the application's global DPI mode. Borderless
fullscreen bounds use the full monitor instead of the work area. Monitor matching,
visible fallback, maximization and minimized-to-visible behavior are retained. Once a
window is restored successfully, subsequent polls do not override a user's move.

Library assessment distinguishes Compatible, Incompatible — Not a library, Unable to
Verify, and non-C++ packages requiring a compatibility decision. Bounded read-only
inspection looks for reusable native interfaces/metadata; it does not execute code or
establish compatibility. Empty/OS-only headers and app-private targets without an
exposed interface are insufficient. Umbrella headers can export local headers within
the source root. GitHub registration requires an actual selected reusable target/link
probe or explicitly compiled public header. Local integrations retain preview approval
and consumer verification; new source/package targets are checked for CMake library
types. Exported executables cannot pass that check. No permanent non-C++ language
policy was introduced.

Older exact owned dependency snippets remain recognized for approved edits/removals;
arbitrary user modifications are still refused. A failed change rolls back the previous
configuration/metadata and does not delete already configured libraries. Local passive
scanning uses the existing background-work dialog.

## Repairs found during the work

- Restored Auto Build rather than letting Auto Run replace its control/setting.
- Removed the implicit global Auto Build -> per-project Auto Run preference fallback.
- A compiler failure after successful configuration now refreshes discovered targets;
  it no longer unnecessarily leaves the selector empty/unconfigured.
- Replacement failure text distinguishes a retained running process from one that has
  independently exited.
- Watcher failures synchronize the two preferences, controls and scheduler.
- Disabling Auto Run during automatic shutdown cancels its pending relaunch.
- New local/package snippets reject application targets, while recognizing older
  unchanged owned snippets for compatibility.

## Files changed for this task

- `src/gui.cpp`: independent controls, scheduler decisions, target/error handling,
  argument form and program launch.
- `include/cpplivelab/workspace.hpp`, `src/workspace.cpp`: backwards-compatible argument
  persistence and CLI launch.
- `src/application_windows.hpp`, `src/application_windows.cpp`: scoped physical
  coordinates and borderless bounds.
- `src/library_compatibility.hpp`, `src/library_compatibility.cpp`: new passive/verified
  assessment helper.
- `src/library_workflow.cpp`, `src/library_manager.cpp`: compatibility feedback,
  registration gates and background assessment.
- `include/cpplivelab/libraries.hpp`, `src/libraries.cpp`: target-type guards and legacy
  snippet recognition.
- `tests/library_compatibility_tests.cpp`, `tests/gui_flow_tests.cpp`,
  `tests/redirection_tests.cpp`, `tests/test_selection.cmake`, `CMakeLists.txt`: focused
  compatibility and native GUI regressions and test registration.
- This report and `AUTO_RUN_RELIABILITY_MANUAL_TESTS.md`.

The repository already contained substantial modified/untracked Stage 4/5 work; this
list does not attribute that earlier work to this maintenance task.

## Verification

Execution results (Windows, installed VS2022/MSVC, Debug manager build):

- Formatting applied and checked with clang-format 19.1.5: 83 maintained files passed.
- Final Quick: **35/35 passed**, 5.74 seconds; `build/reliability-work/quick-verified.log`.
- Final filtered Full local selection: **51/51 passed**, 180.35 seconds;
  `build/reliability-work/local-regressions-verified.log`. This includes every Quick
  regression and the permitted local integration tests. Build script exit code: 0.
- Focused legacy migration/rollback and real link-probe retest: **2/2 passed**, 53.14
  seconds; `build/reliability-work/focused-retest.log`.
- Environment doctor passed: installed CMake, Ninja, MSVC and Clang discovered;
  missing GCC does not block the MSVC toolchain. `doctor_without_path` also passed.
- `git diff --check` passed; generated build executables/logs remain ignored.

One newly added fixture initially failed because text-mode output converted exact
legacy managed bytes to CRLF. The fixture now writes binary bytes. The application's
ownership protection was retained, and the corrected migration test passed. That
intermediate run finished 50/51; it is not presented as a passing suite.

Logs are retained under `build/reliability-work/`; CTest details are under
`build/reliability/Testing/Temporary/`.

Native GUI regression coverage includes independent toggles, initial compiler errors
with configured target discovery, failed replacement PID retention, successful restart,
interactive input after restart, Debug/Release, manual actions, rapid/identical saves,
OFF, delayed native windows, placement/maximization and no repeated movement. A native
fixture requires actual arguments, an asset in a separate working directory, a runtime
DLL and an inherited environment canary before opening its window.

Compatibility coverage includes real configured/linkable local libraries, explicit
header evidence, docs/apps/non-C++/missing metadata, legacy snippet migration,
executable-package rejection/rollback and existing add/edit/remove/repair behavior.
The current GUI was also visually inspected in generated native Dark/Light screenshots.

Network-dependent `stage5_unified_raylib`, `stage5_libraries` and optional
`stage5_live_libraries` were not executed, respecting the user's prior local-only
restriction. This is not an unfiltered Full approval result.

## Limits and manual acceptance

Physical secondary-monitor/disconnection and mixed-DPI hardware behavior still require
manual acceptance. Synthetic disconnected/negative-monitor geometry is covered, but
does not replace those hardware tests. Exclusive fullscreen, Snap groups, application
memory/game state and application-specific logical-DPI scaling are not restored.
Borderless handling restores monitor bounds; the program remains responsible for
recreating its rendering mode and window styles.
Window recreation after successful restoration and indistinguishable multiwindow
class/title identities remain best-effort cases. Delayed-window retries are bounded.

Passive source inspection is conservative evidence, not C++ semantic analysis or a
security guarantee. Unknown/truncated/unavailable evidence reports Unable to Verify.
Some unusual or headerless app-plus-library layouts may need an explicit supported
interface. Consumer probes verify configuration/compilation/linkage, not complete
third-party runtime/API correctness. Approved CMake is still executable build code.

Auto Run updates a currently running managed program; saves do not automatically launch
one that the user stopped. Both automatic modes OFF means no save-triggered work.

Updated GUI:

```text
C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab\build\reliability\cpplivelab-gui.exe
```

Follow `AUTO_RUN_RELIABILITY_MANUAL_TESTS.md` for numbered acceptance steps and exact
local retest commands. Stop here for manual review; no subsequent redesign or rebrand
is included.
