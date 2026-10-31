# Stage 5 implementation report

This records the initial implementation. The subsequent refinement and multi-library
catalog are documented in [STAGE5_UI_REPORT.md](STAGE5_UI_REPORT.md); the complete
current acceptance guide is [STAGE5_UI_MANUAL_TESTS.md](STAGE5_UI_MANUAL_TESTS.md).
Stage 5 still awaits manual approval.

## Scope and architecture

Stage 5 retains the native Win32 GUI, CLI build workers, owned process jobs,
debounce/coalescing scheduler, console input, independent consoles and custom palettes.
It adds a separate managed-project workflow rather than converting imported projects.
No Stage 6/7 functionality, debugger, custom editor or embedded graphics host was added.

Opening/inspecting a CMake source folder does not execute CMake or change its contents.
The GUI stores recent projects, editor preferences and per-project/configuration
settings in `%LOCALAPPDATA%\CppLiveLab`. Managed builds are under that directory,
with separate caches for source folder, Debug/Release, configure preset and vcpkg root.
`CPPLIVELAB_STATE_HOME` optionally overrides that manager-owned location; it must not
be inside an imported source folder. Automated tests use isolated temporary state.

## Implemented capabilities

- Read-only inspection finds source/header files without requiring src/include layout,
  preset files, asset directories and CMake dependency declarations. Evidence is
  labelled as evidence, not a claim that a dependency is installed. URLs are not logged
  because they can embed credentials.
- CMake's File API resolves executable target names and actual artifact paths after
  explicit configuration, including renamed executables and custom output subfolders.
  Multiple programs require selection; the manager does not guess a project-name EXE.
  Configured library target names/types are shown after successful builds.
- Build/Run respect selected configure presets (including inheritance), project flags
  and compiler/toolchain choices. Only selected configuration and an isolated binary
  directory are supplied. Supported generators are Ninja, Ninja Multi-Config and
  Visual Studio 17 2022. Program working directory defaults to the project root and
  can be overridden in Project Settings. No project presets are rewritten.
- Ninja dependency probes retain intelligent Run. Multi-Config probes select the
  configuration-specific Ninja file. VS-generated projects use a real incremental
  build before Run because Ninja cannot inspect an MSBuild graph.
- Project > New Project generates a new empty C++20 or multi-file C++20 project with
  src/include/assets folders. Multi-file includes a separately compiled greeting.
  Creation validates names/reserved devices and publishes a completed staging folder
  without replacing an existing destination. Projects are standard standalone CMake.
- Optional **fmt 11.0.2** integration for new projects uses the official release archive
  with SHA-256 `6cb1e6d37bdcb756dbbe59be438790db409cdb4868c66e888d5df9f13f7c027f`.
  The archive was downloaded and its checksum measured, then real generated projects
  were configured, compiled, linked and executed using `fmt::fmt`.
- The curated Git URL `https://github.com/fmtlib/fmt.git` is recognized by `new --library`
  and mapped to that same pinned integration. Arbitrary Git repositories are rejected
  before creation; CMake support and target names are not guessed.
- An explicit installed vcpkg toolchain root can be selected without editing source
  configuration. Existing preset toolchains take precedence: the manager refuses to
  override one with a separate vcpkg root. Missing roots are diagnosed before setup.
- Preferred Editor offers VS2022, VS Code, CLion and Custom. Common VS/VS Code/CLion
  locations are searched; an explicit executable override is available. Arguments are
  separate lines, `{project}` is substituted, and Windows arguments are quoted without
  a shell. Changing editors does not change the compiler or manager build directory.
- Recent Projects supports reopen and removal from the list without deleting files.
  Target, preset, working directory, optional vcpkg root and last configuration persist
  per project. Switching cancels old pending requests/watchers, and an owned active
  build/program must finish or Stop before switching.

## Existing-project protection and limitations

CppLiveLab does not rewrite imported source, CMake, presets, dependencies or assets.
It does not move or rename imported files, use an IDE's existing binary directory,
or automatically remove libraries. Executables configured outside the isolated
directory are refused before compilation to avoid overwriting existing outputs.

CMake itself is executable project code, not a read-only data format. Static inspection
flags common file-writing commands, including FarmGame's commands, and Build/Run
requires explicit authorization for that session before configuring those projects.
Authorization is not persisted. The check is advisory and conservative, not a security
sandbox or a complete CMake interpreter; review imported scripts before executing them.
Opening alone performs no configuration. External IDEs may create their own metadata
when the user explicitly opens them; the manager does not create it on their behalf.

The original `create`, `build` and `run` CLI behavior remains for Stage 1–4 compatibility,
including their previously documented `.cpplivelab` generated output directories.
Use the GUI or `manage-build`/`manage-run` for the new fully external build workflow.

Only CMake source folders are supported. Solution-only/MSBuild, Make-only and other
build systems are diagnosed rather than converted. Select the folder actually
containing CMakeLists.txt. Debug and Release are the selectable configurations; other
custom configuration names are not implemented. Preset names are entered in Project
Settings; use CMake's own `--list-presets` command to enumerate complex included presets.
After externally renaming targets, reopen the project to refresh the cached selector.

