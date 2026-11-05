# Two-tree Auto Run implementation and validation

2026-11-04. No release version change, installation, project migration, cache copying,
cleanup, commit, tag, push or publication. The prior cache-persistence investigation
and hot-reload feasibility report remain intact.

## Architecture

Previously: original -> A -> B -> A. Each directory separately configured CMake,
compiled dependencies and PCH, and maintained PDB/ILK state.

Now: original -> alternate -> original -> alternate. The original build participates
directly. A small shared selector prefers a cached inactive original, otherwise an
existing cached inactive A/B; if no peer cache exists it selects A from original,
or original from another slot. Existing A/B-only installations keep their warm pair.
Old A/B and recovery pointers remain accepted, and all historical directories stay
on disk. No compatibility claim relies on copying path-sensitive outputs.

Explicit internal slot `base` selects the original directory. An empty slot still
follows the profile's persisted pointer. Activation writes `base` or existing
`fresh-*` names through the existing atomic write-through pointer update after
verified build/handoff. Slots are not added to user project files or serialized
as transient project-option overrides.

The GUI only changes its inactive-tree selection call. Existing leases, running
process ownership, cancellation, source freshness, CMake validation, worker limits,
target selection, readiness, window placement and Safe/Seamless lifecycle remain.
An invalid selected cache still reports its normal build/recovery error; it is not
silently erased or replaced. Debug/Release/profile/PCH directory keys are unchanged.

See [the tree guide](../development/AUTO_RUN_BUILD_TREES.md) for downgrade constraints:
older binaries cannot interpret the new `base` pointer. Forward use of existing
caches is supported; mixing manager versions on one state directory is discouraged.

## Changed maintained files

- `include/cpplivelab/workspace.hpp`: shared inactive-tree selection contract.
- `src/projects/workspace.cpp`: original-tree resolution, selection and activation.
- `src/gui/gui.cpp`: uses the shared selector instead of hardcoded A/B rotation.
- `tests/projects/unified_tests.cpp`: base/old A/B/recovery/profile selection,
  preserved cache content, real atomic activation and no-work return to original.
- `tests/gui/gui_flow_tests.cpp`: real native GUI lifecycle rejects creation of a
  third A/B cache alongside the original.
- `docs/development/ARCHITECTURE_MAP.md`, `AUTO_RUN_BUILD_TREES.md`, this report.

## Controlled CLI build comparison

The existing local static-library/two-source/CMake-PCH fixture was reused. No Raylib
download or real-project copy. The before run used the retained original Release
v0.1.1 binary. The after CLI run used the updated Debug binary; all *fixture* builds
used the same MSVC x64 Debug/CMake/Ninja configuration and existing diagnostic flags.
Small differences in CLI/controller overhead are not evidence of a compiler speedup.

`EASIERCPP_PROFILE_BUILD=1` records stage durations and Ninja edges. VS environment
was prepared once in the controller, as in warm GUI replacement. Initial, first two
updates and ten subsequent updates were measured. Each replacement retained the
previous fixture process during compilation, checked the new output, then gracefully
closed/released the old process. No readiness check or dependency check was skipped.

| Case | Before ms | After ms |
|---|---:|---:|
| Initial fresh build | 4918.30 | 4888.51 |
| First replacement (cold alternate) | 4413.47 | 4573.72 |
| Second replacement | 4323.42 | 662.57 |
| Ten warm updates, median | 567.34 | 562.41 |
| Ten warm updates, mean | 582.11 | 578.68 |

The second update saved **3660.85 ms (84.68%)**. The initial and first replacement
show ordinary run-to-run variation; no improvement is claimed there. Warm median
changed by less than 1%, with no meaningful warm-build regression demonstrated.

Second-update breakdown:

| Stage | Before ms | After ms |
|---|---:|---:|
| Configure checkpoint | 1400.12 | 0.06 |
| Compile command edges, including PCH | 1995 | 128 |
| Executable link edge | 585 | 367 |
| Ninja command count | 7 | 2 |

Seven commands are two PCH compiles, three ordinary translation units, one library
archive and one executable link. Returning to original compiled only the edited
translation unit and linked. PCH/compiler edge sums may overlap and are not wall
time to add to configuration/link measurements. All four after-run PCH files kept
one distinct hash and timestamp each across source edits.

