# EasierCPP v0.1.0 — local portable release candidate

Historical v0.1.0 results. For the current v0.1.1 candidate, see
[V0.1.1_FINAL_VERIFICATION.md](V0.1.1_FINAL_VERIFICATION.md).

## Status

The local Windows x64 Release candidate has passed the executed checks below.
**Nothing has been published, committed, tagged or pushed.** No confirmed Critical
or High functional blocker remains in the tested scope. Owner acceptance and
validation on other machines remain separate steps; this is not production or
cross-machine certification.

Work resumed at the outstanding packaged worker check. Previously completed build,
Quick and packaged checks were reused rather than rerun. The worker diagnostic
rerun passed, then the remaining local-library consumer check passed. All 12 staged
package files were subsequently hash-matched against that tested extraction, and
the ZIP checksum remained unchanged.

## Artifacts

- Release GUI: `C:\Users\omer2\OneDrive\Desktop\Projects\EasierCpp\build\release-v0.1.0\cpplivelab-gui.exe`
- Staged GUI: `C:\Users\omer2\OneDrive\Desktop\Projects\EasierCpp\build\portable-v0.1.0\EasierCPP-v0.1.0\cpplivelab-gui.exe`
- ZIP: `C:\Users\omer2\OneDrive\Desktop\Projects\EasierCpp\build\portable-v0.1.0\EasierCPP-v0.1.0-windows-x64-portable.zip`
- SHA-256 sidecar: the ZIP path followed by `.sha256`.

| Artifact | Bytes |
|---|---:|
| Portable ZIP | 750879 |
| Release GUI | 1128448 |
| Release CLI | 517632 |
| Extracted package, 12 files | 1687518 |

ZIP SHA-256:

```
031de6d4f03e30ca1798605f5d4660755ba00446faf3c0bbe2baf38ee1d2c9a0
```

Contents beneath `EasierCPP-v0.1.0/`:

```
cpplivelab-gui.exe
cpplivelab.exe
README.md
RELEASE_NOTES.md
LICENSE
THIRD_PARTY_NOTICES.md
resources/cmake/EasierCppPch.cmake
resources/cmake/EasierCppPchHook.cmake
resources/scripts/file_api.cmake
resources/scripts/discover_targets.cmake
resources/scripts/github-library.ps1
resources/scripts/detect-project-libraries.ps1
```

The support scripts are intentionally distributed first-party runtime resources.
There are no C++ implementation sources, test binaries, caches, PCH/object files,
PDBs, audit reports, user projects, proprietary runtime DLLs or development tools in
the ZIP. Executable names were retained to preserve sibling CLI/restart behavior.

## Readiness audit and source changes

The original clean Release GUI/CLI build succeeded, but runtime script/PCH/File API
paths depended on a compiled absolute checkout location. That was a distribution
blocker. The fix adds executable-relative `resources` lookup and uses it for runtime
CMake and library helpers. `CPPLIVELAB_PORTABLE=ON` omits the checkout definition
entirely; ordinary development builds retain checkout fallback and their source
test command. Portable ownership guards use the application directory.

The package scan found no checkout-path strings in UTF-8/UTF-16 content. A negative
test temporarily removed support resources in the extraction: the packaged CLI
failed with the re-extract guidance rather than falling back to the still-accessible
checkout. It therefore does not need the repository for normal runtime operations.

Changes are limited to:

- `CMakeLists.txt`: portable option, required resource staging/install rules, and a
  generated version file reused by packaging. Existing PROJECT_VERSION remains
  authoritative for CLI/package version 0.1.0.
- `include/tools/tool_workflows.hpp`, `src/tools/tool_workflows.cpp`: runtime lookup
  and development-only source-suite behavior.
- `src/projects/workspace.cpp`, `src/projects/projects.cpp`, `src/process/session.cpp`,
  `src/libraries/library_detection.cpp`, `src/gui/library_workflow.cpp`: consume the
  relocatable support paths and retain the existing project ownership boundaries.
- `scripts/release.ps1`: separate Release build, Quick checks, strict 12-file
  allowlist, checkout-path scan, local ZIP and checksum. It refuses existing output
  and another project's/non-Release build cache.
- `tests/portable.ps1`: extraction-based acceptance with isolated settings/caches
  and ordinary-terminal tool discovery.
- `tests/gui/gui_flow_tests.cpp`: test-only optimizer completion synchronization and
  failure output, so failed checks expose Build & System details instead of only a
  generic assertion. No optimizer/application behavior changed.
- `LICENSE`: MIT, copyright 2026 Omer Sertkan, as explicitly requested.
- `THIRD_PARTY_NOTICES.md`, release user guide/notes/report and README entry point.

No runtime-linking flags were changed: MSVC remains dynamically linked. There are
no external assets/icons to distribute; the native interface uses Windows resources
and system fonts. Auto Run, A/B build ownership, PCH, worker policy and replacement
visual behavior were not redesigned.

## Prerequisites and data

Launch requires Windows 11 x64 and the MSVC v14 x64 runtime, at least as recent as
the 14.44 build toolset. Dependency inspection identified MSVCP140.dll,
VCRUNTIME140.dll, VCRUNTIME140_1.dll, UCRT and normal Windows DLLs; none is silently
bundled or installed. Use the official Microsoft runtime download linked in README.

