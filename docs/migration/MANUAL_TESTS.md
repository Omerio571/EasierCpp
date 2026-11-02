# EasierCpp rename — manual acceptance

Use the new executables explicitly. Older build folders and the previous portable
ZIP were retained and can still contain old binaries. Close the old manager first.
Do not remove either AppData tree during testing.

1. **Open ordinary PowerShell and the checkout.**
   ```powershell
   Set-Location 'C:\Users\omer2\OneDrive\Desktop\Projects\EasierCpp'
   .\build\rename-debug\EasierCpp.exe --version
   .\build\rename-debug\EasierCpp.exe doctor
   ```
   Expect `EasierCpp 0.1.0`, an EasierCpp Doctor heading, exit 0 with a working MSVC
   toolchain, and installed tools distinguished from current-terminal availability.
   GCC is optional. No PATH changes or developer shell are required.

2. **Launch the Debug manager.**
   ```powershell
   .\build\rename-debug\EasierCpp-Gui.exe
   ```
   Expect EasierCpp — Make Your Life Easier, the existing customized appearance,
   one Settings destination, and renamed test/restart actions. Explorer → Properties
   → Details should show EasierCpp and version 0.1.0 for both binaries.

3. **Check your migrated profile.** Expect recent projects, saved editor, workers,
   project configuration/arguments, Auto Run/window preferences and known libraries
   to remain available. Metadata now lives under `%LOCALAPPDATA%\EasierCpp`;
   appearance uses `HKCU\Software\EasierCpp\Preferences`. On first migration, read
   `%LOCALAPPDATA%\EasierCpp\migration-report.txt`. Existing new data wins by whole
   project/catalog/settings unit: conflicting old data remains in CppLiveLab and
   is reported, not silently merged. An existing new registry preference key wins
   intact. Original root-level EasierCpp values take priority over CppLiveLab when
   initializing the new preference subkey. Keep the original data: preserved PCH
   snapshots/recipes can still reference its absolute paths. Caches are not copied;
   a cold first build after migration is expected. A pending old library transaction
   must be recovered in the old manager before retrying migration.

4. **Create a disposable console project, separately from EasierCpp.**
   ```powershell
   $demo = Join-Path $env:TEMP ('EasierCpp-Manual-' + [guid]::NewGuid().ToString('N'))
   .\build\rename-debug\EasierCpp.exe create $demo --demo calculator
   .\build\rename-debug\EasierCpp.exe run $demo --config Debug
   ```
   Enter Ada, 2 and 3 on separate lines. Expect `Hello, Ada! Sum: 5`. Run again with
   `--config Release`. Expect the same result and independent `.easiercpp/build-Debug`
   and `build-Release` output folders. Existing `.cpplivelab` CLI workspaces continue
   using their old location; source/project names are not renamed.

5. **Open that project in the GUI.** Build, choose the discovered `app` target, Run,
   enter input, and Stop. Expect companion CLI launch without a missing-executable
   error. Switch Debug/Release and repeat. A target selector awaiting configuration
   must populate after the first successful build; old build manifests were not moved.

6. **Exercise Auto Run in the disposable project.** Run it and leave it waiting for
   input; enable Auto Run, then make a real saved source change in your editor.
   Expect one incremental build and a replacement after success. Rapid saves must
   retain the latest edit. Temporarily add `#error Manual acceptance` to the source:
   expect visible errors while the old program stays available. Remove the error;
   expect recovery. Stop must stop the owned program and disarm automatic launching
   as before. Enabling Auto Run alone must not unexpectedly relaunch the program.

7. **Open an existing project read-only.** Check its selected target, working
   directory, runtime arguments, libraries, assets and DLL loading. Build/Run should
   use the external manager workspace. Source files, CMake, assets and directory
   layout must not be reorganized. The first migrated build can be cold. In a native
   graphical project, move its window and use manual Restart, then test a real save
   with Auto Run in both Safe and Seamless modes. Expect existing placement behavior.
   Do not accept a library/configuration edit unless its preview is what you intend.

8. **Check Library Manager.** Confirm migrated known/hidden entries, selected project
   libraries, and managed/external ownership. Use a disposable project's local library
   to test adding/removing after the normal preview/approval. Imported/manual entries
   must remain read-only. Public GitHub/GitLab checks retain their existing warnings;
   this migration did not authorize or test live third-party dependency execution.

9. **Run development Quick tests from the Debug GUI.** Tools → Run EasierCpp Tests,
   Quick, one worker. Expect real output in Build & System, separate from Program
   Console. A moved manager-owned test cache is preserved and an `.easiercpp-N` sibling
   is selected/reused. A foreign or unverified cache must stop with an actionable
   message. Cancel Running Tests must cancel the owned test tree without closing the
   manager/editor. Comprehensive tests run in `build/integrated-tests` or its verified
   recovery sibling, not the selected user project's build folder.

10. **Repeat the development test action from normal Release.**
    ```powershell
    .\build\rename-release\EasierCpp-Gui.exe
    ```
    Expect the source checkout to be found independently of the terminal's working
    directory. Close Debug first so single-instance coordination does not activate
    that earlier window. Release/source tests use a separate developer test build.

11. **Try the fresh portable ZIP.** Extract
    `dist/EasierCpp-v0.1.0-windows-x64-portable.zip` into a
    fresh folder with spaces outside the checkout. Keep EasierCpp.exe,
    EasierCpp-Gui.exe and resources together. Expect both executables to launch,
    Doctor to work, and normal disposable-project Build/Run/Auto Run to work without
    repository scripts. Tools → Run EasierCpp Tests must explain that development
    tests require a non-portable source build and point to Toolchain Diagnostics /
    `EasierCpp.exe doctor`. It must not search for or execute another checkout.

12. **Restart and reopen.** Change a preference, close/reopen the new manager, and
    reopen the disposable and an existing project. Expect persistence and no repeated
    metadata import or cache reset. Keep both original and new profiles until satisfied.
    The previous ZIP, historical reports and real projects were not deleted or migrated
    in place. Update any shortcuts to the new binary names. No commit/push is included.

For automated Quick verification from the source checkout:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -BuildDirectory build/rename-debug -TestMode Quick -Jobs 1
```

Full includes separately restricted network-dependent tests. Consult FINAL_REPORT.md
for the exact offline selection executed; do not equate it with unfiltered Full or
the optional live-library suite.
