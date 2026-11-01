# Universal PCH integration and workflow follow-up

## Resumed state

The interrupted tree contained edits to `scripts/benchmark-farm-auto-run.ps1` and
`tests/gui/gui_flow_tests.cpp`, plus untracked `scripts/validate-farm-pch.ps1`.
Those changes were preserved. Existing Phase 3 included a FarmGame-specific optional
fragment and benchmark tools, but no general manager PCH configuration or runtime integration.
The old FarmGame validation script was not rerun or treated as verified by this task.

## Finished implementation

- `cmake/EasierCppPch.cmake`: reusable explicit `easiercpp_target_pch(target HEADERS ...)`
  for concrete library/executable targets, with CXX-only headers and conservative eligibility
  checks. No library-specific header guesses or global compiler changes.
- `cmake/EasierCppPchHook.cmake`: CMake deferred recipe evaluation after subdirectory
  targets exist; standard target_precompile_headers handles dependency rebuilding.
- `ProjectOptions::pch_config`, settings persistence and CLI `pch-config`: reviewed recipes
  are snapshotted outside projects. Identical contents reuse the snapshot; changed snapshots
  get separate build profiles. Original non-PCH profile names and old settings remain valid.
- Configuration changes refuse active old-profile build/program leases. Debug/Release,
  target and A/B artifacts remain independently owned. No PCH binary/object sharing or
  general build-slot architecture change was introduced.
- GUI workers load the saved setting through existing project options; no new automatic
  mode, editor, layout, window preference or library-support feature was added.

Explicit presets retain their hooks and conservatively skip automatic overlay injection.
The reusable module can be composed by the project owner into an approved preset hook
or normal project CMake. Imported/interface/alias/C-only targets, existing/disabled PCH,
unsupported compilers and source-specific flags are skipped. Bad opted-in angle headers
can still produce ordinary compiler errors; no retry conceals errors. PCH stays off by
default. This is reusable across compatible targets/libraries, not a claim that arbitrary
CMake graphs or header selections can safely be optimized automatically.

No imported project files, FarmGame inputs, dependencies, PATH or Codex configuration
were changed. There were no installs, commits or pushes.

## Performance (executed isolated local test)

Fixture: Windows/MSVC x64 Debug, C++20 application with a C source plus a static C++
library in a subdirectory, transitive include directory, stable vector/string/iostream
headers, two warm alternating build slots. This uses the real managed build/run pipeline;
figures are **worker build-and-execute times**, not full watcher-to-restored-window latency.

| Case | No PCH | PCH |
|---|---:|---:|
| Cold slot A | 4,906 ms | 4,406 ms |
| Cold slot B | 4,204 ms | 4,765 ms |
| Cold two-slot mean | 4,555 ms | 4,585.5 ms |
| Warm median, six samples | 1,367 ms | 859.5 ms |
| Warm maximum / nearest-rank P95 | 1,562 ms | 1,047 ms |

Warm median improved **507.5 ms (37.1%)** on this library-dependent fixture. Cold results
were mixed (mean approximately 0.7% slower with PCH). Both slots retained unchanged PCH
timestamps throughout source-only edits. A stable header change rebuilt PCH/consumers;
a compiler-definition change invalidated it; no-op builds retained it. Runtime checks
ensure the linked library's value matches the changed header.

Individual samples: [UNIVERSAL_PCH_SAMPLES.csv](UNIVERSAL_PCH_SAMPLES.csv). This is the first
isolated run, without concurrent regression tests. Later Full-suite timings are not used
as uncontaminated performance samples. Small sample sizes, machine load, header selection
and cold/disk costs limit generalization; enable only after measuring your workload.

## Executed checks and fixes

- First focused PCH integration: 1/1 passed, 44.88 seconds, including the timing matrix.
- Expanded focused PCH integration: 1/1 passed, 65.03 seconds. Actual Clang-cl PCH,
  MSVC Debug/Release, mixed C/C++, subdirectory library targets, snapshot identity,
  active-lease refusal, native project PCH preservation, source-flag fallback and
  preset-hook preservation were exercised.
- Earlier expanded attempts exposed two fixture errors: selecting GNU-style C Clang
  with C++ clang-cl, and applying the fixture's native C++ `<string>` PCH to its C
  source. Both were corrected (consistent clang-cl CC/CXX, CXX-scoped native PCH).
- The real GUI lifecycle initially passed 1/1, 72.84 seconds with PCH enabled for its
  console/native DLL workloads. A later added PCH-header-error assertion exposed that
  the fixture had switched to Release while PCH was configured only for Debug. The
  fixture now opts both configurations in; its final retest is recorded below.
- Final corrected GUI lifecycle retest: **1/1 passed**, 77.71 seconds. It explicitly
  verified that a failed PCH-header compilation retained the working process, header
  repair restarted successfully, and native DLL/arguments/assets/window restoration
  still worked with PCH enabled. All 52 selected local cases ultimately passed across
  the Full run plus this affected-test rerun; this is not a single 52/52 Full run.
- Quick: **35/35 passed**, 11.80 seconds.
- Local Full: **51/52 passed**, 252.44 seconds; only the above fixture assertion failed.
  The other 51 regressions, including `stage5_pch`, passed. Network-dependent Raylib/fmt
  and optional live-library tests remain excluded under the existing authorization.

Full logs stay in ignored `build/universal-pch-logs/`: focused.log, extended.log,
retest.log, final-focused.log, quick.log, full-local.log and gui-final.log. Failed attempts
are retained. Never interpret the filtered local Full selection as unfiltered Full approval.
Scoped clang-format checks passed for the six changed C++ files; `git diff --check`
passed. No unrelated formatting was applied and no repository-wide format run was repeated.

## Smaller AI workflow

- [Architecture map](../../development/ARCHITECTURE_MAP.md) lists subsystem entry points
  and relevant tests instead of requiring repeated large-file reads.
- AGENTS.md now explicitly recommends targeted symbols/small sections, retained logs,
  focused iteration and milestone regression runs, while preserving safety/style/UI rules.
  The broken design-guide link was corrected.
- `.gitignore` adds missing PCH artifact formats and generated `_deps` directories.
  Maintained src/build/include/build/tests/build exceptions remain intact.
- Search-only `.ignore` hides logs and numeric historical measurement directories from
  broad searches. Verification: broad docs search sees zero Phase 3 measurement files;
  targeted override still accesses all 22 files. Nothing was deleted.
- Existing tools/docs were reused; only one focused Context7 query was needed. No extra
  plugin indexing or external source upload was performed.

Practical default: read this map/status, search one owner, run one relevant test, retain
the full output on disk, and broaden checks only when the change crosses subsystem
boundaries. Do not repeat full builds/tests or documentation lookups for unchanged code.

Usage, on/off commands and numbered manual checks: [PCH guide](../../development/PCH.md).
Updated executable:
`C:\Users\omer2\OneDrive\Desktop\Projects\EasierCpp\build\universal-pch\cpplivelab-gui.exe`.
