# Phase 3 Auto Run: measured performance and architecture evaluation

Baseline: `7a458ff`, Windows 11, VS2022 Community, MSVC 19.44.35228, x64 Debug,
CMake 3.31.6-msvc6/Ninja. The working tree was clean before this task.

## Shipped change versus project-specific experiment

The application now avoids a repeated vswhere process in an already prepared build
worker. It validates the absolute VS installation, matching setup script, selected
default toolset/compiler, x64 host/target and populated INCLUDE/LIB before reusing that
instance. Files are checked every time; missing/inconsistent hints fall back to normal
discovery. Doctor still performs fresh discovery. Standalone CMake/Ninja/compiler search
and child-only PATH setup are retained. No SDK environment is logged or persisted.

FarmGame-copy tool discovery fell from **82.64 to 8.43 ms median** (maximum 138.90 to
9.62 ms). This is the directly attributable production saving, approximately **74 ms**.
End-to-end samples also varied in compilation, linking and process/window scheduling;
do not attribute every measured difference to this small change.

Stable-header PCH proved substantially faster in copied FarmGame projects. A tested,
optional [FarmGamePch.cmake](../../experiments/auto-run/FarmGamePch.cmake) is retained for
project-owner review/use. It is **not automatically enabled in the original FarmGame**
or injected into arbitrary projects. No original FarmGame source, CMake, asset or layout
was changed. That preserves the existing-project edit/approval boundary.

## Reproducible workload and scope

`benchmark-farm-auto-run.ps1` copies only CMakeLists.txt, Source, Include and Assets to a
new marked temporary FarmGame directory. It verifies original input SHA256 values after
the run and removes only its owned copy. The local folder is not a Git checkout.

The disposable consumer compiles all eight actual game sources with C++20, the original
include paths and the existing local Debug raylib library/headers from
`FarmGame/out/build/x64-Debug/_deps/raylib-build/raylib`. System libraries match the
original link line. No FetchContent download or third-party CMake script executes.
The fixture replaces source-generating CMake/FetchContent with a reviewed local imported
target and preserves source glob verification and the post-link asset copy. It therefore
measures actual game compilation/runtime, not a dependency download/cold raylib rebuild.

The real Win32 controller, watcher, scheduler, build workers, leases, runtime startup and
window restoration run in the existing GUI test host. After a manual Run, the game is
moved/resized; every replacement must preserve its rectangle. Two automatic slots are
warmed before ten measured source-comment saves alternating A/B. There is also one
initial cold manual Run and one project-header comment edit. No artificial 600 ms
window delay is present in FarmGame (unlike the older native test fixture).

Every run uses a new isolated manager state/registry test key. Games stop gracefully;
no unrelated process is closed. Cases run sequentially without concurrent tests/builds.
Source contents and compiler/configuration are consistent between variants. Temporary
sources are outside OneDrive synchronization, so real-folder synchronization/antivirus
effects are not reproduced. Comment edits force real compilation and PDB/link updates;
larger semantic edits or different project workloads can cost more.

P95 uses nearest rank; for ten samples it equals the maximum. Medians average the two
middle values for even counts. These small sequential samples are exploratory, not a
randomized statistical trial or a guarantee against future outliers. Tick-based GUI
stages have roughly 16 ms resolution; worker phases use a higher-resolution clock.
Visible/restored window readiness is measured, not completion of every rendered frame
or game-state initialization task. Memory is sampled maximum single build-child peak
working set, not aggregate system RAM or the cold PCH generator's memory peak.

## FarmGame results

All values are milliseconds. Each warm row has ten successful replacements.

| Variant | Save -> restored window median | P95 / max | Replacement build median |
|---|---:|---:|---:|
| Preserved baseline | 2,827.5 | 3,328 | 1,969 |
| Shipped discovery reuse | **2,578** | **2,703** | **1,703** |
| Discovery reuse + optional PCH copy | **1,929.5** | **2,140** | **1,070.5** |
| Discovery reuse + forced full link, no PCH | 3,133 | 3,859 | 2,117 |

