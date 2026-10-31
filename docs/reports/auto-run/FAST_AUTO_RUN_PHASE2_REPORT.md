# Fast Auto Run Phase 2: profiling and verified metadata reuse

Date: 2026-10-31. Baseline commit: `ec21f25`. Windows 11, VS2022 Community,
MSVC 19.44, Debug/Ninja. No third-party dependencies were installed.

## Result and implementation

The repository already had one Auto Run control, no Auto Build event handler/menu,
safe automatic launch policy and legacy settings migration. No duplicate UI removal
was necessary. The native controller test now also inspects navigation actions and
Button/Static captions, in addition to checking that retired control ID 116 is absent.
That ID and the shared AutoBuildScheduler/watcher remain intact.

The new optimization removes a redundant post-build `cmake -P file_api.cmake` process
when the indexed graph and all derived metadata remain identical. It does not skip
Ninja/MSBuild or their dependency checks. No compiler flags or process/window lifecycle
logic changed. Both automatic build slots retain separate objects/PDBs/executables.

`file_api.cmake` atomically publishes a manager-owned `file-api.snapshot` after writing
the four manifests. The snapshot records configuration, latest index filename, exact
hex bytes of executables/generator/libraries/runtime-paths and the reader script itself.
The C++ validator requires the matching immutable index, timestamp guards, identical
bytes, a supported generator and parsable target manifest. Missing, legacy, changed,
malformed or unreadable cache data takes the normal reader path. Windows snapshot line
separators are normalized; the captured manifest/script bytes are not normalized.
Cache comparisons are bounded; files above 8 MiB fall back to the reader.

