# Auto Run and compatibility acceptance checks

Use disposable projects for source edits and dependency changes. Do not perform these
experiments on FarmGame or another real project without approving the individual edits.

## 1. Launch the updated application

Close any older Cpp manager window normally, then run this in ordinary PowerShell:

```powershell
Set-Location 'C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab'
& '.\build\reliability\cpplivelab-gui.exe'
```

Expected: the existing interface and customized theme are retained. Open a project;
Auto Build and Auto Run appear beside each other. Settings still has one destination.

## 2. Manual build, run, console and configurations

1. Create a new disposable C++ project through Project. Keep it outside CppLiveLab.
2. Leave both automatic switches OFF. Build Debug, select the executable target, then Run.
3. Exercise any console prompts with Send or Enter. Click Stop while a program is waiting
   for input. Run again, then click Restart.
4. Select Release while idle, Build, then Run. Switch back to Debug while idle.

Expected: clear build output/errors; independent Debug/Release artifacts; working input;
Stop terminates only the managed process tree. Restart launches the last successful
artifact without requiring a new successful source build. A compiler failure after
configuration should not erase a discovered executable target.

## 3. Auto Build is independent

1. Stop the program. Set Auto Build ON and Auto Run OFF.
2. Change and save a source file. Wait for the debounce/build to finish.
3. Run an application that stays open, then change/save the source again.
4. Stop it and allow the pending build to finish. Turn Auto Build OFF.

Expected: an idle save builds without launching a program. While an application holds
its current output, Auto Build defers safely and does not restart it. A pending build
can finish after Stop. Auto Run remains OFF throughout.

## 4. Auto Run success and failure

1. With Auto Build OFF, enable Auto Run and Run the project explicitly.
2. Save a visible code change. Watch Build & System.
3. Add a syntax error and save while the application stays open.
4. Fix the error and save again. Save several distinct small changes rapidly.
5. Save identical contents once more. Finally turn Auto Run OFF and save a change.

Expected: Auto Run includes building, independently of the Auto Build switch. The old
application stays running during replacement compilation. A failed build shows errors
and retains that application; a successful current build replaces it once. Newer saves
supersede obsolete replacements. Identical contents do not trigger another build.
OFF prevents further automatic restarts. If both switches are OFF, saves do no work.
Stop does not cause an automatic relaunch loop. Auto Run does not launch an already
stopped application simply because a source file was saved.

## 5. Native window placement

For a dependency-free window sample, create a NEW empty project, replace its generated
source with `tests/consumers/native_window.cpp` from this repository, and add
`target_link_libraries(app PRIVATE user32)` to that disposable project's CMakeLists.txt.
Use its actual executable target name if different. These are explicit manual edits
to your new test project only.

1. Run it, move/resize its window, enable Auto Run and save a source comment.
2. Wait for the new window. Move it yourself after restoration and wait several seconds.
3. Maximize it and save again. Also test restarting while minimized.
4. If you have another monitor, move the window there and repeat. Test your actual
   mixed-DPI configuration if available. Disconnect that monitor before a restart to
   check that fallback placement remains visible.
5. Introduce a compiler error; verify the previous window stays in place.
6. Keep another application open throughout; verify its placement remains unchanged.

Expected: restored normal rectangle or maximized state, with a visible monitor fallback.
Minimized windows reopen visibly. Restoration retries for delayed windows and stops
moving a window once restoration succeeds. Secondary-monitor and physical mixed-DPI
checks require your actual hardware; they are not implied by single-monitor tests.
Exclusive fullscreen and Windows Snap groups are not restored as operating-system state.

## 6. Runtime settings

1. In Project Settings, choose a working directory for a disposable application that
   opens a relative asset. Under Advanced Options, enter one program argument per line.
2. Run, automatically rebuild/restart, then manually Restart.
3. Repeat using an application with a DLL built alongside its executable.

Expected: initial launch and both restart paths retain arguments, working directory,
relative asset access and the existing child environment/DLL search behavior. Empty
argument lines represent empty arguments; quotes are argument contents, not shell syntax.

## 7. Library compatibility and existing management

1. In the disposable project, Add Library -> Local header-only directory. Use a directory
   containing `value.hpp` with `inline int answer() { return 42; }`. Approve the exact
   integration preview, then include/use it in the consumer and Build.
2. Add a second local CMake library with an actual library target; verify both entries
   remain listed. Exercise Edit Integration, Repair and Remove on these test entries.
3. Attempt to add a documentation-only directory, an application-only CMake project,
   and a directory containing only an OS-header wrapper. They must not be registered as
   compatible libraries. An unresolved candidate may report Unable to Verify.
4. Try a missing source directory: expect Unable to Verify. Try a Python-only package:
   expect a separate compatibility-decision message, not native integration.
5. An installed CMake package selecting an exported executable must fail validation
   and restore the previous project dependency files/metadata.
6. For GitHub testing later, explicitly approve the exact repository/commit before
   configuring it. A tag is not mandatory: a resolvable branch/commit can be verified.
   A network/metadata failure must say Unable to Verify, not Not a library.

Expected: passive evidence alone never reports verified compatibility. The GitHub
workflow requires a discovered reusable target plus consumer link probe, or an explicitly
compiled public header. Local/package integrations require their approved consumer build;
new non-header targets must be library types. Existing records remain recorded if
reverification fails. Shared dependency sources are not deleted by project removal.

## 8. Repeat automated local checks

Close this build's GUI normally before rebuilding it:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\format.ps1 -Check
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -BuildDirectory .\build\reliability -TestMode Quick -Jobs 4
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -BuildDirectory .\build\reliability -TestMode Full -TestFilter '^(library_compatibility_|redirection_|reliability_|stage5_(restart|library_|unified_(unit|projects)|ui_|github_versions|unit$|projects$|presets$)|stage[234]_|doctor_|maintenance_|cli_|test_selection)' -Jobs 4 -Doctor
```

The filtered command runs 51 local tests, not an unfiltered Full approval run. It excludes
the two network-dependent Full tests and the optional live-library test, respecting the
existing local-only restriction. No system PATH changes or additional installations are
required. Leave approval, commits and pushes until after manual acceptance.
