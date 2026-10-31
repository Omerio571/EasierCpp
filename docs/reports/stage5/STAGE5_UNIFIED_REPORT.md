# Stage 5 unified architecture and reliability report

Date: 2026-10-29. Scope: the unified Stage 5 request. No Stage 6/7, commit or push.

## Existing functionality retained

The code already provided installed-tool discovery, child-only developer environments,
CLI/Win32 GUI, read-only imports, recent projects, presets, target selection through
CMake File API, isolated builds, preferred editors, approved dependency previews and
rollback, console input/output, safe process jobs, Auto Build/debouncing/coalescing,
Intelligent Run, themes, diagnostics and integrated tests. These were reused.

## Generation and CMake

One shared `project_templates::cmake_project` now generates the New Project root,
legacy calculator/hello roots and single-file wrapper. It uses the selected name,
safely normalized for a CMake identifier, C/CXX, project-wide and per-target C++20,
disabled extensions, explicit source lists, include paths and the VS startup target.
The folder/display name remains intact. Lowercase src/include/assets preserves the
approved layout. New sources are created only by New/Create; configuration never
recreates a deleted main.cpp.

Selected dependencies go into `cmake/Dependencies.cmake`; no libraries means no
unnecessary dependency file. Asset copying uses `$<TARGET_FILE_DIR:app>`. Raylib
starters are emitted only for the explicitly selected raysan5/raylib target; arbitrary
libraries do not turn the core project into a graphical application.

Generated C++ and headers are checked with the installed ClangFormat 19.1.5 and the
actual repository .clang-format. Generated CMake is checked for four-space indentation,
no tabs and readable line lengths. Tests cover empty, multi-file, calculator,
fmt/JSON and Raylib starter families. The permanent requirements are centralized in
[GENERATED_PROJECT_STANDARDS.md](GENERATED_PROJECT_STANDARDS.md), referenced by
AGENTS.md and CODING_STYLE.md.

## GitHub input and dependencies

The production form initializer always resets a fresh Add session. URL input is
empty, cue text is not input, prior results/errors are absent, and Retrieve Versions
stays disabled until the URL validates. Native-control tests cover repeated sessions,
valid/invalid input and disabled state. The empty field and disabled button were
also observed in the actual running GUI through the computer-use skill.

Release/tag/commit resolution, network/rate-limit distinctions, explicit verification
approval, actual target probing and rollback remain in the existing workflow. Shared
GitHub snippet generation now keeps the complete commit and SHA256 while using a
short dependency directory key. New archive extraction uses extraction-time timestamps.
Copied legacy registry snippets shorten only their content key, preserving full pins
and leaving existing project files untouched. Multiple selected libraries remain supported.

## Confirmed failure and fix

The unchanged executable reproduced both the throwing recursive iterator failure and
the exact Ninja 100-regeneration failure on a disposable copy of MyCppProject.
Ninja 1.12.1 `-d explain -n` reported:

```text
output CMakeFiles/3.31.6-msvc6/CMakeSystem.cmake of phony edge with no inputs doesn't exist
```

That file actually existed through an extended Windows path at **269 characters**.
The nested full-SHA FetchContent subbuild made the path excessively long. The
reproduction's timestamps were not in the future. A different probe language setup
was found, but it was not established as the cause of this regeneration loop.

