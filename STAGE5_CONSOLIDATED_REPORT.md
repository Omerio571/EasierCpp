# Stage 5 consolidated implementation report — October 29, 2026

## Existing work preserved

Before this task, Stage 5 already had read-only CMake import, isolated manager-owned
builds, preset and vcpkg-root settings, targets, recent projects, editor preferences,
Empty/Multi-file creation, fmt/JSON selections, shared themed forms, and external
dependency previews. Stages 1–4 already provided discovery, CLI builds, interactive
console I/O, process ownership, automatic-build scheduling and Intelligent Run.
Baseline verification: Quick 25/25; Full 35/35, 115.67 seconds, Doctor successful.

## New workflow

- Project/Tools navigation on the left, one Settings destination on the right;
  redundant Build navigation removed. Existing toolbar handlers remain authoritative.
- No-project welcome with New/Open and recognizable recent names/paths. No implicit
  project reopening. Missing prerequisites and missing recent folders get helpful status.
- Shared Basic/Advanced forms, resizing, scrollable content, compact keyboard-usable
  information glyphs and theme-colored tooltips. Customized palettes are unchanged.
- GitHub source selection shared by New Project and Manage Libraries. Existing known
  fmt/JSON integrations and multiple growing library rows remain available.
- Concise diagnostic discovery summary, with original detailed paths/terminal report
  in a separate Tools action. Discovery never claims compile compatibility or Ready.
- Tools > Run CppLiveLab Tests: Quick/Full, 1–16 workers, default 4, streamed actual
  output, elapsed time and exit status. Cancellation uses a separate owned session;
  it cannot stop the user-program session. Program Console is preserved.

## Confirmed layout cause and repair

The old shared layout individually hid/showed/moved controls without batching,
retained rounded regions across moves, recursively laid out scroll changes, and
invalidated the parent without erasing old parent-painted input borders. It lacked
a guarded resize path. These are concrete sources of intermediate clipping and stale
button/border artifacts; no arbitrary timing delay was added to application layout.

The replacement computes one placement/visibility plan, guards re-entry, disables
intermediate redraw for visible forms, resets regions, batches positioning (with a
complete fallback), recalculates scrolling/size from current styles, regenerates
rounded regions, then erases/invalidates parent and children. One OK/Cancel pair is
reused. WM_SIZE and DPI handling preserve logical layout and reachable actions.
Tests repeatedly expand/collapse at 96/144/192 logical DPI, verify field/action
separation/visibility and resource counts. Native off-screen Dark/Light expanded
settings renders were inspected; actual desktop and mixed-monitor acceptance remains manual.

## Dependency methods actually supported

1. Existing curated fmt 11.0.2 and JSON for Modern C++ 3.11.3 snippets, with pinned
   archives/hashes and generated examples tested by real builds.
2. Public GitHub repositories with usable root CMake library targets. API release/tag
   discovery is passive. Latest Stable excludes draft/prerelease entries and resolves
   to an exact commit. Specific tags/releases/commits can be entered. Listings expose
   their first-100-releases/first-100-tags limit; older valid tags may be typed.
3. Explicit suitable header-only include directory/header: an actual include compile
   is required. Portable relative paths are accepted; arbitrary include layouts and
   package-specific setup may require manual integration.
4. Existing preset/vcpkg-root integration is preserved. No automatic package installation
   or new vcpkg package-store implementation was added.

Discovery separates missing/inaccessible versions from network and rate-limit/API
refusals. After download, a separate approval identifies repository and exact commit
before any dependency CMake runs. A disposable configure collects ACTUAL static,
shared and interface targets, then lets the user select one. Only a successful
minimal consumer build/link registers a pinned integration. A minimal consumer does
not exercise every API, optional feature, graphics window or runtime dependency.
Downloaded source alone is never labelled verified. Default-incompatible projects
explain failure and require their own documented manual options/dependencies.

New projects copy independent URL/SHA256 CMake snippets, not absolute manager-cache
paths. Existing-project management first saves a preview outside source. A second,
exact-change approval can append one managed block to root CMakeLists.txt, retain
a backup in project metadata and verify the real project build. Failure/cancellation
restores the original CMake bytes. Stale previews and duplicate managed blocks are
refused. Source/assets/presets are not edited by the manager. Third-party scripts run
under the user's account after approval; this is not an execution sandbox.

## Test Runner and diagnostics implementation

The runner locates the compiled-in manager source checkout, validates scripts and
launches Windows PowerShell through ConsoleSession without a visible console.
`scripts/build.ps1 -BuildDirectory` lets it use `build/integrated-tests`, separate
from the live GUI and selected project. Existing default command behavior remains.
A per-output file lease rejects conflicting script sessions. CTest descendants have
a recursion guard. Cancellation terminates only the owned test tree. Result lines
come from real CTest output; the GUI adds actual exit and elapsed time.

