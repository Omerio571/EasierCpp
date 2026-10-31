# EasierCPP V1 — Phase 1 repository audit

Audit date: 2026-10-30. Repository: `C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab`.
This report describes the actual working tree, including existing uncommitted work.
Mockups, earlier plans and historical test reports were not treated as verification.

The new public direction is **EasierCPP — Make Your Life Easier**. The application
currently displays **Cpp**, and its engineering version is **0.1.0**. Public branding
and the distinction between V1 and engineering version numbers belong in Phase 2.
No major interface redesign or later feature phase was started.

## Scope and preservation

Read AGENTS.md, CODING_STYLE.md, .clang-format, UI_DESIGN_GUIDELINES.md,
GENERATED_PROJECT_STANDARDS.md, CMake/presets, application modules, test registrations
and relevant test bodies. Retained C++20, Win32 and the existing CMake architecture.
No installer, framework, permanent PATH change, internal rename, commit/push,
FarmGame build or unrelated-project mutation occurred.

Built separately in `build/easiercpp-phase1`. Raw evidence and audit-only probes are
in `build/easiercpp-phase1-audit`; projects and manager state used owned temporary
roots. The source-before/source-after SHA256 inventories cover **97** maintained
source/script/config/guidance files: only `src/workspace.cpp` and
`tests/stage5_tests.cpp` changed. The other **95** remain byte-identical. Existing
dirty/untracked work was preserved. These two audit/plan documents are new.

The existing local-only test restriction remains in force. Graphify was not queried
with repository information; its separate isolation pilot remains uncleared.
Context7/GitHub were used only for public CMake documentation, not repository upload.

## 1. Current architecture

Two executables, `cpplivelab.exe` and `cpplivelab-gui.exe`, share `cpplivelab_core`.
The native GUI uses Win32 controls, Rich Edit, GDI, COM, Common Controls, UxTheme and
DWM. The manager requires neither Raylib nor SDL. Its CMake minimum is 3.20; new
starter templates require 3.24. Displayed branding can change without renaming the
executables, namespace, repository, state paths or managed CMake ownership markers.

| Responsibility | Existing owner and evidence | Minimal boundary recommendation |
|---|---|---|
| Project configuration/import | `workspace.hpp/cpp`: `ProjectOptions`, `inspect_project`, profile/build paths; `workspace_cli.cpp` | Keep configuration/preset/target/path identity here; expose factual target state instead of deriving it from widgets. |
| Creation/templates | `projects.cpp`, `workspace_creation.cpp`, `project_templates.hpp::cmake_project` | Preserve shared templates, new-folder safeguards and ordinary CMake output. |
| Library operations | `libraries.hpp/cpp`: records, schemas 1–3, previews, transactions, rollback, caches; `library_manager.*`/`library_workflow.*` adapt UI | Backend owns changes/verification; forms produce proposals. Preserve separate approval and exact-file ownership. |
| CMake/toolchain execution | `doctor.cpp`, `process.cpp`, `workspace.cpp::managed_build`, CLI, `scripts/file_api.cmake` | Retain discovered tools and child-only developer setup; reuse execution rather than inventing a build system. |
| Scheduling/freshness | `auto_build.*`: `ProjectWatcher`, `AutoBuildScheduler`; `build_freshness.*` | Scheduler decides when; executor builds; lifecycle chooses replacement. Keep content debounce and one pending follow-up. |
| Process lifecycle | `session.*::ConsoleSession`, `windows_support.hpp`, `build_lease.hpp`; transitions in `gui.cpp` | Sessions own jobs/handles/I/O. Incrementally extract one small lifecycle controller with immutable operation identity. |
| Window tracking | `application_windows.*`; restoration polling in GUI | Keep native API work here; separate preference persistence and restoration coordination from widgets. |
| GUI state/events | `gui.cpp::Application`, window procedure, shared forms/theme/console adapters | Reuse toolkit/actions. The 3,334-line GUI currently mixes too many responsibilities; extract by tested responsibility, not rewrite. |
| Output/diagnostics | `console_model.*`, `console_view.*`, `output_decoder.*`, `build_diagnostics.hpp` | Preserve separate channels, bounded history, prompts and raw compiler output. |
| Settings | `appearance.cpp`, `workspace.cpp`, `editors.cpp`, `libraries.cpp`, `application_windows.cpp` | Keep formats compatible; avoid parallel GUI/project state and pass one profile snapshot to each operation. |
| Manager restart | `restart.*::GuiInstance`/`RestartLaunch` | Preserve one-shot acknowledged handover, separate from user-program Restart. |

