# Experimental zero-modification hot reload: feasibility checkpoint

Reviewed 2026-11-04. Decision: **do not integrate the reviewed Blink backend**.
No runtime feature, dependency, preference, release version or build flag was changed.

## Evidence and compatibility

Reviewed upstream commit `19b1560a8bcfb4ccdcd62cfc52ea904ca868025a`
(latest fetched commit, dated 2023-12-18). Source checkout is disposable and ignored:
`build/hot-reload-feasibility/blink`. This old commit date does not establish that
the project is abandoned, but current VS2022 compatibility is not demonstrated.

- [Blink documentation](https://github.com/crosire/blink) describes MSVC, x86/x64,
  matching PDBs and source availability. It recommends `/ZI` when recovering compiler
  commands fails. This is an upstream claim, not a successful local runtime test.
- `--no-compile PID` watches object files: a possible future bridge to our CMake
  build worker without asking users to add headers or reorganize into DLLs.
  Architecture must match. Modern MSVC/PCH/object-format compatibility remains untested.
- `source/main.cpp` uses remote allocation, writes and remote threads to load its
  runtime into the target. This is code injection, not an out-of-process compiler
  service. No injection was performed during this investigation.
- BSD-2-Clause permits redistribution with the copyright, conditions and disclaimer
  retained in source and binary distribution materials. Nothing was bundled.
- Documented semantics are function-entry redirection: an existing active invocation
  is not rewritten. Existing initialized data is reused rather than reinitialized.
  Changed object layouts, signatures, inlining and optimized code cannot be advertised
  as supported. No local function-change support has been established.

## Reliability stop conditions found in actual source

In `source/blink_linker.cpp` at the pinned commit:

1. Lines 449–451: unknown relocation logs a message and `break`s out of its switch;
   execution proceeds to function redirection and a successful return at lines 456–464.
   Unsupported object input is therefore not reliably rejected before patching.
2. Line 372 updates the persistent `_symbols` map while resolving input. Later
   unresolved symbols can free the new module and return false (for example lines
   315–320), with no corresponding map rollback. A subsequent patch could consult
   stale entries. This is a source-level failure-recovery concern, not a reproduced crash.
3. Thread suspension does not prove that suspended instruction pointers are outside
   the function bytes being overwritten. `write_jump` changes 12 bytes on x64 without
   an instruction-pointer safety check. Optional synchronization callbacks exist, but
   requiring application callbacks would violate the zero-source-change objective.
4. Successfully loaded patch allocations intentionally remain until process exit.
   Long-running repeated reloads need memory-growth measurement and limits.

These findings require backend validation/transactional failure handling and thread
safety work before a reliability-preserving integration. Per the requested budget
and stop rule, no replacement patching engine or backend fork was started.

## Existing architecture and possible future boundary

`src/gui/gui.cpp` already owns watcher dispatch, application launch and candidate
promotion (`begin_program`, `poll_candidate`). `src/projects/workspace.cpp` owns
CMake execution and A/B artifacts. A future optional backend should consume successful
object builds separately from replacement, scoped to the manager-owned PID, and must
never silently restart after a patch failure. It would need explicit readiness,
failure and shutdown contracts. Existing replacement readiness cannot establish that
in-process patching succeeded. None of these components was modified.

## Visual Studio alternative and numeric variables

[Microsoft's native Hot Reload guidance](https://learn.microsoft.com/en-us/visualstudio/debugger/supported-code-changes-cpp)
documents `/ZI`, incremental linking and IDE-controlled debugging, with restrictions
on data/layout changes, optimization and attachment. This is the simplest existing
user workflow to evaluate in Visual Studio; the reviewed documentation does not
provide a standalone redistributable patch-service API for EasierCpp.

Numeric editing is technically plausible using PDB type/location information and a
Windows debugger engine. [DbgEng WriteVirtual](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/dbgeng/nf-dbgeng-idebugdataspaces-writevirtual)
provides writes, but is not itself safe variable discovery or validation. A minimal
editor still needs owned-process identity checks, matching symbols, stopped execution,
addressable numeric type validation, range checks, full-length read/write verification,
resume/exit handling and explicit consent. Locals require valid scope/frame/lifetime;
optimized-out values must be rejected. Changing a source initializer does not update
an existing variable. No variable editor or memory writes were implemented.

## Executed checks and limitations

- Read current repository guidance, architecture map and targeted process code.
- Downloaded and inspected the pinned source; reviewed its small CMake input. Did not
  configure, build or execute third-party code, installers or scripts.
- A PowerShell source check passed: symbol mutation precedes relocation processing,
  and the unsupported-relocation branch breaks rather than returning failure before
  the later success path. This is a static check, **not a runtime hot-reload test**.
- No graphical fixture, same-PID/state assertion, repeated patch, compiler-failure,
  target-exit, recovery or performance benchmark was executed. Safe/Seamless tests
  were not rerun because application code and builds were unchanged.
- Existing release executable remains unchanged. No real user project was touched.

Only this report was added. Git status was clean before the investigation. The
ignored checkout can be removed later by deleting only the explicitly resolved
`build/hot-reload-feasibility` directory; it is not used by EasierCpp.

Further effort: a narrow disposable demonstration might take hours if compatible;
reliable backend hardening/integration is plausibly several days or more, with no
guarantee of universal support. These are estimates, not commitments. A maintained
backend with demonstrable failure isolation should be evaluated before spending
that effort. No additional implementation is required to keep current EasierCpp
working, and no unfinished feature has been exposed to users.
