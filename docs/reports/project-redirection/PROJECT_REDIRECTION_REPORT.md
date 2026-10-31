# Cpp — implementation and verification report

## Result and scope

Implemented the requested product redirection on the existing native Win32/C++20
application, without replacing the application or renaming compatibility identifiers.
The product is now **Cpp — Make Your Life Easier**. The active-project title retains
that identity and tagline. No universal memory hot reload, checkpoints, memory copying
or application-state restoration was introduced.

A pre-edit repository inspection and concise plan were provided; see
[PROJECT_REDIRECTION_PLAN.md](PROJECT_REDIRECTION_PLAN.md). Existing worktree changes
were preserved. No Git commit or push, global PATH/configuration change, installer,
Graphify execution, or FarmGame mutation occurred. The running build/debug manager
was left alone after its executable could not be overwritten; verification uses
**build/redirection** instead. Close the older manager normally before launching it.

## Implemented behavior

### Main interface and libraries

- Prominent Add Library / Manage Libraries controls and current project library
  summary. Build, Run, Stop, program Restart and Auto Run remain directly visible.
- Manager Restart is a separate Tools action. One Settings destination remains.
  Existing customized Dark/Light palettes, consoles and native controls are reused.
- Open Editor works while an application runs; the controller test launches a
  disposable editor fixture and verifies that the running program's PID survives.
- Existing pinned FetchContent/GitHub integrations and their repository/commit
  approval, target discovery, consumer verification, transaction locks and rollback
  remain in use. No competing dependency system was added.
- New installed CMake package, local CMake source and local header-only integration
  forms. Proper exported/imported targets are used where available; header-only paths
  get a dedicated INTERFACE target. Precompiled packages need imported CMake targets;
  a raw DLL without metadata is insufficient.
- Edit Integration supports local paths, package version, targets and CMake settings.
  FetchContent version/source updates retain the pinned verification workflow.
  Update/Remove/Repair/Inspect reuse the existing manager. Missing local paths can be
  repaired or removed without requiring the missing directory to exist for preview.
- Per-project metadata records method/package/path/components alongside existing
  pins/options/targets. Schemas1/2 remain readable; schema3 stores the added fields.
- Ownership, method, version, targets and recorded availability are shown. Update
  availability is explicitly **not checked automatically**. A successful metadata
  record is not presented as proof that arbitrary packages are installed.
- Existing managed duplicates and recognized manual target overlap are refused.
  Exact owned CMake sections are previewed before approval; unrelated source,
  assets, presets, manual CMake and shared local library directories are preserved.
  Arbitrary nested/custom CMake is not automatically adopted as manager-owned.

### Auto Run and lifecycle

- Reuses ProjectWatcher, content fingerprints, debounce and AutoBuildScheduler.
- Builds a replacement through the same managed-build CLI into alternating external
  manager-owned slots (`fresh-auto-a` / `fresh-auto-b`). Later builds reuse each
  slot's CMake/Ninja cache; the first use of a slot requires configuration.
- The old program and interactive I/O remain alive while the replacement compiles
  and links. Build errors retain that application and report the requested message.
- One replacement worker at a time. Rapid edits coalesce and obsolete results are
  discarded. Freshness is rechecked after graceful shutdown to handle a save during
  the restart boundary. A locked/unreadable input cannot trigger a repeated launch
  exception loop.
- Only a successfully built selected artifact is activated. Original outputs and
  failed-build diagnostics remain; no running executable is overwritten.
- Graceful restart/Stop posts WM_CLOSE to windows in the owned process tree and
  sends console EOF. A project timeout and optional force-stop policy are persisted;
  force-stop defaults OFF. Termination, when enabled, targets only the owned job.
- Manual Stop cancels queued automatic work. Later saves may build, but do not
  automatically launch a stopped/exited program. Crashes do not trigger a restart
  loop. Explicit Run/Restart launches it. Restart uses the last successful artifact
  without compiling, including when current editor content contains errors.