Persistence is real, not mockup state: `%LOCALAPPDATA%\CppLiveLab` is the normal
root; `CPPLIVELAB_STATE_HOME` isolates tests. Project/configuration/preset/toolchain
identity and active build slots determine external build paths. Project settings,
recents, `auto-run.settings`, editor settings and library manifests live outside
imported source trees. Appearance/legacy AutoBuild/debounce remain under
`HKCU\Software\CppLiveLab`. Configure-write authorization is session-only.

There are two existing execution entry paths: legacy `build/run` in `projects.cpp`
and managed `manage-build/manage-run` in `workspace.cpp`. Keep their public behavior
and deliberately share execution policy where needed; do not remove legacy commands.

## 2. Feature inventory

**W:** Working and tested in the stated scope. **I:** Implemented but insufficiently
tested. **P:** Partially implemented. **N:** Not implemented. **B:** Broken/requiring
repair. None of these bounded tests proves compatibility with every C++ project.

| Feature | Status | Implementation evidence and verification/limit |
|---|---|---|
| CLI help/version/doctor | W | `main.cpp`, `doctor.cpp`; `cli_*`, `doctor_unit`, `doctor_without_path`. Actual product text is still Cpp. |
| Discovery outside terminal PATH | W | `doctor.cpp::system_discovery_inputs/discover_toolchain`; VS installation and separate/VS CMake/Ninja/Clang searches; installed-vs-terminal distinction and optional GCC tested. |
| Automatic MSVC environment | W | `process.cpp::developer_environment` captures trusted VsDevCmd changes for children; actual ordinary-shell builds passed. |
| Installed Clang usability | W | Audit-only explicit Clang preset compiled/linked/ran a C++20 iostream program using the installed Windows SDK. Not a compiler-version support matrix. |
| Single-file, multi-file and calculator projects | W | `projects.cpp`, shared templates; Stage 2/3 integration and real GUI calculator flow. |
| New empty/multi-file managed projects | W | `workspace_creation.cpp`; no-overwrite, generated style, project and unified tests. |
| Read-only CMake import/custom layouts/assets | W | `workspace.cpp::inspect_project`; external profiles/builds and source/assets/preset snapshot regressions. Non-CMake projects are refused. |
| CMake/Ninja Debug/Release | W | Actual compile/run, configuration separation and freshness regressions. |
| VS2022/Ninja Multi-Config presets | W | `stage5_presets` builds/runs actual configurations and checks original preset directories remain untouched. |
| Project/toolchain/vcpkg configuration | P | ProjectOptions/forms and vcpkg toolchain wiring exist. A real vcpkg package ecosystem was not installed or exercised. |
| File API executable discovery | W | `file_api.cmake`, `workspace.cpp::executable_targets`; fresh/multiple/library-only/failed-build probes. |
| Empty/unconfigured selector UX | B | Multiple states share an unexplained empty combo; confirmed first-compile-failure refresh defect, below. |
| Corrupt target metadata | W after fix | New strict parser error and regression; missing/valid-empty metadata remain valid. |
| Multiple targets/selection | W bounded | Two actual executables discovered; selected `second` ran. Sole-target GUI selection verified. Stale saved selection needs clearer UX. |
| Manual Build/Run/freshness | W bounded | Leases, fingerprints, selected artifacts and Ninja dry-run tests. VS-generator Run still invokes configure/build rather than the Ninja reuse fast path. |
| Program Stop/Restart/tree ownership | W bounded | Suspended launch, job assignment, explicit inherited handles, async input, graceful close/EOF and opt-in force-stop; local process/native regressions passed. Uncooperative UI timeout combinations need more qualification. |
| Manager restart | W | `restart.*`, real isolated GUI handover/acknowledgment tests. |
| Interactive input/stdout/stderr | W | Session/decoder/model/view tests cover prompts, send/EOF, streams, clear/history and retained input. |
| Compiler diagnostics/editor locations | W bounded | Parser and editor argv/navigation fixtures pass. Actual VS instance/column navigation was not manually observed here. |
| Editor launch while running | W bounded | Custom editor fixture verified running PID survives. VS2022 discovery exists; a user's IDE session was not launched during this audit. |
| Native GUI/themes/forms/welcome/recents | W bounded | Native UI/controller/layout and contrast regressions. Physical mixed-DPI/manual desktop acceptance remains needed. |
| Project libraries/multiple additions | W bounded | LibraryRecord, schema 3 with schemas 1/2 readable, project manifests; local two-library consumer and persistence tests. |
| Local CMake/header-only/installed package integration | W bounded | Proper source/imported/INTERFACE targets; actual owned offline consumer compile/run, repair/removal and missing-package rollback. |
| Dependency transactions/caches | W bounded | Exact previews, approval token, locks, backup/rollback, managed blocks and registered-owned-cache tests. |
| GitHub FetchContent/version/target workflow | I | Passive discovery and separately approved configure/link probe implemented; offline fixtures pass. Current live network execution is deferred. |
| Real Raylib/SFML/SDL3/GLFW/fmt/JSON matrix | I | Representative consumers and opt-in live suite exist, but were not executed in this local-only audit. |
| Library Edit/Update/Repair | P | Pins/options/targets/package/path changes and revalidation exist. Local version fields can be descriptive; requested package version is not a resolved installed-version inventory. |
| Automatic update availability/compatibility solver | N | UI explicitly says update availability is not checked. No universal automatic dependency solver/package installer exists. |
| Manual/transitive dependency adoption | P | Passive evidence and conservative target/owned-block overlap checks; arbitrary nested CMake is not exhaustively understood or adopted. |
| Watch/debounce/content deduplication | W | Notification/filter/overflow/content and scheduler tests. Source reconciliation still scans synchronously; large-project responsiveness needs measurement. |
| Auto Run while old program survives | W bounded | Alternate slots, existing CLI, coalescing/freshness and selected artifacts; real GUI failure/PID retention, repair, rapid/identical saves, OFF, crash-loop and restart tests. |
| Normal/maximized window restoration | W bounded | Native API tests plus real restarted Win32 rectangle/maximization in GUI flow. |
| Multiple/delayed/recreated windows | P | Class/title/deterministic fallback and 15-second polling; ambiguous identities and unusual readiness/recreation need wider end-to-end coverage. |
| Multi-monitor/mixed-DPI/disconnection | I | Monitor identity/geometry/fallback and negative-coordinate unit tests exist. Saved DPI is not used to derive scaled restoration; no physical display matrix ran. |
| Borderless/exclusive fullscreen/Snap Groups | P | External rectangle restoration is best-effort and work-area clamped. No exclusive-mode cooperation or native Snap Group restoration. |
| Preference persistence | W bounded | Registry/files and isolated round-trip tests; retained paths/formats. Exhaustive crash/power-loss qualification is absent. |
| EasierCPP displayed branding | N | Only this audit/plan adopt the new name; Phase 2 must update displayed product text. |
| Universal hot reload/state migration/new language/AI debugger | N, excluded | Intentionally outside V1. Process restart/window placement do not preserve game memory. |

