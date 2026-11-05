# EasierCpp v0.1.2

**Make Your Life Easier**

A lightweight Windows C++ development companion for building, running, and managing
standard CMake projects alongside your preferred editor.

## What is EasierCpp?

EasierCpp simplifies the edit → build → run workflow. Write and debug your code in
Visual Studio 2022 or another editor; use EasierCpp to manage the project's builds,
application process, and supported dependencies.

## Features

- **Auto Run:** incrementally builds and launches projects when enabled, including
  when you reopen a project with Auto Run already on. Saved source changes trigger
  subsequent builds and application replacement.
- **Safe and Seamless Replacement:** keeps the current application running during
  compilation. Experimental Seamless mode also starts the replacement before closing
  the old instance, where supported. This replaces processes; it does not provide
  in-process hot reloading or preserve game state.
- **Window restoration:** independently keep application window position and size.
  New projects keep position by default while allowing code-defined dimensions.
- **Library Manager:** discover, download, verify, and integrate supported libraries
  through repository, local, and CMake package workflows. Compatibility is verified;
  arbitrary repositories are not guaranteed to work automatically.
- **CMake projects:** create C++20 projects or open existing layouts, select executable
  targets, and review compiler output. Existing-project edits require previews and
  approval where applicable.
- **Build settings:** Debug/Release, configurable parallel workers, optional CMake
  precompiled headers, and installed toolchain discovery.
- **Two-tree incremental builds:** the original build and one alternate normally
  rotate during Auto Run, retaining each tree's compiled dependencies and caches.
- **Program controls:** Build, Run, Stop, Restart, interactive console input/output,
  and launching your editor.

## What's new in v0.1.2

Auto Run now returns to the original compilation tree after its first alternate
update, avoiding a third cold build. Compatible unchanged projects can launch
without preparing the compiler SDK; compilation still initializes it when needed.
Existing caches, project layouts and replacement safety checks are preserved.
See [release notes and compatibility guidance](docs/release/RELEASE_NOTES.md).
This is a locally prepared candidate until publication is approved.

## Technologies

EasierCpp is built using **C++20** and native Windows technologies, with a focus on performance, lightweight design, and compatibility with existing C++ development workflows.

| Category | Technologies |
|---|---|
| **Programming Language** | C++20 |
| **GUI & Platform** | Win32 API, Windows SDK |
| **Build System** | CMake, Ninja |
| **Compiler & Toolchain** | MSVC, Visual Studio 2022 Build Tools |
| **Build Automation** | PowerShell, CMake Scripts |
| **Testing** | CTest, C++ Unit & Integration Tests |
| **Version Control** | Git, GitHub |
| **Library Management** | GitHub API, GitLab API, CMake FetchContent, find_package, add_subdirectory, vcpkg (optional) |
| **Code Formatting** | clang-format |
| **Process Management** | Windows Job Objects, Win32 Process APIs |
| **Build Optimization** | Incremental Compilation, Incremental Linking, Precompiled Headers (PCH), Parallel Builds |

### Development Tools

- **Visual Studio 2022** — C++ development and debugging.
- **CMake & Ninja** — Project configuration and incremental builds.
- **PowerShell** — Build automation and toolchain discovery.
- **CTest** — Automated testing and regression verification.

EasierCpp is designed to work alongside existing code editors and IDEs rather than replace them.

## Getting started

1. Get the supported Windows x64 portable ZIP from the
   [releases page](https://github.com/Omerio571/EasierCpp/releases).
2. Extract the **entire ZIP** into a writable folder.
3. Launch **EasierCpp-Gui.exe**. Keep the companion executable and resources together.
4. Install the required compiler/SDK and build tools below if missing; run Doctor.
5. Create a project or open an existing CMake project.
6. Enable **Auto Run** to build and launch. Projects with Auto Run already enabled
   start automatically when opened. If multiple executables exist, select one.
7. Edit and save files in your preferred editor. Check build output for errors and
   approve CMake configuration when prompted.

If no release is published yet, use the source-build instructions below.

Manual Build, Run, Stop, and Restart remain available. Stop pauses automatic launch
for the current open project. Run, toggling Auto Run, or reopening an enabled project
rearms launch. Auto Run OFF performs no automatic work.

Settings → Preferences contains **Keep Window Position (When Restarted)** and
**Keep Window Size (When Restarted)**. These apply to your application, including
manual restarts and Auto Run. Uncheck size restoration to use new dimensions from
code. Choices persist per project.

## Requirements

- Windows 11 **x64**.
- Microsoft Visual C++ v14 x64 runtime for the packaged application.
- To compile projects: a supported C++ compiler, Windows SDK, CMake, and build tool.
  The verified setup is **Visual Studio 2022 / Build Tools**, with **Desktop development
  with C++**, **CMake 3.24 or newer**, and **Ninja**. CMake/Ninja may be installed with
  Visual Studio; EasierCpp discovers installed paths outside the terminal PATH.
- Existing project presets, dependencies, runtime assets, and toolchains must be
  compatible with the project. Downloads require an internet connection.

The portable ZIP includes **no compiler or SDK** and does not permanently modify
PATH. Run `EasierCpp.exe doctor` for installed-tool diagnostics. See the
[portable user guide](docs/release/README.md) for runtime prerequisites and details.

Seamless Replacement and window restoration depend on the application's behavior;
exclusive-resource or fullscreen applications may require Safe mode. Physical
multi-monitor/DPI and individual library compatibility should be checked with your
own projects. Application memory and Windows Snap Groups are not restored.

## Building from source

With the requirements above installed, run from the repository folder in PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -TestMode Quick -Jobs 2
.\build\debug\EasierCpp-Gui.exe
```

The build script initializes the developer environment for its own processes.
See the [development guide](docs/development/ARCHITECTURE_MAP.md) for focused testing
and the [v0.1.2 release notes](docs/release/RELEASE_NOTES.md) for release behavior.

## Upgrading and troubleshooting

Close EasierCpp and its managed applications, extract the new ZIP to a fresh folder,
then launch it. Keep existing project folders, settings and build caches. Older A/B
and recovery slots remain supported. Older binaries may not understand v0.1.2's
`base` active-slot pointer; avoid sharing state between versions or downgrading
without reviewing the [two-tree compatibility guide](docs/development/AUTO_RUN_BUILD_TREES.md).
For missing resources, toolchain or runtime errors, see the
[portable troubleshooting guide](docs/release/README.md).

## License

EasierCpp is free and open source under the [MIT License](LICENSE).
Third-party software retains its own licenses; see [third-party notices](THIRD_PARTY_NOTICES.md).

## Feedback and contributions

Report bugs, suggest improvements, or contribute through
[GitHub](https://github.com/Omerio571/EasierCpp) and the
[issue tracker](https://github.com/Omerio571/EasierCpp/issues).
For bug reports, include the version, toolchain, reproduction steps, and relevant
build output with private information removed.
