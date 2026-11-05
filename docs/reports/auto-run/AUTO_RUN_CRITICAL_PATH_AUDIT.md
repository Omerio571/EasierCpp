# Auto Run critical-path audit

November 4, 2026. Version remains 0.1.1. No production optimization was justified
by this audit. SDK startup and two-tree changes already in the working tree were
preserved. Only the existing GUI benchmark harness and this report/data were changed.
No user project, compiler flag, dependency, release package, global setting or Git
history was changed.

## Method and boundaries

Reused `gui_flow_tests --benchmark-project` and `--benchmark-replacement`, the real
Application controller, `EASIERCPP_PROFILE_BUILD`, and Ninja command timing. Fixtures
are owned disposable projects, with isolated manager state. The controller creates
native windows but its manager window is offscreen. No Raylib/FarmGame copies,
downloads or added dependencies were used. Benchmarks ran sequentially without
concurrent builds/tests; Windows background activity was not controlled.

Fixtures: small C++20 native Win32 application plus a local static library; a console
variant with rendered PID-specific READY output and stdin/stdout echo; medium native
application with 20 additional translation units including a shared header. All use
MSVC Debug/Ninja and native CMake PCH for application/library. The medium fixture
models compilation fan-out, not a large real-world dependency graph or GPU workload.

Seven warm source samples per project; separate initial build, first cold alternate,
return to original, one shared-header change, same-content save and five rapid saves
20 ms apart. Source edits append comments, so they require compilation without
altering application behavior. Header edits also append comments, correctly exercising
dependency recompilation. Warmups are reported separately, never in warm medians.
Replacement comparison interleaves ten Safe and ten Seamless samples after two
warmups. Default replacement benchmark debounce is the actual shipping **150 ms**;
project comparisons explicitly exercise 250, 150 and 100 ms settings.

GUI events use monotonic GetTickCount64 (coarse Windows tick resolution); replacement
observer uses steady_clock and a 5 ms message-pump loop. Worker checkpoints use
steady_clock. Observer endpoints include polling delay. First visible HWND does not
prove a frame was painted, a texture uploaded or keyboard focus granted. Console
endpoint is READY text present in the manager's Rich Edit, not merely process creation.
P95 uses nearest rank; with 7 or 10 samples it equals the maximum, not a robust
population tail estimate. Cold/header observations are n=1 per fixture, not statistical
comparisons. Small setting-comparison batches are sequential, not randomized pairs.

Raw logs/controllers are retained under `build/critical-path/`,
`build/measure-critical-path.ps1`, and `build/critical-path-*.log`. Maintained numeric
evidence accompanies this report in `AUTO_RUN_CRITICAL_PATH_TIMINGS.csv`.

## Complete pipeline and ownership

| Step | Actual code and behavior |
|---|---|
| Save/detect | `src/build/auto_build.cpp::ProjectWatcher::poll`, overlapped ReadDirectoryChangesW; GUI `poll_watcher` consumes notifications on its 50 ms timer. |
| Settle/confirm | Scheduler waits since last notification; `consume_changes` hashes changed paths, suppresses same-content saves. Directory/overflow events reconcile snapshots. It does not rescan the whole project every timer tick. |
| Prepare/dispatch | GUI `start_replacement_build`: collect existing SDK future, select inactive compatible tree, snapshot relevant source/configuration, construct managed-build command; begin scheduler build. |
| Worker | Owned `EasierCpp.exe manage-build`; `workspace.cpp::managed_build` inspects project, acquires leases, validates profile/cache, reuses prepared SDK block, discovers cached tool paths. |
| Configure/metadata | Explicit configure skipped when eligible; CMake/Ninja still checks regeneration prerequisites. File API reader skipped only when indexed metadata snapshot remains valid. |
| Compile/link | CMake --build invokes Ninja, configured compiler workers, native PCH and linker. Each tree retains independent objects/PCH/ILK. No caches copied between paths. |
| Verify | Worker validates metadata/artifact, registers dependency caches, drains output/exits. GUI rechecks sources/pending edits and selected target artifact before any replacement. |
| Safe replacement | `request_program_restart` captures placement, closes old window/stdin, waits for owned process/output completion; activates tree, creates replacement with original args/workdir/runtime paths; restores window after appearance. |
| Seamless replacement | `start_candidate` builds runtime environment/lease and creates candidate while old process remains alive. `poll_candidate` observes windows and restores placement early, independently of readiness. |
| Protected handoff | Candidate must remain alive at least 1000 ms since launch, and graphical candidate visible at least 250 ms; then source snapshot revalidated, old process closed gracefully, final snapshot checked at promotion. Tree pointer/ownership/output transfer completes only after old process/output retires. |

