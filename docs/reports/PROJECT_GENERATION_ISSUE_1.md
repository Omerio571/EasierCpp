# Issue 1 project generation verification

The local fix adds automatic source discovery to new standard projects.
Single-file wrappers retain explicit input files. Existing projects are not
converted. The shared generator emits .gitignore, a named README, include/
and assets/. Manual source management uses EASIERCPP_AUTO_SOURCES=OFF and the
generated APP_SOURCES list. No project-creation UI control was added.

The Library Manager recognizes the new starter marker so a new project's app
target remains available before its first build.

## Verification scope

Disposable projects cover Ninja and Visual Studio 2022/MSVC, mixed .cpp/.cc/.cxx/.c
sources, nested Game.cpp plus Game.hpp, rename and removal, ignored build/vendor
trees, independent CMake builds, manual lists, runtime asset copying, no-op artifact
timestamps, unchanged object timestamps, Debug/Release, and manager slot rotation.

Quick includes real Git check-ignore calls using a temporary work tree and the
existing repository metadata read-only. No git init, commits or remotes are created.
The GUI lifecycle regression additionally enables discovery on its manual-list
fixture and adds, renames and deletes a linked implementation during Auto Run.
It retains the existing PCH, failed-build and process-replacement assertions.

The offline Full selection excludes stage5_libraries, stage5_unified_raylib and
the optional live-library test. Local dependency consumers, imported project
protection, single-file wrappers, presets and PCH remain included.

Quick passed 39/39 in 27.81 s. The offline Full run passed 61/63 in 504.61 s.
Its two failures were old freshness tests assuming that every generated project
used an explicit source list. Their fixtures now select manual mode for the same
strict no-SDK/Ninja dry-run assertions. The freshness retest passed in 32.33 s.
The GUI test additionally switches its cached option explicitly to ON before
testing source additions. Its first retest retained OFF in CMake's cache and
correctly preserved the running application when the new symbol could not link.
The final GUI lifecycle retest passed in 58.54 s, including the new nested
implementation, rename to .cc, deletion, PCH checks, and failed-build recovery.
All 63 selected Full checks have now passed across the Full run and focused
retests. A second complete Full run was not performed.

## Limitations

CONFIGURE_DEPENDS performs a file-list check on each build. CMake warns that
generator support is not universal. Ninja and Visual Studio are tested here.
Source-list changes regenerate each independent build tree on its next use.
The existing dry-run freshness check remains conservative, so unchanged manual
Run can perform a real no-op build instead of a direct cached launch.

Reserved build/dependency directory names are documented in the generated README.
Use explicit source lists for unconventional layouts. Selected external libraries
retain their existing approvals and compatibility checks.

The runnable development build is build/ui-v0.1.3-final/EasierCpp-Gui.exe.
Published release archives were not rebuilt. No commit, push, release, or issue
closure was performed. See ../development/PROJECT_GENERATION.md for manual steps.

## Measured build work

Wall times in milliseconds for the disposable fixture, running CMake's build
command. Configuration and application launch are outside these measurements.
The Full suite used two workers, so the figures include concurrent test load.
No-op entries are medians of three samples.

| Operation | Ninja/MSVC | Visual Studio 2022/MSVC |
| --- | ---: | ---: |
| First compilation/link | 1453 | 3187 |
| Add mixed-language sources and link | 1079 | 2547 |
| Automatic no-op | 141 | 1109 |
| Manual no-op | 93 | 984 |
| Change one implementation | 734 | 1281 |
| Rename implementation | 829 | 1485 |
| Remove unused implementation | 765 | 1406 |

The measured no-op overhead was 48 ms with Ninja and 125 ms with Visual Studio.
Unchanged executable timestamps and the unrelated Ninja object timestamp were
verified independently of these timings.
