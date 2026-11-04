# Initial Auto Run

Auto Run is a per-project preference. Opening an enabled project schedules one
initial build/launch without a source save. Turning Auto Run on while idle does the
same. Turning it on while an app is already running preserves that process.

The existing GUI timer, build worker, scheduler, freshness checks, File API reader,
process supervisor and replacement slots perform the work. There is no second build
queue or automatic-start preference. Initial work is deferred past project loading
and modal configuration/target selection, keeping the interface responsive.

The worker checks the current graph before compiling application targets. Cached
Ninja builds use their build-system regeneration edge; missing/suppressed rules and
other generators use an authorized normal configure. Cold configuration is not
repeated unnecessarily. A valid saved target or sole current executable can run;
ambiguous targets stop before compilation and require a selection. Cancel leaves
the workspace selector available. Choosing an executable resumes startup.

Existing CMake write authorization remains mandatory. Declining does not execute
CMake or disable the saved Auto Run preference. Compiler failure retains Auto Run;
a later save can recover. Latest source checks and the existing pending-change
coalescer prevent stale first-build launches. Stop/OFF/close cancel startup; opening
another project during initial compilation stops and drains the obsolete owned job
before opening the new project. Repeated Open does not duplicate a live initial job.

## Manual acceptance

Use fresh temporary projects, not FarmGame or another personal project.

1. From the extracted release's PowerShell directory, create a calculator project:

   ```powershell
   .\EasierCpp.exe --version
   .\EasierCpp.exe doctor
   .\EasierCpp.exe create "$env:TEMP\EasierCpp-Initial-Calculator" --demo calculator
   ```

   Expect version 0.1.1, installed-tool diagnostics, and successful project creation.
   Choose another new folder name if it already exists; do not delete existing data.
2. Open that folder in the GUI. With Auto Run OFF, expect no build/run. Turn Auto Run
   ON without clicking Build/Run or saving a file. Expect the preparing message,
   compilation, `Initial Auto Run completed`, and the calculator's name prompt.
3. Enter a name, 2 and 5. Expect sum 7. The finished program must not restart in a
   loop. Reopen the project with Auto Run still ON: expect another prompt without
   manually initializing Run. Unchanged sources should reuse build outputs.
4. Stop the program. Save a change while launch is paused: expect no automatic
   launch. Run or toggle Auto Run to resume. Turn Auto Run OFF, close/reopen the
   project, and confirm no automatic compilation or launch occurs.
5. Create another fresh disposable project. Before its first Build/Run, add a second
   executable to CMake, for example `add_executable(tool tool.cpp)`, with a valid
   `tool.cpp` containing its own `main`. Open it and enable Auto Run. With no saved
   target, expect a target choice, not an arbitrary launch. Cancel once, then choose
   a target in the workspace; expect only that target to run.
6. Add a compiler error to the temporary source, reopen with Auto Run ON, and inspect
   Build & System. Expect useful diagnostics and no executable launch. Repair/save;
   expect automatic recovery without disabling/re-enabling the preference.
7. Make two rapid saves during the first compilation. Expect the latest source to
   run once, with at most one queued follow-up build. While initial compilation is
   active, open a different enabled project; expect the old startup cancelled and
   only the newly selected project launched. Repeat with Stop and manager closure.
8. For a trusted disposable CMake project that writes files during configuration,
   decline its authorization first. Expect no configuration write or program launch.
   Reopen and explicitly authorize; expect normal startup. Do not approve unknown code.
9. Repeat the [window-restoration acceptance guide](WINDOW_MANAGEMENT.md): all four
   position/size combinations, program-defined dimension changes, manual Restart,
   Safe/Seamless replacement, and persistence after reopening the manager.

Live multi-monitor/mixed-DPI and individual SDL/SFML applications require manual
acceptance. A temporary extraction is not a clean-machine or universal compatibility
claim. The portable application includes no compiler/SDK or Microsoft runtime.
