# Project libraries: management and read-only evidence

Manage Libraries combines the existing catalog and project-managed records with
passive observations. Detection runs in the existing modal background worker when
the dialog opens or Refresh is selected. It is not on the build/save/Auto Run path.
No network request, CMake configure, dependency download, or user-project write is
performed to populate this list.

## States

| Entry | Meaning | Available operations |
|---|---|---|
| Managed | Recorded project integration with manager ownership metadata or explicit managed sections | Existing supported edits, removal, repair; repository version updates for FetchContent |
| Catalog | Known integration, not attached to this project | Existing catalog/cache operations and Add |
| External | Library target reachable from a selected executable in an existing configuration snapshot | Inspect, Add Library, Refresh |
| Unverified | Unevaluated declaration, manifest requirement, stale snapshot, or unreadable metadata | Inspect, Add Library, Refresh |

Ownership and detection confidence are separate fields in `LibraryObservation`.
Compatibility remains the separate existing `LibraryAssessment` concern and is
never inferred from either field. Staleness changes confidence without granting
or removing ownership. Passive detection always reports
compatibility as Unable to Verify. It never reports a successful consumer build.
Versions without reliable managed metadata are Unknown. Package constraints in a
manifest do not establish an installed version. Legacy source text resembling a
known recipe does not grant ownership or authorize automatic adoption.

## Evidence sources

- Read the latest existing CMake File API index in the selected manager build
  directory. Check its source/build identity and selected configuration. Follow the
  selected executable's dependency closure; when no executable is selected, follow
  all executable roots. Repeated target IDs are visited once; unrelated targets
  are omitted. Newer codemodel abstract/imported/interface target entries are read
  when supplied by CMake.
- Recognize stale snapshots when known configuration inputs have changed or are
  missing. The reader consumes cmakeFiles input metadata when already available;
  it never requests regeneration to obtain it.
- Without a usable configuration, show literal find_package, FetchContent_Declare,
  and add_subdirectory declarations as Unverified. Root find_package declarations
  are also retained as hints with older CMake, whose File API omits imported targets.
- Read top-level vcpkg.json dependencies as Unverified requirements. Existing
  vcpkg toolchain configuration and installed-CMake-package integration remain the
  established build mechanisms. This feature neither installs vcpkg ports nor
  introduces another package manager.

Exact managed target identities and package identities suppress corresponding
observations. New additions are checked against exact existing target/package
evidence before project-change previews. Similar names are not treated as identity.
An uncertain existing declaration may conservatively block a duplicate addition;
inspect the original CMake configuration rather than silently adopting it.

## Limits

- A configuration snapshot is historical evidence, not proof of the current
  successful build. Without complete cmakeFiles replies, freshness cannot be fully
  established for arbitrary included scripts outside known directories.
- Older CMake may omit imported/interface targets, aliases, or transitive package
  provenance. Aliases without structured backing-target evidence are not guessed;
  a managed alias and its backing target can remain separate entries.
- Source hints scan only CMakeLists.txt and up to 64 files directly under cmake/,
  with bounded file sizes. They do not evaluate macros, variables, generator
  expressions, nested arbitrary includes, or conditional activation. They do not
  infer libraries from include directories or arbitrary linker flags.
- Manifest detection covers top-level vcpkg requirements, not optional feature
  activation, installed ports, version resolution, or Conan. No bare library name
  is assumed to identify a package uniquely.
- Only the selected existing manager build/profile is inspected. Other IDE build
  directories are not searched recursively. A missing configuration yields hints,
  not an automatic configure operation.
- Unknown/edited legacy dependency layouts stay read-only when ownership cannot
  be established. Existing approved preview/build/rollback remains required for
  supported additions; arbitrary project migration is not provided.

## Manual acceptance

1. Launch `build/compile-workers/EasierCpp-Gui.exe` and open a project. Select the
   intended Debug/Release configuration and executable target.
2. Open Project → Manage Libraries. Expect one selector containing managed,
   catalog, external, and unverified entries, with ownership shown in the labels.
3. Select an external or unverified entry. Expect source, Unknown version where
   appropriate, configuration evidence, and Unable to Verify compatibility. The
  action dropdown must contain only Inspect, Add Library, and Refresh. A stale
  external snapshot retains external ownership while its confidence is stale.
4. In a disposable existing project, open the dialog before configuring. Literal
   dependencies should be unverified hints, including conditional declarations.
   Opening the dialog must not configure, download, or change project files.
5. Through the normal approved Build workflow, configure/build the disposable
   project and reopen or Refresh the dialog. Reachable real library targets should
   appear as external snapshot evidence. A CMakeLists.txt alone is not Compatible.
6. Change the root CMake configuration, then Refresh before rebuilding. Previous
   targets must be marked stale/unverified when the changed input is known. Select
   Release without a Release snapshot: Debug evidence must not be presented as Release.
7. In a disposable generated project, add a trusted GitHub/GitLab/local/package
   library using the existing compatibility and project-change approvals. Build/run
   it. Managed entries should retain supported removal/edit/repair operations.
   Packages/local paths must not expose a misleading automatic Update Version action.
8. Attempt to add an exact existing target/package again. Expect a conflict or
   duplicate error before any project write. Failure/cancellation must leave the
   original project intact. No fuzzy library-name matching is performed.
9. Finally exercise ordinary Build, Run, Stop and Auto Run. Opening or refreshing
   Library Manager must not add network/configuration work to source-save cycles.

For FarmGame, existing configured Raylib targets can supply evidence without
changing FarmGame. If no usable snapshot exists, the display remains unverified
until the normal user-approved build/configure workflow supplies one.

Reference: [CMake File API](https://cmake.org/cmake/help/latest/manual/cmake-file-api.7.html).