Summary diagnostics show Found/Not installed and available developer setup. The
detailed doctor remains copyable and distinguishes installed paths from terminal
PATH. No underlying discovery or persistent PATH modification was introduced.

## Executed verification

- Requested unfiltered Quick: **26/26, zero failures, 3.73 seconds**.
- Requested unfiltered Full: **36/36, zero failures, 121.87 seconds; Doctor passed**.
- Production runner command/process driver: real Quick **26/26**, CTest 5.28 seconds,
  overall 41.672 seconds including isolated first build, exit 0.
- Production runner Full: **36/36**, CTest 146.98 seconds, overall 160.172 seconds,
  Doctor passed, exit 0. These used the real isolated runner directory outside CTest.
- Real GitHub: discovered raylib versions, resolved **5.5** to
  `c1ab645ca298a2801097931d1079b10ff7eb9df8`, downloaded SHA256
  `aacb13c34e996e86267223f64f8631ed0b408e9b5dc234d8a844d7a5a97a26dc`,
  collected the real `raylib` target, compiled it and linked the consumer with exit 0.
  Its CMake deprecation warning was retained. No graphical window was exercised.
- Offline API fixtures cover real response processing, stable/prerelease selection,
  tags, exact commit pinning, older selection, bad URLs, missing versions, rate-limit
  refusals and disconnected networking. Quick adds no external network dependency.
- Unit tests cover failed/thrown verification rollback, stale preview refusal,
  duplicate block refusal, approved writes preserving assets, recursion/worker limits
  and an actual second script rejected before configuration by an existing lease.
- Native GUI controller covers welcome/no-project state, actual background output,
  overlap rejection and test cancellation while interactive program PID remains alive.
- A standalone GUI test exposed a fixture's missing `-ExecutionPolicy Bypass` argument
  (the CTest parent already supplied it via environment). Corrected the fixture and
  reran both CTest and standalone native-controller checks successfully. Welcome and
  workspace captures in both themes were also inspected. A rapid hide/show caused a
  compositor-only blank capture; keeping the test window shown between theme captures
  fixed that artifact without changing application painting.
- Formatting verified for **56 maintained C++ files**; Git whitespace checks passed.
- Protected scheduler/process/freshness/palette sources: **8/8 SHA256 matches**.
  Real FarmGame: **24/24 original files match, still 24 total files**. It was not built,
  configured or edited during these tests.

## Files for this refinement

New: `src/tool_workflows.hpp/.cpp`, `src/library_workflow.hpp/.cpp`,
`scripts/github-library.ps1`, `tests/github_library.ps1`, `tests/workflow_probe.cpp`,
this report and `STAGE5_CONSOLIDATED_MANUAL_TESTS.md`.

Updated: `src/gui.cpp`, shared `src/project_dialogs.hpp/.cpp`, `src/ui_theme.cpp`,
`src/workspace_creation.cpp`, `scripts/build.ps1`, `CMakeLists.txt`,
`tests/stage5_tests.cpp`, `tests/ui_forms_tests.cpp`, `tests/gui_flow_tests.cpp`,
`tests/test_selection.cmake`, `AGENTS.md`, `UI_DESIGN_GUIDELINES.md`, `README.md`,
`TESTING.md`, `TEST_RESULTS.md`. Many pre-existing uncommitted Stage 4/5 files were
already present; they were not reset, committed or attributed to this refinement.

## Limits and manual approval

Generic integration intentionally stops at supported default-configurable CMake
libraries or suitable explicit headers. Private repositories/authentication, arbitrary
build systems, package-specific option editors, multiple alternative header profiles
for one pinned repository, automatic updates, and automatically rewriting an existing
managed dependency block are deferred. Discovery is bounded; failed probes may leave
manager-owned caches for diagnosis. Project edits require a configured executable
target and root CMakeLists.txt; unusual project integration belongs in manual CMake.
The Test Runner requires the source checkout and installed toolchain; it is not a
standalone binary-only testing distribution. No stages 6/7, reload/recovery or other
language support was implemented.

Use [STAGE5_CONSOLIDATED_MANUAL_TESTS.md](STAGE5_CONSOLIDATED_MANUAL_TESTS.md) for the
full A–X acceptance guide, including FarmGame, VS2022, dependency approval, appearance,
resize/DPI and integrated Quick/Full/cancellation. No commit or push was performed.
Stage 5 remains awaiting your real-project/manual approval.
