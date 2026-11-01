# Early Seamless placement checkpoint — 2026-10-31

## Completed scope

Checkpoint 1 is implemented and verified. Existing Safe and Seamless remain the only
replacement modes. The concurrency review for checkpoint 2 found no measured reason
to add threads. Worker-count preferences and Optimize Workers (checkpoints 3–4) are
deferred; they have not been implemented. Final worker/UI acceptance is also pending.
This is a completed placement checkpoint, not completion of the entire worker request.

Previously, placement waited behind the one-second root-survival readiness gate.
Seamless now restores the candidate on the first visible-window poll while the old
application remains running. The one-second survival and 250 ms visible-window checks,
15-second timeout, job ownership, candidate cleanup and old-process protection remain.
During startup, already-restored windows are checked for changed geometry: an
application's delayed recentering is corrected; unchanged geometry is not reapplied.
Promotion clears the previous session's restoration bookkeeping, preventing a second
restoration after the old process closes. Placement Off remains Off.

This uses the existing visible-window enumeration and UI timer. It does not intercept
window creation, inject code, hide user windows, modify application sources, or guarantee
positioning before the first rendered frame. The first visible frame may still appear
at application-selected coordinates before the next manager poll. An application that
explicitly repositions itself after the handoff can still move its own window.

## Executed checks

Initial build in `build/window-management` failed with Windows LNK1168 because the GUI
was open. The running instance was left untouched. A complete separate build succeeded
in `build/early-placement`.

Five focused regressions passed, **5/5 in 90.64 s**:

- `redirection_unit` — preference migration, persistence, isolation, geometry checks.
- `redirection_windows` — existing actual Win32 restoration.
- `stage5_pch` — incremental/PCH/compiler regressions.
- `reliability_gui_flow` — existing Safe, manual lifecycle, console/runtime/PCH workflows.
- `window_management` — Seamless success/failure/timeout/rapid-save/Stop/preferences,
  plus new early placement and simulated application recentering checks.

The two slow-start native checks observed correct placement **15 ms and 16 ms after
first window detection**, with the old process alive and the candidate not yet ready.
Both deliberately moved the candidate again during startup and verified correction
before handoff. These are message-pump observations, not display-frame instrumentation.
The earlier implementation had a one-second gate; no instrumented pre-change
visible-to-placement measurement was collected, so no precise before/after placement
speedup is claimed.

Doctor passed. Changed C++ passed clang-format dry-run; `git diff --check` passed.
Full regression, dedicated visual desktop audit, physical multi-monitor/mixed DPI and
worker optimization tests were not run in this checkpoint. The earlier environment
inventory exposed one display. No additional tools or dependencies were installed.

## Unchanged timing boundaries and repeated benchmark

The existing `--benchmark-replacement` ran successfully: 10 Safe + 10 Seamless samples,
with two excluded slot warmups, alternating modes/order on the same disposable C++20
Debug native fixture. Headers are unchanged; each run saves a unique source comment.
No PCH recipe, external library or artificial startup delay is present in this benchmark.

First visible window is distinct from complete managed handoff. The latter includes
Seamless readiness, old-job retirement and promotion. Build includes child-worker
overhead. Overlapping stages are not added together. Windowless gap is sampled by the
message pump; zero means no gap observed.

| Median, milliseconds | Previous Safe | Current Safe | Previous Seamless | Current Seamless |
|---|---:|---:|---:|---:|
| Save → first visible window | 1322.955 | 1306.045 | 1214.580 | 1339.265 |
| Save → complete managed handoff | 1364.795 | 1353.930 | 2231.485 | 2362.745 |
| Build | 798.779 | 781.093 | 892.057 | 1011.695 |

| Current average, milliseconds | Safe | Seamless |
|---|---:|---:|
| Save → first visible window | 1315.756 | 1324.286 |
| Save → complete managed handoff | 1358.098 | 2339.537 |
| Build | 790.652 | 988.911 |
| Windowless gap | 218.659 | 0 |

There is **no demonstrated overall cycle-speed improvement**. Seamless samples also
had higher compilation time in this run; these separate batches do not isolate why.
The targeted improvement is placement ordering, with the old process still alive.
Previous raw data is retained in `REPLACEMENT_MODE_TIMINGS.csv`; current data is
`EARLY_PLACEMENT_TIMINGS.csv`. Full current benchmark log:
`build/window-management-logs/early-placement-benchmark.log`. Test and Doctor log:
`build/window-management-logs/early-placement-isolated.log`. Per-test placement output:
`build/early-placement/Testing/Temporary/LastTest.log`.

## Concurrency assessment and remaining work

Toolchain discovery/environment preparation already runs in `std::async`, with its
prepared environment reused. Owned CLI build workers and asynchronous process pipes
keep the GUI responsive. Ninja schedules compilation; slot leases and the single GUI
lifecycle controller coordinate ownership. Placement sampling and candidate readiness
run on the existing timer. No additional threads or asynchronous cleanup were added.
The single-source benchmark does not demonstrate a benefit from extra compile jobs.

`src/projects/workspace.cpp` currently delegates to `cmake --build` without a selected
job count. Existing `scripts/build.ps1 -Jobs` and GUI test-worker settings control CTest
parallelism; they are not compilation-worker preferences. Future checkpoint 3 needs a
separate persisted compile limit passed to subsequent CMake builds, with a CPU/memory
derived range (not a fixed 16 maximum). Checkpoint 4 then needs isolated multi-unit and
incremental workloads, sequential candidate trials, median selection, cancellation,
failure rollback, resource limits, progress and manual override. No recommendation has
been benchmarked or selected yet.

## Changed files and manual acceptance

Changed maintained C++: `include/process/application_windows.hpp`,
`src/process/application_windows.cpp`, `src/gui/gui.cpp`,
`tests/gui/gui_flow_tests.cpp`. Documentation: this report,
`docs/development/WINDOW_MANAGEMENT.md`, current timing CSV. All prior modifications
were retained; no unrelated user projects, permanent PATH, commits or pushes changed.

1. Close the old EasierCPP instance yourself when ready. Launch
   `build/early-placement/cpplivelab-gui.exe`.
2. Open a disposable graphical CMake project. Settings → Preferences: choose Session
   or Between Sessions, and the existing Seamless option. Run and move its window away
   from the center. Enable Auto Run and save a real source change.
3. Expect the replacement to be positioned over the old application promptly during
   startup; the old application remains until readiness passes. Check for movement
   after the old window closes. The first-frame limitation above still applies.
4. Test maximized placement, then Placement Off. Expect maximization to be retained;
   Off should let the application choose its own geometry.
5. Introduce a compile error; expect the old process to survive. Repair and save.
   Select Safe and repeat to check existing behavior. Exercise Stop/Run and Restart.
6. Repeat on actual projects and additional monitors if available. Current worker
   controls and Optimize Workers are not part of this executable yet.

Exact updated GUI:
`C:\Users\omer2\OneDrive\Desktop\Projects\EasierCpp\build\early-placement\cpplivelab-gui.exe`.
