# EasierCpp v0.1.1

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
