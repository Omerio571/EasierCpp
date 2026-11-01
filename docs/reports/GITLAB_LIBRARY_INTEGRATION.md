# GitLab library integration

Implemented public gitlab.com support through the existing Library Manager.
Repository detection, anonymous API discovery, tags/releases, explicit refs, exact
commit resolution, SHA256-pinned archives, cache reuse, and CMake generation share
the GitHub workflow. Nested GitLab namespaces are supported. GitHub's stored
identities and public/internal entry-point names remain compatible.

The existing compatibility verifier, multi-target/header-only selection, user
approval, transactional edits, rollback, removal, and repair remain authoritative.
Nothing changes Auto Run, PCH, worker selection, or unrelated projects.

## Files

- `scripts/github-library.ps1`: provider recognition, API routing, errors, stable
  release filtering, and identical pinned archive download URL.
- `src/projects/workspace_creation.cpp`: GitLab catalog acceptance and hash-pinned
  FetchContent generation using the same API archive URL.
- `src/gui/library_workflow.cpp`: shared form accepts GitLab, restores qualified
  identities on management operations, and generates provider-specific catalog IDs.
- `src/gui/library_manager.cpp`, `src/gui/project_dialogs.cpp`: existing workflow
  labels now mention GitLab; no additional provider menu or redesign.
- `tests/gitlab_library.ps1`: controlled offline API/archive tests.
- `tests/libraries/library_management_tests.cpp`: catalog/recipe assertions and a
  GitLab variant of the existing real CMake consumer lifecycle tests.
- `tests/gui/ui_audit_tests.cpp`: native controls accept nested GitLab URLs and
  host-qualified project paths, reject query credentials, preserve fresh empty forms.
- `tests/libraries/library_live_tests.cpp`: follows the renamed shared dialog title;
  the live third-party test itself was not run.
- `CMakeLists.txt`, `tests/test_selection.cmake`: test registration and suite coverage.
- `docs/development/GITLAB_LIBRARIES.md`: formats, behavior, limits, API references,
  and numbered manual acceptance instructions.

## Executed checks

- Standalone offline GitLab and GitHub PowerShell fixture scripts passed in the
  Windows host environment. An initial GitLab `.git` parsing issue was fixed and
  retested. The initial sandbox run failed the cache-reuse check; host execution
  passed and verified actual cache reuse without changing machine configuration.
- Focused local CTest selection: 15/16 passed in 127.69 seconds. Only the test-suite
  registration assertion failed because its expected Quick list lacked the new test.
  That list was corrected and retested in the complete Quick suite below.
- The focused pass includes GitHub consumers (74.62 seconds), GitLab consumers
  (68.77 seconds), configured/unit compatibility, discovery, transactions, caches,
  metadata, native UI audit/forms/layout, watcher, and scheduler checks.
- Final Quick: 36/36 passed in 13.60 seconds, including both providers' API fixtures
  and the corrected test-selection assertion. Doctor passed.
- Existing GUI Build/Auto Run lifecycle regression: 1/1 passed in 77.42 seconds.
- Changed maintained C++ files passed clang-format dry-run; git diff --check passed.
  The build emitted an existing wide-to-narrow conversion warning in the live-test
  fixture, outside the changed title line.

Live GitLab API/download behavior and real external GitLab library builds have not
been executed. No third-party source was configured during this work; only maintained
local fixtures were executed. Full/network suites were not run. No installs, permanent
PATH changes, commits, pushes, or unrelated project edits were performed.

Executable: `C:\Users\omer2\OneDrive\Desktop\Projects\EasierCpp\build\compile-workers\cpplivelab-gui.exe`.

See the development guide for private/self-hosted exclusions, bounded listing,
conservative Latest Stable semantics, and manual acceptance.
