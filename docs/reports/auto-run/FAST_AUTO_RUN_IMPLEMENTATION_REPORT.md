# Unified Auto Run implementation and measurements

Date: 2026-10-31. Repository: `C:\Users\omer2\OneDrive\Desktop\Projects\EasierCpp`.

## Result

There is one automatic mode/control, **Auto Run**. ON watches relevant saves, builds
incrementally and launches the selected current executable, including from idle.
Enabling/opening alone does not launch or stop a program. OFF cancels automatic workers
and pending automatic launches. Explicit manual Run requests survive switching OFF.
Manual Build/Run/Stop/Restart remain available.

Stop cancels current owned work and pauses automatic launching. Saves may still compile
while ON (paused), but cannot launch until Run, Restart or OFF/ON rearms it. The pause is
session-local. Natural exit/crash does not create a restart loop; a later relevant save
can launch again. A successful replacement also launches if its old instance exited
during compilation. No-op saves do not dispatch builds or restart processes.

One existing scheduler/watcher supervises the pipeline. Separate build/process jobs,
leases, source-fingerprint checks, target/artifact checks, shutdown policy, runtime
arguments/environment, working directories, assets, DLL lookup and native placement
restoration remain. Project mutations and manager handoff stay blocked while a
replacement job is owned, even after its old application exits.

## Preference compatibility

- Per-project `auto-run.settings` remains the single persisted mode setting, outside
  project sources. Its existing four-field format and window/stop options are retained.
- An explicit project ON/OFF wins. Only a missing file imports the old global AutoBuild
  default. Corrupt project settings fail closed. Existing files are not rewritten on open.
- The old registry value is read-only migration input for unopened projects, not an
  active second mode. Appearance/debounce saves no longer write it.
- Fresh profiles use **150 ms** debounce. **Existing saved values are preserved**.
  To use the faster setting in an existing profile, set Settings -> Build Preferences
  -> Auto Run debounce to `150`.
- Control ID 116 and internal scheduler/test names remain reserved/stable; the old
  Auto Build control is not created.

## Performance decisions

Reused the already implemented prepared-SDK environment, guarded File API metadata
reuse, Ninja dependency checks, coalescing and alternating automatic slots. Idle build
workers now also receive the GUI's prepared environment when it is available.

