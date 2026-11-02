> Historical checkpoint snapshot. Superseded by [FINAL_REPORT.md](FINAL_REPORT.md).

# Renaming checkpoint report — 2026-11-01

Status: checkpoints 1–2 implemented. Checkpoints 3–6 are pending; this is not
completion of the entire compatibility migration. Stop at this verified boundary
before introducing persistent-data changes, as permitted by the request.

## Completed

- Inventory/classification and the remaining plan: RENAMING.md.
- CMake project EasierCpp; targets EasierCppCore, EasierCpp, EasierCpp-Gui.
- Output files EasierCpp.exe and EasierCpp-Gui.exe. No duplicate legacy builds.
- GUI title/welcome/tagline, test/restart menus and dialogs, CLI version/help/doctor,
  errors, VS Code task, and library HTTP client identification use EasierCpp.
- GUI build/run/library/worker/restart subprocess lookup uses the renamed companion.
- Tests and release/portable scripts reference the new filenames; packaging still
  derives its version from CMake. Packaging execution is a later checkpoint.
- Native GUI test now asserts the actual process window title starts with EasierCpp.
- Source and historical release/audit work already present in the working tree was
  preserved. No user projects or existing state were edited, and no Git writes ran.

## Deliberately retained

- Public cpplivelab namespace and include/cpplivelab interfaces, existing CPPLIVELAB
  configuration/test environment variables and CMake options. These are compatibility
  contracts, not visible application branding. Avoid an unnecessary API-wide rename.
- Legacy Win32 class/property/mutex/pipe names, so instance discovery and existing
  restart/coordination behavior do not change during this checkpoint.
- LOCALAPPDATA/CppLiveLab and HKCU/Software/CppLiveLab remain active **for now**.
  No automatic copy, merge, deletion, or migration marker has been introduced.
- Project CMake markers, .cpplivelab folders, catalog IDs and cache ownership stamps
  remain readable/writable under their existing contracts until checkpoint 3.
- Historical reports, old artifacts and their original test results/hashes are
  historical records. Previously built executables were not deleted or renamed.

## Remaining work

1. Copy/validate supported persistent metadata into EasierCpp, retain originals,
   implement deterministic conflict reporting and interrupted-migration fixtures.
   Regenerate disposable builds; do not copy absolute-path CMake caches.
2. New-project metadata identity with dual legacy readers; authorized ownership
   boundaries only. Preserve existing project files and saved library integrations.
3. Owned stale test-cache detection/recovery; source-root discovery and actual test
   execution checks from Debug and normal Release; clear portable diagnostics.
4. Finish current documentation and produce a **new** portable candidate directory,
   containing only the two renamed binaries and required runtime resources.
5. Migration/portable acceptance and justified integration regression tests.

The earlier portable v0.1.0 ZIP was preserved. No renamed ZIP has been produced
or verified by this checkpoint. Do not interpret the earlier release report as
verification of the newly named executables.

## Manual checks

From the EasierCpp repository in ordinary PowerShell:

```powershell
.\build\rename-debug\EasierCpp.exe --version
.\build\rename-debug\EasierCpp.exe doctor
.\build\rename-debug\EasierCpp-Gui.exe
```

Expect EasierCpp 0.1.0, tool discovery independent of terminal PATH, and the
EasierCpp — Make Your Life Easier title. Keep both executables and resources
beside each other. In a disposable C++ project, Build/Run, enter console input,
enable Auto Run and save a real source change, then Stop. The companion CLI
must start successfully and the existing restart/window behavior should remain.
Release equivalents are under build/rename-release. Close the manager before
rebuilding it. Do not launch old binaries from earlier build directories by mistake.

## Files changed in this checkpoint

- Build/launch: CMakeLists.txt, .vscode/tasks.json, scripts/build.ps1,
  scripts/release.ps1, tests/portable.ps1.
- Application: src/main.cpp, src/cli/doctor.cpp, src/gui/gui.cpp,
  src/process/session.cpp, src/projects/projects.cpp, include/build/build_lease.hpp,
  scripts/github-library.ps1 (HTTP User-Agent only).
- Regression references/assertions: tests/cli.cmake,
  tests/build/performance_probe.cpp, tests/process/reliability_tests.cpp,
  tests/gui/gui_flow_tests.cpp, tests/gui/ui_audit_tests.cpp,
  tests/gui/restart_gui_tests.cpp.
- Current instructions/reporting: README.md, docs/release/README.md,
  docs/release/RELEASE_NOTES.md, docs/migration/RENAMING.md, this report.

Other modified/untracked files were present before this checkpoint and were retained.
The original release/audit reports and verification records were not rewritten.
