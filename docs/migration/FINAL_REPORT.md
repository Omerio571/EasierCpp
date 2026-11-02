# EasierCpp v0.1.0 — release, migration and cleanup report

Completed 2026-11-02. No commit, push, publication, installer, new dependency or
system-wide PATH change was performed. Real projects including FarmGame were not
edited. Existing uncommitted source/release/audit work was preserved.

## Ready-to-use release

- ZIP: `dist/EasierCpp-v0.1.0-windows-x64-portable.zip` (774001 bytes).
- SHA256: `c9a5b0c705669d3f872e9cb77ba1453399f2310858cbf8e548e3f66162d9bb76`.
- GUI: `dist/EasierCpp-v0.1.0/EasierCpp-Gui.exe`.
- CLI: `dist/EasierCpp-v0.1.0/EasierCpp.exe`.
- Windows 11 x64; Microsoft v14 x64 runtime required. Building C++ projects also
  requires the existing supported VS2022 C++/SDK/CMake/Ninja toolchain. Discovery
  and child developer-environment initialization remain available without PATH edits.
- Copy the ZIP to another computer, extract the complete folder and launch the GUI.
  Keep both executables and resources together. Projects/settings are not embedded
  in the ZIP; select/copy your projects separately. No second-PC/clean-VM claim.

The 12 runtime files include both executables, six first-party support files,
README, release notes, MIT LICENSE and third-party notices. No old executable,
source tree, cache, test controller, personal project or compiler is shipped.
Version comes from CMake PROJECT_VERSION, including Windows executable metadata.

## Naming and compatibility

CMake project/targets, executable paths, GUI/CLI identity, dialogs/test labels,
current instructions, new starter/ownership markers and release packaging use
EasierCpp. There are no duplicate old executable targets or transitional EXE copies.
Namespaces/include paths remain `cpplivelab` as a stable API. Existing environment
and worker protocols, legacy library IDs/cache stamps, File API client names,
Win32 coordination identifiers and serialized placement-version strings remain
compatible. The old portable CMake option is accepted as a deprecated input;
EASIERCPP_PORTABLE is authoritative and conflicting unsafe settings are rejected.

[LEGACY_REFERENCES.csv](LEGACY_REFERENCES.csv) inventories remaining maintained
old-name references by source location/reason. Historical reports, fixture identities
and user/third-party files were not mechanically renamed. New scratch builds use
`.easiercpp`; an existing `.cpplivelab` CLI workspace remains supported. Both scratch
names are ignored by the watcher. New CMake markers are written only during project
creation or an approved edit; readers accept both marker generations.

## Settings and project metadata

Default state is now LOCALAPPDATA/EasierCpp. Supported metadata is copied in complete,
validated units, retaining originals. This includes recent/editor/worker preferences,
known/hidden/retired library records and recipes, project references/configurations,
arguments, working directories, manifests, Auto Run settings, PCH snapshots and
placement files. Existing new units win intact; ambiguous conflicts are retained and
reported in migration-report.txt, linked from the startup output. There is no silent
project/catalog merge. Staging and an exact completion marker support interrupted retries.
A live legacy manager or pending library transaction blocks migration before unsafe work.

Registry preferences use HKCU/Software/EasierCpp/Preferences. Supported typed values
are staged and atomically published. Existing destination preferences win; established
values at the new product root take precedence over CppLiveLab when initializing it.
Original keys are retained. Unsupported old value types retain the previous safe-default
behavior. Tests use isolated keys, not saved user preference values.

CMake/build/download caches, active-slot pointers, process/restart state and cache
registries are not copied into new locations. The first migrated build is cold.
Original PCH/library absolute references can still use the retained original data:
**do not remove the old AppData folder as part of upgrading.** Rollback uses the old
ZIP/original profile; newer changes in the new profile are not automatically merged back.
CPPLIVELAB_STATE_HOME remains supported (and wins if both overrides are defined);
EASIERCPP_STATE_HOME is available. Overrides bypass automatic default-profile migration.

## Build/test discovery and cache recovery

The old integrated cache actually recorded the former Projects/CppLiveLab source/build
paths. Recovery selects/reuses a verified `.easiercpp-N` sibling inside the manager's
build directory; it never transplants or deletes the mismatched cache. Foreign, linked
or outside unverified directories are refused with diagnostics. Valid Debug/Ninja
incremental caches are reused. The original integrated cache SHA256 remained
`5efc168932775e3023321f971eebe36580a513a652a8602d448c0653c6e72700`.
Later authorized cleanup removed its obsolete compiled files but retained that exact
cache and lease; the resolver still selects the same current recovery directory.