Injected compilation failure returned nonzero in 219.55 ms while the old application
remained alive and the alternate ILK hash stayed unchanged. Repair built successfully
in 547.78 ms and produced the expected result. The fixture also exercised subsequent
no-change/catch-up builds in fresh worker processes.

## Disk usage

| Fresh local-library fixture | Before | After |
|---|---:|---:|
| Compilation trees | 3 | 2 |
| Tree file bytes | 354,186,941 | 236,411,131 |
| PCH bytes | 340,328,448 | 226,885,632 |

Total tree-file savings were 117,775,810 bytes (about 112.3 MiB, 33.25%). This is
new-fixture allocation, not disk reclaimed from existing users: historical trees
remain untouched. Different PDB/ILK histories and test samples can affect totals.

## Graphical save-to-visible-window comparison

The existing native GUI profiler drove the same disposable Win32 window project
with a local static dependency and PCH, ten measured source saves plus a shared-header
edit. Both primary graphical runs used Release manager/profiler builds. Two startup
updates are excluded from this profiler's warm samples; their cold-build comparison
is provided separately by the CLI experiment above. No FarmGame copy was needed.

| Ten warm graphical saves | Median ms | Mean ms | Range ms |
|---|---:|---:|---:|
| Original architecture baseline | 1304.5 | 1334.3 | 1266–1547 |
| Two trees, first comparison | 1555.0 | 1559.6 | 1328–1782 |
| Two trees, no concurrent regressions | 1304.5 | 1467.3 | 1234–2094 |
| Retained historical three-tree Release control | 1797.0 | 1803.2 | 1750–1907 |

The first after run overlapped the final regression test; its slower result triggered
the uncontended repeat rather than being hidden. The repeated median matches baseline,
but its mean is 10% higher because of tail samples. Individual link edges ranged
from about 359 ms to 859 ms in that run. The contemporaneous historical control also
slowed; it is an older build, not an exact replacement for the primary baseline.
These data support substantial host/run variability and no demonstrated systematic
warm median regression, **not** a guarantee of unchanged tail latency or a graphical
warm speedup. Longer paired trials on an idle machine would improve that conclusion.

The shared-header edit compiled two translation units and linked (three commands),
retaining all four PCH timestamps. Placement assertions passed on every measured
replacement. Graphical PCH bytes fell from 337,575,936 to 225,050,624; ILK bytes
from 1,942,144 to 1,314,946. PDB totals were 17,420,288 versus 12,263,424 bytes.

The profiler samples the largest individual job-member peak working set, not aggregate
build memory. Largest warm observations were 31,592,448 bytes in the original baseline,
59,555,840 in the quiet two-tree repeat, and 59,658,240 in the historical control.
No aggregate/cold peak-memory saving is claimed.

Initial manual Run-to-window was 11,859 ms before, 13,672 ms in the first after run,
and 9,890 ms in its quiet repeat. There was no startup optimization; these single
fresh-build samples must not be presented as an attributable initial-launch speedup.

## Startup and prewarming

The before graphical profiler's cold worker measured project inspection 2.10 ms,
tool discovery 116.35 ms, SDK environment preparation **5506.48 ms**, configuration
1923.29 ms and Ninja 3881.78 ms. Initial manual Run to visible window was 11859 ms
with this genuinely fresh fixture. These stages are distinct from the earlier
5–7-second no-work GUI launches; they show that SDK preparation can independently
be substantial. Prepared warm workers avoid that SDK setup cost.

No startup optimization was made without a controlled benefit. In particular, the
GUI prepares an environment asynchronously while an unprepared initial worker may
prepare its own. Changing their ordering/reuse could affect startup and cancellation;
this task does not serialize them or persist potentially sensitive environments.

Background prewarming remains deferred. Two-tree rotation already removes one cold
pass without adding scheduling or resource contention. The first cold alternate
remains a known cost. A prewarm job would need its own dependency approvals, pending
edit reconciliation, cancellation and resource policy before becoming a safe default.

The real updated GUI was also launched/closed/relaunched twice using isolated state:

