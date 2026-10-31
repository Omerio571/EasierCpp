# Stage 5 final manual acceptance

Use new disposable projects for library changes. Opening an existing project is
read-only; editing its CMake requires a separate preview and approval. These tests
do not require changing FarmGame, its assets, or its folder layout.

## 1. Build and launch from ordinary PowerShell

1. Close CppLiveLab. Leave Visual Studio open if you want.
2. Run:

   ```powershell
   Set-Location 'C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab'
   powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Quick -Jobs 4
   .\build\debug\cpplivelab.exe --version
   .\build\debug\cpplivelab.exe doctor
   .\build\debug\cpplivelab-gui.exe
   ```

3. Expected: Quick succeeds, version remains `0.1.0`, and Doctor distinguishes
   installed tools from tools available in that terminal. Missing GCC is acceptable
   with the working Visual Studio toolchain.
4. Expected: Project and Tools are left; **Restart** and one **Settings** button
   are right. An ordinary launch shows the welcome screen.

## 2. Create a disposable project and add a library

1. Choose **Project > New Project**. Select an existing parent outside CppLiveLab,
   then a new folder name, such as `Stage5-SFML-Acceptance`.
2. Create an empty C++20 project without libraries. It should contain `src`,
   `include`, `assets` and `CMakeLists.txt`.
3. Choose **Project > Manage Libraries**.
4. Select action **Add Library**, then Continue. In the simple Add popup choose
   Add Library and the GitHub repository option.
5. Expected: the repository URL input is genuinely empty every time.
6. Use a row from this table. Retrieve versions, enter the exact tag, and validate.

   | Library | Repository URL | Version input | Target selection | Consumer file |
   |---|---|---|---|---|
   | Raylib | `https://github.com/raysan5/raylib` | `6.2` | `raylib_static`, or `raylib` under Advanced | `raylib.cpp` |
   | SFML | `https://github.com/SFML/SFML` | `3.1.0` | `SFML::Graphics`, `SFML::Window`, `SFML::System`, `SFML::Audio`, `SFML::Network` | `sfml.cpp` |
   | fmt | `https://github.com/fmtlib/fmt` | `11.0.2` | `fmt::fmt` | `fmt.cpp` |
   | JSON | `https://github.com/nlohmann/json` | `v3.11.3` | `nlohmann_json::nlohmann_json` | `json.cpp` |
   | GLFW | `https://github.com/glfw/glfw` | `3.4` | `glfw` | `glfw.cpp` |
   | SDL3 | `https://github.com/libsdl-org/SDL` | `release-3.4.18` | `SDL3::SDL3` | `sdl3.cpp` |

7. Expected: CppLiveLab shows the resolved immutable commit. In **Approve dependency
   verification**, choose CMake library. To reproduce the common automated-test
   options, expand Advanced Options and enter:

   ```text
   BUILD_EXAMPLES=OFF
   BUILD_TESTING=OFF
   BUILD_SHARED_LIBS=OFF
   ```

   Use the exact tags above to reproduce these results. Latest Stable should also
   appear when the repository publishes a non-draft, non-prerelease release;
   it is resolved to an exact commit before approval. Do not invent a version
   when that option is unavailable.

8. Approve verification only for the intended repository and commit. Wait for
   configuration, then select the targets from the table. Use Ctrl-click for several.
9. Expected: a fresh target selector has no preselected target. It scrolls; Graphics
   and Audio are visible for SFML. Public aliases appear before internal libraries.
   `SFML:Graphics` with one colon is invalid.
10. Optional: select **Refresh Discovery** and Continue. The refreshed list should
    match the currently configured dependency. Advanced Options contains internal
    targets, details, and a manual exact-target field validated by CMake.
11. Finish verification, select the new known entry in Add Library, and continue.
    Review **Approve Library Changes** before approving the project edit.
12. Expected: only the previewed managed dependency configuration changes. Existing
    source and assets remain unchanged. A consumer build must succeed before the
    project change is reported complete. Probe success alone is labelled separately.

## 3. Compile and run a real consumer

1. In the disposable project only, replace its generated `src/main.cpp` with the
   corresponding tested source from `CppLiveLab\tests\consumers`.
2. For example, edit `$project` to the disposable SFML project you just created:

   ```powershell
   $repo = 'C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab'
   $project = 'C:\replace-with-your-disposable-project\Stage5-SFML-Acceptance'
   Copy-Item -LiteralPath "$repo\tests\consumers\sfml.cpp" -Destination "$project\src\main.cpp"
   ```

3. Click **Build**, choose target `app` if required, then **Run**.
4. Expected: Raylib, SFML, GLFW and SDL3 open a window for about 1.5 seconds and exit
   successfully. fmt checks formatting; JSON checks parsing and serialization. Each
   prints `CPPLIVELAB_CONSUMER_OK`.
5. Repeat using a separate disposable project for each table row. Build Release too.
6. Optional CLI verification for that same project:

   ```powershell
   & "$repo\build\debug\cpplivelab.exe" manage-build $project --config Debug --target app --allow-configure-writes
   & "$repo\build\debug\cpplivelab.exe" manage-run $project --config Debug --target app --allow-configure-writes
   & "$repo\build\debug\cpplivelab.exe" manage-build $project --config Release --target app --allow-configure-writes
   ```

7. Expected: Debug and Release remain separate. Building does not require opening
   a developer terminal. An unchanged incremental Run must not relink unnecessarily.

## 4. Repeated additions and project switching

1. In one disposable project add fmt, then JSON, then GLFW without restarting.
2. Expected: three added-library rows, followed by one Add Library control; each
   dependency remains present. A new GitHub URL field starts empty each time.
3. In another disposable project add Raylib, then SFML. Repeat in reverse order in
   a third project. Select the appropriate public targets explicitly.
