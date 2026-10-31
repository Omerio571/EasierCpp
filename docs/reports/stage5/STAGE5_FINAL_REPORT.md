# Stage 5 final implementation and acceptance report

Date: October 30, 2026. Scope: library discovery/management, build reliability and
application restart. No commit, push, Stage 6 live reloading or Stage 7 restoration.

## Architecture and discovery findings

The existing native Win32 forms, PowerShell GitHub worker, discovered toolchain,
CLI build pipeline, CMake generator and outside-source build directories remain
the foundation. The GUI continues to supervise the CLI instead of maintaining a
second compiler pipeline. The manager itself has no graphics-library dependency.

Previously the library workflow relied on an artifact-oriented CMake target list,
a single target choice and catalog recipes. That is insufficient for public aliases,
interface/imported targets, independent project pins and subsequent library edits.
The new discovery script recursively inspects the configured directory graph;
CMake Graphviz supplies alias candidates, and every candidate must pass an actual
`TARGET` check in the consumer scope. A bounded second configure validates aliases
after Graphviz generation. Target type, backing alias, source owner and imported
status are retained. Public aliases precede backing names; internal dependencies
and main/test/example helpers are available separately under Advanced.

SFML's Graphics/Audio options being ON does not prove what an earlier disposable
probe exported. The original failed probe was not retained in the supplied evidence,
so its exact historical seven-item omission cannot be conclusively attributed to
one option, result limit or stale directory. The confirmed architectural defect was
using incomplete artifact-oriented enumeration as the selection contract and
allowing an unsuitable single target such as `sfml-main`. The replacement checks
the actual graph, has no arbitrary result truncation and makes no initial selection.
The real SFML 3.1.0 run discovered and selected all five public module aliases;
Graphics/Audio compiled, linked and ran in the maintained consumer below.

The selector supports multiple selection, scrolling, details, Advanced targets,
refresh and an explicitly validated manual target override. Namespaced `::` spelling
is preserved. No SFML target names are hardcoded into production discovery.

## Real library compatibility

These are executed production-workflow results, using temporary projects and
isolated state. Each test drives the actual Add forms, resolves a commit, downloads
and hashes its archive, discovers/selects targets, builds the generated project and
executes an API-using consumer. Graphical cases require an observed visible window
and a clean process exit. An empty link probe alone is not counted as this result.

| Library | Version | Selected actual target(s) | Configure | Compile/link | Run | Window |
|---|---|---|---|---|---|---|
| Raylib | 6.2 | `raylib` backing target; public `raylib_static` alias also discovered | PASS | PASS | PASS | PASS |
| SFML | 3.1.0 | `SFML::Graphics`, `SFML::Window`, `SFML::System`, `SFML::Audio`, `SFML::Network` | PASS | PASS | PASS | PASS |
| fmt | 11.0.2 | `fmt::fmt` | PASS | PASS | PASS | N/A |
| JSON for Modern C++ | v3.11.3 | `nlohmann_json::nlohmann_json` | PASS | PASS | PASS | N/A |
| GLFW | 3.4 | `glfw` | PASS | PASS | PASS | PASS |
| SDL3 | release-3.4.18 | `SDL3::SDL3` | PASS | PASS | PASS | PASS |

Exact resolved commits:

```text
Raylib  02566453880e06bec97f5b8160fa6fe1e98397ca
SFML    8835b6b955c96fa1ad1278a8dae6f05c4143effc
fmt     0c9fce2ffefecfdce794e1859584e25877b7b592
JSON    9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03
GLFW    7b6aead9fb88b3623e3b3725ebb42670cbe4c579
SDL3    829a65d769d935c4852f8159e964312c0957260a
```

Evidence: `build/stage5-final-live5.log` contains the first five successful
consumer results; `build/stage5-final-live-resumed2.log` contains SDL3's successful
consumer result. The original CTest live invocation was interrupted after its
worker incorrectly omitted SDL's Latest Stable choice. The release-array defect
described below is fixed. The driver now reports reopened dialogs, uses a pinned
SDL tag for reproducibility and can resume a marked owned fixture.
Completed consumer results are reused only after checking retained pins, source,
all nonempty generated CMake lines and artifact existence. This is test-fixture progress,
not an application checkpoint feature. A green uninterrupted Libraries CTest run
is not claimed from that interrupted invocation.

The final resumed native driver **exited 0** and printed `LIVE_MANAGEMENT ... passed`
in `build/stage5-final-live-resumed4.log`. Its six retained consumer results were
validated without repeating their completed workflows. The remaining executed
matrix passed: fmt then JSON then GLFW, a real combined API consumer, unchanged
no-work build, JSON removal with expected compilation failure, re-add, update to
v3.12.0 and execution, independent old project pin, Raylib/SFML in both addition
orders with visible windows, an actual incompatible SFML target change with failed
build and rollback, execution of the old working SFML program, catalog deletion
with project metadata retained, and reopening/building the original fmt project.
The owned temporary fixture was removed after success.

