# EasierCPP reliability, compatibility and performance audit

Date: November 1, 2026. Checkpoints 2–6 were authorized after the original
Checkpoint 1-only request. This is a focused audit on the available Windows 11
Home host (build 26200), not certification of every Windows/toolchain combination.

No application source, UI, compiler flags, project CMake, or runtime behavior changed.
The build reported `ninja: no work to do`. Only this report/data and ignored audit
artifacts under build/ were created. No installs, permanent PATH changes, commits,
pushes, or real FarmGame edits occurred.

## 1. Reliability

Reviewed watcher fingerprints and overlapped cancellation, scheduler coalescing,
GUI replacement completion/freshness checks, Stop/disarming, candidate readiness,
slot leases, candidate discard/drain, process-tree cleanup, library ownership and
transaction boundaries, and resource-limited worker selection/persistence.

The inspected implementation checks newer source fingerprints before replacement,
keeps a working process through failed compilation, retains candidate leases until
termination/output drain, and limits forced termination to owned jobs. Stop disarms
automatic launches. Source matching alone does not grant library ownership.

Checkpoint 1 selection: **12/12 passed, 121.99 seconds**:
`stage4_watcher`, `stage4_scheduler`, `reliability_gui_flow`,
`reliability_lifecycle`, `stage3_tree`, `redirection_unit`,
`stage5_library_detection`, `stage5_library_metadata`,
`stage5_library_transactions`, `stage5_github_versions`,
`stage5_gitlab_versions`, `compilation_workers`.

This includes actual GUI/controller fixtures, failed-build retention/recovery,
interactive I/O, Stop/Restart, switching, PCH/A-B behavior, settings, optimizer
cancellation/failure/success/manual override, and controlled provider errors.
It does not prove absence of every race or long-duration leak.

## 2. Compatibility and the observed failure

The selected compatibility/diagnostics run had **11/12 passes, 101.04 seconds**.
Passing checks: `stage5_projects`, `stage5_presets`, `stage5_pch`, `doctor_unit`,
`doctor_without_path`, `maintenance_paths`, `maintenance_arguments`,
`maintenance_handles`, `maintenance_output`, `stage4_console_view`,
`stage4_prompts`.

`stage5_unified_projects` initially failed compiling a disposable consumer with:

```
fatal error C1090: PDB API call failed, error code '23': (0x000006BA)
```

An isolated rerun passed **1/1 in 61.09 seconds**, without killing a compiler
service, changing debug flags, deleting caches, or editing the application.
This is a confirmed compiler-service failure incident with an **unestablished
cause**, not a confirmed EasierCPP regression. Concurrent test compilation or
machine/service state are possibilities, not proven causes. Approximately 2.88 GiB
RAM was available when subsequently queried; low memory is not established as cause.

The exercised fixtures cover source/artifact paths with spaces/Unicode, multiple
targets, custom outputs, Debug/Release separation, presets, tool discovery without
terminal PATH, stale/mismatched metadata and safe recovery. The installed MSVC
toolchain is exercised. The PCH test includes an installed-Clang-cl conditional
case; this is not a compatibility claim for arbitrary Clang distributions.

Actual Build Tools-only installations, different drives, alternate compiler/SDK
versions, every long-path/read-only layout and cloud-sync failure mode were not
independently exercised. Discovery unit fixtures do not certify those environments.
Existing source/cache identity checks reject stale source/generator identity and
require explicit recovery rather than silently launching an old executable.

## 3. Performance

Existing timing tools were run sequentially after test builds finished. Disposable
local and OneDrive projects were created and removed by the existing probe. No
third-party libraries were downloaded or configured. FarmGame was not benchmarked.

### Simple console fixture

Durations below are means in milliseconds. Configure/build durations include
subprocess startup and output drain. Compile and link are **combined**, not separate.