Shipped end-to-end median improvement: **8.8%**. Optional PCH improved the candidate
median a further **25.2%**, or **31.8% versus baseline**, in the copy. This PCH result is
not the default performance of an untouched real FarmGame project.

All samples and per-case stage profiles are in [phase3-measurements](phase3-measurements).
`farm-before-complete`, `farm-after`, `farm-pch`, `farm-full-link` identify the four rows.
The earlier ten-sample pilot is also preserved as `farm-pilot-timings.csv`; its median
was 2,813 ms and maximum 3,141 ms, but its first detailed console capture was incomplete.
The corrected baseline above is the comparison dataset. Raw logs remain under
`build/phase3-measurements` in this workspace.

### Pipeline breakdown (independent warm medians)

| Phase | Baseline | Shipped | Optional PCH copy |
|---|---:|---:|---:|
| Save -> detection | 23.5 | 24 | 15 |
| Detection -> build dispatch | 288.5 | 265 | 273.5 |
| Worker startup estimate | 47 | 47 | 47 |
| Project inspection | 5.50 | 5.54 | 5.64 |
| Tool discovery | 82.64 | 8.43 | 8.27 |
| Reused SDK environment | 0.65 | 0.63 | 0.64 |
| CMake/Ninja build wall | 1,718.60 | 1,552.45 | 906.67 |
| Compiler command wall | 906 | 875 | 251 |
| Link + post-build command wall | 583 | 531.5 | 530 |
| Post-build metadata validation | 5.09 | 5.11 | 5.05 |
| Previous-process shutdown | 148.5 | 125 | 140.5 |
| Slot activation | 15 | 15.5 | 7.5 |
| Launch preparation/validation | 62 | 47 | 54.5 |
| New-process startup | 125 | 109.5 | 109.5 |
| Process started -> restored visible window | 281 | 250 | 234 |

Build-wall includes startup, generator glob/dependency checks, compiler/link commands
and gaps; compiler/link rows are subsets and must not be added again. The link edge
includes CMake's MSVC link/manifest wrapper and POST_BUILD copy. Copying was measured
separately below; it is not presented as an independently timed in-pipeline edge.
Artifact-before/after validation, configure arguments, leases/cache checks and explicit
configure timings are retained in the CSV profiles. Launch preparation includes fresh
artifact/target/lease/runtime checks; no separate pure validation duration is claimed.
Independent medians do not necessarily sum to median total.

### Alternating slots

Each slot has five measured warm samples; no objects/PDBs/ILKs were shared.

| Variant | Slot A median / max | Slot B median / max |
|---|---:|---:|
| Baseline | 2,859 / 3,235 | 2,796 / 3,328 |
| Shipped | 2,578 / 2,703 | 2,578 / 2,594 |
| Optional PCH copy | 1,906 / 1,953 | 1,969 / 2,140 |

First automatic-slot warmups were excluded from statistics; their individual cold
timings were not exported. Initial manual cold and warm A/B measurements are exported.
No 13–21-second warm stall occurred in these samples; absence in this sample cannot
prove such stalls are impossible.

## Asset copying

FarmGame currently has **5 assets totaling 2,627 bytes**. Ten unchanged-copy subprocess
measurements per command produced:

| Command | Median | P95 / max |
|---|---:|---:|
| copy_directory | 53.37 | 61.39 |
| copy_directory_if_different | 49.66 | 56.23 |

The new command preserves unchanged destination timestamps; existing copy_directory
rewrites them. Both correctly propagated changed bytes and a newly added file. Both
**retain destination files whose sources were deleted**: these are merge operations,
not mirroring. This deletion behavior was explicitly tested. No automatic deletion or
cleanup of user outputs was added. The command startup cost dominates these tiny files.

