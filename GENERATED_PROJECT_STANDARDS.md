# Generated projects and build reliability

These requirements apply to every future template and code-generation feature.
They supplement CODING_STYLE.md and UI_DESIGN_GUIDELINES.md.

## Generation

- Generated C++ must pass the repository `.clang-format`: Allman braces, four
  spaces, readable statements and descriptive names. Imported user code is never
  reformatted automatically. Run `stage5_unified_unit` to check all starter families.
- `project_templates::cmake_project` is the shared root CMake generator for New
  Project, the calculator and the single-file wrapper. New managed projects use
  `src/`, `include/`, `assets/`, and optional `cmake/Dependencies.cmake`. Lowercase
  directories retain compatibility with approved templates; imported layouts stay intact.
- Use the chosen folder name as the CMake project identifier. Unsupported UTF-8
  bytes, spaces and punctuation become underscores; a leading digit gets `Project_`.
  The folder/display name is preserved. The executable target stays `app` for compatibility.
- Declare C and CXX, require C++20 per target, disable extensions and set the VS
  startup target. Use explicit source lists. Add/remove source entries deliberately;
  do not recursively glob build/dependency trees or recreate deleted source files.
- CMake uses four-space indentation, quoted paths, one command per logical
  operation, multiline source/dependency declarations and meaningful section comments.
- New managed projects copy assets with `$<TARGET_FILE_DIR:app>` after linking.
  Imported projects retain their own asset and working-directory conventions.

## Dependencies and fresh Add sessions

- A fresh Add GitHub Repository form has empty actual URL input, no version or
  repository results, neutral validation and disabled retrieval until the URL is
  valid. A cue banner is not input. Never prefill Raylib or a previous Add session.
- Metadata discovery is passive. Downloaded CMake requires explicit approval before
  verification; existing-project edits additionally require preview/approval and rollback.
- Pin the complete resolved commit and archive SHA256. Keep content directory keys
  short; do not put a full commit in deeply nested FetchContent directory names.
- New-project dependencies live in `cmake/Dependencies.cmake`, with recognizable
  managed comments. No dependency file is needed without selected libraries.
- Prefer FetchContent direct population (`CMP0168 NEW`) where available. Manager
  configure commands set its policy default without editing imported CMake; explicit
  project policy settings remain authoritative. New snippets use extraction-time
  timestamps, not archived timestamps. Neither setting substitutes for testing.
- Default profiles with discovered FetchContent declarations use a compact per-build
  dependency directory under manager state (`d/<build-key>`), keeping legacy full-SHA
  project files unchanged. Explicit presets retain their dependency-directory choices.
- Arbitrary GitHub repositories are not necessarily compatible libraries. Actual
  configured targets and real consumer builds verify compatibility. Probe success
  alone is not generated-project success.

## Traversal, artifacts and recovery

- Traverse with error codes, extended Windows paths and controlled error reporting.
  Do not follow reparse points. Exclude generated build/dependency directories from
  source watching. Incomplete/unreadable source scans cannot prove freshness.
- Use CMake File API artifacts, including configuration-specific/custom paths.
  A successful compiler exit with a missing selected artifact is a distinct failure
  (exit 76), and must not run an older executable.
- Never infer a safe deletion target from a folder called build/out. Recovery is
  explicitly approved and creates a fresh manager-owned build slot. Reject escaped
  paths and active leases; activate the new slot only after success. Keep prior
  outputs, failed recovery diagnostics and all project files. No automatic destructive
  cleanup or retry loop is permitted. Old slots consume space until reviewed manually.
- Keep the approved scheduler: an active build finishes; later saves coalesce into
  one follow-up build. This is ordinary rebuilding, not hot reload.

## Actionable output

- Preserve raw output. Parse MSVC/Clang/GCC file, line, column, code/message and
  CMake locations. Recognize build-system/linker errors without inventing locations.
- Double-click a file diagnostic or press Enter on its line in Build & System.
  Tools > Review Build Errors provides a keyboard-accessible selection and actions
  for editor, libraries, project settings, detailed output and approved fresh recovery.
- Custom editor arguments support `{project}`, `{file}`, `{line}` and `{column}`
  as separate, safely quoted argv entries without a shell. VS uses `/Edit` for file
  navigation, Code uses `--reuse-window --goto`, and CLion uses `--line`.
  Exact Visual Studio column navigation and choosing among multiple VS instances
  are editor limitations; do not claim them verified without observing them.

## Verification

Quick must remain offline. Full includes real downloads and disposable console and
Raylib 6.2 projects, Debug/Release builds, actual graphical window creation/closure,
incremental builds, errors/repair and recovery. All regression labels remain enabled.
Use STAGE5_UNIFIED_MANUAL_TESTS.md for human acceptance. Never test writes on FarmGame.
