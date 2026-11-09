# EasierCpp test map

Keep existing test names stable for scripts and CTest filters. Stage numbers in
names describe their origin; they do not imply that the behavior is obsolete.

| Area | Tests and fixtures |
| --- | --- |
| Project generation | projects/unified_tests.cpp, source_discovery_tests.cpp; stage5_unified_unit, stage5_unified_projects, project_source_discovery |
| Toolchain and CLI | cli/doctor_tests.cpp, doctor_no_path.cmake, cli.cmake, stage2.ps1; empty-PATH discovery and independent wrappers |
| Build and Auto Run | build/stage4_tests.cpp, gui/gui_flow_tests.cpp; watcher, scheduler, initial_auto_run, compilation_workers, reliability_gui_flow |
| Process and replacement | process/session_tests.cpp, reliability_tests.cpp, redirection_tests.cpp, restart_tests.cpp; owned trees, I/O, locks, freshness, window_management, window_geometry |
| GUI and settings | gui/ui_audit_tests.cpp, ui_forms_tests.cpp, console_view_tests.cpp, restart_gui_tests.cpp; navigation, native help, themes, persistence, inline forms |
| Libraries | libraries/; metadata, compatibility, approvals, transactions, rollback, caches, local consumers and host-specific workflows |
| Migration and filesystem safety | projects/migration_tests.cpp, maintenance/, test_build_directory.ps1; data compatibility, path boundaries, recovery, ownership |
| Public distribution | portable.ps1 plus restart_gui_tests.exe --public; actual extracted CLI/GUI, missing resources, public capabilities and local library consumption |

## Running tests

Run scripts/build.ps1 -TestMode Quick -Jobs 4 for fast offline checks.
Run scripts/build.ps1 -TestMode Full -Jobs 4 for the comprehensive suite.
Full contains downloaded dependency tests. The separately approved Libraries label
exercises additional live dependencies. Use TestFilter with exact test names for
local changes. For an offline Full run from a VS2022 Developer PowerShell with
CMake and Ninja available, use the configured Development build:

```powershell
ctest --test-dir <development-build> -L "^full$" -E "^(stage5_libraries|stage5_unified_raylib)$" --output-on-failure --parallel 2 --no-tests=error
```

Report this selection as offline Full, with the two exclusions.

Quick membership is explicitly reviewed in test_selection.cmake. Full includes
all regular registrations; live third-party tests remain separate. The CTest
JSON listing is the current registration inventory. There is no configured CI
workflow in this repository.

Public acceptance uses Development controllers and an independently built public
ZIP. Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/portable.ps1 -Archive <zip> -BuildDirectory <development-build> -ReportDirectory <ignored-report-directory>
```

The native controller launches the actual extracted public executable. Source GUI
controllers do not substitute for the public GUI. -Performance additionally runs
the existing replacement and two-tree benchmarks. Standard acceptance skips
these benchmark repetitions. Internal tests stay in Development builds.

## Maintenance

- Add meaningful regressions for new behavior. Avoid duplicating a scenario without
  a distinct contract, entry point, or runtime context.
- Retire tests when their behavior is deliberately removed. Preserve unique safety,
  compatibility and rollback assertions before consolidating overlapping tests.
- Keep Quick offline and fast. Reserve compiler, GUI lifecycle, network and benchmark
  work for Full, Libraries, or explicitly requested performance runs.
- Use isolated disposable projects and preferences. Protect imported projects and
  keep teardown restricted to owned paths.
- Store logs, executables, CTest output and temporary benchmarks under ignored build/
  directories. Commit maintained reports and useful representative measurements.
- Review old evidence periodically. Prefer non-destructive untracking for useful
  local logs and exact duplicates, with a retained representative and an alias map.

Historical reports describe the code and test results at their recorded revision.
Current test commands and package behavior are documented here and in RELEASE_POLICY.md.
