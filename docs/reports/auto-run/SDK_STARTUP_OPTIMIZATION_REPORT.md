# SDK initialization and startup optimization

2026-11-04, Windows 11 x64, VS2022/MSVC 19.44, CMake/Ninja. Version remains 0.1.1.
No real project, system PATH, Windows setting, dependency installation or release
archive was changed. No commit, tag, push or publication.

## Cause and implemented boundary

Previously `Application::start` launched the initial worker without a prepared
environment while independently starting `request_environment`. Both called
`developer_environment`, so both could execute VsDevCmd. `managed_build` prepared
the SDK before discovering that Ninja had no work, and `begin_program` waited for
the GUI's preparation too. Session reuse was already implemented, but did not help
a fresh manager process.

A standalone setup took 4733 ms; two concurrent setups took 4500 ms wall time in a
small diagnostic. Sharing alone would not remove the startup delay. Interleaved
trials of VS's optional telemetry skip switch were inconsistent (4421/4728 ms with
skip, 5052/4588 without), so that switch was **not** adopted.

The new cached-launch path applies only to ordinary default MSVC/x64 Ninja profiles:

1. Validate owned project/profile/cache identity, current compiler installation,
   File API snapshot and selected target. Blank and explicit `-` default-Ninja
   choices are supported. Named presets, vcpkg/custom toolchains, other compilers/
   generators and suppressed regeneration retain the full path. A project containing
   presets must explicitly select `-` to bypass them, just as before.
   Preserve the ordinary build pipeline's artifact ownership fence as well.
2. Use the installed MSVC DUMPBIN to inspect executable/DLL imports without loading
   application code. Check runtime availability using the executable, Windows,
   working and inherited/declared runtime search directories. Small project DLL
   graphs are checked recursively; missing/unknown tools, output, imports or graphs
   exceeding 16 modules fall back to the normal path.
3. Run absolute-path Ninja `-n <target>`. Only the existing strict no-work result,
   unchanged source/artifact observations and still-valid metadata permit launch.
   The existing freshness API records executable/build-file stamps before inspection
   and requires them to remain unchanged afterward, in addition to source checks.
   Dirty/ambiguous/missing state requires the normal build. No CMake generation or
   custom build commands are executed by the probe.
4. Preserve inherited user variables and add the cached compiler-local runtime
   directory plus the existing CMake runtime paths. Working directory, arguments,
   target and normal process/lease/window handling are unchanged.

This is not a general Windows loader emulator. Windows API-set contracts are virtual
names; SxS/custom loading, dynamically chosen SDK plugins and runtime dependencies
on generated SDK variables may need the full environment. Force the existing path
with process-scoped `EASIERCPP_REQUIRE_DEVELOPER_ENV=1`, without changing project
configuration or cache identities. Unknown/localized DUMPBIN output is conservative
fallback, not proof of compatibility. No DUMPBIN binary is bundled or installed.

SDK-only INCLUDE/LIB and compiler/SDK utilities are needed for configuration and
compilation, not for Ninja's read-only no-work check. For actual builds the GUI now
prepares the environment asynchronously once, then gives the existing verified
prepared block to its worker. Existing script/architecture checks and fallback in
`developer_environment` remain authoritative. Nothing sensitive is persisted.

The existing two-tree selection/activation, PCH/linker settings, readiness checks,
freshness, cancellation, DLL manifests and Safe/Seamless handoff were retained.
The CLI's normal `manage-build`/`manage-run` behavior remains available. The new
internal `manage-probe` returns 78 when the ordinary path is required.

## Modified files for this follow-up

- `include/cpplivelab/workspace.hpp`: probe/fallback and cached runtime API.
- `src/projects/workspace.cpp`: scoped probe, dependency availability, runtime block.
- `src/cli/workspace_cli.cpp`: internal probe dispatch through the existing CLI.
- `src/gui/gui.cpp`: cached launch, asynchronous `PreparingBuild` coordination,
  shared SDK result, SDK-failure cleanup, cached Restart and numeric profiling.