Development tests use a validated compiled checkout root, with executable-ancestor
fallback after relocation; they do not depend on CWD or a Debug-specific directory.
Normal Release was previously confused with the existing explicitly portable build
(CPPLIVELAB_PORTABLE=ON), where source scripts are intentionally absent. Portable
builds now provide an explicit Doctor/Toolchain Diagnostics alternative and never
execute source tests merely because a checkout happens to be nearby. Cancellation
and the owned test-process lifecycle remain unchanged.

## Actual verification

| Executed check | Result |
|---|---|
| Final portable Release build + offline Quick | 39/39 passed |
| Final extracted ZIP outside checkout, sanitized terminal PATH | 14/14 passed |
| Actual GUI startup/restart from final dist after cleanup | Passed |
| Final dist CLI version and Doctor | EasierCpp 0.1.0; both exit 0 |
| Debug application service executing source Quick tests | 39/39; runner exit 0 |
| Normal Release build Quick | 39/39 passed |
| Normal Release application service executing source Quick tests | 39/39; runner exit 0 |
| Focused Debug Auto Build, migration/cache/selection | 4/4 passed |
| Focused Release Auto Build | 1/1 passed |
| New/legacy deferred-warning classifier, Debug + Release | 1/1 in each configuration |
| Scoped clang-format and git diff --check | Passed; line-ending notices only |

Extracted checks cover Debug/Release calculator builds and input/output, missing-resource
failure without checkout fallback, portable test diagnostics, actual GUI restart,
interactive I/O/EOF/Stop, Auto Run/A-B/failure retention/recovery, native Safe/Seamless
replacement and placement, UI/library observation, worker optimization/preferences,
and the controlled local-library consumer lifecycle. ZIP bytes remained unchanged.
The final runtime-folder files were hash-matched to the tested package.

The earlier broad offline run was interrupted before a summary after 55 passes and
one delayed-build fixture failure. That fixture mixed old/new scratch paths; it was
corrected and passed focused Debug/Release retests without weakening its assertions.
Its remaining selection/migration tests passed in later focused/Quick runs. A developer
harness Get-FileHash module-import failure and PowerShell-wrapped diagnostic assertion
were also corrected/retested. A final naming audit found the deferred-warning parser
accepted only the old prefix: both names now classify correctly and quoted compiler
errors remain errors. No unresolved observed test failure is being reported as passed.

Unfiltered Full and live third-party library builds were not performed: the existing
restrictions on Raylib/fmt/json downloads/configuration remain. No manual FarmGame or
second-machine acceptance was claimed. Full was not repeatedly rerun to obtain a
clean summary. Detailed successful logs/manifests are in
`docs/release/verification/renamed-v0.1.0`; the original failed/interrupted logs remain
under build for diagnosis.

## Cleanup

22 verified superseded manager build directories were removed (15.55 GiB logical files).
Another 0.37 GiB of obsolete integrated-test outputs were trimmed while preserving the
original cache/lock. Total obsolete build files removed: **15.920 GiB**, not a physical
free-space measurement. Historical screenshots/results/cache records were copied and
hash-validated before removal. Redundant staged release copies and the owned failed
extraction were removed. One original rollback ZIP/checksum was retained.

Current Debug, development Release, portable Release and recovered self-test profiles
remain, as do source, uncommitted work, `.vs`/`.vscode`, AppData, real projects and
unverified experimental/source-containing directories. No file was deleted merely
because an internal identifier looked old or a header appeared unused.
Cleanup targets were bounded to verified workspace build paths, with ownership,
tracked-file, active-process and reparse checks. An interactive guard expression on
an obsolete extracted copy produced a PowerShell type error; that known copy was
removed, the rollback ZIP was verified unchanged, and subsequent cleanup/final-release
checks used corrected terminating guards. No unrelated source or user project was removed.

See `docs/reports/cleanup-2026-11-02/removed-builds.csv`, `trimmed-legacy-build.csv`
and retained-build-evidence for the deletion/evidence record.

## Files and manual acceptance

Major changes: CMake/Windows version resource, GUI/CLI and launch names, state migration
module, preference/restart compatibility, dual project/library marker readers,
watcher/scratch naming, source/test discovery, build-cache guards, release/portable
scripts, regression fixtures and maintained guidance. CHANGED_FILES.txt records the
working tree, including pre-existing release/audit changes preserved in place.

Follow [MANUAL_TESTS.md](MANUAL_TESTS.md). For the portable build, extract the ZIP
outside this checkout, launch EasierCpp-Gui.exe and use Doctor before opening/building
a disposable project. Test real projects yourself before approving the migration.
No later stage, Git commit, push or release publication was started.
