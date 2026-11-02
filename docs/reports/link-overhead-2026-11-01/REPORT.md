# Targeted link-edge and no-op overhead investigation

November 1, 2026. **No application changes.** Existing incremental linking, A/B
isolation, PCH, manifests, Debug symbols, Release settings and Safe/Seamless code
were retained. Only disposable fixtures, ignored audit helpers and this report/data
were created. FarmGame and other user projects were not inspected or modified.

## Method

Reused the preceding disposable C++20/PCH/static-library fixture and existing
`EASIERCPP_PROFILE_BUILD`/Ninja-edge profiling. Five traced and five untraced warm
source builds were interleaved; another five pairs measured no-op builds. SDK setup
was prepared once per controller, outside measurements, as in the warm GUI path.

An audit-only native observer used a job completion port to identify owned descendant
processes, then GetProcessTimes for creation/exit/CPU time and GetProcessIoCounters
for accounting bytes. It did not inject code, replace RC/MT/LINK, change their
arguments, assign priorities/limits, or add kill-on-close behavior. It retained
process handles until accounting completed. No administrative trace session or new
dependency was installed.

Times are process **lifetimes**, not simply CPU consumption. Parent and child
lifetimes overlap. The decomposition below subtracts children from their parent
intervals; cvtres is nested inside LINK and is not counted again in the total.
All expected processes were observed with no incomplete-trace warnings.

## 1. Link-edge breakdown

Means of five traced warm Debug builds, milliseconds. Means are used here so the
non-overlapping pieces add correctly; a sum of separate medians would not.

| Component | Mean ms | Boundary |
|---|---:|---|
| cmd wrapper outside CMake helper | 149.94 | cmd lifetime minus helper lifetime |
| CMake MSVC helper outside RC/LINK/MT | 59.78 | Helper lifetime minus sequential tool lifetimes |
| RC | 24.77 | rc.exe creation to exit |
| LINK, including its cvtres child | 93.74 | link.exe creation to exit |
| MT | 24.20 | mt.exe creation to exit |
| Remaining Ninja edge boundary overhead | 3.77 | Ninja edge minus cmd lifetime |
| **Complete executable-link edge** | **356.20** | Existing Ninja command timing |

The helper's inclusive mean was **202.49 ms**. The cmd wrapper's inclusive mean
was **352.43 ms**. These are containers, not additional durations to add above.
The cmd residual split into **142.98 ms before helper process creation** and
**6.96 ms after helper exit**. This identifies where the delay occurs, not the
internal OS/security/loader operation causing every millisecond.

LINK's process lifetime was 87.82–104.10 ms, with a median of 93.36 ms. Its cvtres
child averaged 21.34 ms, already included. A whole process lifetime includes
startup/initialization/exit work that LINK's earlier internal `/TIME` intervals did
not fully describe. The prior 47–63 ms internal timing is not a substitute for
the complete link edge.

### Startup and I/O controls

Five harmless no-work controls per tool, measured through process creation/exit
timestamps with the same hidden/redirected style, showed:

| Control | Median ms | Mean ms |
|---|---:|---:|
| cmd /d /c exit 0 | 77.81 | 77.06 |
| cmake -E true | 49.51 | 48.81 |

Neither HKCU nor HKLM Command Processor AutoRun was configured. No registry value
was changed or its content logged. Generic process/tool initialization is therefore
substantial on this host; an AutoRun hook was not the explanation. These controls
do not exactly isolate the link command's child-creation costs or prove a particular
Windows component is responsible.

Mean LINK accounting was approximately 1.38 MB read and 0.23 MB written; RC read
3306 bytes/wrote 2292, and MT read 787 bytes. cmd recorded 549 bytes read. Full
accounting and coarse CPU counters are in tool-io.csv. These are **not physical-disk
latency measurements**: image/mapped-file paging, filesystem filters and activity
in other processes are not fully attributable through these counters. No claim
is made that antivirus, storage or a specific file explains the remaining waits.

## 2. Warm no-op breakdown

All ten measured no-ops performed **zero compile/link commands**. There was no
CMake configuration/regeneration. Untraced end-to-end invocations had a median of
**155.79 ms**, mean **168.31 ms**, range **152.01–203.27 ms**. This is consistent
with the preceding approximately 150 ms observation, with ordinary sample variation.

Mean untraced breakdown from the existing checkpoints:

| Component | Mean ms |
|---|---:|
| CMake --build/Ninja invocation | 80.95 |
| Leases/options/cache identity | 17.62 |
| File API metadata checks before/after | 11.59 |
| Artifact checks before/after | 6.83 |
| Tool discovery | 8.61 |
| Project inspection | 2.58 |
| Prepared environment/configure-argument bookkeeping | 4.78 |
| Launch, unprofiled tail, logging/exit and other residual | 35.34 |
| **Total** | **168.31** |