- `tests/projects/unified_tests.cpp`: unchanged/changed source/header/CMake, missing
  or invalid artifact, SDK override, real explicit-default cache, named-profile
  fallback and inherited-variable checks.
- `tests/projects/stage5_tests.cpp`: actual shared DLL consumer and missing DLL.
  Its preset suite also builds/runs an actual custom-toolchain preset and verifies
  conservative probe fallback and unchanged project files.
- `tests/gui/gui_flow_tests.cpp`: cold SDK count, cached initial/manual Run/Restart,
  simulated SDK failure preserving the running process, initialization-time close
  and target-discovery expectations.
- Architecture map and this report. All previous uncommitted two-tree work remains.

`src/process/process.cpp` and `src/cli/doctor.cpp` were inspected; their existing
validated prepared-environment protocol was reused without modification.

## Matched Release measurements

The prior Release binaries/resources were retained under `build/sdk-before` before
rebuilding. Both versions used the same generated C++20 workload: local static
library, two application translation units, native CMake PCH and an interactive
console/native Win32 window. Independent disposable roots contained identical
workload/settings except absolute marker paths and edited numeric constants.
No FarmGame files or third-party downloads were used.

The controller verifies computed output and a visible HWND owned by the new PID.
Times include manager/process dispatch and are sampled every 25 ms. Visible-window
time is an observation after the output marker, not a precisely traced first pixel.
The GUI is timed separately. Initial/fresh cases are individual samples; no universal
speed guarantee or cold-build distribution is claimed.

| Case | Before output ms | After output ms |
|---|---:|---:|
| Fresh initial build/launch | 8985.14 | 8593.79 |
| First cold alternate, after that build | 4522.93 | 4368.17 |
| Second update returning to original | 1202.94 | 1171.71 |
| Same-session no-change reopen | 459.07 | 432.88 |
| Cached Manual Run, Auto Run OFF | 4839.21 | 556.72 |
| Manual Restart after that run | 175.45 | 135.62 |

| Ten cached GUI launches | Before | After |
|---|---:|---:|
| Output median ms | 4787.18 | 712.31 |
| Output mean ms | 4808.84 | 729.80 |
| Output P95/max ms | 5489.01 | 908.54 |
| Visible app-window median ms | 4788.37 | 713.77 |
| Visible app-window P95/max ms | 5491.58 | 909.88 |
| Manager GUI-window median ms | 170.11 | 179.26 |
| Application creation-to-observed-output median ms | 86.50 | 82.04 |

Cached output median improved **4074.86 ms (85.12%)**. P95 uses nearest rank; with
ten samples it equals max and is not a strong tail-latency estimate. The manager GUI
was already quick: the improvement is application startup, not a UI redesign.

These are the primary matched measurements. After the final artifact-stability
guard, another ten cached launches passed with zero SDK preparations and zero
compilation commands: median **1839.97 ms**, mean **1932.27 ms**, max **2876.72 ms**.
Independent components were also slower: one sample's discovery was 344.031 ms
versus the earlier 94.529 ms, and runtime checking was 264.366 versus 101.183 ms.
A contemporaneous three-launch retained-old-build control measured median
**13750.33 ms**, mean **15103.39 ms**, max **19253.23 ms**; one worker's SDK stage
alone took 15507.629 ms with no compilation. These later samples show substantial
tool/host timing variation, not a universal subsecond guarantee. The underlying
cause of the later slowdown was not established. Do not omit this final validation
or present the earlier 0.71 s as guaranteed final-host latency.

One after cached launch measured project inspection 0.943 ms, discovery/eligibility
checkpoint 94.529 ms, runtime dependency check 101.183 ms and dependency/freshness
checkpoint 63.098 ms, **zero SDK preparations and zero compilation commands**.
Checkpoints include surrounding bookkeeping; they are not pure subprocess CPU time.

