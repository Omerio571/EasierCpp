# Unified Library Manager implementation

## Reused functionality and findings

| Area | Existing implementation reused | Verification/boundary |
|---|---|---|
| GitHub and public GitLab | Shared PowerShell discovery/resolution/download, full commit and SHA256 pinning, cached archives | Offline provider fixtures; live provider services and third-party builds not executed |
| Local source and headers | Existing local compatibility assessment and generated CMake integration | Existing compatibility and project/consumer regression fixtures |
| Installed CMake packages | Existing find_package/target integration and version constraints | Reused; no package-manager installation or automatic upgrade mechanism added |
| vcpkg | Existing selected toolchain option and package consumption | New passive manifest requirements; no full port installation/resolution added |
| Compatibility | Existing Compatible/Incompatible/Unable to Verify and approved consumer probes | Detection never substitutes for compatibility validation |
| Project edits | Existing exact previews, owned sections, isolated consumer validation, rollback, removal and repair | External observations never become editable integration records |
| Persistence | Existing catalog, project manifests, project-specific pins and cache registry | No new ownership inferred from similar names or matching source text |
| UI/background jobs | Existing native forms, theme, modal worker, hidden ConsoleSession | Unified selector and ownership-dependent actions; no provider UI redesign |

## New behavior

`LibraryObservation` is separate from managed `LibraryRecord`. Ownership,
detection confidence (declaration/configured/stale/unavailable), and compatibility
are independent. A stale external target retains external ownership, while its
confidence and label become unverified. Passive observations always have untested
compatibility. Unproven installed versions remain Unknown.

Detection reads existing File API codemodel replies, selected configuration and
executable dependency closure, optional cmakeFiles inputs, bounded literal CMake
declarations, and top-level vcpkg manifest requirements. Imported/interface entries
are consumed when the installed CMake supplies them. It performs no configure,
network, dependency install, vendor-tree traversal, or project/build write.

The Library Manager displays managed, catalog, external-snapshot, and unverified
entries together. External/unverified entries expose only Inspect, Add Library,
and Refresh. A dispatch guard also rejects mutation if a stale action is submitted.
The shared form now refreshes dependent ComboBox choices when their backing choices
or selected values change; this prevents stale destructive actions after changing
the selected library. Native tests exercise the actual manager dialog.

Repository Update Version remains available for managed FetchContent integrations.
Local/package integrations retain their existing explicit Edit Integration workflow
instead of exposing a misleading automatic version-update operation.

New additions check exact CMake target and package identities against existing
evidence before the existing approved write workflow. Similar names do not match.
Legacy source matching without recorded ownership or explicit owned sections stays
read-only; automatic adoption/migration is refused. Existing source/configuration
is preserved on detection, cancellation, conflict, and failed verification.

The reader uses an existing hidden job-owned ConsoleSession, with a 15-second bound,
and runs inside the existing modal worker. Failures can be displayed without losing
the managed catalog. Display discovery is never called from Auto Run's save path.

## Files for this task

- `include/cpplivelab/libraries.hpp`: separate ownership/confidence/observation API.
- `src/libraries/library_detection.cpp`: bounded hidden reader and exact conflicts.
- `scripts/detect-project-libraries.ps1`: passive File API/CMake/vcpkg evidence reader.
- `src/gui/library_manager.cpp`, `include/gui/library_manager.hpp`: unified display,
  provenance details, read-only actions, conflict checks and ownership guards.
- `src/gui/project_dialogs.cpp`: native dependent action ComboBox refresh.
- `tests/library_detection.ps1`: controlled metadata and immutable-file tests.
- `tests/libraries/library_management_tests.cpp`: C++ reader/conflict assertions and
  detection against actual configured disposable consumers.
- `tests/gui/ui_audit_tests.cpp`: actual dialog read-only action/source-preservation tests.
- `CMakeLists.txt`, `tests/test_selection.cmake`: detection source/test registration.
- `docs/development/PROJECT_LIBRARY_DETECTION.md`, architecture map, and this report.

The working tree already contained the prior GitLab integration. Those changes
were preserved; the file list above distinguishes this task from carried changes.
No user projects, PATH, installed dependencies, Git commits or remote branches changed.

## Checkpoints and executed verification

1. Foundation: 3/3 focused tests passed, 4.93 seconds. An initial missing declaration
   include prevented compilation; the include was corrected, rebuilt and retested.
2. Unified native display: 4/4 passed, 20.47 seconds.
3. Conflict/ownership/compatibility/rollback: 5/5 passed, 20.46 seconds.
4. Real configured GitHub/GitLab-style offline consumers plus detection: 3/3 passed,
   76.09 seconds. These fixtures build/run maintained local libraries, then exercise
   failed updates, rollback, removal, re-addition, cache deletion and repair.
5. Final Quick after independent confidence fields and hidden process reader:
   37/37 passed, 14.92 seconds. Doctor passed.
6. Final selected offline integration run: 13/13 passed, 372.84 seconds. This includes
   both provider-style consumer lifecycles, configured compatibility, library
   discovery, generated projects, native UI audit, PCH, GUI Auto Run/A-B/replacement
   lifecycle, window management, queue/freshness, Auto Build scheduling, worker
   preferences and actual Optimize Workers execution. This was a filtered local
   integration selection, not the complete Full or live-library suite.
7. Changed maintained C++ passed clang-format dry-run; git diff --check passed.

Final logs are retained under `build/library-final-quick.log` and
`build/library-final-integration.log`. No final test failures remain.

The passive fixture covers root and bounded module declarations, conditional hints,
comment exclusion, local subdirectory hints, duplicate references, unknown versions,
vcpkg requirements, transitive target closure, unused-target exclusion, imported
header-only targets, Debug/Release separation, stale and malformed metadata, and
hash comparisons proving project/build files remain unchanged.

## Limits and deferred work

This is evidence-based visibility, not interpretation of arbitrary CMake. Older
CMake replies may omit aliases/imported/interface targets or package provenance.
Unprovable alias relationships are not merged; a managed alias and its backing
target can appear separately. Without complete configured input metadata, arbitrary
external include changes cannot be fully freshness-checked; snapshot evidence is
explicitly historical. Only the selected manager build is read, not all IDE folders.

Optional vcpkg feature activation, installed-port/version resolution, arbitrary
macro/include evaluation, automatic legacy adoption, private/self-hosted provider
authentication, and another package manager are deferred. Source declarations can
be conditional; exact package conflicts are deliberately conservative. Unsupported
identities remain unknown instead of being guessed.

No live provider-dependent test or real FarmGame modification was performed.
Real-project acceptance remains manual. See the numbered acceptance guide in
`docs/development/PROJECT_LIBRARY_DETECTION.md`.

Executable:
`C:\Users\omer2\OneDrive\Desktop\Projects\EasierCpp\build\compile-workers\cpplivelab-gui.exe`.
