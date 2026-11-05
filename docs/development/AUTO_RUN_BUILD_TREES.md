# Auto Run compilation trees

Auto Run reuses the original managed compilation tree and one inactive alternate.
It never copies CMake caches, objects, PCH, PDB or ILK between directories. Each tree
keeps independent dependency tracking and linker state. Existing leases, freshness,
failure handling and Safe/Seamless handoff remain authoritative.

`replacement_build_options` in `src/projects/workspace.cpp` prefers a cached inactive
original, then an existing inactive A or B. Without a cached peer, it chooses A when
running the original, otherwise the original. Normal new-project rotation is
`original -> fresh-auto-a -> original -> fresh-auto-a`.

Existing A/B-only projects retain their warm pair. Recovery pointers remain valid.
Historical third trees are not deleted. Project/configuration/preset/toolchain/PCH
identities remain unchanged. Cache presence is a selection hint: normal worker
validation and CMake/Ninja checks still decide whether a build is valid. Invalid
caches report existing recovery guidance instead of being copied or erased.

The atomic `.active` pointer now accepts `base` for the original directory alongside
existing `fresh-*` values. Empty `recovery_slot` still follows the pointer; explicit
`base` bypasses it. Transient slot overrides are not serialized as project options.
Activation still requires executable metadata and write-through atomic replacement.

Older binaries do not understand `base`. Do not mix versions on one state directory.
To downgrade: close the manager and managed applications, back up the relevant
`.active` file, and remove only a pointer whose entire content is `base`. An absent
pointer selects the original directory. Do not delete compilation trees; use isolated
state for version comparisons.

Prewarming is not enabled. The first alternate can still be cold; this change removes
the second redundant cold replacement. Prewarming needs separate scheduling,
cancellation, dependency-authorization and resource-policy validation.

## Manual acceptance

1. Launch `build/release-v0.1.2/EasierCpp-Gui.exe`. Open a disposable CMake project,
   enable Auto Run and wait for its application. Source/configuration stays unchanged.
2. Save a real source edit. The running application survives compilation, then is
   replaced. The first alternate may require a full build.
3. Save another edit. It returns to the original compilation tree, reusing PCH and
   dependencies. Further saves alternate between the same two trees.
4. Introduce a compiler error: the current application remains running. Repair/save:
   only the corrected build launches.
5. Edit a shared header: all required sources rebuild; unrelated PCH remains cached.
6. Test Safe and Seamless separately, including your window position/size preferences.
7. Close/reopen EasierCpp, then save twice. Both trees remain incremental. Repeat
   after a manual Windows reboot; reboot persistence has not been automated.
8. Check Debug/Release, target selection, arguments/assets/DLLs, Stop, Restart,
   rapid saves and project switching. Stop pauses launch; cancellation never promotes
   stale outputs.
9. For a legacy A/B/recovery project, record directories first. None should be renamed
   or deleted, and warm caches should remain reusable.

A reboot may cool OS filesystem caches without deleting persistent build artifacts.