Both slots retain their own compiled objects/dependencies. The benchmark confirmed that
source-only changes retain the unchanged static library and header changes rebuild it.
Sharing raw compiler/PDB outputs between different CMake trees was not introduced.
No new compiler flags, compiler-cache dependency, persistent worker, executable-staging
architecture, shortened shutdown timeout or window-readiness delay was introduced.
Compiler/link time remains dominant on the measured fixtures. File API replies continue
to follow the existing indexed-reply ownership rules described by
[CMake's File API documentation](https://cmake.org/cmake/help/latest/manual/cmake-file-api.7.html).

## Isolated save-to-window measurements

The preserved pre-change executable and final executable ran sequentially without
concurrent tests. Each ran three warm source-only replacements on the same fixture
design/configuration, after both slots were warm. Each fixture requires actual DLL,
argument, environment and asset handling. Its window creation deliberately waits 600 ms
to test delayed restoration; that delay is included below. Profiles were fresh, so the
comparison includes the default debounce change from 400 to 150 ms.

All numbers are milliseconds. Component medians are independent and need not sum to
the median total. Event timestamps use the existing GUI test hooks and Windows tick
resolution; zero indicates below the observation resolution, not literally no work.

| Stage / metric | Before median | After median |
| --- | ---: | ---: |
| File notification detection | 47 | 47 |
| Debounce/dispatch after detection | 531 | 266 |
| Replacement build pipeline | 1562 | 1547 |
| Shutdown through observed old-process completion | 16 | 16 |
| Slot activation, metadata/freshness preparation | 94 | 78 |
| New process creation | 93 | 94 |
| New process to restored window | 718 | 656 |
| Save to process creation completed | 2359 | 2063 |
| **Save to restored window** | **3015** | **2750** |

Before visible totals: **3078, 3000, 3015**. After: **2594, 2782, 2750**.
Median visible reduction: **265 ms / 8.8%**. Median detection-plus-dispatch was 578 ms
before and 313 ms after, matching that reduction. There is no manager executable-copy
staging step; slot activation is included in preparation. Project-defined copy commands
would be included in build time.

The **sub-second target was not reached**. This is a small local sample, not a FarmGame
benchmark or a guarantee. Existing profiles keep their old debounce until changed.
Window code, polling frequency and shutdown timeout are unchanged.

## Additional before/after benchmark cases

`scripts/benchmark-auto-run.ps1` creates an owned C++20 project with a static dependency,
uses discovered installed tools and records CSV/logs. SDK preparation is measured once
per isolated PowerShell child, then reused for its CLI calls. CMake configure and direct
compile/link rows are tool-level timings; managed CLI rows include their full pipeline.
Compilation and linking are measured together, not claimed as independent CPU timings.

| Case | Before ms | After ms |
| --- | ---: | ---: |
| SDK preparation once | 4513.24 | 6098.93 |
| Direct cold CMake configuration | 1639.68 | 1306.35 |
| Direct cold compilation/link | 1349.63 | 1399.01 |
| Direct Ninja no-op | 65.74 | 67.45 |
| Cold automatic slot A | 2857.97 | 2990.54 |
| Cold automatic slot B | 2882.26 | 2899.18 |
| Managed warm no-op build | 244.05 | 249.86 |
| Source edit sample 1 | 1297.82 | 1341.91 |
| Source edit sample 2 | 1361.12 | 3175.50 |
| Source edit sample 3 | 1300.06 | 1422.53 |
| Header dependency change | 1432.17 | 1525.94 |
| Intentional compile failure | 254.30 | 246.93 |
| Recovery | 1277.27 | 1289.60 |
| Console execution with supplied stdin | 243.70 | 90.35 |

No compile/link speedup is claimed: source-edit medians were 1300.06 and 1422.53 ms,
and the after run had a 3175.50 ms outlier. SDK/tool timings also varied. Those algorithms
were retained; these samples do not establish the cause of timing variation.
The intended measured improvement is the fresh-profile save/debounce interval.

Rapid saves, failed-build retention/recovery, first/subsequent native restarts, normal/
maximized restoration, manual controls and interactive input all passed in both GUI
runs. Individual before/after rapid-save/failure stage latencies were not separately
instrumented; the CLI failure/recovery rows and GUI correctness assertions are reported
without inventing missing phase measurements.

## Actual tests and repairs

- Baseline initially failed to compile because a relocated GUI test used the old source
  include/sample paths. Corrected those fixture paths; **4/4 baseline tests passed**.
- Unified-policy focused checks passed **4/4**, including migration and launch-from-idle.
- **Quick: 35/35 passed**, 9.70 s.
- Permitted local Full selection: **50/51 passed**, 217.83 s. The failing legacy
  `stage4_auto_build` fixture raced buffered output against process exit. Replaced its
  sleep/output synchronization with an owned marker/release handshake, retaining the
  assertion that edits occur during an active build. **Targeted retest: 1/1 passed**,
  44.28 s. The other 50 tests passed unchanged; this is not presented as a single clean
  51/51 Full run.
- After final replacement-ownership guards, native GUI flow, native UI audit and actual
  manager-restart tests passed **3/3**, 78.12 s. The two final isolated GUI runs also
  exited **0**, and both benchmark scripts passed their assertions. The deliberately
  invalid source case returned nonzero as required, followed by successful recovery.
- Standalone environment doctor passed. Scoped formatting passed for all nine changed
  C++ files; `git diff --check` passed.
- Repository-wide format check still fails on the pre-existing unfinished/untracked
  `include/cpplivelab/performance_timer.hpp`. It was preserved byte-for-byte and excluded
  from commits. No new product build/test failure remains in the executed checks.
- Windows PowerShell 5.1 initially treated a harmless native CMake stderr warning as a
  terminating script error. The benchmark wrapper now captures stderr while deciding
  success from native exit codes; the final isolated runs used that corrected wrapper.

Network-dependent `stage5_unified_raylib`, `stage5_libraries` and optional
`stage5_live_libraries` were not run under the existing local-only restriction.
The historical FarmGame first-placement issue remains previously intermittent and
currently not reproducible; native first-replacement checks passed. No FarmGame writes
or rebuilds, physical mixed-DPI/secondary-monitor acceptance, game-state restoration,
GitLab support, major redesign or tool installation were included.

## Changed components and repository recovery

- `src/gui/gui.cpp`: one control, runtime arm/Stop policy, idle launch, cancellation,
  latest-result checks and replacement-job ownership guards.
- `include/appearance/appearance.hpp`, `src/appearance/appearance.cpp`: one-mode
  preference ownership, legacy reader and fresh debounce default.
- `include/process/application_windows.hpp`, `src/process/application_windows.cpp`:
  safe missing-file preference migration; placement implementation unchanged.
- GUI flow/restart, legacy scheduler/appearance and redirection tests were updated.
- `.gitignore` and `scripts/format.ps1` now recognize maintained `src/build`,
  `include/build`, `tests/build` modules. Eight previously ignored existing module files
  were added to version control so a checkout retains the required implementation/tests;
  this is not a new build architecture. Generated outputs remain ignored.
- Added the repeatable benchmark script, current behavior documentation and manual guide.

Local commits include `98dd984` (fixture-path repair) and `9520818` (unified mode and
maintained-module recovery), followed by the final measurement/report commit. No push.
The four pre-existing deleted maintenance scripts and unfinished timer header remain
outside these commits.

## Handoff

GUI: `C:\Users\omer2\OneDrive\Desktop\Projects\EasierCpp\build\fast-auto-run\cpplivelab-gui.exe`.

Follow [the numbered manual acceptance guide](../../manual-tests/auto-run/FAST_AUTO_RUN_MANUAL_TESTS.md).
Close an older manager normally before launching this build. Confirm fresh/paused/Off
behavior and your real project assets/DLLs, then check actual monitor/DPI configurations.

Raw data lives in ignored `build/fast-auto-run-work/`: `isolated-before/`,
`isolated-after/` contain CSVs and command logs; `isolated-gui-before.log` and
`isolated-gui-after.log` contain stage rows and assertions. Quick/local suite, targeted
retest, doctor and formatting logs are retained there too. Further compilation-time
work needs project-specific profiling; no dependency checks were skipped to chase a
latency target.
