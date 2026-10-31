# Focused Auto Run speed improvement

## Current restoration status

The previously intermittent FarmGame first-replacement placement issue is **not
currently reproducible**, based on the user's recent tests. It is not a development
blocker. The existing native regression was run before implementation and passed
(97.93 seconds overall); it explicitly checks the first manual-Run-to-Auto-Run
replacement rectangle and subsequent maximized replacements. Window management is
unchanged in this pass. No real FarmGame files were modified or rebuilt.

## Completed

Warm incremental builds previously invoked CMake's File API manifest reader both
before and after every build. They now reuse the existing pre-build target metadata
when its generator is supported, the manifest parses correctly, and it is at least
as recent as the File API indexes. Missing, malformed, unreadable or older metadata
uses the original CMake reader. Output-path checks still run before compilation.

The post-build File API refresh always remains: CMake can regenerate during a build
and change target artifacts. Artifact existence/selection checks remain. Fresh
configuration and non-incremental builds use the original path. This removes one
redundant CMake subprocess/JSON-reading pass in the normal warm incremental case.

The two build slots, cached objects/dependencies, watcher/debounce, graceful shutdown,
old-process retention during compilation, failed-build handling, runtime environment,
assets/DLL lookup, working directories and restoration implementation are unchanged.
No compiler flags or external dependencies were added.

Files changed this pass:

- `src/workspace.cpp`: guarded reuse of pre-build metadata.
- `tests/stage5_tests.cpp`: missing, malformed and stale metadata recovery checks,
  including unchanged artifact timestamps and refusal to use an outside artifact.
- This report. The temporary benchmark script/data are ignored under `build/`.

All pre-existing uncommitted library and Auto Run changes were preserved.

## Actual verification

- Initial native Auto Run regression: **1/1 passed**, 97.93 seconds.
- Updated isolated build succeeded; focused regressions: **3/3 passed**, 88.39 seconds
  overall: `stage5_unit`, `stage5_projects`, `reliability_gui_flow`.
- Native flow includes failed compilation preserving the old process, repair/restart,
  interactive input, first/subsequent window placement, rapid/identical saves, Stop,
  runtime DLLs/assets/arguments and Debug/Release behavior.
- Formatting applied/checked: clang-format 19.1.5, 83 maintained C++ files.
- `git diff --check` passed. No complete Full/Quick or live dependency suite was run;
  focused verification follows the user's limited-budget instruction.

Logs: `build/reliability-work/autorun-baseline.log` and
`build/reliability-work/reload-tests.log`.

## Measured performance

One disposable local C++20 CMake project was compiled after each source edit using
the same warm build tree. Four before and four after samples alternated order.
No third-party libraries or live downloads were involved. Each warm run was checked
not to report an explicit CMake reconfiguration.

| Warm incremental CLI build | Before (ms) | After (ms) |
| --- | ---: | ---: |
| Round 1 | 4982.86 | 5030.42 |
| Round 2 | 5346.08 | 4667.94 |
| Round 3 | 5102.40 | 4873.73 |
| Round 4 | 5162.96 | 4649.70 |
| Median | 5132.68 | 4770.835 |

Median reduction: **361.845 ms, approximately 7.0%**. One pair was slower; four
samples do not establish a universal improvement or a FarmGame performance claim.
These are total incremental CLI build times, not separately measured compiler/linker,
shutdown/startup/restoration stages or full save-to-window latency. Overall regression
test durations above are not used as performance evidence.

Data/script: `build/reliability-work/reload-benchmark/samples.csv`,
`build/reliability-work/reload-benchmark.log`, and
`build/reliability-work/benchmark-incremental.ps1`. Temporary source/build fixtures
were removed after measurement; logs and samples are retained.

## Manual acceptance

1. Close the older manager normally. Launch:
   `C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab\build\reload-speed\cpplivelab-gui.exe`.
2. Open your usual test project. Run manually, move/resize the window, enable Auto Run,
   and save a source edit. Verify the **first** replacement and two subsequent ones.
3. Once both automatic slots are warm, repeat small source edits. Unchanged dependency
   objects should remain reusable. First-use slot compilation still costs more.
4. Save a syntax error while running: the old application should stay alive. Fix/save,
   then check successful replacement, placement, Stop and interactive input.

Focused repeat command, from the repository root after closing this output's GUI:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -BuildDirectory .\build\reload-speed -TestMode Full -TestFilter '^(stage5_(unit|projects)|reliability_gui_flow)$' -Jobs 4
```

## Deferred

GitLab support was not started in this pass to conserve usage and finish one verified
improvement. Next session: add provider-aware URL/project identity (including nested
GitLab groups), API metadata/pin/archive handling and offline fixtures, while reusing
the current dialogs, uncertainty warning, approvals and transactions. Namespace caches
by provider as well as repository, and preserve existing GitHub records/URLs.

No commits, pushes, system PATH changes, dependency installation or interface redesign.