## 3. Executable target selector: actual findings

`managed_build` writes the codemodel query, configures CMake, invokes
`file_api.cmake`, reads `executables.txt`, builds, then refreshes artifacts.
`reload_targets` in `gui.cpp:943` clears/repopulates the combo and selects a valid
remembered target or a sole target. It does not guess among multiple executables.
The `targets` CLI reads metadata; it does not configure.

CMake replies are generated during configuration/generation, not compilation;
see the [official File API manual](https://github.com/Kitware/CMake/blob/master/Help/manual/cmake-file-api.7.rst).
The current UI reaches this through Build, without a separate Configure action.
A build in Visual Studio's own directory is not the manager's configured profile.

| Scenario | Observed result | Diagnosis |
|---|---|---|
| Fresh project before manager Build | No manifest; empty combo; CLI says Build first | Expected unconfigured manager profile, not general discovery failure. |
| Successful single-executable Debug build | `app` metadata/artifact; GUI selects app | Discovery and sole selection work. |
| Release after only Debug configured | No Release targets | Expected independent configuration state. |
| Two executable targets | Both names discovered; selected second runs | An empty selection can be intentional: choose a target. |
| Configured static-library-only project | Successful build, zero executable targets | Expected non-runnable project; “Build first” is misleading here. |
| Initial compilation fails after configure | CLI has app target, artifact absent | Discovery succeeded; compilation failed. Target presence is not proof of a runnable binary. |
| Same failure in actual hidden GUI controller | Backend has 1 target; combo has 0 items | **Confirmed refresh defect**: compiler-failure branch does not reload targets. Repaired successful Build populates/selects app. |
| CMake configuration fails in a fresh profile | No manifest | Fix configuration/preset/toolchain; make this state explicit. |
| Remembered target removed/renamed | Saved name has no matching selected item | Static inspection: stale selection, not necessarily discovery failure. Not separately fault-injected in this GUI probe. |
| Truncated/blank manifest | Previously silent empty/partial data; now specific error | Phase 1 parser fix. Reconfigure/rebuild the selected profile; do not delete arbitrary directories. |

“Empty” therefore needs explicit unconfigured/configure-failed/no-executable/
selection-required/stale-selection/metadata-error presentation. Phase 2 should
refresh from validated current configure results even when compilation fails, while
avoiding stale metadata after an unsuccessful reconfiguration.

## 4. Tests actually executed

Windows 11 Home 64-bit, build 26200 (10.0.26200); VS2022 Community/MSVC 19.44;
CMake 3.31.6-msvc6; Ninja 1.12.1; Clang 19.1.5. Fresh build: `build/easiercpp-phase1`.

| Check | Before fix | After fix |
|---|---|---|
| Formatting, clang-format 19.1.5 | Passed | Passed, 80 maintained C++ files |
| Quick | 34/34 passed; 7.94s | 34/34 passed; 5.40s |
| Permitted local selection from Full, 4 workers | 49/49 passed; 204.62s; Doctor passed | 49/49 passed; 170.83s; Doctor passed |
| New malformed-manifest regression on original parser | Failed as expected, 1/1; 0.15s | Passed within Quick/local regressions |
| Target-state probe | 17 expected checks plus 1 defect reproduction | 19 expected checks passed, including Clang build/run and corrupt metadata error |
| Hidden real GUI target-state probe | Fresh empty state and successful selection verified | First-failed-build refresh defect reproduced on post-parser-fix GUI; left for Phase 2 |
| Source/Git checks | Existing dirty state recorded | Whitespace check passed; 97-file hashes show only 2 intended code/test changes |

There were **no failures in the original permitted baseline suite**. The parser
issue was a pre-existing coverage gap exposed by a new test, not introduced by the
fix. Deliberate configure/compiler errors are expected negative probe cases.
The standalone GUI audit harness initially lacked its include path; that harness
setup was corrected and successfully rerun, not counted as an application failure.

Registration is 52 tests: 34 Quick, 51 regular Full and 1 separate live-library
suite. Selections overlap. Under the existing local-only restriction, these were
**not run**: `stage5_unified_raylib`, `stage5_libraries`, `stage5_live_libraries`.
No test was removed, disabled or relabelled. Filtered local Full is not unfiltered
Full. Real FarmGame, physical mixed-DPI/displays, fullscreen and real IDE interaction
were not exercised during this audit.

Raw evidence under `build/easiercpp-phase1-audit`: `quick-baseline.log`,
`local-regressions-baseline.log`, `target-regression-before-fix.log`,
`quick-after-fix.log`, `local-regressions-after-fix.log`, `target-probe-baseline.json`,
`target-probe-results.json`, `target-probe-after-fix.log`, `gui-target-probe.log`,
`format-baseline.log`, `format-final.log`, `os.json`, `registered-tests.json`,
`test-labels.json`, `source-before.json`, `source-after.json`, and audit probe sources.
Owned temporary fixture cleanup completed; reproducible evidence remains in build/.

## 5. Small foundation fix

Only `workspace.cpp::executable_targets` and the existing `stage5_unit` test changed.
Incomplete name/artifact pairs, blank records, existing unreadable files and read
errors now yield manifest-specific diagnostics. Missing metadata remains expected;
an empty complete manifest remains valid for library-only projects.

The permanent regression covers missing/empty data, orphan/blank records, refusal
to return partial results, useful error text and valid multiple-target parsing
afterward. Read-error branches are not claimed as exhaustively fault-injected.
No GUI layout, branding, target naming, settings path/schema or user project changed.

## 6. Problems ranked by severity

No P0 or new confirmed data-loss failure was observed. Gaps and static risks are
distinguished from reproduced defects.

| Priority | Finding/evidence | Recommended owner/action |
|---|---|---|
| P1 release qualification gap | Real-project/live-library and physical monitor/DPI behavior remain insufficiently verified for dependable everyday V1 use. | Approved dependency tests, then separately approved real-project/manual acceptance. Local fixtures do not establish production readiness. |
| P2 confirmed defect | First failed compilation leaves GUI selector empty despite valid current target metadata. | Phase 2 target-state UI/current configure-result identity. |
| P2 diagnostic gap | Library-only, missing metadata, stale selection and configuration failure share ambiguous empty/unconfigured messages. | Explicit discovery/configuration status and actionable messages. |
| P2 fixed defect | Truncated manifest silently yielded empty/partial targets. | Completed Phase 1 parser regression/fix. |
| P2 static correctness defect | `poll_auto_run` unconditionally says the previous application is still running when replacement fails; it may have exited meanwhile. Not dynamically fault-injected here. | Phase 4 lifecycle/status tests and state-aware notification. |
| P2 architecture risk | GUI coordinates too many mutable flags, worker lifetimes, activation and restoration responsibilities. | Small tested controller extraction in Phases 2/4; no rewrite. |
| P2 qualification gap | Uncooperative graceful timeout/force-stop, ambiguous multiple windows, very late startup and recreation lack a full matrix. | Phases 4/5 controlled fixtures/manual checks. |
| P2 partial restoration | DPI is recorded without an explicit scaling policy; borderless restoration is work-area clamped. | Phase 5 physical/logical policy and display/fullscreen qualification. |
| P2 partial libraries | Metadata is not exhaustive resolved dependency inventory; arbitrary manual/transitive CMake is not adopted. | Phase 3 ownership/version/status semantics and consumer matrix. |
| P3 performance risk | Source reconciliation/inspection and metadata work can run on UI thread. Not benchmarked on large real projects here. | Profile before adding threads; retain bounded owned/cancellable work. |
| P3 behavior limitation | VS-generator unchanged Run invokes configure/build rather than Ninja fast-path reuse. Builds remain incremental. | Phase 4 measure/test correctness before optimization. |
| P3 product/docs debt | Cpp/CppLiveLab text, historical reports and internal 0.1.0 differ from EasierCPP V1 identity. | Phase 2 display-only branding and current docs; retain compatibility identifiers. |

CMake write detection is advisory regex inspection, **not a sandbox**. External
build directories do not prove arbitrary CMake cannot write source. Continue to
require trusted/approved project/dependency execution; this is an architectural
limit, not a newly reproduced exploit.

## 7. Next phase and reproduction

See [EASIERCPP_V1_IMPLEMENTATION_PLAN.md](EASIERCPP_V1_IMPLEMENTATION_PLAN.md) for all
five requested workstreams. Phase 2 should primarily modify GUI/display text/current
docs and target-state helpers/tests, reusing workspace/File API, native forms/theme
and palettes. No framework, dependency controller or process rewrite is needed.

Close only a manager launched from the audit build before rebuilding it:

```powershell
Set-Location 'C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab'
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\format.ps1 -Check
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -BuildDirectory .\build\easiercpp-phase1 -TestMode Quick -Jobs 4
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -BuildDirectory .\build\easiercpp-phase1 -TestMode Full -TestFilter '^(redirection_|reliability_|stage5_(restart|library_|unified_(unit|projects)|ui_|github_versions|unit$|projects$|presets$)|stage[234]_|doctor_|maintenance_|cli_|test_selection)' -Jobs 4 -Doctor
.\build\easiercpp-phase1\cpplivelab.exe --version
.\build\easiercpp-phase1\cpplivelab.exe doctor
Start-Process .\build\easiercpp-phase1\cpplivelab-gui.exe
```

Manual target check on a new disposable project: open before Build (empty expected),
introduce a syntax error and Build (confirmed stale GUI selector), repair and Build
(app becomes selected), then compare unconfigured Release and configured library-only
or multiple-executable projects. Do not edit an unrelated real project for this check.
Displayed product text remains Cpp until approved Phase 2 branding.
