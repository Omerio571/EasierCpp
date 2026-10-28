# Post-Stage 3 maintenance review

This is a readability, maintenance and QA pass. Stage 4 was not implemented.
The CLI commands, public core APIs, GUI control IDs and version remain unchanged.

## A. Readability improvements

- All 19 maintained C++ source/header files follow the validated Allman style: four
  spaces, no tabs, expanded bodies, separated functions and approximately 100 columns.
- Private/local names now describe the resource or responsibility: `job_handle`,
  `running_process_handle`, `child_stdin_read_handle`, `build_output_directory`,
  `compiler_executable`, `active_project_directory` and `operation_in_progress`.
- Windows comments explain HANDLE ownership, failure values, thread/process handles,
  pipe inheritance, EOF, suspended startup, job assignment, async write lifetime,
  cancellation and why GUI polling must not block.
- The asynchronous input-pipe setup and the three Stage 3 test workflows are named
  helpers instead of unrelated blocks inside one long entry-point function.
- `src/windows_support.hpp` consolidates the unchanged Windows argument-quoting
  algorithm and CloseHandle owner. Quoting is checked through real child processes.
- `src/project_templates.hpp` replaces dense escaped source fragments with readable
  new-project examples. Their prompts/results are unchanged. Existing user examples
  were not rewritten or reformatted.
- `.clang-format`, `scripts/format.ps1`, `AGENTS.md` and `CODING_STYLE.md` establish
  the convention for later approved work. The formatter excludes generated/external
  directories and never selects projects outside this repository.

## B. Git fixes and checks

- `.gitignore` retains user-specific CMake/VS exclusions and adds missing CMake/Ninja,
  object/library/debug-output and `.cpplivelab` rules. Duplicate directory patterns
  were consolidated; `.vs/` already covers that directory at every depth.
- `.gitattributes` defines predictable text line endings, including LF for C++ and
  CRLF for PowerShell scripts. It does not replace the existing editor task settings.
- `git status` works without `.vsidx` permission errors while the local `.vs` data
  remains in place. No Visual Studio or unrelated process was force-closed.
- `git ls-files .vs` and `git ls-files -ci --exclude-standard` return no files.
  There was nothing to remove from the index; no `git rm` was necessary.
- `git check-ignore -v` confirms `.vs` indexes, build executables, PDB files, CMake
  caches, compile commands and user-project build artifacts are ignored.
- Required CMake files, templates, headers and test sources are not ignored.
- The final `git diff --check` succeeds. No files are staged. A credential-signature
  scan of review candidates found no matches; inspect your staged diff before committing.
- The pre-existing `.vscode/tasks.json` is byte-for-byte identical to the maintenance
  baseline. Its unrelated user edits remain available for optional review/staging.

## C. Confirmed behavior fixes

1. **Projects on another drive or UNC root.** The previous `lexically_relative` guard
   treated an empty result as "inside the manager". Different roots legitimately
   produce an empty result, so valid external projects could be rejected. Both CLI
   and GUI now use one case-insensitive component-prefix check after canonicalization.
   Sibling names, `..`, trailing separators and manager-tree rejection remain covered.
2. **Split UTF-8 program output.** A pipe can end a read in the middle of a character.
   The previous per-chunk conversion fell back to ANSI for the incomplete bytes and
   corrupted the display. A small decoder retains the incomplete suffix across reads,
   flushes it on EOF/Stop, and resets between operations. Legacy complete-text fallback
   remains available; this does not add a terminal emulator.
3. **Unicode diagnostic paths.** `path.string()` depends on the system ANSI code page
   and can fail for a legitimate Windows project/tool path. Diagnostic paths now use
   UTF-8. A real project in a Unicode-named folder was created, selected, built and run.

No architectural rewrite, watcher, hot reload, library manager or restoration feature
was introduced. The existing per-configuration lock and owned-job Stop behavior remain.

## D. Executed automated results

Final command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Doctor
```

**16 tests passed; 0 failed; 75.45 seconds; command exit 0.** All original 11 tests remain.
The five new cases are `maintenance_paths`, `maintenance_output`, `maintenance_handles`,
`maintenance_arguments` and `maintenance_unicode_project`. The full initial rebuild
compiled all changed product sources and both the CLI and GUI; the final run rebuilt
the last test-only readability changes before executing the complete suite again.

Formatting was applied and checked with `scripts/format.ps1` / `-Check` using
**clang-format 19.1.5**, with **19 maintained files verified**. Configuration parsing
also succeeded before formatting. Direct ordinary-terminal `--version` and `doctor`
both returned exit 0; installed tools were found even with terminal availability No.
Detailed per-test results are in [TEST_RESULTS.md](TEST_RESULTS.md).

## E. Independent manual testing

Follow [MAINTENANCE_MANUAL_TESTS.md](MAINTENANCE_MANUAL_TESTS.md): 16 beginner-friendly
checks cover building, version, ordinary-shell doctor, GUI, Stage3Calculator, Debug,
interactive sum 6.25, Stop/rebuild, Release, Visual Studio, readable source, formatting
and Git. Your independent maintenance approval is still required.

## F. Limits of verification

- The final GUI executable was rebuilt; the complete human GUI walkthrough was not
  repeated during this maintenance pass. The original process/project regression
  tests passed, and the numbered independent walkthrough is the next approval gate.
- Cross-drive and UNC containment are tested deterministically as Windows paths.
  An actual second-volume or remote-share compilation was not executed.
- MSVC x64 on Windows is the verified compiler. Other compilers were not build-tested.
- Console input remains line-based; output is plain text with bounded history, not
  ANSI/full-screen terminal emulation. Stop remains forceful within the owned job.
- No test proves the absence of every possible bug; report a failing manual step and
  its output before deciding to commit or proceed.

## G. Safe Git checkpoint

Use [GIT_CHECKPOINT.md](GIT_CHECKPOINT.md) after your manual review. Nothing was staged,
committed or pushed automatically. Include the still-untracked approved Stage 2/3
sources so the checkpoint is complete; generated directories stay excluded.

Review classification relative to the **start of this maintenance pass**:

| Category | Files / scope |
| --- | --- |
| Formatting only | `src/main.cpp` |
| Comments, names and organization; no intended behavior change | Public headers, `src/process.cpp`, shared quoting/handle helper, readable project templates, existing discovery/session tests |
| Product behavior fixes | Directory guard and UTF-8 path diagnostics in `src/projects.cpp` / `src/session.cpp` / `src/doctor.cpp`; streaming decoder used by `src/gui.cpp` |
| Regression/build changes | `tests/maintenance_tests.cpp`, fixture argument mode, corresponding CMake target/tests and decoder source |
| New documentation/configuration | `.clang-format`, `.gitattributes`, `AGENTS.md`, `CODING_STYLE.md`, formatter script and maintenance review/manual/Git guides |
| Updated documentation/configuration | `.gitignore`, README, test report, current-test-count wording in prior manual guides |
| Pre-existing unrelated edit, preserved | `.vscode/tasks.json` |

Git's diff against HEAD also contains approved, uncommitted earlier stages. A local
pre-maintenance review snapshot is kept under ignored `build/maintenance/before` to
distinguish this pass from those earlier changes; it is not a product checkpoint feature.

## H. Next-stage readiness

The automated foundation is ready for your independent maintenance testing. Proceeding
to Stage 4 still requires your explicit approval and Stage 4 instructions. No Stage 4
work has begun.
