# Focused library reliability pass

## Completed

- Initial GitHub release/tag lookup is passive: no source archive, clone, CMake
  configuration or compilation. The existing asynchronous owned worker now has a
  five-second deadline for this phase; HTTP requests share that short lookup budget.
- Published-version metadata is cached for five minutes under manager state in
  `github-metadata`, keyed and checked against the repository. Repeated lookup reuses
  it without remote requests. This is metadata caching, not a claim that arbitrary
  code or every version is compatible.
- Uncertain new GitHub additions display the requested warning and **Try Adding
  Anyway / Cancel**. A failed initial lookup no longer immediately exits Add; the
  user can proceed to the existing version/reference form and retry resolution.
  Repository/version values remain in the current flow. Tags remain optional.
- Confirmed non-library source evidence is blocked with an explanation. Unfamiliar
  evidence can reach the existing supported integration/probe workflow; it is not
  automatically called incompatible. Non-C++ evidence now uses the Unable to Verify
  label with an explicit compatibility-decision explanation.
- Downloads and compile/link verification failures offer explicit Retry/Cancel
  instead of silently returning out of the current Add operation. Retry uses the
  same pin and integration settings. Worker startup/execution exceptions become
  logged Unable to Verify failures. A missing root CMakeLists.txt gives inline
  feedback in the integration form instead of immediately throwing out of it.
- Exact repository/commit approval before executing CMake, hash/pin checks,
  project-change preview/approval, target-type checks and transactional rollback
  remain. Try Adding Anyway does not bypass them or register an unsuccessful probe.

Changed this pass: `scripts/github-library.ps1`, `src/library_workflow.cpp`,
`src/library_workflow.hpp`, `src/library_compatibility.cpp`,
`tests/github_library.ps1`, `tests/ui_audit_tests.cpp`, and this report.
The pre-existing Auto Run optimization changes/report were preserved.

## Diagnosis and limits

The prior code allowed 30 seconds per metadata API request and immediately returned
out of Add on discovery, download, or probe failure. These are confirmed mechanisms
for long waits and flow exit. The exact reason for the user's two-minute Raylib
failure is **not established** without that attempt's Build & System output; no crash,
specific compiler error or network failure is asserted.

The quick check and full integration verification are deliberately distinct. Source
download/extraction, source assessment and explicitly approved compilation/linking
can still take longer than five seconds. The full verification limit remains fifteen
minutes, with cancellation; this pass does not impose a five-second compile timeout.
No uncertain metadata result is converted into fabricated compatibility. Existing
Known Library integrations remain available through the existing catalog workflow.

No live Raylib or network-dependent dependency execution was performed. Core tests
use local fixtures and native dialogs. Complete API/runtime compatibility of a real
library still requires its approved consumer build and manual acceptance.

## Verification and performance

Isolated build succeeded. Final focused tests: **8/8 passed**, **58.16 seconds**;
script exit code **0**. Formatting was applied and checked with clang-format 19.1.5
across 83 maintained C++ files. `git diff --check` passed.

The controlled warm metadata-cache lookup took **38 ms**. This measures cached
script execution in the offline fixture, excluding worker startup and live HTTP.
The native slow-worker test verified cancellation and an Unable to Verify result
within five seconds. No final focused test failed. Full/Quick complete suites,
live Raylib downloads and real-project benchmarks were not run in this limited pass.

Coverage: real local reusable-target compilation/linking; header/native/non-C++/
docs/application/missing-source classification; existing library metadata and
transaction rollback; repeated cached API lookup; controlled slow replies, request
timeout and rate-limit failure; actual native warning accept/cancel; end-to-end Add
through cached metadata to version selection and explicit cancellation; cancellation
of an owned slow worker within five seconds; existing shared dialog regressions.

No before/after real-network or FarmGame build-speed benchmark was run. A controlled
warm cached metadata timing excludes process startup and live network latency.

Logs: `build/reliability-work/library-fix-verified.log` and
`build/library-reliability/Testing/Temporary/LastTest.log`.

## Manual acceptance

1. Close the older manager normally, then launch:
   `C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab\build\library-reliability\cpplivelab-gui.exe`.
2. Use a disposable project. Add Library -> Known / GitHub -> enter your repository.
   The initial metadata phase should stop at roughly five seconds or finish sooner.
   A new unknown integration should show Unable to Verify and the exact warning.
3. Choose Cancel: no library/project integration should be added. Repeat, choose
   Try Adding Anyway, and verify your repository/version selection remains available.
   Repeating the same repository within five minutes should reuse version metadata.
4. Try a branch or exact commit if no tags are available. If lookup fails, your input
   should remain in the version form; retry later or Cancel deliberately.
5. On the permitted library you trust, review the exact pin and separately approve
   verification. This is the slower phase. If it fails, read/copy Build & System and
   check Retry Verification/Cancel. Download failures have Retry Download/Cancel.
6. Approve project changes only after reviewing the normal preview. If integration
   fails, previous dependency configuration should be restored. Verify a previously
   working library still builds. Test edits/removal only on the disposable project.

## Deferred / next session

The FarmGame first-window failure and further build-speed work are unchanged. This
limited-budget pass stops after the focused library change. Next, manually reproduce
the Raylib attempt with its exact Build & System error, confirm retry behavior, and
collect a narrow window-capture/restoration trace for the first FarmGame transition.
Full-library probe acceleration and broader compatibility-result reuse need separate
measurement; no additional dependency/staging architecture was introduced.

No commits, pushes, permanent PATH changes or unrelated user-project edits.