A Ninja/MSBuild-triggered CMake regeneration publishes a new index and therefore forces
a refresh after the build. Selected-artifact existence checks and error 76 remain.
The manager never writes/deletes CMake-owned reply files. This follows
[CMake's immutable indexed reply contract](https://cmake.org/cmake/help/latest/manual/cmake-file-api.7.html).

Added opt-in `EASIERCPP_PROFILE_BUILD=1` diagnostics and benchmark `-ProfileBuild`.
Diagnostics contain numeric durations/counters, never environment blocks. Ninja v5/v6
logs supply command wall durations for newly appended commands only; DLL/import-library
outputs count as one link. Rewritten/unsupported logs report unavailable. Profiling is
silent unless explicitly enabled. Production behavior has no new service or worker.

## Measurements

Before/after runs used separate manager executables and new disposable projects, run
sequentially without concurrent builds/tests. The baseline had profiling added but no
metadata optimization. Its first report used the previous File API reader. Each slot's
first build is cold; subsequent source samples use a warm slot and prepared SDK.

The fixture links a static dependency, verifies that source-only edits retain it,
verifies header-driven rebuilding, intentionally fails compilation and recovers, then
executes with piped interactive input (`Ada 2 3` -> `Ada sum 5 library 43`). All assertions
passed. Three source samples per executable are reported as medians below. These are
small local samples, not a statistically controlled compiler benchmark.

All numbers below are milliseconds. Independent component medians need not sum to
the median total. Windows tick startup estimates have roughly 16 ms resolution.

| Warm source pipeline stage | Before | After |
|---|---:|---:|
| Worker startup estimate | 31.00 | 31.00 |
| CLI dispatch median | 0.00 | 0.00 |
| Project inspection | 1.62 | 1.52 |
| Leases/options/cache checks | 15.32 | 13.94 |
| Installed tool discovery | 77.79 | 69.11 |
| Reused SDK environment | 0.64 | 0.58 |
| Configure arguments/query preparation | 4.17 | 3.60 |
| Explicit configure step | 0.08 | 0.06 |
| Pre-build metadata validation | 3.46 | 4.78 |
| Pre-build artifact/path validation | 3.40 | 2.94 |
| CMake/Ninja build wall time | 1,267.48 | 1,136.25 |
| Compiler command wall time (inside build) | 773.00 | 656.00 |
| Link command wall time (inside build) | 416.00 | 383.00 |
| Build wrapper/dependency-check residual median | 83.09 | 71.22 |
| Post-build File API/metadata work | 54.04 | 5.97 |
| Post-build artifact validation | 3.23 | 2.85 |
| Whole worker invocation | **1,480.85** | **1,272.11** |

The directly attributable improvement is about **48 ms** in post-build metadata work,
with about 1.3 ms extra pre-build validation: roughly **47 ms net** on these source
samples. The whole invocation improved **14.1%**, but compiler/link times also varied
despite unchanged sources/flags; do not attribute the entire 209 ms difference to the
cache. Compiler/linker work remains dominant. Sub-second warm latency was not achieved.
The residual includes CMake/Ninja startup, dependency checks and gaps; those cannot
be separated further by the command log. Compilation/link command wall times include
tool startup and can overlap in multi-source cold/header builds; their sum is not wall
time for parallel builds. Failed commands are not recorded in Ninja's successful-edge
log, so failure compiler time is unavailable rather than inferred as zero work.

| Case (one run unless noted) | Before | After | Result |
|---|---:|---:|---|
| Prepare SDK once | 5,955.35 | 5,513.12 | Success; reused by subsequent workers |
| Direct cold CMake configure | 1,824.57 | 1,472.83 | Success |
| Direct cold compilation/link | 1,663.30 | 1,482.95 | Success |
| Direct CMake/Ninja no-op | 72.39 | 74.16 | No work |
| Managed cold slot A | 3,309.89 | 3,093.39 | Success |
| Managed cold slot B | 3,431.39 | 2,896.76 | Success |
| Managed warm no-op build | 328.95 | 209.99 | Executable timestamp unchanged |
| Source edit (3-sample median) | 1,480.85 | 1,272.11 | Unchanged dependency retained |
| Header edit | 1,963.54 | 1,490.95 | Dependency rebuilt |
| Invalid source | 308.27 | 284.81 | Expected nonzero exit 1 |
| Recovery | 1,738.67 | 1,245.97 | Exit 0 |
| Console startup/input/output/exit | 179.87 | 101.06 | Correct sum and library value |

Numeric input data is preserved in [phase2-measurements](phase2-measurements).
Full per-case logs remain under `build/phase2-measurements` in this workspace.
No-op *saves* and rapid consecutive saves are additionally covered through the real
watcher/controller test, not inferred from a CLI no-op build. Individual rapid-save and
failure/recovery lifecycle stage timings were not separately instrumented.

### Real native GUI replacement

The existing real controller fixture was run separately with each executable, without
concurrent builds/tests. Both slots were warm before three timed source saves. It uses
a local DLL, runtime argument/environment and assets, verifies real window rectangles
and maximization, and deliberately waits **600 ms before window creation**. That delay
is retained in these totals. These timing runs use identical debounce/environment reuse;
they do not repeat the earlier debounce optimization. All lifecycle assertions passed.

| Native event interval (3-sample median, ms) | Before | After |
|---|---:|---:|
| Save -> detection | 31 | 47 |
| Detection -> build dispatch | 266 | 266 |
| Replacement build pipeline | 1,625 | 1,704 |
| Graceful shutdown | 31 | 31 |
| Launch preparation | 63 | 62 |
| Process startup | 93 | 94 |
| Startup -> restored window (includes deliberate delay) | 687 | 672 |
| Save -> process launch | 2,140 | 2,219 |
| Save -> restored window | **2,812** | **2,875** |

Before save-to-window samples: 2,812 / 2,687 / 3,015 ms. After: 2,594 / 3,000 / 2,875 ms.
The median native total was **63 ms (2.2%) slower** in this small sample, with overlapping
ranges and variable build time. Thus this run does **not** demonstrate an end-to-end
native speedup; the demonstrated improvement is the isolated metadata phase. Compilation,
linking, process scheduling and filesystem variability can exceed this small saving.
No compiler/window/shutdown changes were made to force a better number. All samples,
including the slower ones, are retained in `phase2-measurements/native-timings.csv`.

## Architectural decisions and remaining costs

- Reused the existing prepared toolchain environment; no repeated warm VsDevCmd setup.
  Discovery still checks installed tools per worker (roughly 69 ms in this run).
- Reused existing guarded pre-build metadata reuse, strengthening its validation and
  extending it after builds. CMake reconfiguration remains driven by configuration
  changes and normal generator regeneration.
- Reused the existing alternating slots. Sharing raw objects/PDBs across CMake trees
  would risk dependency/configuration correctness, so it was not introduced.
- No persistent worker: about 31 ms startup is small relative to compiler/link work;
  measurements do not justify a new long-lived ownership/protocol subsystem here.
- Graceful shutdown, launch preparation, debounce and window restoration are unchanged.
  No shorter safety timeout, skipped dependency check or unverified executable launch.

## Verification scope and limitations

Executed final checks:

- Quick: **35/35 passed**, 7.33 seconds, four CTest workers.
- Permitted local Full selection: **51/51 passed**, 238.07 seconds, four workers.
  This includes the real GUI flow, restart, selected-target/artifact freshness,
  Debug/Release, Ninja/Multi-Config/VS2022 presets, local libraries, process ownership,
  console I/O, watcher/scheduler, settings and Stage 1 tool-discovery regressions.
- Doctor after that Full run: exit **0**, CMake/Ninja/MSVC/Clang found; GCC absent
  and not required. Ordinary-terminal discovery is also covered by doctor_without_path.
- Focused metadata/scheduler retest: **2/2 passed**, 89.33 seconds. The artifact-freshness
  check also passed separately (46.27 seconds), then passed in the final local Full run.
- Baseline CLI version: **1/1 passed**. Before and after CLI benchmark scripts exited
  **0**, including their deliberately failing compilation and successful recovery.
- Baseline isolated GUI flow: exit **0**, actual first/repeated placement restoration,
  maximization, rapid saves, no-op save and console assertions passed.
- Optimized isolated GUI flow: exit **0**, the same lifecycle assertions and the added
  checks for obsolete Auto Build controls/captions/navigation actions passed.
- Scoped clang-format and diff whitespace checks passed; benchmark PowerShell syntax
  parsed without errors. Full-suite and Quick logs were preserved under
  `build/phase2-measurements/full-LastTest.log` and `quick-LastTest.log`.

Focused metadata tests exercise unchanged-cache reuse, missing generator/library files,
malformed and valid-looking damaged target data with old/fresh timestamps, corrupt DLL
paths, missing/truncated snapshots, actual build-time CMake regeneration, renamed
executables and a relocated local DLL consumed through both refreshed and cached data.
The scheduler test also checks opt-in profiling, exclusion of historical Ninja commands,
DLL/import-library deduplication, no-op logs and rewritten-log rejection.

The first new cache test failed because the snapshot had native Windows CRLF separators
while the comparison expected LF. This was corrected; exact manifest bytes remain
checked. The artifact-freshness test passed during that failed focused run.
One attempted Full invocation used a regex feature unsupported by CTest and selected no
tests; the final run uses the established explicit local-only selection instead.

Scoped formatting passes for all six changed C++ files and `git diff --check` passes.
Repository-wide `format.ps1 -Check` still fails at the pre-existing untracked unfinished
`include/cpplivelab/performance_timer.hpp`. Its SHA256 remains
`0A37123E9D7BC5F46FD866100FED34D09377C50FC9E4A3D362C8302C9F0F4F96`.
That header and the four pre-existing deleted organizer scripts remain untouched.

Network-dependent `stage5_unified_raylib`, `stage5_libraries` and optional
`stage5_live_libraries` remain deferred under the existing local-only restriction.
FarmGame was not modified or rebuilt. Physical mixed-DPI/multi-monitor acceptance and
real-project behavior remain manual checks. There are no new dependencies, permanent
PATH changes, stock Graphify hooks/skills, cloud uploads or remote Git writes.

See [the numbered manual guide](../../manual-tests/auto-run/FAST_AUTO_RUN_PHASE2_MANUAL_TESTS.md)
for the exact launch/build/test commands and expected behavior.