Seamless first visibility, restored placement, old-window retirement and completed
handoff are different endpoints. Its one-second safety hold is not a delay before
showing the new window. Safe mode necessarily has a window gap after old retirement.

## Timing results

Milliseconds. Warm source endpoint for graphical projects is **restored window**, not
the earlier first visibility; console endpoint is rendered READY. First visibility is
measured separately in the interleaved replacement table.

| Fixture / debounce | Initial Run | First cold alternate | Return to original | Warm median / mean / P95 (n=7) | Header (n=1) |
|---|---:|---:|---:|---:|---:|
| Small graphical / 250 | 8969 | 4719 | 1703 | 1500 / 1520 / 1656 | 1469 |
| Medium graphical / 250 | 12016 | 10391 | 1719 | 1672 / 1678.57 / 1796 | 4219 |
| Console / 250 | 9469 | 4875 | 1781 | 1875 / 1995.57 / 2532 | 2797 |
| Small graphical / 150 (shipping default) | 10437 | 5672 | 1469 | 1563 / 1643 / 1984 | 1531 |
| Small graphical / 100 (existing preference) | 10953 | 5360 | 1313 | 1343 / 1359.14 / 1438 | 1421 |

The non-monotonic 250/150 totals demonstrate host/build variation: they are not a
clean causal comparison. Detection-to-dispatch medians were 328, 266 and 203 ms for
250/150/100 respectively. This interval includes notification settling/re-notification,
content confirmation, preparation and UI scheduling, not just a literal sleep. The
250-to-100 comparison improved median total by 157 ms (10.5%), with identical 875 ms
outer build medians. The later 150 batch had a slower 1016 ms build median; its 220 ms
total difference from 100 cannot all be credited to debounce. Expected configured
150-to-100 saving is about 50 ms plus scheduling effects, not a proven 220 ms product
optimization. No default or existing saved preference was changed.

Every same-content save observed for at least 1203 ms produced **zero builds and zero
restarts**. Each five-save burst produced one build; first-save-to-ready totals were
1594/4313/1984/1703/1563 ms in table order. Medium rapid timing follows a shared-header
edit, so the other tree also owed that header rebuild; it is not solely burst overhead.
Existing stronger GUI regression separately checks latest-content/failure semantics.
The seven warm source and header checks retained all four PCH timestamps per fixture.
Warm source builds ran exactly two Ninja commands (one compile, one link), no archive
or explicit configure. Medium header edit ran 23 commands (22 compiles + link).
The medium cold build ran 27 commands including PCH/library work. Independent-tree
first compilation remains necessary; this is not evidence of cache loss.

Cold/first-alternate worker detail, n=1 each, configure wall / Ninja wall / link
edge ms (nested in the end-to-end table, not additional time):

| Fixture / setting | Initial worker | First alternate worker |
|---|---:|---:|
| Small / 250 | 1793.90 / 2264.44 / 576 | 1555.96 / 2390.88 / 805 |
| Medium / 250 | 1787.07 / 4727.19 / 845 | 1615.81 / 8017.34 / 813 |
| Console / 250 | 1809.79 / 2714.76 / 634 | 1606.66 / 2494.87 / 603 |
| Small / 150 | 1793.49 / 3352.67 / 823 | 1632.64 / 3338.48 / 727 |

The medium first-alternate Ninja run was substantially slower than initial Ninja,
despite the same 27-command count. These single observations do not prove a new
cache regression: they are fresh independent trees and the cold execution mix/host
timing varies. Compilation, library and PCH creation are real cold work; subsequent
source updates returned to two commands. No reboot/cache-persistence experiment was
repeated; see [the earlier persistence report](../AB_CACHE_PERSISTENCE_2026-11-04.md).

### Default 150 ms small graphical pipeline

Arithmetic **means** from the same seven samples, so the non-overlapping outer
intervals add to the measured total. These are separate from the other tables' medians.

