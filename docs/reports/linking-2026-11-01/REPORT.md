# MSVC/CMake/Ninja incremental-link investigation

November 1, 2026. No application code, production compiler/linker flags, PCH
configuration, A/B architecture, or Seamless behavior changed. FarmGame was
inspected read-only. No installs, commits, or pushes.

## Conclusion

**Debug incremental linking is enabled, actually executes incrementally in the
controlled A/B test, and retains independent linker state in each slot.** It is not
a single shared linker database. Returning to A reuses A's state; returning to B
reuses B's state. Release uses its existing non-incremental link configuration.

FarmGame's existing `fresh-auto-a` and `fresh-auto-b` Ninja files both contain:

```
LINK_FLAGS = /machine:x64 /debug /INCREMENTAL /subsystem:console
CMAKE_CXX_FLAGS_DEBUG:STRING=/Zi /Ob0 /Od /RTC1
CMAKE_EXE_LINKER_FLAGS_DEBUG:STRING=/debug /INCREMENTAL
```

Both existing slots contained separate FarmGame.exe, FarmGame.pdb and FarmGame.ilk
files. The observed ILKs were 3,636,053 and 3,583,485 bytes. These observations prove
configuration/state presence, **not that every past/future FarmGame link succeeds
incrementally**. No FarmGame source/configuration was changed or rebuilt for this
investigation.

## Pipeline ownership

- `src/gui/gui.cpp::start_replacement_build` selects the inactive `fresh-auto-a/b`
  slot. Source-only changes with a usable cache pass `--incremental`.
- `managed_build_directory` keeps distinct directories per project, configuration,
  profile and slot. Slot activation changes the active pointer rather than copying
  executables, ILKs, PDBs, object files or PCH between A and B.
- `managed_build` leases the selected directory, checks source/generator identity,
  skips explicit configure for a valid incremental build, and runs CMake/Ninja with
  the existing worker limit. Ninja still performs dependency checks and can regenerate
  configuration if required.
- The running program keeps ownership of its own slot. Ordinary inactive-slot
  compilation does not clean or overwrite the running slot's linker state.
- New slots, explicit clean/recovery, changed configuration/profile/toolchain or
  project settings may correctly require cold configuration/link state. Imported
  projects retain their own linker options; Debug is not a universal guarantee.

## Experiment and evidence

An owned disposable C++20 project used a static dependency, two application source
files, and native CMake PCH on both targets. The real manager CLI built Debug A/B
slots, kept a previous console process alive while compiling, checked updated
program output, and separately built/ran Release. No external dependencies were fetched.

The fixture alone added `/VERBOSE:INCR /TIME` for diagnostic output from its first
configuration. No optimization, `/INCREMENTAL`, runtime-library or debug-symbol
setting was overridden. These diagnostic linker options never reached EasierCPP
or FarmGame's real project settings.

CMake's MSVC helper normally suppresses successful tool output. A second diagnostic
pass used **process-local** `VERBOSE=1` to expose it, without changing link options.
Warm A and B each reported:

```
LINK : 0 new modules and 1 (out of 48) modules have changed since prior linking
IncrPass2: ...
```

As a control, only the disposable A slot's ILK was backed up and removed. The
result used full `Pass 1`/`Pass 2`, not `IncrPass2`. The following source edit again
used `IncrPass2`. This confirms actual database reuse, not just an `/INCREMENTAL`
flag or an ILK file existing. The control is not a recommendation to delete real ILKs.

Actual Release output retained `/INCREMENTAL:NO` and full link passes. Its resulting
program returned the expected value. Release optimizations and Debug symbols were
not traded away for performance.

### State checks executed

- Six alternating Debug source edits: correct updated output each time, old process
  alive during compilation, then graceful shutdown after the replacement started.
- A and B ILKs retained their original creation times throughout the ordinary
  cycles, while their content changed. The inactive/running slot's ILK hash remained
  unchanged during every build of the other slot.
- All **six PCH files** (two targets across Debug A, Debug B and Release A) retained
  their original hashes and modification times through the experiment, including
  the subsequent diagnostic builds.
- Every warm build retained the `build.ninja` modification time. No warm source-only
  case ran a CMake configuration/regeneration process.
- A warm no-op executed zero compile/link commands and preserved the executable.
- Intentional compilation failure did not terminate the running program or alter
  the destination ILK. Recovery succeeded.
- A static-dependency edit rebuilt the dependency in both slots as each caught up.
  A also needed the application source changed since its previous use. This is the
  expected independent-cache consequence of A/B, not unwanted reconfiguration.

All intended build/run assertions passed. One temporary diagnostic-helper invocation
hit PowerShell's `Measure` alias before building; its helper name was corrected and
the diagnostic sequence rerun successfully. No application defect was involved.

## Separately measured timings

Existing `EASIERCPP_PROFILE_BUILD=1` instrumentation supplied configure checkpoints
and deduplicated Ninja compile/link/archive edges. No new product instrumentation
was added. Developer-shell setup was performed once before the primary measurements:
**4446.94 ms**, excluded from the following build totals. Prepared environment reuse
inside the six warm worker invocations averaged **0.57 ms**.

### Cold versus warm

Milliseconds; cold figures are individual observations. Compile is a sum of command
elapsed durations, including PCH when generated, **not CPU time**. Cold compiles can
overlap, so compile/link/archive sums must not be added to obtain build wall time.