- Debug/Release, selected targets, discovery/developer environment, standard CMake
  builds, console input/output and Visual Studio editor integration are retained.
  Developer environment changes affect child processes only.

### Window placement

- Captures visible, unowned top-level windows only from known job process IDs,
  immediately before requesting shutdown. Records class/title, normal/maximized
  placement, physical bounds, display/work area and DPI information.
- Matches replacement windows by class/title, then deterministic class/enumeration
  order. Polls for delayed windows for up to15 seconds. No unrelated desktop windows
  are repositioned.
- Selects the recorded monitor when present, or a visible nearest-monitor fallback.
  Screen and workspace coordinates are handled separately. Minimized windows return
  to a visible normal placement. The desired monitor is selected before maximizing.
- Verifies the resulting rectangle or maximized monitor before declaring success;
  unmatched/constrained windows eventually report partial restoration.
- Ordinary position/size and maximization were exercised with a real restarted
  native test application. Multi-monitor/DPI/fullscreen limitations are below.

## Files in this change

New maintained code/sample:

- src/application_windows.hpp / src/application_windows.cpp
- tests/redirection_tests.cpp
- tests/consumers/native_window.cpp

Modified components:

- src/gui.cpp: branding, controls, shared action routing, replacement lifecycle.
- src/library_manager.hpp/cpp: direct Add and integration editing/inspection.
- include/cpplivelab/libraries.hpp / src/libraries.cpp: backward-readable metadata,
  validated target/package/local generation, overlap and ownership checks.
- include/cpplivelab/workspace.hpp / src/workspace.cpp / src/workspace_cli.cpp:
  validated explicit build slots and verified-slot activation.
- src/main.cpp / src/doctor.cpp: displayed identity (command names stay stable).
- CMakeLists.txt: new module/tests, UTF-8 MSVC sources, native system linking,
  persistent UI captures from the real controller test.
- tests/gui_flow_tests.cpp, tests/restart_gui_tests.cpp, tests/cli.cmake,
  tests/test_selection.cmake: intentional identity/workflow regression coverage.
- README.md, UI_DESIGN_GUIDELINES.md and the three PROJECT_REDIRECTION documents.

The repository already had many modified/untracked Stage5 files before this task;
Git's total diff is not an accurate measure of this request's changes alone.

## Actual test evidence

Build: installed MSVC19.44, C++20, CMake and Ninja, separate build/redirection.
Formatting: clang-format19.1.5, **80 maintained C++ files verified**.
`git diff --check`: passed (Git notes pre-existing CRLF-to-LF normalization warnings).

| Executed check | Actual result |
|---|---|
| Local selection from Full, 4 workers | **49/49 passed**, 174.17 seconds, exit0 |
| Doctor after that local run | **Passed**, MSVC/CMake/Ninja/Clang found; GCC optional |
| Quick after final editor/title refinements | **34/34 passed**, 4.72 seconds, exit0 |
| Expanded final GUI-controller rerun | **1/1 passed**, 77.35 seconds, exit0 |

The 49-test run includes all previous local regression labels plus the new unit,
real-library and window tests. It is a **FILTERED local run**, not unfiltered Full.
After the final editor/title refinement, Quick and the expanded GUI controller test
were rerun rather than claiming that an earlier binary tested the final UI change.

New exercised checks include:

- Geometry with negative upper-monitor coordinates and disconnected-display fallback;
  per-project preferences and isolation; schema3; multi-target package generation;
  rejection of executable text in package metadata.
- Actual offline CMake consumer: source-library add, second header-only library,
  compile/run, edit, remove one, reopen, preserve manual CMake and shared source;
  repair a moved include directory, find_package imported targets, missing-package
  failure/rollback, remove package while keeping the header library.
- Native API rectangle/size, maximization, visible minimized fallback and exclusion
  of windows outside the supplied process IDs.
