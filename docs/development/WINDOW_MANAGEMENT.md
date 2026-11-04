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

## Independent placement settings (v0.1.1)

Settings → Preferences contains two project-specific checkboxes:

| Keep Window Position | Keep Window Size | Subsequent application launches |
|---|---|---|
| OFF | OFF | Program chooses position and dimensions |
| ON | OFF | Remembered position; program chooses dimensions (new-project default) |
| OFF | ON | Program chooses position; remembered dimensions |
| ON | ON | Remembered position and dimensions |

These apply to manual Run after normal close or Stop, manual Restart, Safe and
Seamless Auto Run, and reopening the project in a new manager session. Choices take
effect on subsequent launches. Native move-only/resize-only flags preserve the
unchecked component. Partial restoration does not change the application's own
maximized/minimized/fullscreen startup state; saved maximization is restored when
both options are enabled. Existing early Seamless positioning/rechecking is reused.
Startup rechecking ends at handoff; normal running windows are not repositioned.

Preferences remain in the project's manager workspace, not in source or CMake files.
The original six Auto Run fields are retained for compatibility, followed by
`geometry-v1 <position> <size>`. Old restoration OFF becomes both OFF; old ON becomes
both ON. Legacy Session is upgraded to persistent retention because the new UI has
no separate session mode. Existing window files are retained and remain readable.
Invalid appended geometry options disable restoration conservatively.

Geometry identity is project + target + configuration, independent of the A/B slot.
Existing versioned UTF-16 records (64 KiB/32 windows maximum) retain class/title,
monitor, normal/actual bounds, maximization and DPI. Invalid/corrupt records are
ignored. Stable captures are sampled at most twice per second and flushed at most
every two seconds, with explicit Stop/shutdown/completion flushes. Empty, hidden,
destroyed or minimized captures do not erase valid history. Both OFF retains old
history without applying or updating it. Enabling a choice does not reopen an app.

Position-only restoration uses the remembered monitor where available and clamps
position to a visible work area without resizing. Size-only restoration scales the
saved outer-window dimensions for the current DPI, bounds size to the current work
area, and does not move the application's chosen position. Mixed physical display
configurations remain manual acceptance; geometry is not application/game state.

## Manual v0.1.1 acceptance

1. Extract the complete v0.1.1 ZIP, run EasierCpp-Gui.exe and open a disposable native
   graphical project. Check the title and CLI `--version` show 0.1.1. Keep resources
   and the companion CLI beside the GUI. Existing user projects need no changes.
2. Open Settings → Preferences. A new project should have Position checked and Size
   unchecked, with no old placement-mode dropdown. Toggle each using Tab/Space and
   mouse. Confirm replacement mode and worker controls still work in both themes.
3. Run a window, move/resize it, then Stop and Run. Repeat after closing the program's
   own window normally. Test all four checkbox combinations against the table above.
   Wait briefly after moving the window so the manager can sample stable geometry.
4. In a disposable Raylib project, use InitWindow(1280, 720, "Test"), Run and move it.
   With Position ON/Size OFF, change to InitWindow(1600, 900, "Test") and rebuild/Run.
   Expect the old position and the new 1600x900 client area. With Size ON, expect the
   remembered dimensions instead. Do not edit FarmGame for this acceptance check.
5. Repeat with manual Restart, then with Auto Run in Safe and Seamless modes. Save a
   valid change, a deliberate compiler error, and a repair. Expect the working app
   retained during compilation/failure, successful replacement and no duplicate jobs.
6. Close EasierCpp, reopen the same project, and Run. Expect both checkbox choices
   and the selected target/configuration's valid geometry to persist. An old ON
   profile should retain both checks; an old OFF profile should retain neither.
7. Maximize/minimize a native app and repeat. Partial restoration must not override
   application-controlled startup state. Both ON retains supported maximized history.
   Repeat with multiple windows and console-only input/output, Stop and Restart.
8. If hardware permits, use a monitor left/above primary, disconnect it, change DPI,
   and relaunch. Expect visible fallback without restoring the unchecked component.
   Repeat with actual SDL/SFML projects; those frameworks are not bundled or injected.
9. From an extraction outside the checkout, create/build/run a disposable project,
   add/use/remove a trusted local library through previews, and rerun the window
   checks. Doctor must discover installed tools without permanent PATH changes.

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
