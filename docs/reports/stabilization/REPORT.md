# Release stabilization checkpoints

Checkpoint 1 complete: reproduced the Windows text-mode CRLF marker mismatch before
fixing it. Binary reads expected LF only; completed imports retried and collided
with the new GUI's intentionally shared legacy instance mutex. Both exact marker
encodings now read correctly; new markers use atomic binary publication. Locks
remain, unknown markers and real Windows errors report accurately.

Executed isolated matrix: Debug 5/5, normal Release 5/5, portable 5/5. Tests cover
fresh/legacy/both/completed/interrupted default profiles, real GUI + companion CLI,
parallel independent Debug/Release Unicode consumer builds, actual original legacy
GUI refusal, registry isolation, restart and single-instance protection. Real user
settings/projects were not changed by these checks. Logs: build/stabilization-*.

Checkpoint 2 focused checks complete: optional formatter discovery is independent
of the selected compiler. It is reported by Doctor but never counts as a compiler
or a project-creation prerequisite. Strict formatting still runs when available;
generated readability assertions also run without it. Formatter/Doctor: 4/4.
The seven reported integration failures passed (299.55 seconds), including Unicode
projects and pinned Raylib Debug/Release graphics. Quick: 39/39 (30.68 seconds).
Unfiltered Full ran once: 61/62 (1021.71 seconds). The sole failure was
compilation_workers: the optimizer correctly aborted when available-memory safety
limits fell during its four-worker trial ("Memory pressure increased; worker setting
unchanged"). No safety limit was removed or weakened. The unchanged focused retry
passed 1/1 in 72.59 seconds (test time 72.58). All cases have now passed across the
Full run and this retry; the original Full run itself remains recorded as 61/62.
The expanded real Raylib test passed in 112.20 seconds. User approved Raylib commit
02566453880e06bec97f5b8160fa6fe1e98397ca and the pinned fmt 11.0.2 / json 3.11.3 Full
integration dependencies in disposable projects; no unpinned dependencies or real
project edits are authorized by that test approval.

Checkpoint 3: reproduced a separate bounded-header failure in 0.54 seconds:
an unrelated oversized header discarded independently verified API/target evidence.
Keep the 128 KiB scan limit, continue collecting evidence after that specific limit,
and stop redundant header scans once public API evidence exists. Missing/unreadable
metadata still fails conservatively; an oversized header without sufficient proof
remains Unable to Verify. Both compatibility regressions passed (14.91 seconds),
including actual CMake consumer compilation/linking. The real Raylib regression now
also runs the Library Manager's target discovery/consumer proof and assessment.
The final GUI failure summary no longer mislabels cancellation/compile/link failures
as configuration failures; original tool diagnostics remain in Build & System.

Checkpoints 4/5 inspection: preserve the existing standard CMake architecture.
New dependency-bearing projects already use cmake/Dependencies.cmake, explicit
targets, C++20 and standard FetchContent/find_package/add_subdirectory linking.
Exact owned blocks, previews, conflict refusal, validation and rollback already
protect user edits. No imported project configuration was rewritten. Library details
already distinguish catalog records from project availability, external ownership,
pins, targets and stale passive observations; these safeguards were retained rather
than replacing the UI or introducing another dependency-file naming convention.

## Manual acceptance

1. Close the old manager and launch the new portable EasierCpp-Gui.exe. Expect
   normal startup; repeated startup and companion CLI actions must not request
   closing the very same EasierCpp instance. Never delete migration data to retry.
2. Create an empty temporary multi-file C++ project outside the EasierCpp repository.
   Build and Run in Debug, then Release. Expect a valid app target and program output.
3. Open Manage Libraries, add a trusted local CMake or header library, approve its
   disposable verification, then attach it to the temporary project's app target.
   Review the preview before authorizing edits. Include/call its API and Build/Run.
   Expect successful compilation/linking; a catalog record alone is not proof.
4. Repeat with raysan5/raylib 6.2 at the approved exact commit above. Approve source
   verification, select raylib, attach it and Build/Run. Expect the graphical window
   and assets. Internet/toolchain/platform requirements still apply.
5. Reopen the manager and verify the records/pins persist. Add a second library,
   build its consumer, remove only one managed library through the preview and
   rebuild. Expect unrelated declarations/source/assets and the other entry retained.
6. Test a deliberately invalid dependency in the temporary project. Expect detailed
   configuration/compile/link output, no false success and safe rollback. External
   observations must remain protected from managed removal.
7. Enable Auto Run, save a valid source change, then a compile error, then repair it.
   Expect replacement on success and the working app retained during a failed build.
   Check interactive input, Stop, Safe/Seamless replacement and native window placement.
8. Extract the entire portable ZIP into a different folder with spaces. Keep the
   resources folder beside both executables. Run Doctor from ordinary PowerShell
   and repeat the temporary project check. Windows 11 x64, the MSVC runtime, and an
   installed supported development toolchain are still required for compilation.

Real FarmGame and other user projects were not modified. Remote provider metadata
unit tests are distinct from live integration of every possible public repository;
arbitrary libraries can still require their own options, dependencies or approval.

## Release verification

Normal Release rebuilt successfully; focused migration/restart/discovery/generation
and actual library compatibility checks passed 9/9 (28.23 seconds).
Portable Release rebuilt successfully; Quick passed 39/39 (27.82 seconds).
The producer verified the exact 12-file runtime payload and absence of the local
checkout path before making the ZIP. Extracted-package acceptance passed 14/14:
ordinary-terminal discovery, Debug/Release builds and interactive input, missing
resources/source-suite guard, actual GUI restart, Auto Run/window restoration,
read-only UI/library audit, worker optimization/override, and local library consumer
operations. The worker check also passed in this extracted package. The ZIP hash
was unchanged by acceptance testing; full logs are in
`build/stabilization-portable-acceptance/`.

Updated portable files (previous release retained):

- GUI: `dist/stabilized-v0.1.0/EasierCpp-v0.1.0/EasierCpp-Gui.exe`
- CLI: `dist/stabilized-v0.1.0/EasierCpp-v0.1.0/EasierCpp.exe`
- ZIP: `dist/stabilized-v0.1.0/EasierCpp-v0.1.0-windows-x64-portable.zip`
- SHA256: `d636fd4eb78c5924880686efeffb8cb0b7e6c8c492b738f755bedf363b66e68d`

Executed commands (repository root; native tests use process-local developer setup):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -BuildDirectory build/stabilization-debug -TestMode Quick -Jobs 1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -BuildDirectory build/stabilization-debug -TestMode Full -Jobs 1 -Doctor
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/release.ps1 -BuildDirectory build/release-v0.1.0 -OutputDirectory dist/stabilized-v0.1.0 -Jobs 1
powershell -NoProfile -ExecutionPolicy Bypass -File tests/portable.ps1 -Archive dist/stabilized-v0.1.0/EasierCpp-v0.1.0-windows-x64-portable.zip -BuildDirectory build/release-v0.1.0 -ReportDirectory build/stabilization-portable-acceptance
```

Release output directories must be fresh; choose a new OutputDirectory when repeating
packaging. Detailed logs remain under `build/stabilization-*.log`. No commit, push,
tag, publication, installer, permanent PATH change, or real-project edit was performed.