| Measurement | Local | OneDrive | Samples/location |
|---|---:|---:|---:|
| Cold configure | 2390.34 | 2603.40 | 2 |
| Warm configure | 162.40 | 176.43 | 2 |
| Cold compile/link | 1399.52 | 1536.67 | 2 |
| Warm no-op build | 80.95 | 79.67 | 2 |
| One changed source compile/link | 479.68 | 697.19 | 2 |
| Low-level save-to-watcher notification | 0.82 | 1.36 | 10 |
| Changed-file fingerprint | 0.32 | 0.44 | 10 |
| Process launch | 7.79 | 8.17 | 20 |

Shared tool discovery: 81.67 ms (one sample). Fresh developer-environment preparation:
4717.06 ms mean, 4611.32–4771.59 ms (three samples). Independent CLI warm incremental
invocations took 4011.10/4527.88 ms (one per location), dominated by preparation.
These are fresh CLI invocations, **not warm GUI Auto Run cycles**. The GUI already
reuses its prepared environment; no persistent-worker refactor was justified.

The standalone scheduler probe's default 400 ms debounce measured about 404–408 ms.
That is not the GUI preference: the GUI default is 150 ms and the active setting
can differ. Do not add the standalone debounce measurement to GUI benchmark totals.
Two compile samples per location cannot establish that OneDrive caused the difference.

### Controlled native graphical replacement

Existing single-file Win32 fixture; two excluded warmups, then ten measured runs
per mode, interleaved. Placement matched the expected rectangle in every run.

| Boundary | Safe median / mean (ms) | Seamless median / mean (ms) |
|---|---:|---:|
| Save → first observed new visible window | 1348.32 / 1370.37 | 1176.57 / 1224.75 |
| Save → complete managed handoff | 1381.36 / 1406.68 | 2185.24 / 2222.45 |
| Observed no-visible-window gap | 235.62 / 240.89 | 0 / 0 |
| Managed replacement build interval | 784.32 / 823.72 | 851.77 / 894.11 |

Save begins after the fixture's source write; visibility is detected by window
polling, not first rendered frame. Handoff ends after the old process is drained
and the candidate becomes the manager-owned active process. Build interval includes
the replacement worker and preparation, not compiler-only CPU time. Window gaps
are polling observations, not compositor guarantees. These intervals overlap and
must not be summed.

The later Seamless handoff is consistent with the existing one-second readiness
interval; earlier visibility and overlap remain satisfactory. **No visual behavior,
readiness interval or optimization was changed.** Separate link, runtime-copy,
window-detection/placement, and old-process-shutdown durations were not isolated;
they remain within the aggregate boundaries. A new multi-file graphical performance
fixture was not added; multi-target/PCH/library correctness uses the existing tests.

## 4. Resources and lifecycle

An isolated production GUI used the existing test-state/preferences override and
was launched hidden with no project. It closed normally via WM_CLOSE. Five samples
over 5.08 seconds showed:

- Working set maximum: **15.20 MiB** (resident memory).
- Private bytes maximum: **2.41 MiB** (private committed memory).
- **139 handles and four threads**, unchanged across samples.
- CPU delta: **0.0% of one core at the available counter resolution**.

This hidden/no-project short sample is not a visible active-project footprint or
a long-duration leak test. In particular, “approximately 3 MB” is not a verified
whole-application/build footprint.

A separate disposable multi-file CLI cold build succeeded with two workers. Sampled
per-process maxima, deliberately **not summed into a tree peak**, were:

| Process | Working set MiB | Private bytes MiB |
|---|---:|---:|
| CLI manager | 6.55 | 1.07 |
| CMake | 20.89 | 10.30 |
| Ninja | 6.99 | 2.06 |
| cl | 64.23 | 56.18 |
| link | 1.20 | 8.32 |

Shell/conhost helpers are in resources.csv. Sampling can miss short peaks; compiler
and linker each had only two observations. The GUI was closed before this build,
so GUI memory while actively streaming a build was not measured. Detached compiler
services were not attributed to this tree. This resource run was separate from
latency profiling to avoid presenting sampling overhead as pipeline performance.