- Real GUI controller: failed replacement retains the old PID, repaired source
  restarts, interactive I/O survives, manual Restart does not compile, real native
  application window restores rectangle/maximization and rapid saves coalesce. The final rerun also passed Auto Run OFF, identical-content save, editor-while-running, visible Unicode identity, Restart after Stop and crash-loop prevention checks.
- Existing CMake project/preset, Debug/Release, process-tree, cancellation, freshness,
  tool discovery outside PATH, theme/accessibility, UI layout/forms and restart
  regressions remain registered and were executed in the local selection.

Earlier failures were corrected and retested: private test project placement,
first-use preference-directory creation, transaction-token verification, native
Unicode cursor resource typing, and UTF-8 displayed text. An intermediate manager
link failed because the user's older GUI was running; using a separate build avoided
closing it. No failed intermediate run is represented as a pass.

## Tests deliberately not executed

Automatic approval review rejected unfiltered Full because it can download and
execute unreviewed third-party CMake. After the exact dependency versions/pins were
shown, you selected **Run local tests only for now**. Therefore:

- stage5_unified_raylib: NOT RUN for this change.
- stage5_libraries (live fmt/JSON): NOT RUN for this change.
- stage5_live_libraries (opt-in six-library workflow): NOT RUN.

No registered test was disabled/removed or relabelled to disguise this restriction.
Read-only public upstream CMake/docs were inspected for Raylib, SFML, SDL3, GLFW,
fmt and JSON target conventions. That is documentation verification, not a claim of
successful live integration for these six libraries.

## Remaining manual acceptance and limitations

- Physical secondary/vertically arranged/mixed-DPI displays, display disconnection,
  borderless and exclusive fullscreen were not manually exercised. Native API tests
  and primary-monitor native restarts do not prove every graphics application.
- Native Windows Snap Group membership and exclusive fullscreen cannot be guaranteed
  externally. Borderless/fullscreen applications may enforce their own bounds;
  available-work-area clamping is used for safe visible fallback.
- Multiple indistinguishable windows use deterministic fallback matching; windows
  appearing after15 seconds, unusual ownership models or applications that override
  external placement can receive only partial restoration. There is no memory or
  application-state restoration.
- Non-cooperating applications are retained after the graceful timeout unless the
  user opts into force-stop. EOF cannot be reopened for a retained console process.
- Raw precompiled libraries need sufficient CMake/runtime DLL metadata. Arbitrary
  package-manager installation, automatic compatibility solving, exhaustive manual
  CMake adoption and automatic upstream update checks are not provided.
- Alternate build slots use extra disk space and may configure twice initially.
  Old outputs/caches are preserved rather than automatically deleted.
- FarmGame build/run and real-project mutation acceptance remain yours; no FarmGame
  configure/write approval was inferred from this product redirection.

Use [PROJECT_REDIRECTION_MANUAL_TESTS.md](PROJECT_REDIRECTION_MANUAL_TESTS.md) for
numbered commands and expected results. No commit/push or later milestone is approved.

## Primary documentation consulted

CMake integration follows the existing target model and
[official dependency guide](https://github.com/Kitware/CMake/blob/master/Help/guide/using-dependencies/index.rst).
The Win32 implementation keeps coordinate systems distinct as required by
[WINDOWPLACEMENT](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-windowplacement)
and uses [SetWindowPlacement](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowplacement)
with monitor-aware fallback.

Target conventions were inspected in the publishers' sources:
[Raylib](https://github.com/raysan5/raylib/blob/02566453880e06bec97f5b8160fa6fe1e98397ca/src/CMakeLists.txt),
[SFML](https://github.com/SFML/SFML/blob/3.1.0/cmake/Macros.cmake),
[SDL3](https://github.com/libsdl-org/SDL/blob/main/docs/README-cmake.md),
[GLFW](https://github.com/glfw/glfw/blob/3.4/src/CMakeLists.txt),
[fmt](https://github.com/fmtlib/fmt/blob/11.0.2/CMakeLists.txt),
[JSON](https://github.com/nlohmann/json/blob/v3.11.3/CMakeLists.txt).
