# EasierCpp standalone UI preview

Double-click **index.html** in File Explorer. The preview opens locally in Edge, Chrome, or another modern browser. Start on Home and select Workspace to see the sample FarmGame session. Use **Dark mode** / **Light mode** in the header, or Settings → Appearance. Refresh the browser to reset the sample session.

The layout follows the supplied reference, with the original logo displayed from a small clipped region of the reference asset. The two workspace PNGs show the light and dark themes at 1360 × 850.

## Try it

1. **Home** — Open a recent project, forget a recent entry, or use New Project to create an in-memory sample. Check that the project name updates in the sidebar and Workspace.
2. **Workspace** — Select Debug or Release and an executable. Click Build and inspect Build & System. Run the program, send a line in Program Console, click End Input, then Restart to reopen the input stream. Stop pauses automatic launch.
3. **Auto Run** — With Auto Run on and a program running, open the Open Editor dialog. Choose the compiler error example and click Simulate Save. Build & System should show the error while the old sample program stays running. Repeat with the successful example. Toggle Auto Run off and check that a save only records the edit.
4. **Libraries** — Inspect the read-only SDL3 row. Add a catalog library and step through compatibility, preview, and approval. Inspect a managed row to preview update or removal. Try Cache Management separately and confirm the library row remains listed after sample cache removal.
5. **Project** — Change the working directory, executable, or launch arguments. Preview the values, cancel once, and apply on a second attempt. Workspace should use the applied executable and configuration.
6. **Tools** — Open summary and detailed diagnostics, review recovery and manager restart, run and cancel a test, then open Build & System to check its output. Optimize Workers offers a sample recommendation.
7. **Settings** — Try each category. Change the theme, select a custom editor to reveal advanced fields, and preview build and window preferences. Switch to Seamless and simulate a successful save while running to see the replacement mode in the console.
8. **Help and layout** — Hover a circular information icon or Tab to it. The tooltip should appear without triggering the control beside it. Escape dismisses it. Use the arrow keys between console tabs and try a narrow window to see the layout reflow.

## Scope

All sample paths, tool results, library records, timing and process output are simulated. The preview stores edits in memory. Editor, build, test, process, and library actions do not launch programs or change files. Copy uses the browser clipboard when available and otherwise offers a selectable path. Sample projects are identified by unique names. Window preferences demonstrate the controls and approval flow, with replacement outcomes shown as text. Native Windows title-bar behavior is provided by the browser window.

Help wording is adapted from EasierCpp's existing project/settings forms and library, diagnostics, and process workflows.