Library integration is intentionally narrow. Adding/removing dependencies in imported
projects is read-only here; no speculative source/CMake rewriting is provided. New
projects can select fmt at creation. A graphical/Raylib template, arbitrary-Git target
integration and automatic removal are deferred rather than advertised as validated.
Existing Raylib CMake projects can use their own dependency setup, subject to normal
compiler/network/project compatibility. Full's real fmt test needs download access.

No vcpkg installation was available for an end-to-end package-manager test: path/error
validation is tested, while real vcpkg package acquisition remains unverified. Editor
launching is tested with an actual capture executable and Unicode/space arguments;
real VS Code/CLion UI opening and external debugging require manual verification on
installations that provide them. Custom paths cover nonstandard/Toolbox installations.

## Files added/changed for Stage 5

New:

- `include/cpplivelab/workspace.hpp`
- `src/workspace.cpp`, `src/workspace_creation.cpp`, `src/editors.cpp`
- `src/workspace_cli.hpp/.cpp`, `src/project_dialogs.hpp/.cpp`
- `scripts/file_api.cmake`
- `tests/stage5_tests.cpp`, `tests/workspace_test_support.hpp`
- `STAGE5_REPORT.md`, `STAGE5_MANUAL_TESTS.md`

Extended:

- `src/main.cpp`, `src/gui.cpp`: additive CLI commands, native project/editor forms,
  target selector and managed build/run dispatch.
- `src/build_freshness.hpp/.cpp`: external output-directory identities and VS-generated
  build observations. Scheduler, watcher and process/session ownership code is reused.
- `src/project_paths.hpp`: C++20 UTF-8 path conversion for Unicode metadata.
- `tests/gui_flow_tests.cpp`: retain previous behavior checks using actual managed
  artifact paths; add pending/active build switching and dropdown usability coverage.
- `tests/session_fixture.cpp`: editor argument-capture mode.
- `CMakeLists.txt`, `tests/test_selection.cmake`: four new registrations; all existing
  tests retain Full membership. Quick adds only the fast Stage 5 unit fixture.
- README, TESTING and TEST_RESULTS: current commands, coverage and manual handoff.

Earlier uncommitted Stage 4 work is preserved; this list describes Stage 5 additions,
not every line currently different from Git HEAD. No automatic commit or push.

## Testing and FarmGame status

Four Stage 5 registrations exercise actual behavior:

1. `stage5_unit`: read-only inspection, invalid names, non-CMake refusal, no overwrite,
   settings/recent management, custom editor process launch and launch failure,
   dependency URL refusal, configure-write guard and vcpkg root rejection.
2. `stage5_projects`: real custom-layout Unicode/space builds, two executable targets,
   renamed artifact, target selection, asset working directory, Debug/Release
   independence, wrong/ambiguous target refusal and failed dependency acquisition.
3. `stage5_presets`: inherited Ninja, Ninja Multi-Config and VS2022 presets using
   isolated binary directories, with developer-terminal PATH removed before launch.
4. `stage5_libraries`: real fmt creation/configure/build/run plus independent empty
   and multi-file builds; File API library discovery.

Import fixtures compare every file byte-for-byte and every directory before/after
inspection and builds (stronger than relying only on file hashes). The GUI regression
continues interactive input, Clear/state, repeated Run, EOF/Stop, independent
configurations, resizing/rounded controls and shutdown checks, plus project switching.

The initial test caught Windows trimming trailing dots before validation; fixed by
validating the entered name first. A failed-download fixture initially incurred
FetchContent retry backoff; its bounded real download test preserves the failure/no
source-change assertion without that unnecessary delay. Final requested Quick/Full
commands and exact executed results are in TEST_RESULTS.md.

Final executed commands:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Quick -Jobs 4
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Full -Jobs 4 -Doctor
```

Results: **Quick 24/24 passed in 3.38 seconds; Full 34/34 passed in 103.36 seconds;
doctor passed**. Times are CTest real time; manager setup/compilation is additional.
clang-format 19.1.5 validation passed for 49 maintained files; Git whitespace checks
passed and no changes were staged. Eight protected scheduler/watcher/process/toolchain
files matched their earlier hashes. The build-freshness and CLI extensions are the
deliberate changes described above, not scheduler or process-ownership rewrites.

The native GUI check initially queried dropdown dimensions before the hidden host
received any layout event; it now measures after actual resize/layout, keeping the
usability assertion. The production target selector has a full dropdown height.
Preset tests also verify case-sensitive preset names do not share a cache and that
discovered tools are supplied to child environments with developer PATH removed.

FarmGame at `C:\Users\omer2\OneDrive\Desktop\Projects\FarmGame` was inspected read-only.
Its CMake declares Raylib 6.0, copies Assets beside the program, and contains directory
creation/conditional source-write commands. Its code loads `Assets/Sprites/...`, so
the default project-root working directory is appropriate. No configure/build/run
was performed against this original project. Its real acceptance remains yours;
do not interpret fixture success as proof that FarmGame itself builds or runs.
The final check confirmed **24 unchanged file hashes and unchanged file count**.

See STAGE5_MANUAL_TESTS.md for exact GUI/PowerShell acceptance steps and permission
handling. Stage 5 stops for manual approval; Stage 6/7 are not started.