| Outer interval | Mean ms |
|---|---:|
| Save -> first detected change | 31.29 |
| First detection -> dispatch | 268.14 |
| Dispatch -> verified result / shutdown request | 1093.57 |
| Shutdown request -> old process stopped | 60.43 |
| Old stopped -> pointer activated | 6.43 |
| Activated -> CreateProcess boundary | 42.57 |
| Runtime-environment construction / process creation boundary | 82.57 |
| Process created -> restored window | 58.00 |
| **Save -> restored window** | **1643.00** |

Within the 1093.57 ms outer worker interval (do **not** add these again): dispatch
to CLI entry mean 51.29 ms (median 47, max 93); worker entry to managed entry mean
2.29 ms. Profile means: inspection 1.79, leases/options/cache 13.38, tool discovery
10.28, prepared environment 0.46, configure arguments 4.62, skipped-configure
bookkeeping 0.08, metadata-before 4.97, artifact-before 2.11, CMake/Ninja wall
957.07, metadata-after 4.29, artifact-after 1.52 ms. Remaining roughly 42 ms includes
unprofiled worker tail, output completion, GUI validation and timer boundary; it is
not attributed to one function. Configure 0.08 ms does **not** mean CMake configured
that quickly; no configure process was needed.

Within CMake/Ninja, compile edge median 382 ms / mean 408; link edge median 408 /
mean 447.86. These are command-edge durations including wrappers, not compiler/LINK
internal CPU times. Multiple cold/header compile commands overlap; command-duration
sums must not be added to wall time. For example medium cold compile-command sum
6839 ms exceeds its 4727 ms Ninja wall time. Cold configure itself was 1787 ms for
that fixture. Initial Run remainder includes SDK/preparation/launch; this benchmark
does not separately attribute every cold startup operation. See the existing
[SDK startup report](SDK_STARTUP_OPTIMIZATION_REPORT.md) for its independent profiling.

### First visibility versus handoff (default 150 ms)

| Endpoint | Safe median / mean / P95 (n=10) | Seamless median / mean / P95 (n=10) |
|---|---:|---:|
| Save -> first visible new window | 1558.27 / 1708.50 / 2607.91 | 1466.33 / 1510.17 / 2027.85 |
| Save -> observed restored rectangle | 1589.11 / 1740.05 / 2666.85 | 1489.32 / 1545.63 / 2073.86 |
| Visibility -> restored rectangle | 30.79 / 31.56 / 64.17 | 31.24 / 35.46 / 62.61 |
| Save -> completed handoff | 1589.57 / 1740.89 / 2668.30 | 2475.85 / 2523.10 / 3027.56 |
| Visibility -> completed handoff | 31.25 / 32.39 / 65.29 | 1009.52 / 1012.93 / 1059.99 |
| Gap with neither window visible | 211.17 / 214.73 / 261.59 | 0 / 0 / 0 |

All 20 measured replacements and both warmups retained the expected rectangle.
Candidate creation is observed after `start_candidate` returns, not instrumented at
the exact CreateProcess entry. CSV retains that upper-bound observation and raw trace
events. Safe startup trace covers runtime-path/environment preparation and process
creation; first-visible observation supplies its separate endpoint. Seamless has no
equivalent exact worker-finish/CreateProcess-entry trace here; subtracting observer
timestamps cannot isolate those two functions precisely. No unsupported sub-millisecond
claims are made. This uncertainty does not obscure the ~1 second readiness/handoff
distinction or the absence of an observed Seamless window gap.

### Isolated repeated-operation probes

Nine sequential calls after timing, small default fixture. Median / maximum ms:
source snapshot 1.467 / 2.081; project_options 1.766 / 2.208; current_identity
9.534 / 10.557; runtime_environment **including build_directory lookup** 3.416 /
6.069; owned process/window capture 0.112 / 0.499. These are cached small-fixture
costs, not a large repository estimate and not additional end-to-end stages.
Optimizing all window enumeration cannot explain hundreds of milliseconds here.

## Repeated work, subprocesses and caching

* `gui.cpp::project_options`, `build_directory`, `current_identity` repeatedly load
  options and executable manifests. `current_identity` also re-queries targets for
  its single-target fallback. Call-local reuse is plausible; cross-event caching
  requires project/configuration/selection/options/active-pointer/metadata invalidation.
* Full source fingerprints occur at build dispatch, completed-build acceptance,
  candidate readiness and promotion. These cover edits arriving while compilation
  or shutdown runs. A watcher-generation cache alone is unsafe before the next
  notification has been consumed. Do not remove these checks based on tiny fixtures.
