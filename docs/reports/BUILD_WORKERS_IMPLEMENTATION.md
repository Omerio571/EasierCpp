# Parallel compilation workers — 2026-11-01

## Implemented

All three requested checkpoints are complete: manual worker preferences, controlled
Optimize Workers with progress/cancellation, and focused/integration/performance
verification. The existing Seamless replacement implementation was not changed.

Settings → Preferences retains Window Placement and Auto Run Replacement Mode first,
then shows Current Workers, Recommended Workers and Optimize Workers. Numeric selection
saves immediately. One means sequential native build jobs; higher values are maximum
concurrency. There is no additional parallelism switch. A compatibility selection
preserves/restores the build-system default. New installations without existing managed
project metadata initialize a safe automatic count; existing workspaces retain their
previous native/preset/environment behavior until a selection is made.

The maximum uses Windows logical processors across processor groups and available RAM,
reserving 1 GiB and budgeting 768 MiB per compiler process. There is no fixed maximum
of 16. Pure policy tests cover 64/128 processors, low memory, and bounded high-core probe
workloads. Hardware of that size was not physically benchmarked here.

Both managed and legacy/single-file CMake paths snapshot preferences at build entry,
then append `--parallel N` to `cmake --build`. This covers manual, automatic, Debug,
Release and A/B builds without changing compiler flags, PCH/cache identity or source
configuration. Existing leases and the authoritative GUI process controller remain.
No additional internal threads/thread pool or speculative asynchronous work was added.

## Optimization and safety

The GUI reuses the existing owned, cancellable process progress runner with a benchmark
title/status. A separate CLI child prepares a standard-library C++20 static-library
workload in a newly reserved manager-owned directory. It uses discovered CMake/Ninja
and the Visual Studio developer environment, without installing/downloading anything.

The workload scales from 12 to 64 independent translation units. Candidate counts are
1, powers of two, and the safe CPU/memory/workload limit; candidates run sequentially.
Each runs three clean builds and three single-file incremental builds. Median clean
and warm durations receive equal weight; a larger count must improve the best score
by at least 3%. The 64-unit probe bound controls duration, not the manual selector's range.

Only the benchmark's own outputs are cleaned. The GUI remains responsive, shows the
candidate/trial, and refuses optimization while a program/build is owned. The run is
bounded to ten minutes. Increased memory pressure can abort trials. Cancellation,
process error, invalid result or pending manager close/restart preserves the previous
count. Apply/save happens in the GUI only after successful completion. Temporary files
are removed after the owned task drains. The last CSV is retained beside global worker
settings. Manual override remains available and retains the last measured recommendation.

## Final measured result

Test environment: 12 logical processors, approximately 15.8 GiB physical RAM. Available
memory limited this run to three workers. The controlled workload used 12 translation
units. These are medians of three trials, seconds:

| Workers | Clean compilation | One-file incremental | Weighted score |
|---|---:|---:|---:|
| 1 | 7.561 | 0.903 | 4.232 |
| 2 | 4.260 | 0.893 | 2.577 |
| **3 — recommended** | **2.970** | **0.871** | **1.921** |

Three workers reduced measured clean-build time by **60.7% (2.55× throughput)** versus
one. The small warm difference does not establish a reliable warm Auto Run improvement.
Parallelism cannot keep multiple compiler jobs occupied when only one source needs
compilation. No whole save-to-launch/handoff improvement is claimed. Clean means compiler
outputs were removed; OS disk caches were not flushed. Configure, environment preparation
and cleanup are outside the timed build interval; build-process overhead and archiving
are inside it. Different projects/compilers/memory loads may favor another count.

Raw results: [BUILD_WORKERS_BENCHMARK.csv](BUILD_WORKERS_BENCHMARK.csv).

## Executed verification

- Worker preference checkpoint: 3/3 passed, 82.94 s (unit, PCH, existing GUI flow).
- Initial native optimizer check: 2/2 passed, 58.40 s, including cancellation, failed
  child, successful recommendation and override. Doctor passed.
