# Stage 3 manual testing

1. **Build and run all automated tests.** In ordinary PowerShell:

   ```powershell
   cd C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab
   powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Doctor
   ```

   Expected: all registered CTest tests pass, including the original 11 Stage 1–3 cases,
   and the command exits 0. No developer shell setup
   is needed. Close CppLiveLab's GUI before rebuilding the manager itself.

2. **Launch the main application.**

   ```powershell
   .\build\debug\cpplivelab-gui.exe
   ```

   Expected: a native Windows window opens, with no console window. Keep the adjacent
   `cpplivelab.exe` companion in place. You can also double-click the GUI executable.

3. **Open an existing project.** Paste this into **Project folder**, then click
   **Open Project** (or choose it using **Browse...**):

   ```text
   C:\Users\omer2\OneDrive\Desktop\Projects\CppLiveLab-UserProjects\Stage3Calculator
   ```

   Expected: active project `Stage3Calculator` and its full location appear; Build,
   Run and Edit become enabled. This fresh addition demo is separate from both
   CppLiveLab and your previously edited CalculatorDemo. An invalid folder produces
   a clear error without replacing the last valid active project.

4. **Open the editor.** Click **Edit in Visual Studio 2022**.

   Expected: the installed Visual Studio 2022 opens that folder as a CMake project.
   Write/debug code in Visual Studio; save changes there, then return to CppLiveLab.
   Existing presets and CMakeSettings are not rewritten by CppLiveLab. Visual Studio
   may perform its own CMake configuration using its separate build directory.

5. **Build.** Leave **Debug** selected and click **Build**.

   Expected: CMake/Ninja output appears in the output pane, followed by `Build succeeded`
   and `Finished (exit 0)`. The application remains responsive. Your executable is under
   the project's `.cpplivelab\build-Debug`, not the manager's `build\debug` folder.

6. **Run interactively.** Click **Run**. At each prompt, type into **Program input**
   and click **Send** or press Enter:

   ```text
   Ada Lovelace
   2.5
   3.75
   ```

   Expected: prompts appear one at a time, then `Hello, Ada Lovelace! Sum: 6.25`, and
   `Finished (exit 0)`. Build/Run stay disabled while the program is running. For an
   input-driven program, **End Input** sends EOF rather than a line of text.

7. **Stop, then rebuild.** Click **Run** again. While it waits for the name, click
   **Stop**. Wait for `Stopped`, then click **Build** and **Run** again.

   Expected: the owned process tree exits; the interface stays responsive; Build/Run
   become available only after it has exited. The next build succeeds without an
   executable-in-use error. Stop is forceful and does not invoke program cleanup code.

8. **Test Release.** After stopping or finishing the current run, choose **Release**,
   then click **Build** and **Run**. Enter the same three values.

   Expected: the same result, with a separate `.cpplivelab\build-Release` executable.

9. **See a compilation error.** In Visual Studio, add this line at the top of the fresh
   `Stage3Calculator/src/main.cpp`, save, then click **Build** in CppLiveLab:

   ```cpp
   #error Stage3 compiler output check
   ```

   Expected: the output pane displays the filename, line and compiler error, followed
   by `Compilation failed` and a nonzero exit. Clicking Run must also refuse to launch
   the previous executable. Remove the line, save, and Build again; expect exit 0.

10. **Close while running.** Run the calculator and leave it waiting for input. Close
    CppLiveLab's window. Reopen the GUI and the same project, then Build.

    Expected: the previous program is gone, rebuilding succeeds, and Visual Studio
    remains open. Project selection is intentionally not remembered between launches.

11. **Check an existing project with a different executable name.** Open your own CMake
    folder and set **Executable** to its output path relative to the chosen build
    directory, for example `myprogram.exe` or `src\myprogram.exe`. Click Run.

    Expected: the selected program builds and runs. If the path is wrong, the output
    explains that the executable could not be found. Your CMake files stay unchanged.

12. **Stage 1/2 regressions.** From the same PowerShell folder:

    ```powershell
    .\build\debug\cpplivelab.exe --version
    .\build\debug\cpplivelab.exe doctor
    .\build\debug\cpplivelab.exe run ..\CppLiveLab-UserProjects\Stage3Calculator --config Debug
    ```

    Expected: version `CppLiveLab 0.1.0`, installed tools discovered, and the calculator
    runs in the terminal. Run it from only one CppLiveLab instance per configuration;
    another instance will report `already building or running` until the first finishes.