Values are rounded independently. The residual is a measured subtraction, not a per-function attribution. It includes
work outside the checkpoints and the controller's native invocation boundary.
The configure checkpoint itself was approximately 0.07 ms of skipped-branch
bookkeeping, not an actual configure process.

The traced no-op subset provides the nested process detail: CMake --build averaged
**76.25 ms**, of which Ninja's own lifetime was **19.96 ms** and CMake outside Ninja
was **56.29 ms**. The caller's complete build invocation checkpoint was **85.16 ms**,
including process-creation/wait boundaries around CMake. These traced figures are
a separate sample set and must not be spliced into the untraced additive table.

Ordinary same-content saves are already suppressed by the existing watcher
fingerprints; this experiment deliberately requested no-op builds. It does not show
that every unchanged editor save pays this entire build cost.

## 3–5. Bottlenecks, decisions and before/after

Confirmed contributors are process/wrapper startup and orchestration, rather than
lost incremental-link state, regenerated PCH, or repeated CMake configuration.
RC and MT together averaged about 49 ms; they are not the majority of the edge.

No safe, meaningful application optimization was demonstrated:

- Bypassing CMake-generated shell commands would require preserving arbitrary
  pre/post-build commands, quoting and generator semantics. This is not a small
  proven-safe change based on a single fixture.
- Suppressing RC/MT or changing manifest handling would alter required behavior.
  No such trial or product change was made.
- Bypassing CMake --build for a special Ninja path would add a backend-specific
  execution path. The observed tens of milliseconds do not justify that complexity
  or risks to configurations, parallelism and supported generators here.
- Skipping leases, cache/metadata or artifact verification would weaken reliability.
- Persistent workers or new process architecture were not introduced.

**Application before/after optimization: not applicable; no change was made.**
The paired runs are an observer-overhead control, not an optimization result:

| Existing profile interval, mean ms | Untraced | Traced |
|---|---:|---:|
| Source executable-link edge | 348.40 | 356.20 |
| No-op CMake --build invocation | 80.95 | 85.16 |

Tracing perturbed these intervals by about 8 ms and 4 ms respectively in this small
sample. Do not claim a statistically established overhead constant. The observer
also deliberately drains notifications for 100 ms after root exit; traced
`invocation_ms` includes that bookkeeping and is **not** comparable to ordinary
user-facing build latency. Process timestamps and product checkpoints exclude it.

## 6. Focused verification

- 20 measurement cases completed: five traced/untraced pairs each of no-op and
  ordinary source changes, with unchanged compiler/link settings.
- All ten no-ops had zero compilation and linking; no regeneration was observed.
- All six cached PCH files retained original hashes and modification times.
- Both final Debug slot programs ran and returned their expected distinct values.
- The reused fixture setup also executed the existing A/B live-process, failure/
  recovery and Release correctness assertions. No real graphical application was
  changed. Safe/Seamless GUI code and settings were untouched; their suite was not
  rerun for this profiling-only task.
- Ten valid no-work startup controls completed successfully.
- No incomplete process traces were accepted. Initial audit-helper include/quoting
  issues were corrected before accepting their data; these were not product bugs.
- A profiling-column name collision was corrected from the original process CSVs
  and logs: `ninja_wall_ms` remains the product's CMake invocation checkpoint;
  `ninja_process_wall_ms` is the observed Ninja lifetime. Archived measurements use
  this corrected schema.

No full regression suite was run because application sources were not modified.
No commits or pushes were made. Existing uncommitted prior reports were preserved.

## 7. Remaining opportunities

If further work is warranted, investigate the host-specific **pre-helper process-
creation interval** with a dedicated process/file-latency trace. That could distinguish
loader, console, filesystem-filter and scheduler delays; current counters cannot.
A second representative Windows machine would help separate application overhead
from this host's startup costs. Neither step justifies weakening manifests, symbols,
PCH, A/B isolation, Safe/Seamless readiness or existing reliability checks.

Raw process timelines and tables accompany this report. Full logs and audit-only
helpers remain under `build/edge-overhead` and `build/trace-build-tree.*`. The
owned disposable project/state directory was removed after checking its exact
name and temporary parent; recorded paths describe historical samples. The
production executable was not rebuilt:
`C:\Users\omer2\OneDrive\Desktop\Projects\EasierCpp\build\compile-workers\cpplivelab-gui.exe`.

API references:
[job notifications and descendants](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects),
[process timestamps/CPU](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocesstimes),
[process I/O accounting](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getprocessiocounters).
