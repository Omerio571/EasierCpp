# Window management

Settings → Preferences applies to the open project. There is one Settings destination.
Replacement mode changes affect the next replacement, without restarting the current
application. Manual Restart retains its existing Safe behavior.

## Preferences and ownership

- **Safe Replacement** remains the default: build in the inactive slot, gracefully
  close the old job after a successful build, then launch and restore the replacement.
- **Seamless Replacement (Experimental)** launches a separately owned candidate job
  while the old application remains active. It requires one second of root-process
  survival and, for graphical applications, a visible window for at least 250 ms.
  Readiness is bounded at 15 seconds. Console applications do not require a window.
  Placement is applied on the first visible-window poll, independently of the
  readiness interval. During startup, changed geometry is rechecked to correct an
  application's delayed centering; unchanged geometry is not reapplied. Promotion
  clears the old restoration state so it cannot reposition the new application again.
  Placement completes before requesting the old application's graceful shutdown.
  Jobs, pipes and leases are exchanged only after the old job/output have drained.
- Candidate launch failure, early exit, missing-window timeout, or obsolete source
  results cancel the candidate. They do not trigger an automatic retry loop. A later
  actual save can retry. Exclusive-instance applications should use Safe mode; a
  failed candidate never silently switches to stop-first operation.
- Stop cancels the candidate and stops the managed application using existing graceful
  shutdown/explicit force-stop settings. Processes outside the owned jobs are untouched.
- Auto Run OFF cancels a warming candidate. A handoff whose candidate is already ready
  and whose old application has already received close completes as an atomic handoff.
  No new automatic builds or launches are scheduled after disabling Auto Run.

Existing A/B directories, PCH profiles, runtime environment, DLL paths, working directory,
arguments, interactive pipes and build-failure preservation are reused. Startup requests
non-activation; applications can still explicitly activate their own windows.

## Placement memory

New configurations default to **Remember Between Sessions**. Existing four-field
Auto Run settings migrate to Session when restoration was enabled, otherwise Off.
Replacement mode migrates to Safe. The original four fields remain at the start of
`auto-run.settings`, followed by numeric placement and replacement modes.

- **Off:** does not restore or update historical placement; existing stored data is
  retained so changing preferences does not erase it.
- **Remember During Session:** uses manager-owned memory only. A fresh manager does
  not read persistent placement in this mode.
- **Remember Between Sessions:** also reads/writes the existing manager workspace,
  under `workspace_directory(project)/window-placement/<identity>.settings`.
  Source folders and CMake files are never modified.

Identity is project + target + configuration, independent of the A/B slot. The stored
record includes the target/configuration for verification, Unicode window class/title,
monitor identity, normal rectangle, actual bounds, maximization, DPI and monitor/work
rectangles. Versioned UTF-16 records are bounded to 64 KiB/32 windows, validated on read,
and atomically replaced. Unrecognized/corrupt records are ignored.

Tracking samples at most twice per second and requires consecutive stable geometry.
Hidden/destroyed windows, minimized captures and invalid rectangles do not erase valid
history. Changed persistent data is written at most every two seconds while running,
with an explicit flush before Stop/shutdown and at process completion. A crash or user
close does not automatically reopen the program. The existing monitor/DPI restoration
algorithm handles negative monitor coordinates and disconnected-monitor fallback.

## Manual acceptance

1. Launch `build/window-management/EasierCpp-Gui.exe`. Open a disposable graphical
   CMake project. Settings → Preferences should show the project's placement and
   replacement settings. New settings default to Between Sessions + Safe.
2. Choose Session + Safe, Run, move/resize the program, Stop, Run. Expect the same
   placement. Close its window yourself, Run again; expect the same placement.
3. Choose Between Sessions, maximize the application, close EasierCpp, reopen it and
   the same project/target/configuration, then Run. Expect maximization to return.
4. Choose Off, Stop and Run. Expect the application to choose its own geometry. Switch
   back to a memory mode: its retained history should remain available.
5. Select Seamless and enable Auto Run. Save a real source change. Expect the old
   application to remain visible through compilation and replacement startup; only
   after the restored replacement is ready should the old application close.
6. Introduce a syntax error and save. Expect compiler errors and the old program to
   remain running. Repair and save; expect a successful replacement.
7. In a disposable fixture, make the replacement return a nonzero code immediately.
   Expect a startup failure message, no retry loop and the previous program retained.
   For a single-instance application, expect equivalent rejection; select Safe and
   save another change to perform a compatible stop-first replacement.
8. Save several real changes rapidly, including during compilation/startup. Expect
   the latest version, no stale candidate launch and no orphan duplicate processes.
9. Test a console project that reads stdin. Expect its prompt/output and input after
   replacement, with no requirement for a graphical window. Test Stop and manual
   Restart as well. Repeat with Session/Off and both replacement modes.
10. If hardware permits, move a program above/left of the primary display, restart,
    then disconnect that monitor and restart again. Expect preserved placement when
    connected and safe visible fallback when disconnected. Check mixed DPI manually.

## Limits

Readiness is a bounded heuristic, not proof that application initialization/game state
is correct. Launchers whose root exits while descendants continue are conservatively
rejected as experimental candidates. Startup slower than 15 seconds is rejected.
Exclusive resources/fullscreen applications may require Safe mode. A program that
refuses graceful close cancels the replacement unless the existing explicit force-stop
preference is enabled; its stdin may already be closed. Windows Snap Groups, arbitrary
fullscreen modes and application-forced focus changes remain best effort.

No separate latency benchmark was performed for this feature: the deliberate readiness
interval adds overlap rather than making compilation faster. Physical multi-monitor/
mixed-DPI and real-game acceptance require manual verification.