The continuation command executed was:

```powershell
.\build\debug\library_live_tests.exe 'C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab\build\debug\cpplivelab.exe' --resume 'C:\Users\omer2\AppData\Local\Temp\cll5-24364-660248796-0'
```

That fixture no longer exists. To reproduce a fresh complete live suite, use
`powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Libraries -Jobs 1`.

## Build failures found and corrected

- Windows PowerShell workers launched from PowerShell 7 could inherit the wrong
  module search path. The worker prepends its own built-in module directory in its
  child environment so `Get-FileHash` remains available.
- Windows PowerShell's real `Invoke-RestMethod` returned a release array as one
  pipeline object. Filtering that object evaluated arrays of draft/prerelease flags
  and incorrectly omitted Latest Stable. Capturing and returning the response now
  enumerates individual releases. The offline mock reproduces the real nested
  response shape. Actual discovery/resolution then listed Latest Stable and resolved
  JSON to v3.12.0 (`55f93686c01528224f448c19128836e7df245f72`) and SDL to
  release-3.4.18 (`829a65d769d935c4852f8159e964312c0957260a`), both successfully.
  Evidence: `build/stage5-final-latest-stable.log`.
- Bundled CMake 3.31.6 emitted unquoted MSVC module-scanner output paths containing
  spaces. Header-based generated starters/probes set the normal
  `CMAKE_CXX_SCAN_FOR_MODULES` variable OFF. Imported project configurations are not
  rewritten. See the official [CMake modules documentation](https://cmake.org/cmake/help/latest/manual/cmake-cxxmodules.7.html).
- Windows PowerShell's .NET ZIP extractor rejected JSON's long archive entries.
  After containment/symlink validation, the worker uses discovered CMake's archive
  extractor. Offline tests include a real archive entry longer than 260 characters.
- Archive replacement now supplies an explicit backup path; a PowerShell null
  backup argument previously became an invalid empty string.
- A real combined-library no-work test exposed Ninja 1.12's Windows relative-path
  limit. The measured failing header path was 268 characters before normalization
  and 183 after normalization. Read-only Windows API checks returned PATH_NOT_FOUND
  for the relative form and valid attributes for the absolute form. Verified archive
  extraction now publishes a compact `s<8 hex digits>` source directory instead of
  nesting GitHub's full commit directory. Previous source trees remain intact.
  Older long cache layouts are compacted on their next Add/Repair workflow, without
  downloading a matching verified archive again. Existing builds may need an
  approved fresh rebuild after repairing an old long layout. Ninja's implementation
  is documented in [its Windows file checks](https://github.com/ninja-build/ninja/blob/v1.12.1/src/disk_interface.cc).
- SDL's maintained consumer now includes `SDL_main.h` and explicitly returns zero;
  SDL renames the function, so falling off it produced an undefined exit status
  despite printing its success marker. This was a test-consumer defect.

## Library management

**Project > Manage Libraries** provides one compact action form with project pins,
known entries, source/compiled/temporary cache information and readable details.

- **Update Version:** resolves the chosen version, verifies a fresh integration and
  previews the project change. Other projects keep independent copied metadata.
- **Change Targets:** probes the existing exact commit again and lets the user choose
  one or more actual targets. It does not silently upgrade the library.
- **Remove from Project:** removes only the approved managed dependency/links.
  Sources, assets, presets and unrelated libraries remain untouched. Code still
  referring to removed APIs is expected to fail its next build.
- **Delete Known Library:** removes the catalog choice while preserving project pins.
  Protected built-ins use Hide/Show rather than deleting their implementation.
- **Delete Cache:** scans registered owned entries in the background, reports size
  and status and requires explicit confirmation. Known shared references require
  separate approval. Active jobs/programs, `.git` contents, reparse points, escaped
  paths and unregistered legacy directories are protected. Missing/locked/partial
  deletions are reported truthfully and remain repairable/retryable.
- **Reinstall / Repair:** keeps the selected revision, options and targets. Verified
  archives can be re-extracted without downloading; missing archives are fetched
  again with the expected hash. A fresh consumer build validates the repair.
- **Recover Interrupted Change:** recovers a pending manager transaction explicitly.
  Ordinary builds refuse incomplete dependency edits instead of launching stale code.

Changes use previewed byte comparisons, backups, an outside-source journal and
candidate project metadata. A fresh build must pass before the new profile becomes
active. Failure restores only still-owned proposed bytes, preserves subsequent user
edits and retains old outputs. A project mutation gate and per-output/source/cache
leases prevent a build from racing a dependency edit or cache deletion. Existing
legacy recipes can be adopted only when their saved original block still matches.

## Restart

One visible **Restart** button sits beside the one **Settings** destination on the
right; Alt+R uses the same action. Restart starts a real replacement process and
waits for its ready acknowledgement before committing handover. The original remains
open if startup fails. A state-root-specific instance mutex prevents duplicates.

The active project, theme, preferred editor, build configuration and persistent
preferences return. Temporary dialog fields, discovery flags and queued work start
fresh. Active owned work requires confirmation; Cancel keeps it running. Approved
restart stops only owned processes, or waits for a safe metadata/cache boundary.
Visual Studio and unrelated applications are not terminated.

## Automated verification

Final regular verification commands:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Quick -Jobs 4
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Full -Jobs 4 -Doctor
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\format.ps1 -Check
git diff --check
git status --short
```

| Check | Executed result | Evidence under ignored `build/` |
|---|---|---|
| Quick | **32/32 PASS**, 5.06 seconds | `stage5-final-quick11.log` |
| Full | **48/48 PASS**, 260.82 seconds | `stage5-final-full4.log` |
| Doctor | **PASS**, included after Full | `stage5-final-full4.log` |
| Formatting | **PASS**, clang-format 19.1.5, 76 maintained C++ files | `stage5-final-format-check2.log` |
| Git whitespace check | **PASS**, exit 0 | `stage5-final-git-diff-check.log` |
| Generated-file ignores | Build executable and verification logs correctly ignored | `git check-ignore` executed |

Full includes the real GUI flow and restart tests, with 35.05 and 1.81 seconds
respectively in that run. The original regular tests were retained. Separate
targeted runs also exercised archive repair/hash/long paths and legacy source-cache
compaction without redownloading or changing the old source tree. The real consumer
and completed native live-management results are detailed above.

Added/extended tests cover actual CMake aliases/interface/imported targets, CRLF
parsing, multi-select and long details layout, independent metadata/catalog deletion,
transaction rollback/concurrent user edits/interrupted recovery, build/cache leases,
junction/Git/locked/missing/shared cache protections, archive hash/repair/long paths,
a real shared-DLL consumer, configuration/link failures, and real parent/child GUI
restart with isolated preferences. All prior Stage 1-4 regressions remain registered.
Quick is offline; Full includes the previous network-dependent tests; Libraries is
an explicitly selected live-network suite. No accessibility checks were removed.

## Files and manual checks

Main new files: `include/cpplivelab/libraries.hpp`, `src/libraries.cpp`,
`src/library_manager.cpp/.hpp`, `src/restart.cpp/.hpp`,
`scripts/discover_targets.cmake`, `tests/library_management_tests.cpp`,
`tests/library_live_tests.cpp`, `tests/restart_tests.cpp`,
`tests/restart_gui_tests.cpp`, and six maintained `tests/consumers/*.cpp` programs.

Modified shared integration/build/UI files include `src/library_workflow.cpp/.hpp`,
`src/tool_workflows.cpp/.hpp`, `src/workspace.cpp`, `src/workspace_cli.cpp`,
`src/workspace_creation.cpp`, `src/projects.cpp`, `src/project_dialogs.cpp/.hpp`,
`src/gui.cpp`, `src/main.cpp`, `scripts/github-library.ps1`,
`scripts/file_api.cmake`, `scripts/build.ps1`, `CMakeLists.txt`, relevant existing
tests, README and TESTING. The workspace already had extensive uncommitted earlier
Stage 4/5 work; Git status alone does not identify this request's changes.

Actual desktop checks used an isolated project/state/preferences profile: main
header and single Settings placement, Manage Libraries actions/details and keyboard
navigation, Light appearance, and a real Restart restoring the project/theme.
The audit found cramped library details and an outward-opening right Settings menu;
details now use a tested 150-logical-pixel area and the menu aligns inward. Native
automated tests cover the corrected sizing. Every management/destructive confirmation
has not been manually clicked; use the acceptance guide for final human review.

FarmGame's **24/24** protected hashes and both customized appearance files match
the saved baselines. The older MyCppProject snapshot points to a path now absent;
its hash could not be reverified. No recreation or change was attempted. FarmGame
was not built or run during this final task; real-project acceptance remains manual.

## Limits and acceptance

Support is for compatible public GitHub CMake/header-only libraries. Private
repositories, arbitrary build systems and libraries needing additional SDKs may
require manual setup. Graphviz can omit consumer-invisible/private aliases; manual
overrides still require actual validation. Reference counts cover manager-known
projects, not a disk-wide search. Cache cleanup only handles registered owned paths;
old unregistered directories and previous source/archive backups are retained.
Very long custom roots remain subject to the installed compiler/build-tool limits.
Explicit presets remain authoritative. File API-built DLL directories are supplied
to the child program, but arbitrary external SDK deployment is not guaranteed.

Follow [STAGE5_FINAL_MANUAL_TESTS.md](STAGE5_FINAL_MANUAL_TESTS.md) for numbered
steps, expected results, each library's exact repository/version/target and consumer
copy/build/run commands, multiple additions, updates, failure rollback, removal,
catalog/cache/repair, Restart and prior-stage acceptance checks.

Stop here for manual approval. No commit or push was made.
