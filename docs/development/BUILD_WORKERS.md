# Compilation workers

Open a project, then Settings → Preferences. The existing Window Placement and Auto Run
Replacement Mode appear first, followed by Parallel Build Workers and Optimize Workers.

The selector saves immediately, including when Preferences is subsequently cancelled.
Window/replacement changes retain their existing OK/Cancel behavior. One worker limits
the native build to one concurrent job. Higher values are maximum build-job counts, not
promises to occupy that many cores. Test-worker settings and `scripts/build.ps1 -Jobs`
continue to control CTest only.

## Defaults and persistence

Settings are global in `%LOCALAPPDATA%/EasierCpp/build-workers.settings`, or the existing
`CPPLIVELAB_STATE_HOME` override. The file stores selected and last-recommended counts.
It is replaced atomically; imported source/CMake files are not changed.

Existing managed workspaces without a worker setting retain their build-system,
preset/environment defaults. The selector displays **Build-system default** for that
compatibility state; it can also restore that state explicitly. It does not disable
parallelism. **1** is the sequential selection. A fresh installation without existing
managed project metadata starts with a CPU/memory-bounded automatic count.

The range uses logical processors across Windows processor groups and available RAM:
reserve 1 GiB for the desktop and budget 768 MiB per compiler process, with a minimum
of one worker. It is not capped at 16; 64/128-processor resource calculations have unit
coverage. This is a conservative estimate, not a compiler-memory guarantee. If resources
shrink, a subsequent explicit-count build is capped to the current safe range and logs
its effective count. Native-default mode retains native behavior instead.

Both managed CMake builds and the legacy single-file/project path snapshot the setting
at build entry and pass `cmake --build ... --parallel N`. Manual, automatic, Debug,
Release and A/B slots use the same path. No build-cache identity, compiler flags, PCH
recipes or configure inputs depend on the worker setting. Active builds are not stopped
or reconfigured when it changes. Existing directory leases remain authoritative.

CMake maps this flag to Ninja/Visual Studio and other generators that support native
parallel builds. Unsupported generators may warn/ignore it; CMake output remains visible.
It limits native build jobs, not arbitrary threads or nested builds explicitly started
by a user's CMake commands or compiler options such as `/MP`.

## Optimize Workers

Stop the current program/build first. Optimize Workers uses the existing owned,
cancellable child-process progress window. The UI continues pumping messages; each
candidate build runs sequentially in a newly reserved manager-owned directory outside
user projects. Cancel terminates only the benchmark job tree and retains the previous
selected/recommended counts. Failure, timeout, invalid results, increased memory pressure
or a pending manager close/restart also prevent applying a recommendation.

The shipped synthetic workload contains 12–64 independent C++20 translation units using
standard-library templates. It downloads nothing. Candidates are 1, powers of two,
and the resource/workload limit. The workload scales with the safe worker count so
24/32/64-worker candidates can be useful on suitable hardware. The probe is bounded to
64 units/jobs to control duration; the manual selector can exceed 64 when resources allow.

Each candidate runs three clean compilations and three one-file incremental builds.
Only the benchmark's own build outputs are cleaned. Medians are scored with equal
weight for clean and incremental times; a larger count must improve the best score
by at least 3% to replace it. All builds must succeed. The recommendation is an estimate
for this workload, clearly labelled synthetic, not an optimum for every project.
"Clean" means compiler outputs were removed; OS filesystem caches are not flushed.
Configure time and cleanup are outside measured build intervals; compile/link/CMake
build-process overhead are inside them. Warm samples rebuild one changed source file.

Success applies the count and retains a CSV at
`%LOCALAPPDATA%/EasierCpp/worker-benchmark.csv`. Temporary sources/builds are removed
after success, failure or cancellation once owned processes drain. The GUI's run is
bounded to ten minutes. Manual selection remains available afterward and retains the
last recommendation for reference.

## Manual acceptance

1. Close the previous EasierCpp instance when ready and run
   `build/compile-workers/EasierCpp-Gui.exe`. Open a disposable CMake project.
2. Settings → Preferences: verify the three requested sections, numerical worker
   choices, recommendation and Optimize Workers button. Choose **1**, then Cancel.
   Reopen Preferences: expect **1** still selected.
3. Build Debug and Release. Build & System should report `Compilation workers: 1`.
   Choose **2** if resources allow, then build again; expect the new limit in output.
4. Enable Auto Run and save real source changes across several cycles. Expect the
   selected count in each replacement build, working A/B slots, and unchanged PCH
   for unchanged headers. Check existing Safe/Seamless behavior and interactive I/O.
5. While a substantial build is active, change the selector. Expect the current build
   to continue and subsequent builds to use the new selection. Optimize Workers should
   refuse to start while a program/build is owned.
6. Stop the program/build and click Optimize Workers. Expect responsive progress with
   candidate/trial counts. Cancel: expect the previous count and recommendation.
7. Run optimization to completion. Expect a numeric recommendation applied immediately,
   its synthetic-estimate label, and the CSV. Change to **1** afterward to verify override.
8. Close/reopen EasierCpp and the project. Expect selected and recommended counts retained.
   Build-system default should restore inherited native/preset/environment parallelism.

These changes do not alter Seamless placement/readiness, introduce another replacement
mode, or add a general-purpose thread pool. They delegate scheduling to the build tool.
