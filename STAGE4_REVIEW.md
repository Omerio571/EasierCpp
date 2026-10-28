# Stage 4 review

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

GUI Run builds first, then launches the selected executable directly with the captured
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
