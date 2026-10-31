# Independent maintenance checks

Nothing has been committed or pushed for you. Finish these checks, review the changes,
then use [GIT_CHECKPOINT.md](GIT_CHECKPOINT.md) when you decide they are ready.

1. **Open ordinary PowerShell and enter the manager folder.**

   ```powershell
   cd C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab
   ```

2. **Build and test the manager.** Close CppLiveLab's GUI first; keep Visual Studio open.

   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Doctor
   $LASTEXITCODE
   ```

   Expected: 16 tests pass, zero failures, then exit `0`. The build script initializes
   the Visual Studio environment automatically.

3. **Check the CLI version.**

   ```powershell
   .\build\debug\cpplivelab.exe --version
   ```

   Expected: `CppLiveLab 0.1.0`.

4. **Check installed tools from this ordinary terminal.**

   ```powershell
   .\build\debug\cpplivelab.exe doctor
   $LASTEXITCODE
   ```

   Expected: installed CMake, Ninja and MSVC are found; exit `0`. Their terminal
   availability can be `No`. GCC remains optional.

5. **Launch the graphical application.**

   ```powershell
   .\build\debug\cpplivelab-gui.exe
   ```

   Expected: the same native Windows interface, without an extra console window.
   Keep its companion `cpplivelab.exe` in the same build folder.

6. **Open the unchanged Stage 3 example.** Paste this into Project folder and click
   **Open Project**, or select it using Browse:

   ```text
   C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab-UserProjects\Stage3Calculator
   ```

   Expected: `Stage3Calculator` and its location appear. This is separate from both
   the manager and your older, edited CalculatorDemo.

7. **Build Debug.** Choose **Debug**, then click **Build**.

   Expected: output ends with `Build succeeded` and `Finished (exit 0)`.

8. **Run interactively.** Click **Run**. At each prompt, enter one line in Program input
   and press Enter or click Send:

   ```text
   Ada Lovelace
   2.5
   3.75
   ```

   Expected: each prompt appears before input; the result is
   `Hello, Ada Lovelace! Sum: 6.25`, followed by exit `0`.

9. **Stop a process.** Click Run again; while it waits for a name, click **Stop**.

   Expected: `Stopped`; no freeze. Build/Run become available after the owned processes exit.

10. **Rebuild after Stop.** Click Build, then Run again.

    Expected: rebuilding succeeds without an executable-in-use error. Finish the input
    or press Stop before continuing.

11. **Check Release.** Choose **Release**, then Build and Run. Enter the same values.

    Expected: the same sum, with a separate `.cpplivelab\build-Release` executable in
    the user project. The manager stays in its own `build\debug` directory.

12. **Open Visual Studio 2022.** Once idle, click **Edit in Visual Studio 2022**.

    Expected: that project opens in the installed IDE. Edit/debug there and save changes
    before returning to CppLiveLab. Existing CMake presets/settings stay intact.

13. **Read the revised source and style guide.** For example:

    ```powershell
    Get-Content .\src\windows_support.hpp
    Get-Content .\CODING_STYLE.md
    ```

    Expected: separate-line braces, four-space indentation, clear names and explanations
    of handle ownership. `src/session.cpp` explains jobs, pipe endpoints and cancellation;
    `src/project_templates.hpp` contains readable new-project examples. Existing user
    projects were not reformatted.

14. **Verify formatting without changing files.**

    ```powershell
    powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\format.ps1 -Check
    $LASTEXITCODE
    ```

    Expected: clang-format 19.1.5, `Formatting verified: 19 maintained C++ files`, exit `0`.
    When you later edit source, omit `-Check` to apply the same formatting rules.

15. **Verify Git ignores generated files.**

    ```powershell
    git check-ignore -v .vs/CppLiveLab/FileContentIndex/example.vsidx
    git check-ignore -v build/debug/cpplivelab.exe
    git ls-files .vs
    git ls-files -ci --exclude-standard
    ```

    Expected: the first two commands identify ignore rules. The last two print nothing:
    no Visual Studio database or other newly ignored generated file is tracked.

16. **Review Git status and whitespace.**

    ```powershell
    git status
    git diff --check
    ```

    Expected: status succeeds without the `.vsidx` permission error and shows source,
    documentation and configuration changes. The whitespace check has no error output.
    Modified/untracked approved Stage 2/3 files are expected until you commit them.

For additional error/close-while-running checks, use the existing
[Stage 3 guide](STAGE3_MANUAL_TESTS.md). Report any failing step, the visible output and
the command's exit code before committing. Stage 4 remains unstarted.