| Case | Configure checkpoint | Compile edges | Link edge | Archive | Managed build wall |
|---|---:|---:|---:|---:|---:|
| Cold Debug A | 1612.07 | 1898 | 562 | 157 | 3494.00 |
| Cold Debug B | 1513.76 | 1895 | 579 | 157 | 3402.01 |
| Warm Debug median, n=6 | skipped | 84 | 326 | 0 | 570.71 |
| Warm no-op | skipped | 0 | 0 | 0 | 150.60 |
| Cold Release A | 1407.98 | 1658 | 291 | 164 | 2897.27 |
| Warm Release A | skipped | 82 | 286 | 0 | 515.26 |

Warm Debug means: compilation **84.83 ms**, link edge **367.50 ms**, managed build
**605.02 ms**. Warm link edges ranged **318–550 ms**; the single 550 ms result is
an observed spike, with no established cause. Six samples on this small fixture
do not characterize every real project.

The warm configure checkpoint averaged 0.06 ms: bookkeeping around a skipped branch,
not a 0.06 ms CMake configure process. The CLI currently prints `Configuring Debug:`
even when it skips configure; that text must not be used to count actual reconfigures.

### Linker versus link wrapper

The Ninja executable-link edge includes cmd/CMake helper startup, resource compilation,
LINK, manifest processing and their I/O. It is not pure LINK execution time.

| Diagnostic case | Ninja link edge (ms) | LINK `/TIME` total (ms) | Actual LINK mode |
|---|---:|---:|---|
| Warm A | 336 | 63 | Incremental |
| Warm B | 387 | 63 | Incremental |
| Missing-ILK control | 354 | 62 | Full |
| Reuse after control | 350 | 47 | Incremental |

LINK's internal timing does not encompass the whole Ninja edge. Logs show RC and
MT run on each Debug incremental link; their separate process times were not
isolated, so the residual must not be attributed solely to either tool. The missing-
ILK control also recompiled an object after its timestamp was touched; it is not
a matched whole-build speed comparison. This tiny fixture shows **no reliable
performance advantage** from forcing a different link mode.

### Launch, measured separately

For the six warm updated Debug programs:

| Boundary | Median (ms) | Mean (ms) |
|---|---:|---:|
| .NET `Process.Start` call, including redirected-process setup | 67.48 | 68.95 |
| Start of launch request → first flushed program-output line | 100.50 | 102.65 |

First-output times ranged 93.20–113.79 ms. These are console readiness measurements,
not pure kernel CreateProcess time, graphical first frame, or Seamless handoff.
Launch measurements exclude build, watcher/debounce and the existing readiness gate.
They must not be presented as complete Auto Run latency. Seamless code was untouched;
this task did not rerun its graphical timing suite. The preceding audit records the
separately executed Safe/Seamless graphical measurements.

## Avoidable work and recommendations

1. **Do not “enable incremental linking” again.** It is already enabled and demonstrably
   reused for the tested Debug pipeline. Check each imported project's actual link
   flags rather than force overriding it.
2. **Preserve per-slot ILK/PDB/EXE state.** Current normal cycles do. Do not copy an ILK
   between slots or clean the inactive slot every cycle. LINK can fall back to full
   linking for missing/mismatched state, incompatible options or significant changes.
3. **Avoid accidental reconfiguration/toolchain setup.** These are already skipped/reused
   on the measured warm path. Metadata checks were about 5 ms before and 6 ms after;
   the measurement does not justify a risky shortcut around them.
4. **Highest-value next measurement: Debug link-wrapper/manifest process overhead.**
   The complete edge is much larger than LINK's self-reported interval. Attribute RC,
   MT and child startup separately before proposing a CMake/manifest change. Do not
   disable manifests, symbols, safety checks or the readiness gate on this evidence.
5. **A/B catch-up compilation is real but architecture-dependent.** Independent slots
   can each compile the same updated dependency at different times. Sharing caches
   would require toolchain/PCH/path correctness and a separate representative benchmark;
   no such architecture change is justified by this small test.

No optimization or speedup is claimed. This investigation establishes the current
behavior and identifies where a larger representative measurement should focus.

## Artifacts and verification

CSV files accompany this report. Successful-link diagnostic logs are optional
local ignored evidence; the CSVs retain the relevant linker measurements. Full normal
build logs, generated Ninja/cache copies and audit-only PowerShell helpers are
retained under `build/link-slots`, `build/measure-link-slots.ps1` and
`build/verify-incremental-link.ps1`. The disposable source/build fixture was removed
after verification, with its exact name and temporary parent checked first; CSV
paths describe the historical measurements. Product sources were unchanged, so no broad
regression suite or application rebuild was needed. The manager executable remains
`C:\Users\omer2\OneDrive\Desktop\Projects\EasierCpp\build\compile-workers\cpplivelab-gui.exe`.

To manually confirm normal behavior, use Debug Auto Run and save ordinary sources
through several A/B cycles. Expect successful replacement and retained per-slot
ILK/PDB files; unchanged PCH should not regenerate. In Release expect the project's
existing full-link options. Do not delete real build state merely to test fallback.

References:
[MSVC incremental linking](https://learn.microsoft.com/en-us/cpp/build/reference/incremental-link-incrementally?view=msvc-170),
[linker progress diagnostics](https://learn.microsoft.com/en-us/cpp/build/reference/verbose-print-progress-messages?view=msvc-170),
[linker timing](https://learn.microsoft.com/en-us/cpp/build/reference/time-linker-time-information?view=msvc-170),
[CMake MSVC link helper](https://github.com/Kitware/CMake/blob/master/Source/cmcmd.cxx),
[Ninja log](https://ninja-build.org/manual.html#ref_log).
