# EasierCpp v0.1.0 — local public-testing candidate

Core features: native Win32 GUI, standard C++20 project creation, imported CMake
projects, discovered Windows toolchains, Debug/Release builds, interactive program
I/O, manual lifecycle commands, Auto Run with isolated A/B builds, Safe/Seamless
replacement, supported window-placement memory, opt-in CMake PCH, worker preferences
and optimization, and a unified managed/read-only-detected Library Manager.

This renamed candidate uses EasierCpp.exe and EasierCpp-Gui.exe, with matching
Windows product/version metadata and relocatable first-party runtime resources.
It adds conservative legacy metadata/preferences migration, dual CMake ownership
readers, and safe moved test-cache recovery. Original data and the earlier
candidate remain intact. Development tools and the Microsoft runtime are not bundled.
No universal hot reload, memory/state migration, embedded editor or debugger is included.

Known limitations:

- Migration does not merge conflicting catalogs/projects or transplant build caches.
  Original AppData must be retained where preserved absolute PCH/library references
  still use it. Review migration-report.txt before any manual cleanup.
- Comprehensive development tests require a non-portable source build. Portable
  builds provide Doctor/toolchain diagnostics and an actionable test-suite message.

- Only the available Windows 11 x64 host was tested; no clean VM/cross-machine claim.
- The package is unsigned and is not an installer. Windows may display its normal
  downloaded-application trust prompts. No system security setting should be disabled.
- Private/self-hosted GitLab authentication is unsupported. Live third-party library
  downloads/builds require separate trust and licensing review.
- CMake dependency detection is bounded evidence, not arbitrary CMake evaluation.
  Older replies may omit aliases/imported targets. Unknown entries stay unverified.
- Seamless is unsuitable for some exclusive-instance/multi-window applications;
  use Safe. Placement restoration does not restore application data/state.
- MSVC PDB-service errors have occurred intermittently on the development host;
  an isolated rerun passed. Keep the detailed build output if this recurs.
- Synthetic worker recommendations and performance vary with available RAM and
  project workload. Cached development profiles may need approved recovery after
  application relocation, particularly when PCH hooks reference an old location.
- Source test-suite execution is developer-only and is not shipped in this ZIP.

Publication, signing, other-machine validation and user acceptance are separate
steps. This local artifact is not an automatically published GitHub release.
