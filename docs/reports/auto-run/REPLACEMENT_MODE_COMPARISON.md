# Safe versus Seamless — 2026-10-31

Executed `gui_flow_tests.exe --benchmark-replacement` successfully: ten measured
replacements per mode, twenty total, plus two excluded Safe warmups to configure both
A/B slots. The same disposable C++20 Debug Win32 project was used throughout, with
unchanged headers and a unique source-comment save for each incremental build. No
artificial startup delay, external library, or PCH recipe was used. Modes alternated
in pairs, reversing order between pairs to reduce ordering bias. This is a simple
windowed fixture, not a FarmGame benchmark.

The real controller, watcher, debounce, managed CMake/Ninja worker, executable launch,
window restoration and process retirement ran. Every completed replacement retained
the requested rectangle. Original user projects were untouched. Changes for this task
are confined to the reusable test-only benchmark and this report/data.

Timings use `std::chrono::steady_clock`. Window state is sampled by the existing test
message pump, approximately every 5 ms, while GUI work runs on its normal timers.
Window visibility is a Win32 visibility observation, not proof of the first rendered
frame. Zero gap means no windowless interval was observed, not sub-millisecond display
instrumentation. Counts/timings include real system scheduling variability.

Definitions:

- **New window:** source save starts → first replacement window observed visible.
- **Active handoff:** source save starts → old window gone, candidate promoted to the
  active managed session, matching placement restored. Seamless includes its existing
  one-second root-survival readiness period. Safe includes stop-first launch/preparation.
- **Gap:** positive interval from observing the old window absent to observing the new
  window visible. Overlap is reported as zero gap.
- **Build:** controller build-start event → controller observes replacement worker done;
  includes worker/process overhead, not just compiler CPU time.

All values below are milliseconds.

| Run | Safe new window | Seamless new window | Safe active handoff | Seamless active handoff |
|---|---:|---:|---:|---:|
| 1 | 1298.17 | 1249.97 | 1310.39 | 2282.71 |
| 2 | 1346.41 | 1215.45 | 1410.34 | 2250.78 |
| 3 | 1575.05 | 1166.19 | 1623.48 | 2146.53 |
| 4 | 1332.18 | 1213.71 | 1355.62 | 2179.50 |
| 5 | 1636.15 | 1166.14 | 1706.26 | 2146.98 |
| 6 | 1270.51 | 1219.51 | 1298.93 | 2248.71 |
| 7 | 1288.61 | 1248.58 | 1303.70 | 2214.26 |
| 8 | 1274.71 | 1146.44 | 1297.51 | 2113.17 |
| 9 | 1313.73 | 1290.53 | 1373.97 | 2294.68 |
| 10 | 1373.48 | 1211.80 | 1443.75 | 2266.99 |
| Median | 1322.955 | 1214.580 | 1364.795 | 2231.485 |
| Average | 1370.900 | 1212.832 | 1412.395 | 2214.431 |

| Additional metric | Safe median | Safe average | Seamless median | Seamless average |
|---|---:|---:|---:|---:|
| Window gap | 219.684 | 215.254 | 0 | 0 |
| Build | 798.779 | 854.755 | 892.057 | 880.734 |

Interpretation: in this small sample, Seamless exposed the new window about 108 ms
earlier by median, while avoiding the roughly 215 ms average blank-window interval.
Compilation was similar and was not accelerated by Seamless. Full active handoff was
about 867 ms slower by median because Seamless deliberately waits for startup survival
before retiring the old application. This comparison does not establish performance
for a graphics engine, slow resource initialization, exclusive resources, or other
machines. It does not justify removing the readiness check.

Raw measured samples: `REPLACEMENT_MODE_TIMINGS.csv`. Warmups and complete output:
`build/window-management-logs/replacement-benchmark.log`. Build and focused regression
log: `build/window-management-logs/replacement-benchmark-build.log` (`redirection_unit`
1/1 passed, 0.16 seconds). Changed test C++ passed clang-format dry-run; `git diff
--check` passed. No production application behavior changed; no full regression rerun
was needed for the added opt-in benchmark branch. No commits or pushes were made.

Repeat from the repository after building the existing test target:

```powershell
.\build\window-management\gui_flow_tests.exe --benchmark-replacement
```