* `workspace.cpp::reusable_target_metadata` validates indexed metadata before and
  after Ninja because Ninja can regenerate CMake. Post-build validation is required.
  `executable_targets` rereads the small derived manifest; no JSON parser subprocess
  is used for an unchanged warm build.
* `process.cpp::runtime_environment` rereads runtime-paths and constructs a child-only
  environment. Cache would need invalidation after reconfigure/library changes;
  inherited/custom environment and DLL lookup must remain intact.
* `application_windows.cpp` enumerates windows for capture/restore; candidate polling
  can query owned process IDs more than once per tick. A callback-local process list
  can be considered, but descendant creation/exit and window churn forbid indefinite
  caching. Placement sampling is already throttled to 500 ms with stable observations.
* Warm worker SDK reuse is active: environment bookkeeping is sub-millisecond, not
  repeated VsDevCmd/PowerShell. Ordinary warm work still uses owned CLI -> CMake
  --build -> Ninja -> compiler/link helper. No repeated explicit configure or File
  API reader was observed in the warm source profiles. Initial startup is a different
  path and can legitimately prepare the SDK.
* Managed-build dependency-cache registration is after the last worker checkpoint.
  Worker exit, output draining, postbuild GUI validation and polling are included in
  the outer build interval, not separately attributed to that registration function.

Prior [link-edge audit](../link-overhead-2026-11-01/REPORT.md) already measured five
traced/untraced pairs: mean link edge 356.20 ms comprised cmd outside helper 149.94,
helper outside tools 59.78, RC 24.77, LINK including cvtres 93.74, MT 24.20, boundary
3.77 ms. These are **historical measurements**, not the current host's breakdown.
The same report measured explicit no-op build median 155.79 ms, zero compilation;
CMake outside Ninja averaged 56.29 ms in its traced subset. Current same-content saves
avoid invoking that path altogether. RC/MT/manifests/debug symbols remain enabled.
No new attempt to replace CMake-generated arbitrary command wrappers was justified.

## Ranked opportunities and parallelism

| Rank | Exact owner / opportunity | Evidence, proposal, expected effect | Risk, complexity and required validation |
|---|---|---|---|
| High for cold alternates | `workspace.cpp::replacement_build_options`, `managed_build` | First inactive tree legitimately needs independent PCH/objects/library compilation. Existing two-tree work already eliminates a third cold tree. Background prewarming could move seconds earlier, not remove required compilation. | Larger proposal: idle scheduling, CPU/disk contention, cancellation and leases; needs approval. No cache copying or concurrent writes. |
| Medium | `gui.cpp::tick`, `poll_watcher`, `poll_auto_run`, `begin_program` | 50 ms timer and sequential lifecycle phases accumulate observable delay. Event-driven worker completion/watch wakeups might remove tens to low hundreds of ms. These are bounds/estimates, not demonstrated savings. | Moderate lifecycle work; marshal to UI thread, coalesce wakeups, preserve cancellation/switch/exit/late output; benchmark all modes before adoption. |
| Medium for explicit 250 ms settings; low at default | `AutoBuildScheduler::settled`, GUI preferences | Existing supported 100 ms preference can reduce debounce; do not confuse setting comparison with new code improvement. Default is already 150 ms. | No new mechanism needed. Preserve user setting. Longer save bursts may dispatch more builds; verify final-edit freshness and failures. |
| Medium potential, not demonstrated | CMake-generated Ninja executable link edge | Current compilation/link dominates worker time; historical wrapper costs are substantial. Persistent helper/direct-Ninja bypass could reduce process overhead. | Backend/custom-command/preset/toolchain compatibility and process ownership risks; larger proposal, no implementation. Never suppress manifests or linker checks. |
| Low on measured fixture | GUI `current_identity`, `project_options`; workspace target manifest | Repeated small reads can use one local snapshot within a callback. Does not justify broad cross-event cache invalidation machinery. | Low-to-moderate; benchmark larger target graphs and settings/target/slot switches; no stale launch. |
| Low/scale-dependent | `ProjectWatcher::source_fingerprints`; `poll_candidate`, `promote_candidate` | Read-only scan can become expensive on large projects. Async stable snapshots could overlap build preparation but must validate unobserved changes before use. | Moderate/high correctness risk. Require large-tree measurements, rename/overflow/during-build/promotion races before changing. |
| Low | `poll_candidate`, `capture_application_windows`, `restore_application_windows` | Reuse process IDs within one callback; placement preparation already overlaps candidate startup and safety hold. | Tiny measured costs; preserve multiwindow/child processes/minimize/DPI; not worth speculative changes. |

