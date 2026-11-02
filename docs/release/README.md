# EasierCpp — Make Your Life Easier

A lightweight C++ development companion for standard CMake projects. EasierCpp
builds and runs your code while you edit it in Visual Studio or another editor.
It does not include an editor/debugger or preserve game/application memory.

## Launch requirements

- Windows 11 x64. Local testing used Windows 11 Home build 26200; other machines,
  ARM64 and 32-bit Windows have not been verified.
- Microsoft Visual C++ v14 **x64** Redistributable, at least as recent as the
  MSVC 14.44 build toolset used for this package. If an MSVCP140/VCRUNTIME140 DLL
  is missing, use [Microsoft's official download page](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist).
  The package does not bundle or silently install it.
- Extract the complete ZIP, then launch **EasierCpp-Gui.exe**. Keep EasierCpp.exe
  and the resources directory beside it. Ordinary application use requires no
  administrator privileges. Use both binaries from the same extracted package.

## Tools for compiling projects

Install Visual Studio 2022 Community or Build Tools with Desktop development with
C++, an x64 MSVC toolset and Windows SDK. CMake and Ninja may be installed through
Visual Studio or separately. No developer shell or permanent PATH edit is required.
Supported Clang-cl configurations can use the discovered VS/SDK environment;
arbitrary compiler/SDK combinations have not been certified. GCC is not required.
Windows PowerShell is used for library discovery/metadata. Git may be required by
a project's own dependency configuration.

## First project

1. Open Project → New Project and choose a writable folder **outside this package**.
   Or Open an existing folder containing CMakeLists.txt; keep its layout intact.
2. Select Debug or Release and the intended executable target after configuration.
   An unconfigured project may not yet have a target list. Build it, then Run.
3. Edit sources in your preferred editor. Turn **Auto Run** on to build saved
   changes incrementally and run the updated result. Off disables automatic work.
   There is one Auto Run control, not a separate automatic-build mode.
4. Build, Run, Stop and Restart remain manual controls. Stop disarms automatic
   launching until Run or an Auto Run toggle resumes it. Failed builds retain a
   working application when available.
5. Preferences offer **Safe** replacement (close old before launching new) and
   **Seamless** (start a candidate while the old process remains, then hand off).
   Use Safe for exclusive-instance applications. Window placement can be restored
   where supported; this is not application/game-state restoration.

## Libraries and workers

Manage Libraries supports known integrations, public GitHub/gitlab.com repositories,
local CMake sources/headers and installed CMake packages. Choose a version/ref when
applicable; repository integrations pin the resolved commit and archive hash.
Review third-party execution and project-change approvals. Compatibility is not
proved by CMakeLists.txt alone. Detected external libraries are read-only; unknown
versions/compatibility remain unverified. Existing vcpkg toolchain/package usage is
supported; this is not a universal package manager.

Preferences → Parallel Build Workers saves a manual count, subject to current CPU
and available-memory limits. Optimize Workers uses a disposable synthetic benchmark;
its recommendation is an estimate. Cancel preserves the previous preference.

## Troubleshooting and data

Run `EasierCpp.exe doctor` in PowerShell, or use Tools → toolchain diagnostics.
Discovery reports installed tools separately from terminal PATH availability; a
successful build establishes compiler/SDK usability. Compiler, CMake and library
details appear in Build & System; program I/O has its own console.

Settings/caches/builds normally use `%LOCALAPPDATA%\EasierCpp`; appearance and
related preferences use `HKCU\Software\EasierCpp\Preferences`. Supported legacy
CppLiveLab metadata is copied and validated on first use; originals remain intact.
An existing new project/catalog/settings unit wins a conflict without merging.
Review `migration-report.txt` in the new state folder and the startup console note.
Close the old manager before first migration; resolve any interrupted legacy
library transaction using the old manager first. Build/download caches are not
copied. The first build therefore needs fresh configuration/compilation. Some
preserved absolute PCH/library references still use the retained original data:
**do not delete the old AppData folder as part of upgrading.**

This is a portable application
folder, **not a self-contained toolchain or settings-on-a-USB mode**. User projects
stay separate. Removing this folder does not remove user projects or cached state.

If resources are missing, re-extract the complete ZIP. If moving the application
invalidates an existing PCH/CMake cache, review and approve Clean & Rebuild through
the normal recovery flow; do not manually rewrite or delete unrelated project files.
The source regression-suite command in Tools requires a non-portable source build;
the portable package explains how to use Toolchain Diagnostics/Doctor instead.
Doctor and normal runtime features
remain available. Tools are not bundled; network libraries need network access.

## Manual release acceptance

1. Extract to a new folder containing spaces, outside the source repository. Launch
   the GUI, run Doctor, and confirm no repository files are needed.
2. Create a disposable console project; build/run both Debug and Release. Test
   console input, then reopen an existing disposable CMake project.
3. Turn Auto Run on. Save a source edit and expect updated output. Introduce an
   error: the previous running application should remain. Repair and save again.
4. Test Safe and Seamless with a disposable graphical application. Move its window
   before saving; expect the supported placement behavior. Stop must not relaunch.
5. Inspect an external library: no destructive management actions. Add/remove a
   trusted managed library only after reviewing its approvals and project preview.
6. Save a worker count, restart and check persistence. Test Optimize Workers once
   if desired; cancellation must retain your prior count. Close the GUI normally.

See RELEASE_NOTES.md for known limits. Licensed under MIT; see LICENSE.
