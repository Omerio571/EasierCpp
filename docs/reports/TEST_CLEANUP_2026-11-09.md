# Test and repository maintenance

## Inventory and decisions

| Registration | Before | After |
| --- | ---: | ---: |
| Total | 66 | 66 |
| Quick | 39 | 39 |
| Full | 65 | 65 |
| Separate live Libraries | 1 | 1 |

No registered tests were removed, disabled, renamed, or merged. Stage names alone
did not justify removal. The source-discovery work already in progress was kept.
CTest names, commands and labels match the before inventory exactly. No CI
configuration is present locally.

The subsystem map is in tests/README.md. Existing distinct entry points remain
covered, including empty-PATH CLI behavior, process-tree ownership, safe replacement,
manual-list freshness, automatic discovery, PCH, dependency rollback, imported
configuration, settings and migration. Quick intentionally remains a subset of Full.

Available baseline evidence showed Quick 39/39 and an offline Full run with two
old source-list fixture assumptions; both focused corrections passed before this
cleanup. Full network tests remain a separate authorization consideration.

## Portable workflow consolidation

The old portable script copied source GUI controllers beside an extracted package.
These rendered their own GUI and repeated source-suite scenarios. Its development
guard also expected portable-compiled test controllers, conflicting with the current
policy of building controllers in Development.

| Previous script check | Current protection |
| --- | --- |
| portable-development-tests | Native --public controller verifies absent internal controls; extracted CLI still rejects missing resources without checkout fallback |
| packaged-gui-flow | reliability_gui_flow in Full; actual extracted GUI now exercises public Build/Run, I/O, Auto Run and Stop |
| packaged-window-geometry | window_geometry and window_management in Full |
| packaged-initial-auto-run | initial_auto_run in Full and actual public Auto Run smoke |
| packaged-ui-library-audit | stage5_ui_audit in Full and actual public inline-library checks |
| packaged-workers | compilation_workers in Full; public smoke checks Optimize Workers remains available |
| Two packaged performance benchmarks | Preserved behind explicit -Performance |

No native assertions were deleted. The ordinary portable acceptance workflow now
uses the actual public GUI through restart_gui_tests.exe --public and retains the
relocated local-library consumer check. Successful teardown no longer depends on
having run a performance benchmark. Optional benchmarks were not executed for this
maintenance task.

## Artifact cleanup

- Non-destructively untracked 40 generated logs, one inherited-environment dump,
  and 32 byte-identical screenshots. All 73 local files remain on disk and are
  ignored. The deletions shown in the Git index represent untracking.
- Screenshot representatives remain tracked. The SHA-256 alias map is in
  cleanup-2026-11-02/retained-build-evidence/SCREENSHOT_ALIASES.csv.
- Removed 105,982,341 bytes of generated files from the superseded
  build/ui-v0.1.3 tree. Referenced ui-repair-screenshots/ and redirection-ui/
  evidence remains. The current Development build stays at build/ui-v0.1.3-final.
- Removed the disposable public verification build after its package passed.
  Its private test archive and logs remain under ignored build/cleanup-audit/.
- Retained architectural reports, distinct measurements, versioned release records,
  the approved UI reference, and the legacy executable used by migration tests.
  No tracked compiled executable, DLL, object or PDB was found.

Untracking removed 994,436 bytes (about 0.95 MiB, 16.9% of the previous tracked
working-file footprint) of generated evidence. New maintained maps and notes offset
a small part of that reduction. Historical Git objects were not rewritten or
pruned; the initial packed history was approximately 8.5 MiB.

The root ignore rules now cover CTest Testing/, logs, temporary files, the inherited
environment dump and a local cache directory. Existing source/build exceptions are
retained. Exact screenshot aliases have a scoped ignore file. All 73 untracked
artifacts match ignore rules; legitimate source files and the test map remain
trackable.

## Verification and boundaries

The Development build succeeded. Quick passed 39/39 in 38.73 s. Offline Full
passed 63/63 in 548.59 s, including project_source_discovery, the corrected
freshness fixtures, actual GUI lifecycle, PCH, compilation workers, migration
and local dependency integrations. The two downloaded Full tests and the separate
live Libraries suite were excluded. No native test changes were needed.

The updated private portable package passed all 13 standard acceptance checks,
including checksum, version, runtime resources, Debug/Release CLI I/O, actual public
GUI, and local dependency consumption. Its ZIP remained unchanged by testing.
Published archives were not modified.

No commit, push, release, tag or history rewrite was performed. Existing Issue 1
source changes remain in the working tree. Other historical Release/SDK build
trees are retained for comparisons and references. The stale integrated-test cache
is retained for the existing non-destructive recovery workflow. Physical hardware,
clean-machine behavior, live dependency downloads and optional performance runs
were outside this cleanup.
