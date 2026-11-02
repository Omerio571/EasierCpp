# EasierCpp identity migration

## Inventory and compatibility boundaries

Maintained identity references span 96 source/header/build/test/script files.
The repository is already EasierCpp; version remains CMake PROJECT_VERSION (0.1.0).

| Classification | References | Policy |
|---|---|---|
| Rename | GUI titles/actions, CLI usage/version/doctor/errors, executable lookup, CMake project/targets, VS Code task, release file names, current instructions | EasierCpp, EasierCpp.exe, EasierCpp-Gui.exe, EasierCppCore |
| Compatibility | cpplivelab namespace/include API; CPPLIVELAB build/test flags; registry and state overrides; legacy CMake markers, library IDs and ownership stamps; .cpplivelab project builds; Win32 window classes/properties, restart mutex and IPC identifiers | Preserve readers/contracts; do not rename mechanically |
| Persistent state | LOCALAPPDATA/CppLiveLab; HKCU/Software/CppLiveLab; project settings, recent.txt, catalog, manifests, PCH snapshots, window/worker/editor preferences | Copy validated metadata conservatively; preserve originals; never transplant build caches |
| Historical | Stage reports, benchmark reports, original release acceptance records and hashes | Preserve original results |
| External/generated | FarmGame, other user projects, downloaded dependencies, build trees, previously generated project configurations | Never rewrite merely to change branding |

## Implementation order

1. Inventory and boundaries (this document).
2. Rename application/build targets, companion paths and branding; compile and Quick/focused native regressions.
3. Safe persistent-state transition and fixture tests; new generated metadata with legacy readers.
4. Owned test-cache validation/recovery, normal Debug/Release source discovery, portable diagnostics.
5. Current instructions and a fresh portable candidate (never overwrite the previous artifact).
6. Focused integration/portable verification and report remaining compatibility identifiers.

Risks: absolute paths in CMake caches and PCH recipes; serialized dependency ownership;
legacy/new managers simultaneously accessing shared data; accidental fallback to a source
checkout in portable builds. Keep cache migration separate from persistent metadata,
retain original data and compatibility locks, and use disposable fixtures for verification.

## Completion

All six checkpoints are implemented and verified to the scope recorded in
[FINAL_REPORT.md](FINAL_REPORT.md). The ready-to-copy release is in dist; follow
[MANUAL_TESTS.md](MANUAL_TESTS.md) for acceptance. Remaining old identifiers are
intentional compatibility/history references; see LEGACY_REFERENCES.csv.
