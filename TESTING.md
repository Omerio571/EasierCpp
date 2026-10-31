# Quick, Full and live-library testing

Close CppLiveLab before rebuilding. Run these commands in ordinary PowerShell:

```powershell
Set-Location 'C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab'
# Everyday checks: builds the manager and runs 32 fast tests.
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Quick -Jobs 4
# Approval: builds the manager and runs all 48 regular regressions.
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Full -Jobs 4 -Doctor
# Original serial behavior; Full is the default.
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Doctor
```

The script discovers Visual Studio/CMake/Ninja and imports the developer environment
only into its process. It does not permanently modify PATH. `-Jobs` controls independent
CTest workers, not the application's build queue; the default is 1. Four workers were
tested on this 12-logical-processor machine. More workers are not a promised speedup.
`-Timing` prints manager SDK setup, configuration, compilation and test durations.

Quick is for daily feedback. Stage approval requires **unfiltered Full**, including
integration tests. `-TestFilter` remains available for targeted retests; the script
explicitly identifies filtered runs as incomplete approval coverage.

## Exact selections

Quick runs these 32 tests. Full runs all of them too:

```text
reliability_inheritance
reliability_lifecycle
stage4_console_view
stage4_watcher
stage4_scheduler
stage4_console_model
stage4_channels
stage4_appearance
stage4_prompts
doctor_unit
maintenance_paths
maintenance_output
maintenance_handles
maintenance_arguments
stage3_session
stage3_tree
doctor_without_path
cli_version
cli_help
cli_unknown
cli_extra
cli_doctor
test_selection
stage5_unit
stage5_ui_forms
stage5_github_versions
stage5_ui_layout
stage5_unified_unit
stage5_restart
stage5_library_metadata
stage5_library_transactions
stage5_library_caches
```

Full additionally runs these 16 integration/stress tests:

```text
reliability_gui_flow
reliability_queue
reliability_freshness
stage4_auto_build
maintenance_unicode_project
stage3_projects
stage2_integration
stage5_projects
stage5_presets
stage5_libraries
stage5_ui_audit
stage5_unified_projects
stage5_unified_raylib
stage5_library_discovery
stage5_library_consumers
stage5_restart_gui
```

All 30 approved Stage 4 tests remain, with their lifecycle/stress assertions. Stage 5
adds project/preset/library, native forms/audits, GitHub fixtures and unified
generation/recovery/Raylib registrations.
`test_selection` verifies actual
CTest labels, exact Quick membership, Full/live-suite classification, no
disabled tests, and the script's Full default. Its metadata listing is isolated from
the parent run so it cannot overwrite that run's logs.

In an already configured developer terminal, CTest itself supports:

```powershell
ctest --preset windows-debug -L '^quick$' --no-tests=error
ctest --preset windows-debug -L '^full$' --parallel 4 --no-tests=error
ctest --preset windows-debug -N -L '^quick$'
ctest --preset windows-debug -N -L '^full$'
```

CTest writes detailed results to `build/debug/Testing/Temporary/LastTest.log`.
Stage 5 fixtures use separate temporary source and manager-state roots. The real
fmt/JSON integration tests require network access to pinned, checksummed upstream archives.
`stage5_ui_forms` covers shared themes, dynamic library rows, help/advanced controls,
recent actions and high-DPI scrolling. Existing GUI tests cover navigation/routing,
resize, consoles, persistent debounce and the unchanged build/process lifecycle.
Do not run separate CTest invocations concurrently against the same build directory.

## Optional timing investigation

After the normal build, run this opt-in benchmark without other tests/builds running:

```powershell
.\build\debug\performance_probe.exe (Join-Path (Get-Location) 'build\stage4-timing.csv') $env:TEMP (Split-Path (Get-Location))
```

