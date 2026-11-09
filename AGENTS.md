# EasierCpp development instructions

Work only on the stage or maintenance task the user has approved. Preserve approved
Stage 1–3 behavior. Do not start another stage, commit, or push without the user's
authorization. Finish the authorized checks, report the results, and stop for feedback.

For generation, dependency dialogs, traversal, artifact discovery, error navigation
and build recovery, follow [GENERATED_PROJECT_STANDARDS.md](GENERATED_PROJECT_STANDARDS.md).
All future templates inherit CODING_STYLE.md and .clang-format. Fresh GitHub Add
fields start empty. Preserve imported projects and require explicit recovery approval.

For ALL UI work, read and follow [UI_DESIGN_GUIDELINES.md](docs/design/UI_DESIGN_GUIDELINES.md)
before implementation. This applies to every future stage. Reuse shared Win32 styling,
keep the approved sidebar and one settings destination, preserve existing preference
and console behavior, and add relevant native-control and manual acceptance tests.

## Efficient navigation

Start with `git status --short` and [the architecture map](docs/development/ARCHITECTURE_MAP.md).
Read only guidance/source sections relevant to the change; no full-repository or
historical-report reading is required. Use targeted `rg -n` symbol searches, then small
surrounding sections. Respect search ignores; use `--no-ignore` only on a specific
generated/dependency path under investigation. Avoid unnecessary plugins and repeated
documentation lookups. Save verbose output under build/ and show concise results or
the relevant failure tail.

## Readable C++20

- Readability matters more than line count. Use Allman braces, four spaces, no tabs,
  and blank lines between functions and distinct operations.
- Follow `.clang-format` and `CODING_STYLE.md`. Expand function/conditional bodies;
  do not pack several statements onto one line. Aim for 100 columns.
- Keep existing public APIs, command names, configuration keys and control IDs stable.
  Prefer descriptive local/private names; do not rename merely to add length.
- Explain ownership, lifetime, Windows API choices, asynchronous I/O, failure paths,
  tool discovery and build sequencing. Avoid comments that just restate a statement.
- A Windows HANDLE is an owned resource reference. Explain who closes it and when.
  Use RAII for CloseHandle resources; fonts and COM objects require their own cleanup
  APIs. Keep buffers/events alive until asynchronous operations complete or cancel.
- Keep the architecture small. Extract helpers with one clear purpose, not layers of
  unnecessary abstraction. Do not rewrite reliable behavior for cosmetic reasons.

## Preserve the working application

- Keep the CLI and native GUI, Debug/Release builds, console I/O, VS2022 integration,
  safe Stop behavior and existing project layouts working.
- Discover tools instead of assuming PATH. Developer setup must affect child processes
  only. Do not permanently change the user's environment.
- Keep user projects/builds outside the manager source/build tree. Never overwrite a
  user's source/configuration unnecessarily. Preserve existing editor settings and demos.
- Do not touch unrelated files or generated/third-party code when formatting.
- Never log or save credentials, tokens, private keys or full environment blocks.

## Verification and handoff

- Use [tests/README.md](tests/README.md) to find existing coverage before adding
  tests. Keep safety/compatibility regressions, review duplicate or retired cases,
  and store generated evidence in ignored build directories. Maintained reports
  remain separate from disposable logs.

- Follow [RELEASE_POLICY.md](docs/release/RELEASE_POLICY.md) when preparing a
  public distribution. Publication requires separate owner approval.

- Format changed maintained C++ with discovered clang-format (`-i`, then
  `--dry-run --Werror` on those files). Full `scripts/format.ps1`/`-Check` remains
  available for repository-wide work; do not reformat unrelated edits.
- Build/test: `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Doctor`.
  Close the manager GUI before rebuilding its executable; do not force-close Visual
  Studio or unrelated processes. If an owned operation is active, stop it safely first.
- Run focused regressions while iterating, Quick at component/integration boundaries,
  and applicable Full at integration milestones or broad lifecycle/toolchain/build
  changes. Do not repeat full suites for localized/docs-only follow-ups without a
  concrete reason. Respect network-test approval restrictions and report filtered
  scope precisely. Never claim an unexecuted test passed.
- Check `git status`, generated-file ignores and `git diff --check`. Do not discard
  existing changes, delete `.vs`, or run destructive Git cleanup.
- Give beginner-friendly manual checks and expected results. Leave the final review,
  commit and push to the user unless they explicitly authorize them.

## Permanent Stage 5 workflow constraints

Keep the approved Home, Workspace, Libraries, Project, Tools and Settings sidebar.
Preserve the no-project welcome state. Shared form layout must survive resize, DPI and repeated Advanced
Options toggles with erased parent borders and regenerated regions. Keep help subtle
and keyboard accessible. GitHub metadata discovery is passive; configuring third-party
code requires explicit repository/commit approval. Preview and separately approve any
existing-project edit; back up and roll back failed dependency changes. Test tools use
owned background jobs and isolated output, never the selected project's build directory.
Found tools are not proof of compiler/SDK compatibility. See UI_DESIGN_GUIDELINES.md.