- An intermediate local Full run found one new test assertion checking a deliberately
  cleared system console. The assertion was moved immediately after the build. This
  was a test-location defect, not an observed compilation-worker failure.
- Corrected local **Full selection: 54/54 passed, 330.00 s**, including all 35 Quick
  tests. Doctor passed. Network-dependent `stage5_unified_raylib` and `stage5_libraries`
  were excluded under the existing restriction; optional live-library tests were not run.
- After final UI refinements, native forms/layout/audit/worker tests: **4/4 passed,
  90.38 s**, Doctor passed. Cancellation was strengthened to occur during active
  compilation, with progress visible. Recommendation selector refresh was checked.
- After scaling the bounded probe: **2/2 passed, 70.12 s**, Doctor passed.
- Final readable generated C++/CMake and actual optimizer execution: **1/1 passed,
  66.52 s**, Doctor passed. This final run produced the table above.
- Changed maintained C++ passed clang-format dry-run; `git diff --check` passed, with
  only existing line-ending normalization notices.

The Full run precedes the final optimizer-only/UI refinements; those refinements were
rebuilt and covered by the subsequent focused runs. Full logs are in
`build/window-management-logs/compilation-workers-*.log`. The exact local Full selection
is `compilation-worker-local-selection.txt` in that folder.

The GUI flow checks real Debug/Release builds with selected limits and switches counts
during warm A/B cycles while verifying existing `.pch` timestamps remain unchanged.
Native tests click Preferences/Optimize controls, cancel during compilation, inject a
failed benchmark child, check immediate persistence/native-default restoration, apply
the recommendation, then override it. Owned benchmark directories are checked empty.

Rendered actual native Preferences/recommendation and progress controls were inspected:
`build/compile-workers/worker-preferences-recommended.png` and
`worker-optimizer-progress.png`. Existing shared-form theme/layout tests passed. Real
user-project acceptance and physical high-core/multi-monitor testing remain manual.

## Files and limits

New core: `include/build/build_workers.hpp`, `src/build/build_workers.cpp`.
Build integration: `src/projects/workspace.cpp`, `src/projects/projects.cpp`.
UI: `src/gui/gui.cpp`, `include/gui/project_dialogs.hpp`, `src/gui/project_dialogs.cpp`.
Reusable named progress: `include/gui/library_workflow.hpp`, `src/gui/library_workflow.cpp`.
CLI dispatch: `src/main.cpp`. Build/test registration: `CMakeLists.txt`.
Tests: `tests/process/redirection_tests.cpp`, `tests/gui/gui_flow_tests.cpp`.
Documentation: `docs/development/BUILD_WORKERS.md`, architecture map, this report and CSV.

The optimizer requires the discovered Windows C++/CMake/Ninja environment. Its synthetic
recommendation is not project-specific. Other generators use CMake's native mapping;
unsupported generators may ignore parallelism with a CMake warning. A job limit does
not constrain arbitrary nested builds or compiler threads requested by user flags.
Available-memory limits can change between runs. No separate cold/warm settings or
additional concurrency mechanisms were introduced. No requested feature is left partial.

Previous uncommitted work, user projects, libraries, permanent PATH and system settings
were preserved. No commits or pushes were made.

## Manual acceptance and executable

Follow [the numbered guide](../development/BUILD_WORKERS.md#manual-acceptance). Close the
previous manager instance yourself when ready, then launch:

```powershell
& "C:\Users\omer2\OneDrive\Desktop\Projects\EasierCpp\build\compile-workers\cpplivelab-gui.exe"
```

To repeat the focused automated optimizer checks after building:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -BuildDirectory build/compile-workers -TestMode Full -TestFilter '^(compilation_workers|redirection_unit)$' -Jobs 1 -Doctor
```

`-Jobs` here controls CTest concurrency, independently of compilation workers.
