# CppLiveLab coding style

Readable code is a product requirement. A longer, clear implementation is preferable
to a short expression that obscures ownership or control flow.

## Formatting

Use C++20, Allman braces, four-space indentation, no indentation tabs and approximately
100 columns. Put function bodies and conditional bodies on separate lines. Separate
functions and distinct operations with blank lines. Indent switch cases and their bodies.
Use `Type&` / `Type*` consistently. Keep Windows include/macro order intact.

Bad:

```cpp
if (p) { CloseHandle(p); p = nullptr; }
```

Better:

```cpp
// Release the parent's pipe endpoint so the reader can observe end-of-file.
if (output_write_handle != nullptr)
{
    CloseHandle(output_write_handle);
    output_write_handle = nullptr;
}
```

## Naming and comments

Keep existing public `snake_case` function/field names for compatibility. Use PascalCase
for types. Prefer names such as `build_output_directory`, `compiler_executable`, and
`process_information`; short loop coordinates/indexes are fine when immediately clear.
Private renaming must not change CLI strings, stored settings or Windows control IDs.

Document the purpose of important types/functions and the reason for non-obvious checks.
Explain what Windows calls do in this program: who owns each handle, why a child starts
suspended, why a pipe endpoint closes, why a pending write needs stable backing memory,
and how a job limits termination to our process tree. Do not narrate obvious assignments.

## Files and functions

Public interfaces live in `include/cpplivelab/`; implementations and small private helpers
live in `src/`. Headers must include what their declarations need. Use `#pragma once`.
Keep helper functions private when callers do not need them. Explain the public contract
in the header and the implementation-specific reasoning beside the relevant code.

Group a function's work into clear phases. Extract a helper when a block has its own
purpose (for example preparing the asynchronous input pipe). Avoid speculative frameworks.
The CLI coordinates builds; the GUI supervises that CLI instead of copying its pipeline.

## Resource ownership

`src/windows_support.hpp` contains the small CloseHandle owner and the shared Windows
argument-quoting helper. Do not copy an owning handle. Close each resource exactly once,
including on exceptions; distinguish `nullptr` and `INVALID_HANDLE_VALUE` failure values.
Closing a process handle releases a reference, not the process itself. Job termination
is separate and must never target unrelated applications.

Fonts need `DeleteObject`; COM interfaces need `Release`. Do not feed either to the
CloseHandle wrapper. Async buffers and OVERLAPPED events must survive until completion
or acknowledged cancellation. Never log a captured developer environment: it may contain
the user's credentials.

## Formatter commands

From the repository root in ordinary PowerShell:

```powershell
# Apply the repository style to maintained src/, include/ and tests/ C++ files.
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\format.ps1

# Verify without changing files; exits nonzero for a formatting/configuration error.
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\format.ps1 -Check
```

The script finds clang-format on PATH or through Visual Studio's vswhere discovery.
It validates `.clang-format` before touching files. Version 19.1.5 is the version verified
for this maintenance pass. Override discovery with `-ClangFormat 'C:\path\clang-format.exe'`.
Build outputs, other projects and third-party directories are outside its file selection.
New code inside the project-creation templates must also follow this style; formatting
does not rewrite already-created user projects.

For future changes, format, build, run relevant tests, then run the full regression suite
when behavior changes. Explain actual fixes separately from formatting in the review.
