# Stage 4 review

The latest reliability retest adds explicit tool-worker completion, intelligent Run
observations, hidden-controller coverage, input/Stop restart fixes and minimize-layout
protection. See [STAGE4_REPAIR_MANUAL_TESTS.md](STAGE4_REPAIR_MANUAL_TESTS.md) for current
manual checks. The subsequent [final refinements](STAGE4_FINAL_REFINEMENTS.md) add
unfinished-prompt preservation, semantic Dark/Light palettes, measured timing findings
and explicit Quick/Full test modes. Structured diagnostic summaries/Simple-Detailed
presentation remain outside that pass; earlier results below are historical.

Stage 4 extends the existing native Win32 application. It does not migrate frameworks
or create a code editor. CMake, Ninja, tool discovery, child-only MSVC initialization,
CLI commands, VS2022 folder opening and process-tree ownership remain in use.

## Files

New:

- `src/auto_build.hpp/.cpp`: overlapped Windows watcher and deterministic scheduler.
- `src/build_lease.hpp`: shared existing build-lock/executable-use protection.
- `src/console_model.hpp/.cpp`: bounded histories, severity, unread indicators.
- `src/console_view.hpp/.cpp`: native Rich Edit rendering, selection and scrolling.
- `src/appearance.hpp/.cpp`: palettes, user preferences and DPI scaling helper.
- `src/ui_theme.hpp/.cpp`: native control painting, rounded corners and focus/hover.
- `tests/stage4_tests.cpp`: real watcher/build/channel and deterministic model tests.
- `tests/console_view_tests.cpp`: real Rich Edit regression tests.
- `STAGE4_MANUAL_TESTS.md` and this review.

Modified:

- `src/gui.cpp`: controls, scheduling, build/run phases, console routing, themes/DPI.
- `src/session.cpp`, `include/cpplivelab/session.hpp`: separate stdout/stderr and child
  environment support, preserving the original merged-output API used by regressions.
- `src/projects.cpp`, `include/cpplivelab/projects.hpp`: incremental build overload;
  existing callers keep their normal configuration behavior. Existing lock logic is
  reused by both CLI and direct GUI program execution.
- `src/main.cpp`: optional `build --incremental`; existing syntax/version retained.
- `CMakeLists.txt`: system libraries, new modules and seven new CTest cases.
- `scripts/build.ps1`: optional `-TestFilter` for focused runs; default still runs all.
- `README.md`, `TEST_RESULTS.md`: current usage, scope and executed verification.

## Watcher and scheduling

One `ReadDirectoryChangesW` request watches the active project recursively. Its
OVERLAPPED structure, event and 64 KiB buffer remain alive until completion/cancel.
The existing timer checks the event without waiting; there is no directory scan on
every timer tick and no new watcher thread. Normal events fingerprint only changed
files. Directory renames/removals and notification overflow reconcile a snapshot.
Identical content does not trigger a build. Symlink/reparse directories are not followed.

Debouncing defaults to 400 ms (configurable 100–5000). Multiple changes leave one
pending build; further changes during compilation leave one follow-up. A genuine
compile failure waits for another edit rather than retrying endlessly. Config changes
request configuration; source changes reuse the cache and invoke Ninja incrementally.
Ignored output trees cannot cause build loops. Reserved generated folder names are
intentionally excluded even if someone puts hand-written sources there.

GUI Run probes the last observed successful build before deciding whether it needs
another incremental build, then launches the selected executable with the captured
developer environment and separate output pipes. The program owns the same exclusive
build lease used by CLI runs. Automatic builds defer while it or another configuration
operation holds the lock, and retry after exit/Stop. Existing executable-use checks also
protect independently launched binaries. No save kills/restarts an application. Stop
still targets only the owned Windows Job Object and descendants. Pending I/O is drained
before releasing the lease. Turning Auto Build off cancels pending automatic work;
it does not kill a build already underway. Project switches reset the watcher baseline.

## Consoles and themes

Program Console receives stdout, labelled red stderr, and local input echoes. System
operations and compiler diagnostics go only to Build & System. Each has separate bounded
history, clear, unread/error indication, selection and scroll. Appending/recoloring a
hidden Rich Edit never changes its visibility; this has an explicit regression test.
Incoming output does not focus controls or force a tab switch. Explicit Run selects
Program Console. Blue/green/amber/red system severities also have text labels.

Dark and Light use explicit palettes with tested text contrast. Follow Windows reads
the OS app-color setting and reacts to theme/setting notifications. Fonts/control
geometry scale with per-monitor DPI; native controls retain normal keyboard behavior.
Only Theme, AutoBuild and DebounceMilliseconds persist under HKCU\Software\CppLiveLab.
No system-wide PATH, IDE project configuration, execution state or checkpoint is saved.

## Limits and remaining manual checks