Independent compilation already runs under Ninja's worker policy; launching additional
build workers risks oversubscription and lease conflicts. SDK initialization is already
asynchronous and retained once per session. Tool discovery and project inspection are
logically separable, but their small warm costs do not justify thread setup/synchronization.
Build metadata validation cannot safely race a writer without a stable snapshot. Runtime
argument/path preparation could overlap compilation, but the final build graph can change
its DLL paths/target, requiring revalidation. Candidate startup and placement already
overlap the old application's lifetime; old retirement cannot precede candidate readiness
in Seamless without changing its guarantee.

## Changes, failed attempts and release assessment

No application/runtime changes. Test-only additions expose existing debounce settings,
retain warmup timing, distinguish visibility/placement/handoff, benchmark console output
and echo, confirm no-op suppression/rapid-save coalescing, and measure isolated helper
costs. No speculative production experiment was retained or reverted.

Verification executed in `build/release-v0.1.1`:

| Check | Actual result |
|---|---|
| Five disposable project benchmark batches | Passed: 35 measured warm source updates, five header edits, ten reported slot warmups, five no-op checks and five rapid-save checks. Console READY/echo checks executed on its ten loop replacements. |
| Interleaved replacement benchmark | Passed: 10 Safe + 10 Seamless samples, two excluded warmups, all restored rectangles correct. |
| `reliability_gui_flow` | Passed, 67.49 s; includes failures/recovery, stale work, latest edits, input/output and lifecycle behavior. |
| `stage4_watcher` | Passed, 2.10 s. |
| `stage4_scheduler` | Passed, 0.03 s. Three-test combined wall time 69.63 s. |
| Formatting / Git whitespace | Changed C++ formatted; clang-format --dry-run --Werror and git diff --check passed. Existing stage5_tests.cpp CRLF normalization warning is unrelated. |

The final test-only callback scope guard was rebuilt separately; its scheduler check
passed (0.03 s). Its short benchmark smoke also passed: three warm sources, header,
no-op, rapid saves, placement and PCH checks. Results are recorded in
`build/critical-path-trace-guard.log` and `build/critical-path/guard-smoke.log`.
It changes cleanup on benchmark failure only;
the timed data above preceded that guard and was not rerun or substituted.

Reproduce the focused regression from an ordinary PowerShell in the repository:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build\stabilization-focused.ps1 -Build build/release-v0.1.1 -Targets gui_flow_tests -Filter '^(stage4_watcher|stage4_scheduler|reliability_gui_flow)$'
```

The retained ignored controller is an audit helper, not an installed product script.
`build/measure-critical-path.ps1 -Kinds small150 -SkipReplacement` creates a new
owned fixture and records seven samples plus header/no-op/burst checks. Existing
`gui_flow_tests --benchmark-project ROOT 7 150` refuses unmarked projects; do not
mark a real project to bypass that protection.

For manual acceptance, launch `build/release-v0.1.1/EasierCpp-Gui.exe`, open a disposable
native project, Run and move its window, then enable Auto Run and save a visible code
change. Compare Safe and Seamless: Safe can show a brief gap; Seamless should show
the restored new window while the old instance remains briefly protected. Introduce
a compile error: the old application must stay alive. Fix it: replacement should
resume. Same-content saves should produce no rebuild/restart. These manual checks
are instructions, not claimed human acceptance results.

Two harness mistakes were corrected: the first compile used `host.pump()` rather than
the existing free `pump()`; the first medium generator appended target_sources without a
newline. Both were fixed before accepted measurements. The malformed disposable fixture
failed to launch; it is not evidence of a product regression. No user project was touched.

No v0.1.2 readiness claim follows from this focused audit alone. The prior SDK/two-tree
work remains uncommitted; a release still needs the appropriate integration/regression
milestone, real-project acceptance, portable-package validation and explicit version/release
approval. Current executable remains `build/release-v0.1.1/EasierCpp-Gui.exe`; this audit
rebuilt its test harness, not a new product release. No Windows reboot, clean-machine,
mixed-DPI, third-party graphics/assets or Release-managed-project acceptance was performed.