| Real GUI sequence | GUI window observed ms | First fixture output ms | Next A/base saves ms |
|---|---:|---:|---:|
| Open (fixture main changed) | 484.46 | 7278.08 | 1945.52 / 1224.18 |
| Close/relaunch | 286.51 | 6063.64 | 1269.58 / 1107.90 |
| Close/relaunch again | 178.58 | 5550.23 | 1388.97 / 1097.95 |

Each relaunch preserved **25/25** tracked cache/graph/object/PCH/ILK/pointer files
with identical SHA256 and UTC timestamps. The pointer remained `base`; subsequent
updates selected A then base. Source edits were real content changes. GUI output was
read from its owned native console, without changing production profiling code.

On the first no-change relaunch, worker project inspection was 1.513 ms, tool discovery
160.447 ms, SDK preparation 4762.695 ms, initial target-graph-check checkpoint
137.161 ms and Ninja build 207.598 ms with **zero build commands**. The graph and
artifact snapshots did not change. Application creation-to-observed first output
was 72.69 ms (includes polling); the GUI itself was already visible after 286.51 ms.
This reproduces the slow no-work startup and identifies SDK setup as its dominant
measured component, independently of cache persistence. A separately timed raw
CreateProcess call and every residual GUI operation were not instrumented.

Same-session project reopen was independently repeated with the updated GUI: it
reached output in 426.40 ms and retained **25/25** tracked files unchanged. Subsequent
updates selected A/base and reached output in 2118.00/1829.29 ms. This is a separate
single-sample validation, not a replacement for the ten-sample warm comparison.

No Windows reboot was performed. Reboot/physical mixed-DPI behavior remains manual.

## Verification and recommendation

- Debug Quick: **39/39**, 18.00 s elapsed.
- Focused new unit/integration tests: **2/2**, 68.79 s.
- Broader relevant offline regressions: **10/10**, 561.64 s: projects, presets, PCH,
  GUI lifecycle, window management, independent geometry, initial Auto Run,
  process lifecycle, queue/cancellation and freshness. The GUI flow includes
  Safe/Seamless, failures, rapid saves, DLL/assets, input and native placement.
- Release build and updated unit test: **1/1**, 0.61 s test time.
- CLI before/after, graphical profiling and real GUI restart controllers completed
  their assertions. Compilation failure/recovery was tested, not inferred.
- Changed C++ passed discovered clang-format `--dry-run --Werror`;
  `git diff --check` passed. No full/network test suite was run.

Recommend the two-tree behavior as the default after manual real-project acceptance:
the redundant second cold pass is eliminated, existing caches remain available,
focused lifecycle regressions pass and warm medians are preserved. Do not advertise
a warm-latency improvement or guaranteed tail latency. Leave prewarming off and
investigate SDK startup separately. No production release was prepared from this work.

## Reproduction and evidence

Complete profiling output, hashes, timestamps, command-edge counts and fixture paths:

- `build/ab-two-before/{timings,files,launch}.csv`
- `build/ab-two-after/{timings,files,launch}.csv`
- `build/two-graph-before.log`
- `build/two-graph-after.log`, `two-graph-after-idle.log`,
  `two-graph-historical-control.log`, `two-tree-graph-summary.csv`
- `build/ab-two-after/gui-{timings,files}.csv` and `*-system.log` startup profiles
- `build/two-tree-{focused,quick,regression,release}.log`
- Audit controllers: `build/measure-two-before.ps1`, `measure-two-after.ps1`.
- GUI controllers: `build/measure-two-gui.ps1`, `measure-two-reopen.ps1`;
  same-session evidence in `build/ab-two-after/gui-same-session-*.csv`.

The audit controllers are retained locally under ignored build output, alongside
their disposable fixtures. Early excluded controller runs uncovered a retained
.NET process handle that prevented overwriting the *exited* original executable;
the controller now disposes that handle. Production uses its existing session/lease
release path. An initial build command also used a wrong CMake GUI target spelling;
it was corrected before verification. These were audit/build-command errors, not
unresolved application failures.

Manual acceptance steps are in the tree guide. Do not use an older portable ZIP to
test these changes: the updated executable is
`build/release-v0.1.1/EasierCpp-Gui.exe` with adjacent `EasierCpp.exe` and resources.
The version remains 0.1.1; no release archive was repackaged or published.
