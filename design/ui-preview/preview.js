(function preview() {
  "use strict";

  const $ = (selector) => document.querySelector(selector);
  const pages = ["Home", "Workspace", "Libraries", "Project", "Tools", "Settings"];
  const path = (name) => `C:\\Projects\\${name}`;
  const sampleProjects = ["FarmGame", "SDL3 Pong", "Console Sandbox"];
  const state = {
    page: "Home", project: "FarmGame", recents: [...sampleProjects],
    theme: "Light", tab: "program", section: "Window & Process",
    workers: "Default", editor: "Visual Studio 2022", editorPath: "", editorArgs: "{project}",
    debounce: 150, tests: false, unread: true, program: [], system: [], pending: null,
    projects: new Map(), input: ""
  };
  const help = {
    auto: "Auto Run watches saved changes. A failed build keeps the running program. Stop pauses automatic launch until Run or Auto Run is enabled again.",
    restart: "Restart the program using its current executable when it is up to date. Restart EasierCpp is a separate action in Tools.",
    input: "Send one line to the running program's standard input. End Input sends EOF. Run or Restart opens a new input stream.",
    target: "Choose an executable discovered from the configured CMake project. A library target cannot be run.",
    directory: "Relative asset paths resolve here. Leave blank to use the project root. Example: Assets/Textures/player.png.",
    preset: "Use a name from CMakePresets.json. A dash explicitly bypasses presets. Preset files are not edited.",
    vcpkg: "Use an existing installation containing scripts/buildsystems/vcpkg.cmake. Leave blank if your preset already chooses a toolchain.",
    args: "Enter one argument per line. Spaces in one line stay in that argument.",
    pch: "An optional CMake precompiled header recipe can speed up compatible builds. It is specific to the project configuration.",
    position: "Restore this project's application position on Run, Restart and Auto Run, including after reopening EasierCpp. Size is controlled separately.",
    size: "Restore remembered window dimensions. Turn this off to use dimensions requested by your program. Changes apply to subsequent launches.",
    replacement: "Safe closes the old instance after a successful build. Experimental Seamless starts a candidate first and may be unsuitable for exclusive-instance apps.",
    workers: "Global compilation-job limit. One is sequential. Higher counts are maximum concurrency, subject to CPU and memory limits.",
    debounce: "Wait after the last save before compiling. For example, 400 ms combines rapid saves. Allowed range is 100 to 5000 ms.",
    shutdown: "Wait for graceful shutdown after sending WM_CLOSE or console EOF. Force-stop applies only to the owned process after the timeout.",
    editor: "Normally detected automatically. Override for a nonstandard installation or a custom editor. Choose the editor's .exe file.",
    editorArgs: "Each line is one argument. {project} becomes the full folder as one argument. Without it, the folder is appended.",
    library: "Review discovered CMake targets and the proposed project changes before approval. Detected external dependencies are read-only.",
    cache: "Cached downloads and build data are separate from project files. Review cache removal and which projects use it first.",
    doctor: "Doctor distinguishes installed tools from tools on the terminal PATH. Discovery alone does not prove a compiler and SDK can build a project.",
    tests: "Workers run independent CTest cases in parallel. Tests use an isolated test build and send output to Build & System.",
    recovery: "Clean and Rebuild uses a fresh build directory after explicit approval. The previous build remains available.",
    optimize: "The synthetic benchmark estimates a worker count. Real projects can behave differently. Cancel preserves the current preference.",
    forget: "Remove the recent-project entry. The project files stay in place."
  };
  const icons = {
    Home: '<path d="m2 7 6-5 6 5v7H2zM6 14V9h4v5"/>',
    Workspace: '<rect x="2" y="3" width="12" height="10" rx="1"/><path d="m4 6 2 2-2 2m4 0h4"/>',
    Libraries: '<path d="M2 3h4v4H2zm8 0h4v4h-4zM2 10h4v4H2zm8 0h4v4h-4z"/>',
    Project: '<path d="M2 4h5l2 2h5v7H2z"/>',
    Tools: '<path d="m4 13 6-6a4 4 0 0 0 4-5l-3 3-2-2-3 3v3l-4 3z"/>',
    Settings: '<circle cx="8" cy="8" r="3"/><path d="M8 1v3m0 8v3M1 8h3m8 0h3M3 3l2 2m6 6 2 2M3 13l2-2m6-6 2-2"/>',
    Run: '<path d="m5 3 8 5-8 5z"/>',
    Stop: '<rect x="4" y="4" width="8" height="8" rx="1"/>',
    Restart: '<path d="M3 6a5 5 0 1 1 0 4M3 2v4h4"/>',
    Open: '<path d="M8 3H3v10h10V8M9 2h5v5m0-5-7 7"/>',
    Add: '<path d="M8 3v10M3 8h10"/>'
  };
  const escape = (value) => String(value).replace(/[&<>"']/g, (character) =>
    ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[character]);
  const icon = (name) => icons[name]
    ? `<svg class="icon" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.35" aria-hidden="true">${icons[name]}</svg>` : "";
  const info = (key, label) => `<button type="button" class="help" data-help="${key}" aria-label="About ${escape(label)}">ⓘ</button>`;
  const button = (action, label, symbol = "", classes = "", attrs = "") =>
    `<button type="button" data-action="${action}" class="${classes}" ${attrs}>${icon(symbol)} ${escape(label)}</button>`;
  const label = (id, name, key) => `<span class="label-line"><label class="field-label" for="${id}">${escape(name)}</label>${key ? info(key, name) : ""}</span>`;
  const select = (id, choices, current, extra = "") => `<select id="${id}" ${extra}>${choices.map((choice) =>
    `<option${choice === current ? " selected" : ""}>${escape(choice)}</option>`).join("")}</select>`;
  const field = (id, name, control, key) => `<div class="field">${label(id, name, key)}${control}</div>`;
  const input = (id, value, extra = "") => `<input id="${id}" value="${escape(value)}" ${extra}>`;
  const area = (id, value, extra = "") => `<textarea id="${id}" rows="3" ${extra}>${escape(value)}</textarea>`;
  const toggle = (id, checked, caption) =>
    `<label class="switch"><input id="${id}" type="checkbox" role="switch" aria-label="${escape(caption)}" ${checked ? "checked" : ""}></label>`;
  const heading = (eyebrow, title, description, actions = "") =>
    `<div class="heading"><div><p class="eyebrow">${eyebrow}</p><h1>${escape(title)}</h1><p class="muted">${description}</p></div>
     <div class="actions">${actions}<span class="sample">Sample data</span></div></div>`;
  const openEditor = () => button("editor", "Open Editor", "Open");
  const sampleLibraries = [
    ["raylib", "5.5", "CMake source"], ["fmt", "11.0.2", "CMake source"],
    ["nlohmann_json", "3.11.3", "Header-only"]
  ];
  const makeProject = (name, folder = path(name)) => ({
    name, folder, targets: name === "FarmGame" ? ["FarmGame.exe", "asset_tool.exe"] : [`${name.replaceAll(" ", "")}.exe`],
    target: name === "FarmGame" ? "FarmGame.exe" : `${name.replaceAll(" ", "")}.exe`,
    config: "Debug", auto: name === "FarmGame", running: name === "FarmGame",
    inputOpen: name === "FarmGame", armed: true, sourceError: false, lastError: "",
    working: folder, preset: "None (default)", vcpkg: "", args: "",
    pch: "Disabled", position: true, size: false, replacement: "Safe Replacement",
    timeout: 3000, force: false, built: name === "FarmGame", cache: true,
    libraries: name === "FarmGame"
      ? [...sampleLibraries.map(([name, version, method]) => ({ name, version, method, managed: true })),
        { name: "SDL3", version: "Unknown", method: "Existing CMake dependency", managed: false }]
      : []
  });
  sampleProjects.forEach((name) => state.projects.set(name, makeProject(name)));
  const project = () => state.projects.get(state.project);
  function addLog(channel, message, tone = "") {
    const clock = new Date().toLocaleTimeString("en-GB", { hour12: false });
    state[channel].push({ text: `[${clock}]  ${message}`, tone });
    if (channel === "system" && (state.page !== "Workspace" || state.tab !== "system")) state.unread = true;
  }
  function seedLogs() {
    state.program = [];
    state.system = [];
    addLog("program", "EasierCpp ready.", "success");
    addLog("program", `Opened project: ${state.project}`);
    if (project().auto) addLog("program", "Auto Run enabled. Watching for saved source changes.", "success");
    if (project().running) addLog("program", `${project().target} launched (sample).`);
    addLog("system", "Sample MSVC, CMake and Ninja detected.", "info");
  }
  seedLogs();

  function workspace() {
    const p = project();
    const managed = p.libraries.filter((library) => library.managed);
    const running = p.running && p.inputOpen;
    const disabled = state.pending ? "disabled" : "";
    const busy = state.pending ? "Building" : p.running ? "Running" : "Idle";
    return `${heading("Workspace", p.name, "Active CMake workspace. Your source files stay in your preferred editor.", openEditor())}
      <div class="panel toolbar bar">
        <div class="actions">
          ${field("config", "Configuration", select("config", ["Debug", "Release"], p.config, disabled))}
          ${field("target", "Executable target", select("target", p.targets, p.target, disabled), "target")}
          <div class="field"><span class="label-line"><label for="auto" class="field-label">Auto Run</label>${info("auto", "Auto Run")}</span>
          <span class="inline">${toggle("auto", p.auto, "Auto Run")}<span><strong>${p.auto ? "On" : "Off"}</strong> <small>${p.auto ? "Watching saves" : "Manual controls"}</small></span></span></div>
        </div>
        <div class="actions">${button("build", "Build", "Settings", "", disabled)}
          ${button("run", "Run", "Run", "primary", disabled)}
          ${button("stop", "Stop", "Stop", "", p.running || state.pending ? "" : "disabled")}
          <span class="label-line">${button("restart", "Restart", "Restart", "", disabled)}${info("restart", "Restart program")}</span>
        </div>
      </div>
      <div class="panel statusbar bar"><div class="inline"><span class="badge ${p.running ? "good" : ""}">● ${busy}</span>
        <small>${p.auto ? "Auto Run checks saved changes and replaces the application after a successful build." : "Auto Run is off. Build and run manually."}</small></div>
        <small>${p.config} | ${escape(p.target)} | Preview only</small></div>
      <div class="panel librarybar bar"><div class="inline">${icon("Libraries")}<strong>Project libraries</strong>
        <span class="badge">${managed.length} managed</span><small>${escape(managed.map((library) => library.name).join(", "))}</small></div>
        <div class="actions">${button("add-library", "Add Library", "Add")}${button("manage-libraries", "Manage Libraries")}</div></div>
      <section class="panel console" aria-label="Workspace console">
        <div class="console-head bar"><div class="tabs" role="tablist" aria-label="Console">
          ${consoleTab("program", "Program Console", "Workspace")}${consoleTab("system", "Build & System", "Settings")}
          </div>${button("clear", "Clear Console")}</div>
        <pre class="terminal" id="console-output" role="tabpanel" tabindex="0" aria-labelledby="${state.tab}-tab">${state[state.tab].map((line) =>
          `<span class="${line.tone}">${escape(line.text)}</span>`).join("\n")}</pre>
        <form class="console-input actions" id="program-form"><span class="label-line"><label for="program-input">Program input</label>${info("input", "Program input")}</span>
          ${input("program-input", state.input, `placeholder="Type a line and press Enter" ${running ? "" : "disabled"}`)}
          <button type="submit" ${running ? "" : "disabled"}>${icon("Run")} Send</button>
          ${button("eof", "End Input", "Stop", "", running ? "" : "disabled")}
        </form>
      </section>
      <div class="footer bar"><span>Project folder: ${escape(p.folder)} ${button("copy", "Copy")}</span>
        <span>UI reference only. Build and process controls are simulated.</span></div>`;
  }
  function consoleTab(id, name, symbol) {
    return `<button type="button" id="${id}-tab" role="tab" aria-controls="console-output" aria-selected="${state.tab === id}"
      tabindex="${state.tab === id ? 0 : -1}" data-action="tab-${id}">${icon(symbol)} ${name}
      ${id === "system" && state.unread ? '<span class="badge">New</span>' : ""}</button>`;
  }
  const environment = (items) => `<div class="rows">${items.map(([name, detail]) =>
    `<div class="row"><div><strong><span class="badge good">✓</span> ${name}</strong><small>${detail}</small></div></div>`).join("")}</div>`;
  function home() {
    return `${heading("Welcome", "Welcome to EasierCpp", "A lightweight companion for your C++ projects. Your editor stays your editor.")}
      <section class="panel hero"><div><p class="eyebrow">Make Your Life Easier</p><h2>Spend more time making things.</h2>
        <p class="muted">Create or open a CMake project, then build, run, and keep working with Auto Run.</p>
        <div class="actions">${button("new", "New Project", "Add", "primary")}${button("open", "Open Existing", "Project")}</div></div>
        <div class="hero-tile" aria-hidden="true"><span class="hero-mark"></span></div></section>
      <div class="columns home-columns"><section class="panel pad">
        <div class="bar"><div><h2>Recent projects</h2><small>Jump back into a previous workspace.</small></div>
        <span class="sample">Example entries</span></div>
        <div class="rows">${state.recents.map((name, index) =>
          `<div class="row"><div><strong>${icon("Project")} ${escape(name)}</strong><small>${escape(state.projects.get(name).folder)}</small></div>
            <div class="actions">${button(`recent-${index}`, "Open", "Open")}${button(`forget-${index}`, "Forget")}</div></div>`).join("") || '<p class="muted">No recent projects.</p>'}</div>
      </section><section class="panel pad"><div class="bar"><div><h2>Development environment</h2><small>Representative diagnostics display.</small></div>
        <span class="inline">${button("doctor", "Run Doctor", "Tools")}${info("doctor", "Doctor")}</span></div>
        ${environment([["MSVC toolchain", "Visual Studio 2022 detected"], ["CMake and Ninja", "Available for CMake projects"],
          ["Preferred editor", escape(state.editor)], ["Build workers", state.workers === "Default" ? "Auto, based on system limits" : state.workers]])}
        <p class="note">This screen is an example. Tool discovery and build readiness come from EasierCpp.</p>
      </section></div>`;
  }
  function libraries() {
    const p = project();
    const sources = ["Known library catalog", "Public GitHub or GitLab repository", "Installed CMake package",
      "Local CMake source", "Local header-only directory"];
    return `${heading("Project / Dependencies", "Library Manager", "Manage supported integrations without silently rewriting imported CMake files.",
      `<span class="inline">${button("add-library", "Add Library", "Add", "primary")}${info("library", "Add Library")}</span>`)}
      <section class="panel"><div class="toolbar bar"><span class="inline"><strong>${escape(p.name)}</strong>
      <small>Managed libraries are editable. Detected external dependencies stay read-only.</small></span>
      <div class="actions">${button("cache", "Cache Management", "Project")}${info("cache", "Cache Management")}${button("refresh", "Refresh", "Restart")}</div></div>
      <div class="table-scroll"><table><thead><tr><th>Library</th><th>Version</th><th>Ownership</th><th>Integration</th><th>Status</th><th><span class="sr-only">Actions</span></th></tr></thead>
      <tbody>${p.libraries.map((library, index) => `<tr><td><strong>${escape(library.name)}</strong>
      <small>${library.managed ? "Example library record" : "Imported dependency"}</small></td><td>${escape(library.version)}</td>
      <td><span class="badge">${library.managed ? "Managed" : "Detected"}</span></td><td>${escape(library.method)}</td>
      <td><span class="badge ${library.managed ? "good" : "warn"}">${library.managed ? "Verified" : "Read only"}</span></td>
      <td>${button(`inspect-${index}`, "Inspect", "Open")}</td></tr>`).join("") || '<tr><td colspan="6">No libraries in this sample project.</td></tr>'}</tbody></table></div></section>
      <div class="columns"><section class="panel pad"><h2>Supported ways to add</h2><small>Existing workflows from the repository.</small>
      <div class="rows">${sources.map((source) => `<div class="row">✓ ${source}</div>`).join("")}</div></section>
      <section class="panel pad"><h2>Safety and verification</h2><small>The sequence stays visible before anything touches project files.</small>
      <div class="rows">${["Select source and pin a version or commit", "Review discovered CMake targets and compatibility",
        "Preview exact changes to project-owned files", "Approve integration and verify the build with rollback on failure"]
        .map((step, index) => `<div class="row"><span><span class="badge">${index + 1}</span> ${step}</span></div>`).join("")}</div>
      <p class="note">A downloaded library is not automatically compatible. Update availability is not assumed.</p></section></div>`;
  }
  function projectScreen() {
    const p = project();
    return `${heading("Project", "Project Settings", "Project-specific configuration, independent from global preferences.",
      button("new", "New Project", "Add") + button("open", "Open Existing", "Project"))}
      <form id="project-form" class="columns">
        <section class="panel pad"><div class="bar"><h2>Project identity</h2><span class="badge good">Open</span></div>
          <small>Read the current project before changing its configuration.</small><div class="fields">
          ${field("project-folder", "Project folder", `<div class="actions">${input("project-folder", p.folder, "readonly")}${button("copy", "Copy", "Open")}</div>`)}
          ${field("project-type", "Project type", input("project-type", p.type || "Existing CMake C++20 project", "readonly"))}
          ${field("project-target", "Executable target", select("project-target", p.targets, p.target), "target")}
          ${field("project-config", "Configuration", select("project-config", ["Debug", "Release"], p.config))}
          ${field("pch", "Precompiled headers (opt in)", select("pch", ["Disabled", "Sample PCH recipe"], p.pch), "pch")}
          </div></section>
        <section class="panel pad"><h2>Paths and launch</h2><small>Current working directory, CMake preset and runtime arguments.</small><div class="fields">
          ${field("working", "Program working directory", input("working", p.working), "directory")}
          ${field("preset", "CMake configure preset", select("preset", ["None (default)", "windows-debug (sample)", "-"], p.preset), "preset")}
          ${field("vcpkg", "vcpkg root", input("vcpkg", p.vcpkg, 'placeholder="Optional toolchain root"'), "vcpkg")}
          ${field("args", "Program arguments", area("args", p.args, 'placeholder="One argument per line"'), "args")}
          <div><button class="primary" type="submit">✓ Preview Changes</button></div></div></section>
      </form><p class="note"><strong>Imported-project protection:</strong> Opening a CMake project does not grant permission to rewrite CMakeLists.txt. Filesystem changes and dependency integrations still require a preview and approval.</p>`;
  }
  const workerChoices = ["Default", "1", "2", "3", "4", "5", "6", "7", "8"];
  function toolsScreen() {
    return `${heading("Tools", "Diagnostics & Utilities", "Keep repair, testing, and environment checks separate from your build and run controls.", openEditor())}
      <div class="columns"><section class="panel pad"><div class="bar"><h2>Toolchain health</h2><span class="sample">Illustrative</span></div>
        <small>Check installed tools and compiler environment.</small>
        ${environment([["Visual Studio", "2022 / x64 toolset"], ["CMake", "Detected"], ["Ninja", "Detected"], ["Windows SDK", "Detected"]])}
        <div class="actions">${button("doctor", "Summary Doctor", "Tools", "primary")}${button("doctor-detail", "Detailed Report")}${info("doctor", "Toolchain diagnostics")}</div></section>
      <section class="panel pad"><h2>Build errors and recovery</h2><small>Troubleshoot without losing your current project state.</small><div class="rows">
        ${toolRow("Review build errors", "Capture diagnostics and relevant editor locations.", button("errors", "Review"))}
        ${toolRow("Clean and Rebuild", "Uses a fresh directory after explicit confirmation.", button("recovery", "Review") + info("recovery", "Clean and Rebuild"))}
        ${toolRow("Restart EasierCpp", "Separate from restarting the user program.", button("manager-restart", "Review"))}
      </div></section>
      <section class="panel pad"><h2>Test runner</h2><small>Run Quick or Full project test suites from source builds.</small><div class="fields">
        ${field("suite", "Suite", select("suite", ["Quick", "Full"], state.suite || "Quick"))}
        ${field("test-workers", "Parallel workers", select("test-workers", workerChoices.slice(1), state.testWorkers || "4"), "tests")}
        <div class="actions">${button("tests", "Run Tests", "Run", "primary", state.tests ? "disabled" : "")}
        ${button("cancel-tests", "Cancel Running Tests", "Stop", "", state.tests ? "" : "disabled")}</div>
        <p class="note">Test output belongs in Build &amp; System. Running tests leaves the user application running.</p></div></section>
      <section class="panel pad"><h2>Build performance</h2><small>Adjust compilation concurrency based on your system.</small><div class="rows">
        ${toolRow("Parallel build workers", "Global. Inherits the build system by default.", select("workers", workerChoices, state.workers, 'aria-label="Parallel build workers"') + info("workers", "Parallel build workers"))}
        ${toolRow("Optimize Workers", "A synthetic benchmark provides a recommendation.", button("optimize", "Optimize", "Settings") + info("optimize", "Optimize Workers"))}
        ${toolRow("Editor launch", escape(state.editor), openEditor())}</div></section></div>`;
  }
  function toolRow(title, description, actions) {
    return `<div class="row"><div><strong>${title}</strong><small>${description}</small></div><div class="actions">${actions}</div></div>`;
  }
  function settings() {
    const p = project();
    const panels = ["Appearance", "Preferred Editor", "Build Preferences", "Window & Process"];
    let content;
    if (state.section === "Appearance") {
      content = `${field("theme-select", "Theme", select("theme-select", ["Follow Windows", "Light", "Dark"], state.theme))}
        <p class="note">Theme changes apply immediately to this preview.</p>`;
    } else if (state.section === "Preferred Editor") {
      content = `${field("editor-choice", "Preferred editor", select("editor-choice", ["Visual Studio 2022", "VS Code", "Custom"], state.editor))}
        <details ${state.editor === "Custom" ? "open" : ""}><summary>Advanced Options</summary><div class="fields">
        ${field("editor-path", "Editor executable", input("editor-path", state.editorPath, 'placeholder="Optional .exe path"'), "editor")}
        ${field("editor-args", "Custom arguments", area("editor-args", state.editorArgs), "editorArgs")}</div></details>`;
    } else if (state.section === "Build Preferences") {
      content = `${field("debounce", "Auto Run debounce (ms)", input("debounce", state.debounce, 'type="number" min="100" max="5000" required'), "debounce")}
        ${field("settings-workers", "Parallel build workers", select("settings-workers", workerChoices, state.workers), "workers")}
        <div>${button("optimize", "Optimize Workers", "Settings")}${info("optimize", "Optimize Workers")}</div>`;
    } else {
      content = `<div>
        ${toolRow(`Keep window position on restart ${info("position", "Keep window position")}`, "Restore the application's position when possible.", toggle("position", p.position, "Keep window position on restart"))}
        ${toolRow(`Keep window size on restart ${info("size", "Keep window size")}`, "Off lets the application choose its own size.", toggle("size", p.size, "Keep window size on restart"))}
        ${toolRow(`Auto Run replacement mode ${info("replacement", "Auto Run replacement mode")}`, "Safe suits exclusive-instance apps. Seamless is experimental.",
          select("replacement", ["Safe Replacement", "Seamless Replacement (Experimental)"], p.replacement, 'aria-label="Auto Run replacement mode"'))}
        </div><details><summary>Advanced Options</summary><div class="fields">
        ${field("timeout", "Graceful shutdown timeout (ms)", input("timeout", p.timeout, 'type="number" min="100" max="30000" required'), "shutdown")}
        ${toolRow("Force-stop owned program after timeout", "Applies only to this project.", toggle("force", p.force, "Force-stop after timeout"))}
        </div></details>
        <p class="note"><strong>Important:</strong> Replacement restarts a process. It does not preserve your application's memory, game state, or Windows Snap Groups.</p>`;
    }
    return `${heading("Preferences", "Settings", "One place for appearance, editor, build preferences, and project-specific restart behavior.")}
      <div class="settings-layout"><div class="panel settings-nav" role="group" aria-label="Preference category">${panels.map((name, index) =>
        button(`section-${index}`, name, "", "", `aria-pressed="${state.section === name}"`)).join("")}</div>
      <section class="panel pad"><div class="bar"><h2>${escape(state.section)}</h2>${state.section === "Window & Process" ? '<span class="sample">Project scoped</span>' : ""}</div>
        <small>${state.section === "Window & Process" ? "These preferences apply to the currently open project." : "These preferences apply to the preview workspace."}</small>
        <form class="fields" id="settings-form">${content}${state.section === "Appearance" ? "" :
          '<div><button class="primary" type="submit">✓ Preview Changes</button></div>'}</form></section></div>`;
  }

  const screens = [home, workspace, libraries, projectScreen, toolsScreen, settings];
  const dialog = $("#dialog");
  let previousFocus;
  let activeHelp;
  let dialogAction;
  const tooltip = $("#tooltip");

  function hideHelp() {
    activeHelp?.removeAttribute("aria-describedby");
    tooltip.hidden = true;
    activeHelp = null;
  }
  function showHelp(control) {
    hideHelp();
    activeHelp = control;
    const container = dialog.open && dialog.contains(control) ? dialog : document.body;
    if (tooltip.parentElement !== container) container.append(tooltip);
    tooltip.textContent = help[control.dataset.help];
    control.setAttribute("aria-describedby", tooltip.id);
    tooltip.hidden = false;
    positionHelp();
  }
  function positionHelp() {
    if (!activeHelp) return;
    const rect = activeHelp.getBoundingClientRect();
    tooltip.style.left = `${Math.max(8, Math.min(rect.left, innerWidth - tooltip.offsetWidth - 8))}px`;
    tooltip.style.top = `${rect.bottom + tooltip.offsetHeight + 10 > innerHeight
      ? Math.max(8, rect.top - tooltip.offsetHeight - 6) : rect.bottom + 6}px`;
  }
  function applyTheme() {
    const dark = state.theme === "Dark" || state.theme === "Follow Windows" && matchMedia("(prefers-color-scheme: dark)").matches;
    document.documentElement.dataset.theme = dark ? "dark" : "light";
    $("#theme").textContent = dark ? "☀ Light mode" : "☾ Dark mode";
  }
  function render(focusId = "") {
    hideHelp();
    applyTheme();
    $("#navigation").innerHTML = pages.map((name) =>
      `<a href="#${name.toLowerCase()}" ${state.page === name ? 'aria-current="page"' : ""}>${icon(name)} ${name}</a>`).join("");
    $("#sidebar-bottom").innerHTML = `<a class="panel project-card" href="#workspace"><span class="eyebrow">Open project</span>
      <strong>${icon("Project")} ${escape(state.project)}</strong><small>CMake project | ${project().config}</small></a>
      <small>EasierCpp v0.1.2<br>Interactive mockup. No files are changed.</small>`;
    $("#screen").innerHTML = screens[pages.indexOf(state.page)]();
    if (focusId) document.getElementById(focusId)?.focus();
    const output = $("#console-output");
    if (output) output.scrollTop = output.scrollHeight;
  }
  function navigate() {
    const selected = pages.find((name) => `#${name.toLowerCase()}` === location.hash.toLowerCase());
    if (selected) state.page = selected;
    render();
    $("#screen").focus({ preventScroll: true });
  }
  function go(page) {
    const hash = `#${page.toLowerCase()}`;
    if (location.hash === hash) navigate();
    else location.hash = hash;
  }
  function announce(message) { $("#announcement").textContent = message; }
  function modal(title, content, confirm = "", action = null) {
    if (!dialog.open) previousFocus = document.activeElement;
    hideHelp();
    document.body.append(tooltip);
    dialogAction = action;
    dialog.innerHTML = `<form id="modal-form"><h2 id="dialog-title">${escape(title)}</h2>${content}
      <div class="actions">${button("close-dialog", confirm ? "Cancel" : "Close")}
      ${confirm ? `<button class="primary" type="submit">${escape(confirm)}</button>` : ""}</div></form>`;
    if (!dialog.open) dialog.showModal();
    dialog.querySelector("input, select, button").focus();
  }
  function closeModal() {
    if (dialog.open) dialog.close();
  }
  dialog.addEventListener("close", () => {
    hideHelp();
    document.body.append(tooltip);
    if (previousFocus?.isConnected) previousFocus.focus();
    else if (previousFocus?.dataset.action) {
      (document.querySelector(`[data-action="${previousFocus.dataset.action}"]`) || $("#screen")).focus();
    } else {
      const formId = previousFocus?.form?.id;
      (formId && document.getElementById(formId)?.querySelector('[type="submit"]') || $("#screen")).focus();
    }
  });
  function updateProject(name) {
    if (state.pending) {
      clearTimeout(state.pending);
      state.pending = null;
    }
    const old = project();
    if (old.name !== name) {
      old.running = false;
      old.inputOpen = false;
    }
    state.project = name;
    project().armed = true;
    state.recents = [name, ...state.recents.filter((recent) => recent !== name)];
    state.input = "";
    state.tab = "program";
    seedLogs();
    if (project().auto && !project().running) build("run");
    go("Workspace");
  }
  function build(action, saved = false) {
    if (state.pending) return;
    const p = project();
    const fails = p.sourceError;
    const shouldLaunch = action !== "build";
    if (!saved && (action === "restart" || action === "run") && p.built) {
      launch(p, action === "restart");
      render();
      return;
    }
    addLog("system", `Building ${p.target} (${p.config}) using sample CMake and Ninja.`, "info");
    state.pending = setTimeout(() => {
      state.pending = null;
      p.built = !fails;
      p.lastError = fails ? "src/main.cpp(18): error C2143: missing semicolon (sample)." : "";
      if (fails) {
        addLog("system", p.lastError, "error");
        addLog("system", p.running ? "Build failed. The previous program is still running." : "Build failed. No program was launched.", "error");
      } else {
        addLog("system", "Success: simulated build completed.", "success");
        if (shouldLaunch) launch(p, action === "restart");
      }
      announce(fails ? "Simulated build failed." : "Simulated build completed.");
      refreshWorkspaceControls();
    }, 650 + (saved ? state.debounce : 0));
    refreshWorkspaceControls();
  }
  function refreshWorkspaceControls() {
    if (state.page !== "Workspace") return;
    const focused = document.activeElement;
    const action = focused?.dataset.action;
    render(focused?.id);
    if (action) document.querySelector(`[data-action="${action}"]:not(:disabled)`)?.focus();
  }
  function launch(p, restart) {
    const previousRunning = p.running;
    p.running = true;
    p.armed = true;
    p.inputOpen = true;
    const mode = p.replacement.startsWith("Seamless") ? "Seamless" : "Safe";
    addLog("program", `${restart || previousRunning ? `${mode} restart: ` : ""}${p.target} launched (sample).`, "success");
    announce("Sample program running.");
  }
  function addLibrary() {
    const methods = ["Known library catalog", "GitHub repository", "GitLab repository", "Installed CMake package", "Local CMake source", "Local header-only directory"];
    modal("Add Library", `<p class="muted">Select an integration source and a version or ref. This is a simulated compatibility review.</p>
      ${field("library-method", "Source", select("library-method", methods, methods[0]), "library")}
      <div id="library-fields">${libraryFields(methods[0])}</div>`, "Review Compatibility", () => {
      const method = $("#library-method").value;
      const name = $("#library-name").value.trim();
      const version = $("#library-version").value.trim() || "Sample ref";
      if (project().libraries.some((library) => library.name.toLowerCase() === name.toLowerCase())) {
        $("#library-name").setCustomValidity("This library is already listed for this project.");
        $("#library-name").reportValidity();
        return;
      }
      const record = { name, version, method, managed: true };
      modal("Compatibility review", `<p><strong>${escape(name)} ${escape(version)}</strong></p>
        <p class="note">${method === methods[0] ? "Compatible in this sample. Example CMake target discovered." :
          "Unable to verify in the preview. A real integration needs configuration and target verification."}</p>
        <p class="muted">Review the proposed project changes before adding the integration.</p>`,
        method === methods[0] ? "Preview Changes" : "Try Adding Anyway", () =>
          libraryPreview(`Add ${name}`, "Add one managed dependency entry for the selected executable target.", () => project().libraries.push(record)));
    });
  }
  function libraryFields(method) {
    const known = method === "Known library catalog";
    const choices = ["raylib", "fmt", "nlohmann_json", "SDL3", "SFML"].filter((name) =>
      !project().libraries.some((library) => library.name.toLowerCase() === name.toLowerCase()));
    return `<div class="fields">${field("library-name", known ? "Library" : "Library name", known
      ? select("library-name", choices.length ? choices : [""], choices[0], "required")
      : input("library-name", "", "required"))}
      ${known ? "" : field("library-source", "Repository URL, package name or local path", input("library-source", "", "required"))}
      ${field("library-version", "Version or ref", input("library-version", "", 'placeholder="Example: v3.0.0"'))}</div>`;
  }
  function libraryPreview(title, description, apply) {
    modal("Preview library changes", `<p><strong>${escape(title)}</strong></p><p>${escape(description)}</p>
      <pre class="note">${escape(project().folder)}\\CMakeLists.txt\nManaged dependency block for ${escape(project().target)}</pre>
      <p class="muted">A real change would require approval and build verification, with rollback on failure.</p>`,
    "Approve Sample Change", () => {
      apply();
      addLog("system", `${title}: approved in the preview.`, "success");
      closeModal();
      render();
      announce("Sample library list updated.");
    });
  }
  function inspect(index) {
    const item = project().libraries[index];
    modal("Inspect library", `<p><strong>${escape(item.name)} ${escape(item.version)}</strong></p>
      <p>${escape(item.method)}</p><p class="note">${item.managed ? "Managed integration. Review updates or removal before applying." :
        "Detected external dependency. Review it here and edit its original CMake configuration in your editor."}</p>
      ${item.managed ? `<div class="actions">${button(`update-library-${index}`, "Preview Update")}${button(`remove-library-${index}`, "Preview Removal")}</div>` : ""}`);
  }
  function projectDialog(create) {
    modal(create ? "New Project" : "Open Existing", `<p class="muted">${create ? "Create a sample C++20 project." : "Choose a sample CMake project or enter a folder to represent."}</p>
      ${create ? field("new-name", "Project name", input("new-name", "", 'required maxlength="80"')) : ""}
      ${field("new-folder", create ? "Parent folder" : "Project folder", input("new-folder", "C:\\Projects", "required"))}
      ${create ? field("template", "Template", select("template", ["Empty", "Multi-file", "Calculator"], "Empty")) : ""}`,
      create ? "Create Sample Project" : "Open Sample Project", () => {
        const folder = $("#new-folder").value.trim().replace(/[\\/]+$/, "");
        const name = create ? $("#new-name").value.trim() : folder.split(/[\\/]/).pop();
        const nameField = create ? $("#new-name") : $("#new-folder");
        if (!name || !folder) {
          nameField.setCustomValidity("Enter a project name and folder.");
          nameField.reportValidity();
          return;
        }
        const existing = state.projects.get(name);
        if (existing && (create || existing.folder.toLowerCase() !== folder.toLowerCase())) {
          nameField.setCustomValidity("This name already belongs to a sample project. Choose a different name or open its listed folder.");
          nameField.reportValidity();
          return;
        }
        if (!state.projects.has(name)) {
          const sample = makeProject(name, create ? `${folder}\\${name}` : folder);
          if (create) sample.type = `${$("#template").value} C++20 project`;
          state.projects.set(name, sample);
        }
        closeModal();
        updateProject(name);
      });
  }
  function diagnostics(detail) {
    const summary = "MSVC: installed, outside terminal PATH\nCMake: detected\nNinja: detected\nWindows SDK: detected";
    addLog("system", "Doctor sample: MSVC, CMake, Ninja and Windows SDK detected.", "info");
    modal(detail ? "Detailed Report" : "Toolchain Diagnostics", `<pre>${summary}${detail ? "\nArchitecture: x64\nVisual Studio: 2022\nCompiler / SDK compatibility: build required" : ""}</pre>
      <p class="note">Illustrative discovery output. No tool was run.</p>`);
  }
  async function copyPath() {
    const folder = project().folder;
    const control = document.activeElement;
    try {
      await navigator.clipboard.writeText(folder);
      announce("Sample path copied.");
      if (control?.dataset.action === "copy" && control.isConnected) {
        const caption = control.innerHTML;
        control.textContent = "Copied";
        setTimeout(() => { if (control.isConnected) control.innerHTML = caption; }, 900);
      }
    } catch {
      modal("Copy project path", `<p class="muted">Select and copy this sample path.</p>${input("copy-path", folder, "readonly")}`);
      $("#copy-path").select();
    }
  }
  function setTab(tab, focus = false) {
    state.tab = tab;
    if (tab === "system") state.unread = false;
    render(focus ? `${tab}-tab` : "");
  }
  function runTests() {
    if (state.tests) return;
    const suite = state.suite || "Quick";
    addLog("system", `${suite} test suite started in an isolated sample build directory.`, "info");
    state.tests = setTimeout(() => {
      state.tests = false;
      addLog("system", `${suite}: simulated tests completed.`, "success");
      if (state.page === "Tools") render();
      announce("Simulated tests completed.");
    }, 2400);
    render();
    document.querySelector('[data-action="cancel-tests"]').focus();
  }
  function action(name) {
    const p = project();
    if (name.startsWith("recent-")) return updateProject(state.recents[Number(name.slice(7))]);
    if (name.startsWith("forget-")) {
      const index = Number(name.slice(7));
      return modal("Forget recent project", `<p>${escape(state.recents[index])}</p><p>${help.forget}</p>`, "Forget Entry", () => {
        state.recents.splice(index, 1);
        closeModal();
        render();
      });
    }
    if (name.startsWith("inspect-")) return inspect(Number(name.slice(8)));
    if (name.startsWith("remove-library-")) {
      const index = Number(name.slice(15));
      return libraryPreview(`Remove ${p.libraries[index].name}`, "Remove this managed integration. Cached data remains separate.", () => p.libraries.splice(index, 1));
    }
    if (name.startsWith("update-library-")) {
      const index = Number(name.slice(15));
      return modal("Review library update", `${field("update-ref", "Sample version or ref", input("update-ref", "", "required"))}`,
        "Preview Changes", () => {
          const version = $("#update-ref").value.trim();
          if (!version) return;
          libraryPreview(`Update ${p.libraries[index].name}`, `Replace the version pin with ${version}.`, () => { p.libraries[index].version = version; });
        });
    }
    if (name.startsWith("section-")) {
      state.section = ["Appearance", "Preferred Editor", "Build Preferences", "Window & Process"][Number(name.slice(8))];
      render();
      document.querySelector(`[data-action="${name}"]`).focus();
      return;
    }
    switch (name) {
      case "close-dialog": closeModal(); break;
      case "new": projectDialog(true); break;
      case "open": projectDialog(false); break;
      case "build": case "run": case "restart": build(name); break;
      case "stop":
        if (state.pending) {
          clearTimeout(state.pending);
          state.pending = null;
          addLog("system", "Sample build cancelled.");
        }
        p.running = false;
        p.armed = false;
        p.inputOpen = false;
        addLog("program", "Program stopped. Automatic launch paused.");
        render();
        $("#screen").focus();
        break;
      case "eof": p.inputOpen = false; addLog("program", "End of input sent."); render(); $("#screen").focus(); break;
      case "tab-program": setTab("program", true); break;
      case "tab-system": setTab("system", true); break;
      case "clear": state[state.tab] = []; refreshWorkspaceControls(); break;
      case "manage-libraries": go("Libraries"); break;
      case "add-library": addLibrary(); break;
      case "refresh": modal("Refresh library list", `<p>${p.libraries.length} sample records found.</p><p class="note">Detected entries remain read-only.</p>`); break;
      case "cache":
        modal("Cache Management", `<p>${p.cache ? "Sample downloaded source cache for this project." : "No sample cache entries."}</p>
          <p class="note">${help.cache}</p>`, p.cache ? "Preview Removal" : "", () =>
          modal("Review cache removal", "<p>Remove the sample source cache. The library integrations remain listed.</p>",
            "Remove Sample Cache", () => { p.cache = false; closeModal(); announce("Sample cache removed."); }));
        break;
      case "copy": copyPath(); break;
      case "editor":
        modal("Open Editor", `<p>${escape(state.editor)} would open <strong>${escape(p.folder)}</strong>.</p>
          <p class="note">Choose a simulated source edit to try the Auto Run workflow.</p>
          ${field("sample-save", "Sample edit", select("sample-save", ["Compiles successfully", "Contains a compiler error"], "Compiles successfully"))}`,
        "Simulate Save", () => {
          p.sourceError = $("#sample-save").selectedIndex === 1;
          closeModal();
          p.built = false;
          addLog("system", "Sample source save received.", "info");
          if (p.auto) build(p.armed ? "restart" : "build", true);
          else addLog("system", "Auto Run is off. No automatic build was started.");
          go("Workspace");
        });
        break;
      case "doctor": diagnostics(false); break;
      case "doctor-detail": diagnostics(true); break;
      case "errors": modal("Build errors", `<p>${escape(p.lastError || "No compiler errors in the current sample output.")}</p><p class="note">Compiler diagnostics appear in Build &amp; System.</p>`); break;
      case "recovery": modal("Review Clean and Rebuild", `<p>${help.recovery}</p>`, "Simulate Rebuild", () => { closeModal(); build("build"); go("Workspace"); }); break;
      case "manager-restart": modal("Restart EasierCpp", "<p>Restart the manager and reopen the current workspace. In a real session, active work is reviewed before restarting.</p>",
        "Simulate Restart", () => { closeModal(); addLog("system", "Manager restarted (sample)."); go("Workspace"); }); break;
      case "tests": runTests(); break;
      case "cancel-tests":
        clearTimeout(state.tests); state.tests = false; addLog("system", "Sample test run cancelled."); render();
        document.querySelector('[data-action="tests"]').focus();
        break;
      case "optimize": modal("Optimize Workers", `<p>${help.optimize}</p><p class="note">Sample recommendation: 4 parallel workers.</p>`,
        "Use Sample Recommendation", () => {
          state.workers = "4";
          if ($("#settings-workers")) $("#settings-workers").value = "4";
          if ($("#workers")) $("#workers").value = "4";
          closeModal();
          announce("Sample recommendation applied.");
        }); break;
    }
  }
  function previewSettings() {
    const p = project();
    let values;
    if (state.section === "Window & Process") {
      values = { position: $("#position").checked, size: $("#size").checked, replacement: $("#replacement").value,
        timeout: Number($("#timeout").value), force: $("#force").checked };
    } else if (state.section === "Preferred Editor") {
      values = { editor: $("#editor-choice").value, editorPath: $("#editor-path").value, editorArgs: $("#editor-args").value };
    } else {
      values = { debounce: Number($("#debounce").value), workers: $("#settings-workers").value };
    }
    const pairs = {
      position: "Keep position", size: "Keep size", replacement: "Replacement mode", timeout: "Shutdown timeout (ms)",
      force: "Force-stop", editor: "Editor", editorPath: "Executable", editorArgs: "Arguments", debounce: "Debounce (ms)", workers: "Workers"
    };
    modal("Preview preference changes", `<pre class="note">${Object.entries(values).map(([key, value]) =>
      `${pairs[key]}: ${escape(typeof value === "boolean" ? value ? "On" : "Off" : value || "(default)")}`).join("\n")}</pre>`,
    "Apply to Preview", () => {
      Object.assign(state.section === "Window & Process" ? p : state, values);
      closeModal();
      render();
      announce("Preferences applied to the preview.");
    });
  }

  document.addEventListener("click", (event) => {
    const control = event.target.closest("button");
    if (!control) return;
    if (control.id === "theme") {
      state.theme = document.documentElement.dataset.theme === "dark" ? "Light" : "Dark";
      applyTheme();
      if ($("#theme-select")) $("#theme-select").value = state.theme;
    } else if (control.dataset.help) {
      showHelp(control);
    } else if (control.dataset.action) {
      action(control.dataset.action);
    }
  });
  document.addEventListener("input", (event) => {
    if (event.target.id === "program-input") state.input = event.target.value;
    if (event.target.id === "library-name") event.target.setCustomValidity("");
    if (["new-name", "new-folder"].includes(event.target.id)) {
      $("#new-name")?.setCustomValidity("");
      $("#new-folder").setCustomValidity("");
    }
  });
  document.addEventListener("change", (event) => {
    const control = event.target;
    const p = project();
    if (["config", "target"].includes(control.id)) {
      p[control.id] = control.value;
      p.built = false;
      render(control.id);
    }
    if (control.id === "auto") {
      p.auto = control.checked;
      if (p.auto) p.armed = true;
      addLog("program", p.auto ? "Auto Run enabled. Watching for saved source changes." : "Auto Run disabled.");
      if (p.auto && !p.running) build("run");
      render("auto");
    }
    if (control.id === "theme-select") { state.theme = control.value; applyTheme(); }
    if (control.id === "workers") state.workers = control.value;
    if (control.id === "suite") state.suite = control.value;
    if (control.id === "test-workers") state.testWorkers = control.value;
    if (control.id === "editor-choice" && control.value === "Custom") control.closest("form").querySelector("details").open = true;
    if (control.id === "library-method") $("#library-fields").innerHTML = libraryFields(control.value);
  });
  document.addEventListener("submit", (event) => {
    event.preventDefault();
    switch (event.target.id) {
      case "modal-form": dialogAction?.(); break;
      case "program-form":
        if (!project().running || !project().inputOpen) return;
        if (!state.input.trim()) return;
        addLog("program", `> ${state.input}`);
        addLog("program", "Input submitted to the sample console.");
        state.input = "";
        setTab("program");
        $("#program-input").focus();
        break;
      case "project-form": {
        const ids = ["project-target", "project-config", "pch", "working", "preset", "vcpkg", "args"];
        const values = Object.fromEntries(ids.map((id) => [id.replace("project-", ""), $(`#${id}`).value]));
        modal("Preview project changes", `<pre class="note">${Object.entries(values).map(([key, value]) =>
          `${key}: ${escape(value || "(default)")}`).join("\n")}</pre><p class="muted">Changes apply only to this sample project.</p>`,
          "Apply to Preview", () => {
            Object.assign(project(), values, { built: false });
            closeModal();
            render();
            announce("Sample project settings applied.");
          });
        break;
      }
      case "settings-form": previewSettings(); break;
    }
  });
  document.addEventListener("pointerover", (event) => {
    const control = event.target.closest(".help");
    if (control) showHelp(control);
  });
  document.addEventListener("pointerout", (event) => {
    if (activeHelp === event.target && document.activeElement !== activeHelp) hideHelp();
  });
  document.addEventListener("focusin", (event) => {
    if (event.target.matches(".help")) showHelp(event.target);
    else hideHelp();
  });
  document.addEventListener("focusout", (event) => {
    if (event.target === activeHelp) hideHelp();
  });
  document.addEventListener("keydown", (event) => {
    if (event.key === "Escape" && activeHelp) { event.preventDefault(); hideHelp(); }
    if (event.target.matches('[role="tab"]') && ["ArrowLeft", "ArrowRight", "Home", "End"].includes(event.key)) {
      event.preventDefault();
      setTab(event.key === "Home" ? "program" : event.key === "End" ? "system" :
        state.tab === "program" ? "system" : "program", true);
    }
  });
  window.addEventListener("resize", positionHelp);
  document.addEventListener("scroll", positionHelp, true);
  window.addEventListener("hashchange", navigate);
  matchMedia("(prefers-color-scheme: dark)").addEventListener("change", applyTheme);
  if (new URLSearchParams(location.search).get("theme") === "dark") state.theme = "Dark";
  navigate();
})();