The approximately 3.7 ms difference does not justify changing the original FarmGame
configuration. The experimental `-IncrementalAssets` fixture option uses the newer
command only for CMake >=3.26 and falls back for older supported CMake. See
[CMake 3.26 release notes](https://cmake.org/cmake/help/latest/release/3.26.html).
If assets grow substantially, rerun the copy and full-pipeline experiments; changing
deletion behavior requires an explicit ownership/synchronization design.

## PCH tradeoffs

Only `<string>`, `<vector>`, `<iostream>`, `<cmath>`, `<raylib.h>` and `<raymath.h>` were
precompiled. Frequently edited Game/Config/etc. project headers were not included.
CMake owns separate PCH artifacts for each target/configuration/build tree, following
[target_precompile_headers](https://cmake.org/cmake/help/latest/command/target_precompile_headers.html).
No cross-target/build-tree REUSE_FROM or manual object/PDB copying is used.

| Measurement | Shipped, no PCH | Optional PCH copy |
|---|---:|---:|
| Initial manual Run -> visible window | 9,672 ms | 9,625 ms |
| Initial SDK setup component | 4,777.86 ms | 4,709.30 ms |
| Initial explicit configure | 1,518.50 ms | 1,581.37 ms |
| Initial Ninja build wall | 2,445.40 ms | 2,597.40 ms |
| Warm compiler command median | 875 ms | 251 ms |
| Project-header edit Ninja wall | 1,945.79 ms | 1,159.14 ms |
| Project-header edit compiler-command sum | 6,824 ms | 2,164 ms |
| Warm maximum-child peak working-set median | 110,888,960 bytes | 64,385,024 bytes |
| PCH disk across manual + two automatic trees | 0 | 203,685,888 bytes |
| Linked PDB disk across those trees | 14,360,576 bytes | 15,032,320 bytes |

Cold compilation is slightly slower in wall time because generating PCH precedes the
other compiles; aggregate compiler work falls but parallel sums are not elapsed time.
Cold overall differences also include variable SDK/startup work, not a cold speedup.
PCH adds about **67.9 MB (64.75 MiB) per build tree**. Warm sampled compiler memory fell;
the cold PCH generator's peak RAM was not measured. PDBs/debug flags remain enabled.

The same retained snippet compiled and executed a local raylib-header consumer in
**Debug and Release**. Actual FarmGame end-to-end PCH tests used Debug. Full FarmGame
Release/debugger-symbol verification is not claimed: only an existing Debug raylib
binary was available. VS2022/Ninja/Multi-Config compatibility remains regression-tested
for existing workflows; PCH-specific VS multi-config debugger acceptance remains manual.

## Incremental linking and remaining overhead

Original Debug flags already contain `/Zi` and `/debug /INCREMENTAL`. Each slot retained
its own .ilk and PDB. Default incremental runs left approximately 9.1 MB of ILK files
across three trees; forced `/INCREMENTAL:NO` left none. `/VERBOSE:INCR` was requested in
the fixture; no fallback diagnostic lines were surfaced by the CMake wrapper, so an
uninterrupted internal incremental-link decision for every sample is not asserted.
The recorded comparison is command wall time and retained state, not undocumented
linker internals. See [MSVC incremental-link documentation](https://learn.microsoft.com/en-us/cpp/build/reference/incremental-link-incrementally).

Forcing full linking increased link/post-build median from 531.5 to 648.5 ms and
end-to-end median from 2,578 to 3,133 ms. Compilation/startup also varied in that later
run, so the whole difference is not attributable to the linker. Keep existing flags,
PDB ownership and slot state; do not disable debugging to chase a benchmark.

Warm explicit CMake configuration remained effectively zero. Existing Phase 2 File API
reuse remains around 5 ms. Ninja's generator glob/dependency checks are necessary and
were retained. The CLI currently inspects a project before managed_build also inspects
it; a core scan is around 5.5 ms here. That duplication was identified but left intact
because its small saving did not justify changing validation ownership in this pass.
No persistent worker, global compiler flags, debounce change or query/check bypass was
introduced. Next significant costs after PCH are linking, shutdown/window startup and
correctness-preserving scheduling, not SDK setup.

## Build-slot architecture evaluation

**A — Background inactive-slot synchronization:** not installed. It would move some
compilation off the next foreground path but add a complete dependency-check/link pass
after each switch, plus CPU/disk contention while editing or rendering. The experiment
below shows reducing foreground compiles alone is not sufficient evidence of a latency
win. No background-sync timing claim is made; this is a work/ownership assessment.

**B — Optional compiler cache:** sccache is not installed, so no cache hit rate or speedup
is claimed. Its [MSVC documentation](https://github.com/mozilla/sccache) requires careful
PDB handling (typically /Z7 or per-object /Fd with /Zi), and cross-build paths affect
cache identity. [Configuration documentation](https://github.com/mozilla/sccache/blob/main/docs/Configuration.md)
describes base-directory normalization. A reviewed, optional local-only installation,
debugger tests and measured cross-slot hits would be needed before adoption. No tool,
daemon, cloud cache, installer, PATH change or debug-format change was added.

**C — One build tree + staged runtimes:** an isolated console/DLL proof of concept
compiled alternating changes to two source files. It staged EXE, DLL, linked PDB and
assets, checked working directory/runtime argument/asset content, kept the old staged
process alive during every compile, verified the new computed source-version result,
then closed only the owned old process through stdin. All assertions passed.

Eight edits per mode, milliseconds:

| Mode | Compiled files total | Build median | Staging median | Ready median | P95 / max ready |
|---|---:|---:|---:|---:|---:|
| A/B, separately staged for comparison | 15 | 1,421.85 | 51.69 | 1,601.91 | 1,988.04 |
| Shared tree, staged | 8 | 1,734.28 | 54.50 | 1,958.44 | 2,264.36 |

Both modes pay staging cost in this comparison; production A/B normally runs directly
from its protected slot. This sequential synthetic experiment reduced duplicate compiler
work but did not demonstrate a latency benefit. Order, linker history and machine
variation limit generalization. The proof of concept is not a universal runtime
dependency packager: arbitrary DLL/plugin lookup, asset ownership/deletions, generated
custom outputs, multi-target closures, debugger paths, restart/window integration and
configuration/concurrency/cancellation ownership remain unresolved for that architecture.
**Production A/B is retained.** No shared objects/PDBs or staged-runtime architecture
were introduced into the application.

A separate final-script verification after formatting its generated C++ also passed
four edits per mode (7 versus 4 compiled files). Its unselected raw observations are
retained in `phase3-measurements/slot-verification-samples.csv`; it does not replace the
eight-edit comparison above. The shared-tree ready median was again slower in that
small validation run (approximately 1,719 versus 1,634 ms).

## Small-project results

The existing benchmark now accepts `-Samples`; its default remains three for backwards
compatibility. These new before/after runs use ten source samples, static dependency
retention, a header edit that must rebuild the dependency, deliberate compilation failure
and recovery, and actual execution with piped console input.

| Case | Before | Shipped |
|---|---:|---:|
| Source invocation median (10) | 1,364.17 | 1,219.98 |
| Source invocation P95/max | 1,654.98 | 1,304.35 |
| Cold managed slot A | 3,151.70 | 3,039.14 |
| Cold managed slot B | 3,114.54 | 2,859.87 |
| Warm no-op | 212.15 | 144.02 |
| Header change | 1,372.05 | 1,228.46 |
| Expected compilation failure (exit 1) | 260.31 | 215.79 |
| Recovery (exit 0) | 1,386.11 | 1,317.38 |
| Console input/output/exit | 92.38 | 313.97 |

Console assertions passed in both cases; the single after startup/input timing was
slower and is reported rather than discarded. No console/process-start logic changed.
Source total improved 10.6%, with compiler/link variation as well as discovery savings.
This is a CLI worker invocation, not save-to-window latency. Sub-second behavior is
not achieved or promised. All direct configure/Ninja/SDK timings and individual source
samples are preserved in the CSVs.

## Verification, changed files and handoff

Quick **35/35 passed**, 11.42 seconds. Focused discovery/version checks **3/3 passed**,
0.80 seconds. Applicable local Full **51/51 passed**, 226.21 seconds, four workers;
Doctor then exited **0** (CMake/Ninja/MSVC/Clang found; GCC absent and optional). This
includes the native lifecycle/first-restoration/rapid-save/failed-build-retention tests,
interactive console, settings, artifact freshness, local libraries and Debug/Release/
VS2022 preset regressions. This is the explicit local selection, not unfiltered Full.
A separate fresh ordinary PowerShell Doctor also exited **0**: CMake/Ninja/MSVC/Clang
were installed and correctly reported unavailable on that terminal's PATH.
All four ten-sample FarmGame matrix runs, both ten-sample small-project
matrices, asset-copy checks, final slot prototype and Debug/Release PCH smoke exited 0.
The first slot prototype attempts failed due to invalid test slot naming and PowerShell
single-item array indexing; those fixture issues were fixed and the full 16-sample
prototype rerun passed. The initial pilot missed detailed profile lines because the
Rich Edit presentation adds severity prefixes; capture now uses the existing raw build
diagnostics buffer, and the authoritative baseline was rerun.

Scoped formatting of all six changed C++ files, PowerShell syntax checks and whitespace
checks passed. Repository-wide formatting still fails on pre-existing unchanged
`include/cpplivelab/performance_timer.hpp` (SHA256
`0A37123E9D7BC5F46FD866100FED34D09377C50FC9E4A3D362C8302C9F0F4F96`).

Changed components:

- `include/cpplivelab/doctor.hpp`, `src/cli/doctor.cpp`: opt-in prepared-instance discovery.
- `src/projects/workspace.cpp`: opt in only for build workers.
- `tests/cli/doctor_tests.cpp`: valid reuse and wrong marker/architecture/missing SDK/
  removed compiler fallback, plus fresh Doctor behavior.
- `src/gui/gui.cpp`: test-only slot-activation trace; no production UI/layout changes.
- `tests/gui/gui_flow_tests.cpp`: owned-copy performance mode, real window assertions,
  raw worker phase capture, sampled child memory, header case and disk totals.
- `scripts/benchmark-auto-run.ps1`: configurable sample count, preserving default three.
- `scripts/benchmark-farm-auto-run.ps1`: actual game-copy matrix and statistics export.
- `scripts/benchmark-asset-copy.ps1`: asset copy measurements and deletion semantics.
- `scripts/benchmark-build-slots.ps1`: isolated shared-tree/staging comparison.
- `scripts/check-pch-configurations.ps1`: execute the optional PCH fragment in both configs.
- `docs/experiments/auto-run/FarmGamePch.cmake`: opt-in project-specific measured fragment.
- This report, numeric data, and [manual guide](../../manual-tests/auto-run/FAST_AUTO_RUN_PHASE3_MANUAL_TESTS.md).

An exact path/reason inventory is included in [PHASE3_CHANGED_FILES.csv](PHASE3_CHANGED_FILES.csv).
Full and Quick CTest logs are preserved under `build/phase3-measurements` as
`full-LastTest.log` and `quick-LastTest.log`.

Network-dependent `stage5_unified_raylib`, `stage5_libraries` and optional
`stage5_live_libraries` remain deferred under the existing local-only instruction.
Context7/primary documentation informed the experiments; hosted Graphify was not used
under the restricted local-only policy. No repository content was uploaded to another
service, no dependencies installed, no Codex/system settings changed and no push made.

Exact manual acceptance executable:
`C:\Users\omer2\OneDrive\Desktop\Projects\EasierCpp\build\phase3\cpplivelab-gui.exe`.
Original FarmGame inputs remain untouched. Review/approve the optional PCH edit before
applying it to that project; real-project rendering, VS debugging, Release linkage and
physical multi-monitor/DPI acceptance remain manual checks.