It measures discovery/SDK setup, repeated process launches, cold/warm CMake
configuration, cold/incremental/no-op compilation, watcher notifications/fingerprints,
the unchanged debounce, CLI cold/warm builds and output draining after root exit.
It uses new temporary fixture folders in local TEMP and the parent Projects folder,
then removes only its own fixtures. Developer environment values are never logged.
The probe is built with tests but is deliberately not part of Quick or Full.

See [STAGE4_FINAL_REFINEMENTS.md](STAGE4_FINAL_REFINEMENTS.md) for measured findings,
remaining limitations and numbered manual checks.

## Inside the application

Tools > Run CppLiveLab Tests offers Quick/Full and 1–16 workers (default 4).
The same script/CTest runs in `build/integrated-tests`, independently of the active
project and normal manager binaries. Actual output/exit/duration go to Build & System.
Tools > Cancel Running Tests stops only its owned test job. A per-build script lease
rejects overlapping sessions; suite descendants cannot recursively launch suites.

`stage5_github_versions` uses controlled API fixtures, with no external network.
Additional transaction/overlap tests live in stage5_unit; native form toggles/DPI are
in stage5_ui_forms; test-worker cancellation alongside an interactive program is in
reliability_gui_flow. Existing library integration builds in Full still require network.

Optional explicit integration driver (not registered inside CTest):

```powershell
.\build\debug\workflow_probe.exe Quick
.\build\debug\workflow_probe.exe Full
```

It exercises the production runner command and owned process against a real suite;
keeping it outside CTest avoids recursion. The complete desktop acceptance guide is
[STAGE5_CONSOLIDATED_MANUAL_TESTS.md](STAGE5_CONSOLIDATED_MANUAL_TESTS.md).

## Stage 5 UI audit

Full additionally runs `stage5_ui_audit` (production modal-handler stress). Quick
includes the faster `stage5_ui_layout`; shared form pixel/paint/long-text checks
remain in `stage5_ui_forms`. Current totals: Quick 32, Full 48, plus one explicit live Libraries suite. Run the interactive
isolated audit host or reproduce native PNGs as described in UI_AUDIT_MANUAL_TESTS.md.
See UI_AUDIT_REPORT.md for actual desktop versus automated versus pending evidence.

## Unified generation and recovery

`stage5_unified_unit` is offline and checks generated C++ with the discovered
ClangFormat and repository style, CMake formatting, diagnostics, safe editor argv,
long/missing/disappearing/unreadable sources and recovery authorization.
`stage5_unified_projects` performs real console, stale/mismatched cache, missing
artifact, Debug/Release, incremental and fresh-slot recovery checks.
`stage5_unified_raylib` resolves/downloads real Raylib 6.2, creates through the
production New Project backend, builds Debug/Release, opens/closes real windows,
checks assets/no-change builds/source watching/errors/repair and builds the legacy
full-SHA/CXX-only configuration without changing it. Full requires network and an
interactive Windows desktop/OpenGL driver for this test; Quick requires neither.
See [the numbered acceptance guide](STAGE5_UNIFIED_MANUAL_TESTS.md) and
[permanent generation standards](GENERATED_PROJECT_STANDARDS.md).

## Stage 5 final library management and restart

See [STAGE5_FINAL_MANUAL_TESTS.md](STAGE5_FINAL_MANUAL_TESTS.md) for complete manual acceptance.
The current implementation and actual verification report is STAGE5_FINAL_REPORT.md.

The additional live suite is deliberately opt-in:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Libraries -Jobs 1
```

It drives production GitHub Add forms for Raylib, SFML, fmt, JSON, GLFW and SDL3,
then configures/links/runs generated consumers, observes windows, and exercises
multiple-library/version/target workflows. Quick remains offline. Full retains
the original network regressions; no previous test was removed or disabled.
New offline tests cover aliases/interfaces, CRLF metadata, exact pins, legacy
catalog migration, failed real configure/link rollback, shared DLL runtime paths,
deletion/repair, locked files, real junctions and restart safe boundaries.
The real GUI restart test runs serially within Full to avoid desktop-test overlap.
Consumer source files are in tests/consumers. Copy examples only into disposable projects.