Compiling projects separately requires the VS 2022/Build Tools Desktop C++ workload,
Windows SDK, CMake and Ninja. Discovery uses installed paths and child-only SDK
setup; no developer shell or permanent PATH change is needed. Windows PowerShell
handles library metadata. Project-specific tools/dependencies keep their own terms.

Normal cache/settings/build data stays in `%LOCALAPPDATA%\CppLiveLab` and relevant
preferences under `HKCU\Software\CppLiveLab`. Portable means a relocatable app
folder, not a bundled compiler or settings-on-USB mode. No normal app operation
requires writing into a protected installation folder.

## Executed tests

1. Clean Release GUI/CLI build: passed, in a separate directory.
2. Relocatable Release application/test build: passed.
3. Release Quick: **37/37 passed, 26.41 seconds**.
4. All **13 packaged acceptance checks completed across the initial and focused
   resumed runs**:
   - CLI version.
   - Doctor with SDK variables cleared and CMake/Ninja/MSVC absent from terminal PATH.
   - New calculator creation outside the package.
   - Debug build and interactive run.
   - Release build and interactive run.
   - Missing resources fail clearly with no checkout fallback.
   - Actual extracted GUI single-instance/restart/settings/normal-close checks.
   - Extracted runtime GUI-controller flow: existing CMake projects, target/config
     changes, interactive I/O, Auto Run, PCH, A/B, failure retention, Safe/Seamless
     and native window placement.
   - Extracted native UI/library ownership audit.
   - Worker selector/persistence, cancellation, controlled failure, actual synthetic
     optimization/recommendation and manual override.
   - Maintained local-library consumer build/run, update failure/rollback, removal,
     re-addition, cache repair and source preservation.
5. Release-script refusal of existing output and Debug development build reuse:
   both passed; no files/output were overwritten.
6. ZIP SHA-256 unchanged and all 12 tested payload files match staging: passed.
7. Changed maintained C++ formatting and git diff whitespace checks: passed.

The initial optimizer automation assertion failed without retaining its underlying
reason. Test diagnostics/completion synchronization were improved, and the focused
rerun performed a real benchmark successfully: one worker cold/warm medians
6840.99/971.912 ms; two workers 3776.27/795.773 ms; two recommended and applied.
No reproducible product defect was established. The earlier failure remains an
intermittent acceptance-test observation, not a silently erased result; manually
confirm optimizer behavior when testing on another machine.

The extended controller executables were copied into the temporary extraction for
testing only, then removed with the owned fixture. They are not ZIP contents.
The actual product GUI and CLI were also exercised directly from that extraction.
The versioned ZIP was never modified by tests.

Logs: `build/release-package.log`, `build/portable-acceptance/`, and the release safety
guard logs. `passed-final.txt`, `package-manifest.csv`, and `final-summary.json` in
that acceptance directory consolidate the completed results.

## Not performed / remaining limits

- No second Windows installation/VM, clean machine lacking the runtime/SDK,
  ARM64/x86, or every antivirus/policy combination was tested. The local host is
  Windows 11 Home build 26200 x64. Missing-tool unit cases are not a clean-machine test.
- No live GitHub/GitLab library download/configure or real FarmGame modification.
  Controlled provider fixtures and a maintained local library were used.
- No signing, installer, publication, Git tag or remote changes. Normal downloaded-
  executable trust prompts remain possible.
- Existing PCH/CMake caches may retain an old support-hook path after moving the
  application. Use the existing explicitly approved Clean & Rebuild recovery when
  needed; no arbitrary user-project migration is performed.
- External-library detection remains bounded/unverified where evidence is incomplete.
  Exclusive-instance graphical applications may need Safe replacement. Window
  placement is not memory/game-state restoration.
- Source test-suite execution requires the checkout and is deliberately not shipped
  as a portable end-user test framework. The normal runtime features remain shipped.

## Manual acceptance

1. Stop/close any existing EasierCPP development GUI first, so its single-instance
   handling cannot bring an older executable forward.
2. Extract the provided ZIP to a fresh writable folder containing spaces. Launch
   `EasierCPP-v0.1.0\cpplivelab-gui.exe`; keep both EXEs and resources together.
3. Run the extracted `cpplivelab.exe doctor` from ordinary PowerShell. Expect tools
   discovered independently of PATH. Install missing prerequisites through their
   official distributions; this ZIP is not a compiler installer.
4. Create/open a disposable project outside the app folder. Build/run Debug and
   Release; for the calculator, Ada + 2 + 3 should display `Hello, Ada! Sum: 5`.
5. Enable Auto Run, edit/save, and expect updated output. Introduce a compile error
   while a program is running: the previous instance should remain. Repair/save.
   Stop should disarm relaunch until Run/toggling Auto Run resumes it.
6. With a disposable graphical project, test Safe/Seamless and moved-window
   restoration. Use Safe for an application that cannot run two instances.
7. Inspect an external dependency: no active Remove/Update/Repair actions. Test a
   trusted managed addition only after reviewing its execution/project approvals.
8. Set a valid worker count, restart and verify persistence. Run/cancel Optimize
   Workers once if desired. Close normally; user projects must remain intact.

The package's README and RELEASE_NOTES contain the shorter end-user guide.
For another local package without overwriting this artifact:

```
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\release.ps1 -OutputDirectory build/portable-review
```

Review and approval of public distribution remain with the owner.