The after fresh build measured GUI discovery 85.073 ms and actual GUI SDK preparation
3744.191 ms. Its worker's prepared-environment processing was 0.493 ms, configuration
1529.318 ms and Ninja 2345.579 ms with seven commands. This confirms reuse rather
than a second expensive setup. Cold figures vary; no attributable cold-build speedup
is claimed solely from the small total difference.

## Initialization frequency and deferred cost

For ordinary, initially unprepared terminals, source paths and test counters show:

| Operation | Before | After |
|---|---:|---:|
| New GUI cached Initial Auto Run | GUI + worker setup | No setup |
| New GUI cached Manual Run | GUI + worker setup | No setup |
| New GUI cold Build/Run | GUI + worker setup | One GUI setup, worker reuses it |
| First compile after SDK-free cached launch | Already prepared at startup | One lazy setup |
| Later warm updates / verified Restart | Reuse | Reuse / no setup needed |

These counts describe the known call paths with the GUI assertion and existing
prepared-worker protocol tests, not an OS-wide process trace. An already prepared
developer shell may itself avoid setup. Named/custom profiles retain the full path.

**Important tradeoff:** setup is deferred, not made free. A matched additional
control measured the first compile after a cached launch at **1404.79 ms before
versus 4641.46 ms after**. The after GUI SDK stage was 3207.060 ms. The subsequent
update was 1141.60 versus 1131.97 ms. The application remains running during SDK
preparation and compilation. No speculative SDK/prewarm task is started merely
because an unchanged executable can run.

Ten warm updates after SDK preparation measured median 1117.51/1174.07 ms and mean
1120.50/1164.10 ms before/after; P95/max 1213.68/1205.40 ms. The median was about
57 ms (5%) higher in this batch, with similar maxima. No new SDK/DUMPBIN work occurs
on this warm path, and no warm speedup is claimed. Host/link/timer variability needs
to be distinguished from a systematic regression; raw stages are retained.

A sequential warm-only repeat, excluding startup and the first SDK setup, measured
ten saves per version: median **1137.24/1189.34 ms**, mean **1161.93/1195.91 ms**,
max **1349.87/1267.63 ms**. This repeats the approximately 52 ms median increase
(4.6%), with a lower observed maximum. Do not claim exact warm-performance parity.
There are no additional SDK preparations, dependency-import inspections or build
commands on this path; further paired profiling is appropriate if the small shift
persists on real projects. No unrelated linker/compiler changes were made to chase it.

## Verification and limitations

Completed focused freshness/DLL/GUI tests: **3/3**, 227.75 s. Debug Quick: **39/39**,
16.24 s. Final broader offline regressions: **11/11**, 533.33 s: workspace/recovery,
presets, PCH (including installed clang-cl when available), GUI lifecycle, window
management/geometry, initial launch, workers, process lifecycle, queues and freshness.
The final default-profile/initial-GUI follow-up passed **2/2**, 149.35 s. Final corrupt
artifact/custom-toolchain compatibility checks passed **2/2**, 106.99 s. Latest Release
build/unit: **1/1**, 1.62 s. Final Debug Quick after all guards: **39/39**, 18.12 s.
The final artifact-stability integration retry passed **1/1**, 166.48 s; an explicit
Release negative test rejected integrity-consistent external-artifact metadata (78)
and accepted the restored owned cache (0), with exact metadata bytes/timestamps
restored afterward. These selections overlap; do not add their counts as unique
tests. No Full/network suite was run. Changed C++ formatting and `git diff --check`
passed. Earlier successful two-tree behavior is retained.

Failures found and corrected during development:

- CMake may store a compiler as STRING instead of FILEPATH; runtime path parsing
  now accepts the actual cache key independent of those types.
- Stale/suppressed-regeneration graphs must use the existing full configure/target
  discovery path. Discovery tests account for the preliminary read-only probe.
