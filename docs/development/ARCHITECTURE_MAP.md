# EasierCpp navigation map

Read AGENTS.md first. This map is an entry point, not a request to read every file.
Search a named symbol in its subsystem, then read a small surrounding section.
For opt-in PCH usage and compatibility boundaries, see [PCH.md](PCH.md).

| Concern | Starting files/symbols | Relevant tests |
|---|---|---|
| CLI | `src/main.cpp`, `src/cli/workspace_cli.cpp::workspace_command` | `cli_*`, `stage2_integration`, `stage5_projects` |
| Identity/state migration | `src/projects/state_migration.cpp`, workspace `state_root`, appearance `migrate_preference_store`; `scripts/test-build-directory.ps1`, tools `find_development_repository` | `identity_migration`, `migration_processes` (isolated GUI/CLI profiles and legacy-runtime guard), `test_cache_migration`, `test_selection`; [migration guide](../migration/RENAMING.md) |
| Tool discovery/SDK | `src/cli/doctor.cpp`, `src/process/process.cpp::developer_environment` | `doctor_unit`, `doctor_without_path`, `stage5_presets` |
| Projects/builds/settings | `src/projects/workspace.cpp::managed_build`, `load_project_options`, `managed_build_directory`; `include/cpplivelab/workspace.hpp` | `tests/projects/stage5_tests.cpp`, `stage5_projects`, `stage5_presets`, `reliability_freshness` |
| CMake target/runtime metadata | `scripts/file_api.cmake`, workspace `executable_targets`, `runtime_environment` | `stage5_projects`, `stage5_unified_projects` |
| Opt-in PCH | `cmake/EasierCppPch.cmake`, `EasierCppPchHook.cmake`, workspace `configure_project_pch`; CLI `pch-config` | `stage5_pch`, `reliability_gui_flow` |
| Compilation workers | `include/build/build_workers.hpp`, `src/build/build_workers.cpp`, managed/legacy build entry points; GUI `optimize_workers` | `redirection_unit`, `compilation_workers`, `reliability_gui_flow`; [guide](BUILD_WORKERS.md) |
| Project creation/libraries | `src/projects/workspace_creation.cpp`, `include/projects/`, `src/libraries/`; `GENERATED_PROJECT_STANDARDS.md` | `stage5_unified_*`, `stage5_library_*`, `library_compatibility_*` |
| Read-only dependency evidence | `src/libraries/library_detection.cpp`, `scripts/detect-project-libraries.ps1`, `LibraryObservation`; [boundaries](PROJECT_LIBRARY_DETECTION.md) | `stage5_library_detection`, `stage5_library_metadata`, `stage5_ui_audit`, configured consumer tests |
| Native GUI | `src/gui/gui.cpp::Application` (large: search methods first), `project_dialogs.cpp`, `library_manager.cpp`, `library_workflow.cpp`; `docs/design/UI_DESIGN_GUIDELINES.md` | `tests/gui/`, `stage5_ui_*`, `reliability_gui_flow` |
| Watcher/Auto Run | `src/build/auto_build.cpp`, GUI `poll_watcher`, `begin_replacement_build`, `poll_auto_run` | `stage4_watcher`, `stage4_scheduler`, `stage4_auto_build`, `reliability_gui_flow` |
| Freshness/locks/processes | `src/build/build_freshness.cpp`, `include/build/build_lease.hpp`, `src/process/session.cpp` | `reliability_freshness`, `reliability_queue`, `stage3_tree`, `reliability_lifecycle` |
| Window placement/preferences | `src/process/application_windows.cpp` (`WindowPlacementMemory`, migration), GUI `track_placement`, `poll_candidate`, restart methods | `redirection_unit`, `redirection_windows`, `window_management`, `reliability_gui_flow` |
| Console/diagnostics | `src/process/console_model.cpp`, `output_decoder.cpp`, `src/gui/console_view.cpp` | `stage4_console_*`, `stage4_channels`, `stage4_prompts`, `maintenance_output` |
| Theme/editor/restart | `src/appearance/`, `src/tools/editors.cpp`, `src/process/restart.cpp` | `stage4_appearance`, `stage5_restart*`, `stage5_ui_audit` |

## Efficient checks

- Example: `rg -n 'symbol' src/projects include/cpplivelab tests/projects`.
- Example focused test: `scripts/build.ps1 -BuildDirectory build/<task> -TestMode Full -TestFilter '^stage5_pch$' -Jobs 1`.
- Quick is offline. Full includes network-dependent tests; keep their authorization
  separate and report the selection. CMakeLists.txt and tests/test_selection.cmake
  define suite membership. Use Full at integration milestones, not after each edit.
- Keep complete logs in build/<task>-logs/. Show counts/time and a short failure tail.
- Existing .gitignore excludes build/out/CMakeFiles/compiled outputs and preserves
  maintained src/build, include/build, tests/build. Do not remove those exceptions.
- .ignore excludes large logs/numeric history from routine searches only. Never delete
  artifacts to reduce search noise; target a generated path explicitly for diagnosis.
- Prefer local current state. Call external documentation/plugins only when needed;
  reuse already-read documentation instead of repeating broad lookups.

These workflow changes do not alter runtime, safety/approval rules, coding standards,
accessibility requirements, build isolation or process ownership.
