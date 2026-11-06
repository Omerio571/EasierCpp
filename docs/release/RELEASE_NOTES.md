# EasierCpp v0.1.3 - UI Overhaul

Unreleased. See the [v0.1.3 candidate notes](notes/v0.1.3.md).

- Navigate Home, Workspace, Libraries, Project, Tools and Settings from one sidebar.
- Use the expanded Workspace console for program input, build output and diagnostics.
- Use Settings → Appearance to switch both console and desktop palettes, or follow
  Windows. The active session and console contents are retained.
- Hover or focus the information buttons for help with Auto Run, executable targets,
  restarting a program and console input. Project and preference forms retain their help.
- The vector reconstruction of the original logo appears in the desktop shell and
  supplies Windows executable and window icons.

Routine project, library and preference forms open within the main window. Library
management separates managed, detected and available entries and supports search.
Operations that need approval retain their review dialogs. Build trees, caches and
settings use the existing locations and formats. See the
[UI acceptance guide](../design/V0.1.3_UI_ACCEPTANCE.md).

---

# EasierCpp v0.1.2 — Performance and Reliability Improvements

## Highlights

- Auto Run normally uses the original compilation tree and one alternate, avoiding
  a second redundant cold replacement build while retaining independent caches.
- Compatible unchanged default MSVC/x64 Ninja projects can start without compiler
  SDK initialization. The environment is prepared once when compilation needs it.
- Existing project layouts, library workflows, PCH, incremental linking, runtime
  arguments/assets/DLL lookup, Safe/Seamless Replacement and placement are retained.

## What's new

The normal rotation is original -> alternate -> original. Existing A/B-only and
recovery caches remain accepted; historical trees are not deleted or copied.
Debug/Release/profile identities stay separate, and running artifacts remain leased.

The conservative cached-launch probe checks profile/compiler ownership, current
metadata, imports and build freshness. Uncertain runtime dependencies, custom
toolchains/presets and unsupported fast-path configurations fall back to normal
setup. Child-only SDK preparation does not permanently change PATH or Windows.

## Measured performance

Controlled local fixtures, not guarantees for every project:

| Case | Before | After |
|---|---:|---:|
| Second replacement build | 4.32 s | 0.66 s |
| Ten warm builds, median | 0.57 s | 0.56 s |
| Fresh-fixture build-tree storage | Three trees | Two trees, about 33% less |
| Matched Release cached startup to output, median of ten | 4.79 s | 0.71 s |

Sources in the repository: `docs/reports/TWO_TREE_AUTO_RUN_2026-11-04.md` and
`docs/reports/auto-run/SDK_STARTUP_OPTIMIZATION_REPORT.md`. The two-tree comparison
used Release-before/Debug-after managers with identically configured Debug fixture
builds; tiny warm differences are not proof of a compiler speedup. The SDK comparison
used matched Release managers. Later host-variable cached runs had a 1.84 s median;
0.71 s is not a guaranteed startup time. A cached session's first compile pays the
deferred SDK cost. Warm Auto Run trials varied by about 50–60 ms rather than showing
a demonstrated SDK-driven warm speedup. The first alternate can still require a
full configure/PCH/dependency build.

The critical-path audit found no additional justified production optimization.
In its ten-run native-window comparison, first visibility medians were 1.56 s Safe
and 1.47 s Seamless. Seamless completed handoff at 2.48 s while keeping the old app
through its safety hold; it did not wait until then to show the new window.
These are earlier implementation/audit measurements, not final-package guarantees.

## Reliability and compatibility

Compilation failures retain the working process. Source freshness, cancellation,
process ownership, graceful shutdown and candidate readiness remain enforced.
Initial Auto Run, manual controls, interactive console, independent position/size
preferences, library integration and existing CMake project layouts remain available.

**Downgrading:** the new active-slot value `base` identifies the original tree.
Older versions may not understand it. Do not share state concurrently between
manager versions. Back up state and consult `docs/development/AUTO_RUN_BUILD_TREES.md`
in the repository before downgrading, or use isolated state for older-version tests.
Do not delete compilation trees or settings as a routine upgrade step.

## Known limitations

- No in-process hot reload, application-memory migration or Windows Snap Group restoration.
- Windows 11 x64, the Visual C++ x64 runtime and separately installed development
  tools/compiler/SDK are required. No toolchain is bundled.
- Seamless overlaps processes; exclusive-instance/resource applications may need Safe.
- Applications can override their window geometry. Physical mixed-DPI/multimonitor,
  individual graphics libraries and arbitrary toolchains require project acceptance.
- Arbitrary library repositories are not guaranteed compatible or trustworthy.
- First-time dependency compilation can take longer; no background prewarming,
  experimental hot reload or alternative linker backend is introduced.

## Upgrade

Close the manager and managed applications. Extract the whole ZIP into a new folder,
then run EasierCpp-Gui.exe. Keep existing projects, manager settings and caches.
The package README explains diagnostics and external prerequisites. This candidate
is packaged locally; publication, signing and clean-machine acceptance are separate.

---

# EasierCpp v0.1.1 (historical notes)

## New features

### Independent window restoration

Settings → Preferences now provides **Keep Window Position (When Restarted)** and
**Keep Window Size (When Restarted)** independently. These apply to the user's
application on manual Run/Restart and Safe/Seamless Auto Run, including after
reopening EasierCpp. Position-only restoration respects program-defined dimensions.
New projects default to position ON and size OFF.

### Automatic initial run

Opening a project with Auto Run enabled now schedules its initial build and launch
without a manual Run or a source save. Enabling Auto Run on an idle project does the
same; enabling it while an application is already running does not replace that app.
Valid previous build outputs are reused through existing dependency checks.

Initial startup resolves the current CMake target graph. A valid saved executable or
the sole executable can run; multiple executables require selection before target
compilation/launch. Changed cached graphs and suppressed regeneration are handled
conservatively. Existing CMake configuration/write approval remains required.
Compilation failure keeps Auto Run enabled for recovery on a later save. Stop, OFF,
project switches, and manager closure cancel obsolete startup safely.

## Improvements and compatibility

- The main README is now a concise user guide covering features, prerequisites,
  getting started, source builds, licensing, and contributions.
- CLI, GUI title, Windows metadata, and package naming use version **0.1.1** from
  the authoritative CMake version.
- Existing full window restoration migrates to both options ON; previous OFF becomes
  both OFF. Saved geometry is retained. Enabled legacy session preferences become
  persistent in the new UI; target/configuration geometry separation remains.
- Existing build slots, incremental builds, PCH, library management, interactive
  console I/O, and Safe/Seamless replacement remain in use.

## Known limitations

- Windows 11 x64 and the supported Microsoft C++ runtime are required. Compiling
  projects requires installed build tools/compiler/SDK; none are bundled.
- Seamless mode overlaps application instances and is unsuitable for some exclusive
  resources or launchers. Use Safe where necessary. Application/game state is not
  migrated, and Windows Snap Groups are not restored.
- Partial window restoration leaves maximized/minimized/fullscreen startup state to
  the application. Apps can override geometry after startup. Physical mixed-DPI,
  multiple-monitor, and individual SDL/SFML acceptance remain manual.
- Library integration depends on each dependency's build system, platform needs,
  options, licensing, and trust approval; arbitrary repositories are not guaranteed.
- Initial startup can pause for configuration approval, preset configuration, or
  executable selection. Auto Run OFF never performs automatic builds or launches.

This is a locally prepared portable candidate. Publication, signing, and clean-machine
acceptance are separate owner-approved steps; no tag or GitHub Release is created.