4. Expected: no stale repository/version/target list, duplicate FetchContent
   declaration, accidental removal, or required application restart.
5. Switch between projects with Project > Open Existing Project. Reopen the first.
6. Expected: each project retains its own targets, configuration and dependency pins.

## 5. Change targets and verify rollback

1. Open the working disposable SFML project containing `sfml.cpp`.
2. In Manage Libraries select its SFML entry, action **Change Target**, Continue.
3. Expected: current selections are shown. The already pinned archive/source is
   reused; no silent version upgrade occurs.
4. Keep all five tested SFML modules, approve verification, then review and approve
   the project preview. Build and Run should still work.
5. Test a meaningful failure: change the selection to `SFML::System` only while
   leaving the Graphics/Audio/Network consumer source unchanged.
6. Expected: the project consumer fails to compile/link, the failure is reported,
   and the previous project links/pin are restored. Source is untouched and the
   previous usable build/cache remains. Select Build and Run again to verify recovery.
7. Editing a managed block yourself should produce a refusal to overwrite that edit.
   Interrupted edits have an explicit **Recover Interrupted Change** action; later
   user edits must be reviewed before an automatic backup can be restored.

## 6. Update a version

1. Open the disposable JSON project. Note the current version and commit in Manage
   Libraries. Keep another project pinned to the older version for comparison.
2. Select **Update Version**, retrieve available releases, and choose another valid
   release or Latest Stable. Do not invent a tag.
3. Expected: the current pin and proposed exact commit are shown. Review the targets,
   verification result and project preview, then approve.
4. Expected: the updated consumer configures, compiles, links and runs. The other
   project's old pin remains unchanged. Source is never rewritten to handle API changes.
5. Try a clearly nonexistent tag. Expected: actionable validation failure, no project
   edits, and the previous working pin retained. Cancel to leave the old version.

## 7. Remove and re-add

1. On a disposable project, select a linked library and **Remove from Project**.
2. Read the preview, then approve removal.
3. Expected: only that library's managed declarations/links are removed. Other
   dependencies, source, assets and caches stay intact.
4. If source still includes the removed library, Build should fail clearly.
5. Use Add Library to attach the same known entry again and approve the preview.
6. Expected: the original pin is reused, and the unchanged consumer builds/runs again.

## 8. Catalog deletion, cache deletion and repair

1. Select a custom entry in Manage Libraries and choose **Delete Known Library /
   Hide Built-in**. Read its references and confirm only if you intend that deletion.
2. Expected: the custom entry disappears from the Add catalog. Existing projects
   retain independent pins and still reopen. Cache deletion is a separate action.
3. For fmt/JSON built-ins, expected behavior is Hide, not deleting integration code.
   **Show Hidden Built-in** restores visibility.
4. Choose **All Managed Caches** or the selected library's **Delete Cache** action.
5. Expected: readable library names, source/compiled/temporary categories, measured
   sizes, missing/unavailable status, known references and advanced locations.
6. Delete one eligible entry after checking its explicit confirmation. Shared-version
   deletion requires a separate Yes selection. Active builds/programs must block it.
7. Expected: only that registered owned entry is removed. Links/junctions, `.git`
   contents, external installations and unregistered legacy folders are refused.
   A locked-file failure must be reported truthfully; it must remain retryable.
8. Select the affected project entry and **Reinstall / Repair**.
9. Expected: the same pin/options/targets are used. A verified archive can be
   re-extracted without redownloading; deleted archives require a download. A fresh
   consumer build validates repair. Source and other libraries remain unchanged.

## 9. Restart

1. Open a disposable project, choose a theme and preferred editor, and select Release.
2. Click **Restart** beside Settings, or press Alt+R.
3. Expected: a new process replaces the old one; only one visible instance remains.
   The project, theme, editor, configuration and other persistent preferences return.
4. Open Add GitHub Repository again. Expected: empty URL input and fresh temporary
   discovery state. Restart does not reinstall dependencies or restore a checkpoint.
5. Start a build or an interactive/graphical program, then request Restart.
6. First choose Cancel. Expected: the current application/task stays active.
7. Request Restart again and approve. Expected: only owned work stops, then the
   replacement opens. Visual Studio and unrelated programs remain open.
8. During a dependency download, Alt+R should offer the same safe confirmation.
   Cache deletion/metadata work must finish at a safe boundary before replacement.
9. A replacement startup failure must keep the original application open and report
   the failure rather than entering a restart loop.

## 10. Existing-project and Stage 1–4 regression checks

1. Open FarmGame in its existing folder. Expected: its original CMake, sources,
   assets and directory organization remain intact. Opening alone does not configure.
2. Select its existing preset/target/working directory when applicable. Do not approve
   library edits to FarmGame merely to test the new management UI.
3. Build, Run, Stop and Open Editor should retain their approved behavior. A running
   executable must not be overwritten; Stop must leave unrelated processes alone.
4. Recheck the calculator's interactive name/numbers input, Debug/Release, Auto Build,
   compiler error navigation, separate consoles and Dark/Light/Follow Windows.
5. Close CppLiveLab and run the full automated approval commands below.

## Automated approval commands

```powershell
Set-Location 'C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab'
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Quick -Jobs 4
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Full -Jobs 4 -Doctor
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Libraries -Jobs 1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\format.ps1 -Check
git diff --check
git status --short
```

Quick is deterministic/offline. Full retains the original network-dependent
regressions. Libraries is an explicit live-network suite that drives the real Add
forms, builds/runs all six consumers, then exercises multiple-library management.
It can take substantially longer than Quick. Every automated project/state folder
is isolated from your projects and normal catalog/preferences.

Do not commit/push or approve the next stage until you accept these results.