- Failed asynchronous preparation clears pending work instead of permitting a
  retry loop. A simulated failure verifies that the current application survives.
- Unknown import names/output conservatively require the normal SDK path.
- One late integration run timed out at 702.88 s during existing Release CMake
  compiler identification, after its cached-probe assertions had passed. No lingering
  fixture processes were found. A fresh retry temporarily showed a slow owned MSVC
  linker chain and ultimately passed in 166.48 s. Cause remains unestablished; no
  flags, safety checks or test timeout were weakened to hide it. Logs are retained.

Limitations: default MSVC/Ninja fast path only (blank or explicit `-`); no fast-path
claim for named presets, custom toolchains/vcpkg, Clang or other generators. They
still work through their
existing full path. Arbitrary dynamic DLL loading/SDK-variable use needs the override.
SDK initialization itself still takes seconds and is not forcibly interrupted; Stop
cancels dependent work, and Close waits for the existing asynchronous setup to finish.
No reboot, clean machine, physical mixed-DPI or real FarmGame acceptance was performed.

## Manual acceptance

1. Close other manager instances. Launch
   `C:\Users\omer2\OneDrive\Desktop\Projects\EasierCpp\build\release-v0.1.1\EasierCpp-Gui.exe`.
   Open a disposable default MSVC/Ninja project and enable Auto Run. A fresh build
   still needs SDK setup and compilation.
2. Close/reopen the manager and the same unchanged project. Expect its application
   to launch without compilation; Build & System should show
   `Verified current executable without SDK preparation.` for an eligible profile.
3. Save a real source change. The first compile in this SDK-free session may spend
   several seconds preparing the compiler while the old app remains alive. Later
   saves should reuse the SDK and alternate between the existing two trees.
4. Change a shared header or CMake target configuration. Expect required rebuilding
   or target selection, never a stale executable selected just for speed.
5. Stop, then Run; check arguments, working directory, interactive input and DLL/assets.
   Restart should use the last successful executable without an unnecessary build.
6. Introduce a compilation error, then repair it. The current running app should
   survive the failed replacement. Check Safe/Seamless and window preferences too.
7. Test your named presets/custom toolchains and Debug/Release. Full SDK startup for
   a conservative fallback is expected and is not a cache-persistence failure.
8. If the program needs SDK variables/dynamic SDK plugins, close the manager and
   launch from PowerShell with `$env:EASIERCPP_REQUIRE_DEVELOPER_ENV = '1'` before
   starting the executable. Remove that temporary variable afterward or use a fresh
   terminal. This changes no project files or permanent Windows configuration.

## Evidence and documentation

Local ignored evidence: `build/sdk-measure-before/`, `sdk-measure-after/`,
`sdk-before-cached-edit/`, `sdk-startup-summary.csv`, `sdk-startup-components.csv`,
`sdk-startup-cases.csv`, `sdk-*-tests.log`, `sdk-focused-final.log`, `sdk-quick.log`,
`sdk-release-verified.log`, `sdk-regressions.log`, `sdk-final-quick.log`,
`sdk-artifact-snapshot*.log`, `sdk-ownership-verification.log`, `sdk-final-cached/`,
`sdk-final-before-control/`, `sdk-warm-repeat-*/`, `sdk-warm-repeat-summary.csv`.
Controller:
`build/measure-sdk-startup.ps1`; older binaries remain in `build/sdk-before`.
All output logs contain numeric diagnostics/project paths, not environment blocks.

Primary references: [Microsoft DUMPBIN /DEPENDENTS](https://learn.microsoft.com/en-us/cpp/build/reference/dependents),
[Windows DLL search order](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order).
The installed VsDevCmd scripts were also inspected directly without modifying them.

Recommended follow-up: validate the default path on real projects before packaging;
support preset-specific runtime environments only with explicit compatible handling;
consider cancellable SDK preparation or optional post-launch SDK warming separately.
Do not change the two-tree architecture or advertise universal SDK-free launch.