The scanner now handles extended Windows paths. Dependency keys are shorter, and
generated configuration uses CMP0168 NEW where available. Manager configure commands
also set CMAKE_POLICY_DEFAULT_CMP0168=NEW, so the legacy full-SHA/CXX-only project can
build without rewriting its CMake. Explicit project policy choices still win.
Direct FetchContent population avoids the failing nested Ninja subbuild. This is
supported by [CMake's CMP0168 documentation](https://cmake.org/cmake/help/v3.31/policy/CMP0168.html).
Timestamp handling is preventative; it is not claimed as this incident's root cause.
Default profiles with FetchContent declarations also use a compact per-build download
directory under manager state (`d/<build-key>`). This avoids legacy full-SHA archive
extraction paths becoming too long without changing project files. Explicit presets
retain their choices. Profiles without FetchContent do not receive an unused cache option.

Evidence retained under ignored build/: unified-raylib-cmake.log,
unified-ninja-explain.log and unified-path-evidence.json. The original MyCppProject
CMake hash and all 24 protected FarmGame file hashes were unchanged.

## Filesystem safety

`directory_scan.hpp` centralizes error-code traversal and extended-path handling,
does not follow reparse points, and reports controlled incomplete-scan errors.
Project inspection, watching and executable checks use it. Watchers exclude _deps
and existing generated-directory names. Missing/disappearing directories and unreadable
sources cannot establish freshness. Editor discovery also uses error-code iteration.
Scheduler sequencing is unchanged: active compilation finishes; saves coalesce into
one follow-up build. No hot reload was added.

## Recovery and executable discovery

Tools > Clean & Rebuild (Fresh Directory) and CLI `manage-rebuild --approve-recovery`
require explicit approval. Recovery checks canonical ownership and current leases,
creates a new owned slot, builds there, and atomically activates it only on success.
It never deletes old outputs or project files. Failure/cancellation preserves the
previous active build and failed-attempt diagnostics. Invalid slot metadata is refused.

Normal and incremental cache mismatches do not trigger automatic cleanup. Incremental
builds additionally validate cached source/generator identity before reuse. Tests
exercise stale caches, source/generator mismatches, refused active builds, failed
recovery, preserved old output/configuration and successful activation.

Existing File API discovery handles Ninja, VS2022, Debug/Release, multiple targets
and custom artifact paths. The selected artifact is now verified after successful
compilation even for Build-only operations. Missing artifacts return distinct exit
76; the GUI shows Executable Missing and refuses Run. Compile failure remains
Build Failed; actual launch remains Running. No stale fallback program is launched.

## Error navigation

Raw build output remains available. The parser recognizes MSVC and Clang/GCC source
coordinates, CMake locations/context, and Ninja/linker errors without inventing a
source location. Compiler codes/messages, Unicode and space-containing paths are
preserved. Build & System supports double-click and Enter on file diagnostics.
Tools > Review Build Errors offers diagnostic selection, full details, Open in
Editor, Review Libraries, Project Settings and explicitly approved recovery.
Failed builds include a concise next-action summary rather than a popup per error.

Custom editor argv supports project/file/line/column placeholders with safe quoting
and no shell. The production GUI keyboard route was tested against an editor capture
fixture and passed exact project, file, line and column arguments. VS file navigation
uses [/Edit](https://learn.microsoft.com/en-us/visualstudio/ide/reference/edit-devenv-exe?view=visualstudio),
which delegates to an existing instance; Code uses reuse-window/goto, CLion uses line.
Actual editor instance selection and exact VS column positioning remain manual checks.

## Files changed in this refinement

- Generation: src/project_templates.hpp, src/projects.cpp, src/workspace_creation.cpp.
- Builds/recovery: include/cpplivelab/workspace.hpp, src/workspace.cpp,
  src/workspace_cli.cpp, src/main.cpp, scripts/file_api.cmake.
- Filesystem safety: new src/directory_scan.hpp, src/auto_build.cpp,
  src/build_lease.hpp, src/windows_support.hpp, src/editors.cpp.
- Diagnostics/UI: new src/build_diagnostics.hpp, src/gui.cpp, src/console_view.cpp,
  src/project_dialogs.cpp/.hpp, src/library_workflow.cpp/.hpp.
- Tests: new tests/unified_tests.cpp, tests/ui_audit_tests.cpp,
  tests/gui_flow_tests.cpp, tests/session_fixture.cpp, tests/test_selection.cmake,
  tests/workspace_test_support.hpp, CMakeLists.txt.
- Documentation: this report, STAGE5_UNIFIED_MANUAL_TESTS.md,
  GENERATED_PROJECT_STANDARDS.md, AGENTS.md, CODING_STYLE.md, README.md, TESTING.md
  and the appended current test record in TEST_RESULTS.md.

The workspace already contained extensive uncommitted Stage 4/5 work. The list above
describes this refinement; it is not a claim that the entire Git diff originated here.

## Executed verification

Targeted runs passed: unified unit/projects/Raylib, production URL state, GUI error
navigation, themed new forms and earlier GUI flow. Real generated Raylib 6.2 resolved
to 02566453880e06bec97f5b8160fa6fe1e98397ca. Debug and Release opened/closed real visible
Windows windows, copied assets, preserved executable timestamps on no-change builds,
detected source edits, rebuilt, reported actual file/line errors, and repaired successfully.
The legacy full-SHA/CXX-only configuration also built without source/CMake rewrites.
Console tests performed actual runs, incremental builds, Release and recovery.

The first full run was 40/41: Stage 2 expected the original project-wide
CMAKE_CXX_STANDARD declaration. It was restored without weakening the test. A subsequent
link attempt could not create ui_audit_tests.exe; no running audit process was found,
and retrying the normal build succeeded without deleting files or closing editors.
Further complete runs exposed fixture PID reuse and long-path cleanup, followed by
MSBuild tracking/archive-extraction limits after making fixture names longer. Fixtures
now reserve compact unique directories atomically and clean extended paths. Compact
dependency storage handles the corresponding legacy archive path issue. All affected
regressions were retested; final unfiltered results below supersede those failed runs.

Final unfiltered commands and results are recorded below after execution:

- Quick: 28/28 passed, 8.87 seconds of CTest wall time.
- Full/Doctor: 41/41 passed, 289.07 seconds of CTest wall time; Doctor returned success.
- After final whitespace/test-output formatting, a rebuild and another unfiltered
  Quick run passed 28/28 in 9.13 seconds.
- Formatting: verified all 60 maintained C++ files with ClangFormat 19.1.5.
- Git diff --check: passed; status reviewed; no staging, commit or push.
- Empty-terminal-PATH smoke: actual New/Build/Run succeeded in Debug and Release;
  the final incremental Debug run reported no work and printed the greeting.

## Manual acceptance and limitations

Follow [STAGE5_UNIFIED_MANUAL_TESTS.md](STAGE5_UNIFIED_MANUAL_TESTS.md) for exact ordinary
PowerShell commands and numbered independent console, GitHub, multiple-library,
Raylib, error-navigation, recovery and regression checks with expected results.

- Full tests require GitHub/network and a Windows desktop/OpenGL driver. Quick is offline.
- Real graphical window existence/normal close is automated; pixel rendering, physical
  mixed-monitor DPI, exact editor placement and human keyboard feel still need acceptance.
- The visible GUI audit verified New/Add forms and empty GitHub input. A complete
  click-by-click Raylib creation/editor session was not claimed; the actual generation
  backend, real API resolution, native controls and generated application were tested.
- CMake 3.31.6 and the installed VS2022/Clang toolchain were exercised. Older CMake's
  fallback and projects explicitly choosing CMP0168 OLD were not verified to fix this
  long-path failure. Existing projects are never silently migrated or reformatted.
- GitHub repositories requiring submodules, bespoke generators or unsupported platform
  dependencies may still need manual integration. A valid URL is not compatibility proof.
- Recovery retains old/failed build slots and consumes extra disk space. It intentionally
  provides no automatic destructive cleanup. External directory permission/reparse races
  fail closed; not every Windows ACL/junction permutation was physically exercised.
- FarmGame was protected and hash-checked, not rebuilt or reorganized in this refinement.

Stop here for manual approval. No commit/push or later stage is authorized by this report.
