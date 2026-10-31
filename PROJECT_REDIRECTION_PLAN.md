# Project redirection — implementation plan and compatibility

The repository was inspected before edits, including AGENTS.md, CODING_STYLE.md,
UI_DESIGN_GUIDELINES.md, GENERATED_PROJECT_STANDARDS.md, watcher/scheduler, freshness,
workspace CLI, build leases, session ownership, library transactions and test setup.
Existing worktree changes were retained. No rewrite, rename, commit or push.

1. Reuse native controls/theme/forms and library controller. Put Add/Manage Libraries
   next to build controls, display project library summary, move manager Restart to
   Tools and reserve a distinct program Restart control. Keep one Settings destination.
2. Extend LibraryRecord with backward-readable integration metadata. Reuse exact-file
   preview, approval, transaction locks, backup/rollback and existing FetchContent
   workflow. Add installed packages, local source targets and header-only interfaces;
   imported package targets also cover sufficiently described precompiled packages.
3. Reuse the watcher/debounce/fingerprints and managed-build CLI. Add alternate
   manager-owned slots and a second supervised build session so linking never
   overwrites a running executable. Activate only a successful current replacement.
4. Capture only known process-tree windows before graceful close. Restore native
   placement after delayed startup, select the recorded monitor or visible fallback,
   verify rectangle/maximization and report partial support after a bounded wait.
5. Extend real controller regression tests and native/offline fixtures. Preserve all
   registered regressions; execute local-only tests per the user's explicit choice.
   Document hardware-dependent window checks and deferred live dependency tests.

Changed components: gui.cpp, library_manager.*, libraries.hpp/cpp, workspace.hpp/cpp,
workspace_cli.cpp, main.cpp, doctor.cpp, CMakeLists.txt, CLI/test-selection/restart/GUI
regressions, README and UI standards. New application_windows.* isolates native
window operations/preferences; new redirection tests and native sample cover this
request. Historical stage reports and public internal identifiers stay compatible.

Migration: metadata schemas1/2 remain readable, schema3 adds method/package/path/
components. Existing FetchContent records default to their original method. The
legacy global Auto Build choice is used until a project has an Auto Run preference.
Project-specific settings remain in external manager metadata, not source trees.
Existing executable names, namespaces, registry/config paths, ownership markers,
CLI command names and library pins remain unchanged. Displayed product/version and
Doctor headings change to Cpp. No project source/assets/presets were edited outside
fresh disposable test/demo projects; approved library actions still require preview.

Graphify remains uninstalled/unused: its separate isolation gate is unresolved.
GitHub/Context7 were used only for public package/API documentation/metadata.