- No auto-run, live executable replacement, true hot reload, libraries, checkpoints,
  Git integration, or restoration. Stage 5 has not begun. No commit/push was made.
- Console is line-based, not a terminal emulator. Stdout/stderr order is exact within
  each pipe but only approximate across the two pipes. ANSI cursor control/full-screen
  console applications remain unsupported. Program arguments remain unsupported.
- Histories keep at most 400,000 characters/3,000 entries each, so older diagnostics
  eventually drop off; copy important diagnostics before that limit.
- Watcher baseline/rescan fingerprints run on the UI thread. Very large source trees
  or slow/network storage may pause briefly. Current verification uses local folders;
  remote shares and filesystem notification-loss recovery are not end-to-end tested.
- Arbitrarily named generated directories are identified once they contain CMakeCache.txt.
  Generated source outside any identifiable build tree cannot be distinguished from
  hand-written source. Explicit Debug/Release folder names are reserved/excluded.
- Existing CMake projects must remain compatible with Ninja and provide their own
  dependencies/options. CMake preset files are watched, but this stage does not add
  preset selection or override the previous project configuration policy.
- Native scrollbar/title-bar rendering depends on Windows. Dark, Light, Follow Windows
  and resizing are checked on this machine; physical mixed-DPI monitor moves and actual
  OS color changes remain manual acceptance checks. Preference round-trips and DPI
  geometry calculations are automated.

Follow `STAGE4_MANUAL_TESTS.md` for acceptance. Executed results are in `TEST_RESULTS.md`.
Stop here for user testing and explicit approval before starting another stage.

## Reliability implementation and automated coverage

- `src/child_startup.hpp` restricts inherited standard handles and captures setup
  output through root exit rather than waiting for helper-held pipe EOF.
- `ConsoleSession::start_worker` gives CLI/build workers an explicit root-completion
  policy. Once the root exits, polling removes only helpers in its owned job. Native
  program sessions still retain their complete process tree until exit/Stop. The
  original orphan-process regression is preserved. Job accounting alone cannot
  expose completion before the root handle signals.
- `src/build_freshness.hpp/.cpp` keeps independent in-memory observations by project,
  configuration and executable. Source fingerprints plus artifact metadata permit a
  Ninja dry-run probe; only an unambiguous no-work result skips compilation. Missing
  executables, dependency changes, failures, cancellations, restarts and ambiguous
  glob checks conservatively build. No fresh flag is persisted.
- Deferred builds use an explicit CLI result (75), not matching compiler message
  text. Genuine errors quoting old lock wording now retain Error severity.
- Build IDs, worker PIDs, exits and cancellation are logged. Thirty seconds without
  output warns and leaves Stop available; it does not kill legitimate long compiles.
  Stop during a build discards pending automatic/Run requests and establishes a new
  watcher baseline. Edits stay on disk; a later save or explicit build resumes work.
- Each launched program resets stdin availability. Completion resets cancellation
  state so Stop cannot leave Run disabled. Clearing Program shows a neutral input-
  availability status without guessing arbitrary stdout to be a formal prompt.
- Minimized/zero-size WM_SIZE events preserve the previous layout. Moves and rounded
  regions are applied before a single full redraw; child sibling clipping prevents
  cross-control painting, and edit fields no longer retain a stale native edge.
- `tests/reliability_tests.cpp` covers inheritance/EOF, helper lifecycle, repeated
  cancellation, real builds, coalescing, diagnostic collisions and dependency probes.
- `tests/gui_flow_tests.cpp` compiles the real controller/window procedure into an
  unactivated hidden host. It checks repeated Run, EOF/Stop restart, clear/input,
  80 resize cycles, rounded-region geometry, GDI bounds, separate Debug/Release builds
  and execution, and shutdown. A private registry key isolates any test preferences.
  This is automated state/geometry coverage, not a visual desktop walkthrough.

The earlier 23-test completion results below are historical. Current registration
contains 28 tests, including the original 16 Stage 1–3 regressions and all seven
original Stage 4 tests. No existing assertions were removed; CLI session launches
in integration tests now use the explicit worker policy instead of treating vendor
telemetry helpers as part of the user's interactive application.

## Completion pass — October 28, 2026

Fixed empty-console follow state and explicit viewport scrolling after showing a
background tab or appending with redraw suspended. A real Rich Edit viewport test
now runs in an unactivated off-screen test host, checks the bottom scroll position,
and distinguishes caret character positions from Win32 CRLF text lengths. Hidden
tab visibility, selected text and reading an earlier scroll position remain tested.

Expanded watcher tests cover atomic editor replacement, Unicode filenames and
retrying after a temporary exclusive editor lock. All 23 automated tests passed in
the final run, including the original 16 regressions. No existing user-project source
or configuration files were changed. See the dated results section for exact commands,
timing, GUI checks and remaining manual display/OS checks.
