# CppLiveLab development instructions

Work only on the stage or maintenance task the user has approved. Preserve approved
Stage 1–3 behavior. Do not start another stage, commit, or push without the user's
authorization. Finish the authorized checks, report the results, and stop for feedback.

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

- Apply formatting: `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\format.ps1`.
- Check it: use the same command with `-Check`.
- Build/test: `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Doctor`.
  Close the manager GUI before rebuilding its executable; do not force-close Visual
  Studio or unrelated processes. If an owned operation is active, stop it safely first.
- Run targeted regressions for behavior fixes, then the full existing suite. Never
  claim an unexecuted test passed. Report failures and anything not verified.
- Check `git status`, generated-file ignores and `git diff --check`. Do not discard
  existing changes, delete `.vs`, or run destructive Git cleanup.
- Give beginner-friendly manual checks and expected results. Leave the final review,
  commit and push to the user unless they explicitly authorize them.