Source review confirmed watcher cancellation keeps OVERLAPPED buffers alive, modal
workers are joined through futures, sessions use owned jobs, and candidate discard
retains leases through asynchronous termination. Current worker limits use logical
CPUs and **available** RAM, reserving 1 GiB plus 768 MiB per compiler job. A temporary
low dropdown range is not proof of a one-core hardware limit. Safety limits were
not changed. Persistent A/B/dependency caches intentionally remain reusable; no
long-run cache-growth/eviction experiment or speculative deletion was performed.

## 5–6. Diagnostics, findings and decisions

Reviewed raw tool-error propagation, freshness-blocked Run messages, previous-app
retention messages, missing artifacts, Stop timeouts, library failure/cancellation,
and stale-cache recovery. Existing console/path/prompt/handle diagnostics checks
passed in the compatibility selection. Detailed compiler output retained the PDB
incident. No failure was silently converted into a success.

| Severity | Finding | Decision |
|---|---|---|
| Critical/High | None reproduced within the executed scope | No claim that every scenario is defect-free |
| Medium, observed | One C1090/PDB RPC failure; isolated rerun passed | Preserve flags/working code; capture recurrence, retry isolated before diagnosis |
| Medium, unverified risk | Long-running handle/cache growth, unusual multi-window/exclusive-instance apps, alternate SDK/Build Tools machines | Targeted stress/manual/environment testing remains necessary |
| Low, measured opportunity | Fresh CLI SDK initialization dominates trivial independent invocations | GUI already reuses environment; no speculative caching refactor |
| Known detection limits | Historical/incomplete File API data, aliases and arbitrary CMake cannot establish all package identities/versions | Keep read-only/Unknown states; no guessed ownership or automatic migration |

There were **no application fixes** and no reason to repeat a full regression
suite after source changes, since none were made. No live GitHub/GitLab downloads,
external-library builds, cross-machine certification or real FarmGame acceptance
was performed. Performance is observational, not an improvement claim.

## Practical next checks

1. In a disposable project, leave a program running, introduce a compile error,
   and save twice. Expect the old process to remain; repair and save should launch
   the latest version. Stop should disarm automatic launch until resumed.
2. Repeat with Safe and Seamless, moving the native window before each save. Expect
   the current placement behavior; exclusive-instance apps should use Safe.
3. Inspect an external library: no Remove/Update/Repair actions. Cancel an approved
   managed addition: original CMake/source/assets must remain intact.
4. Save a valid worker count, restart, and check it persists. Low available RAM may
   clamp build parallelism. Do not equate this with hardware CPU count.
5. If C1090 recurs, retain Build & System output and run the failing test alone:
   `ctest --test-dir build/compile-workers -R "^stage5_unified_projects$" --parallel 1 --output-on-failure`.
   Use the discovered CTest path from Visual Studio if it is not on terminal PATH.
   Do not automatically terminate unrelated compiler services or delete user caches.

Recommended next audit work: a controlled long-duration lifecycle/handle test and
a clean Build Tools-only Windows VM. Neither requires changing Seamless visuals.

Executable is unchanged:
`C:\Users\omer2\OneDrive\Desktop\Projects\EasierCpp\build\compile-workers\cpplivelab-gui.exe`.

Raw tables are alongside this report. Full logs and the isolated resource-probe
script are retained under `build/audit-*`. The owned resource fixture
`C:\Users\omer2\AppData\Local\Temp\EasierAudit-4cf45068af654f31b18cac74b2aa3ce7`
was removed after successful sampling, with its exact path and temporary parent
validated first. Its GUI preference key used the existing test-only registry
namespace, not the user's normal preference key.

API reference consulted through Context7:
[CMake compiler identity is fixed after initial configuration](https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_COMPILER.html).
