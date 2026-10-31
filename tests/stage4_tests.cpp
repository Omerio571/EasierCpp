#include "auto_build.hpp"
#include "build_lease.hpp"
#include "console_model.hpp"
#include "appearance.hpp"
#include "theme_test_support.hpp"
#include <cmath>
#include <array>
#include "cpplivelab/projects.hpp"
#include "cpplivelab/session.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
namespace fs = std::filesystem;
int checks = 0;

void require(bool condition, const std::string& description)
{
    if (!condition)
    {
        throw std::runtime_error(description);
    }
    ++checks;
}

struct TemporaryProjects
{
    fs::path root =
        fs::temp_directory_path() / (L"CppLiveLab S4 " + std::to_wstring(GetCurrentProcessId()));

    TemporaryProjects()
    {
        require(!fs::exists(root), "Test directory already exists");
        fs::create_directories(root);
    }

    ~TemporaryProjects()
    {
        // Only this previously nonexistent test root is removed, never user examples.
        std::error_code error;
        fs::remove_all(root, error);
    }
};

void write_file(const fs::path& path, const std::string& content)
{
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    require(static_cast<bool>(file << content), "Cannot write test source");
}

std::string contents(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}

std::vector<cpplivelab::SourceChange> observe(
    cpplivelab::ProjectWatcher& watcher, DWORD timeout = 1500)
{
    const auto started = GetTickCount64();
    auto last_event = started;
    bool notified = false;
    while (GetTickCount64() - started < timeout)
    {
        if (watcher.poll())
        {
            notified = true;
            last_event = GetTickCount64();
        }
        if (notified && GetTickCount64() - last_event >= 70)
        {
            return watcher.consume_changes();
        }
        Sleep(10);
    }
    return watcher.consume_changes();
}

void test_watcher()
{
    TemporaryProjects projects;
    const auto root = projects.root / "Project With Spaces";
    write_file(root / "src/main.cpp", "int main() {}\n");
    write_file(root / "include/example.hpp", "#pragma once\n");
    write_file(root / "CMakeLists.txt", "project(Example)\n");
    cpplivelab::ProjectWatcher watcher;
    watcher.start(root);

    write_file(root / "src/main.cpp", "int main() { return 0; }\n");
    require(observe(watcher).size() == 1, "Saved cpp not detected");
    write_file(root / "include/example.hpp", "#pragma once\nint answer();\n");
    require(observe(watcher).size() == 1, "Header save not detected");
    write_file(root / "CMakeLists.txt", "project(Updated)\n");
    const auto cmake_changes = observe(watcher);
    require(cmake_changes.size() == 1 && cmake_changes.front().affects_cmake,
        "CMake changes not classified");
    write_file(root / "src/new.cpp", "int answer() { return 42; }\n");
    require(observe(watcher).size() == 1, "New source not detected");
    fs::rename(root / "src/new.cpp", root / "src/renamed.cpp");
    require(observe(watcher).size() == 2, "Source rename not detected as old/new");
    fs::remove(root / "src/renamed.cpp");
    require(observe(watcher).size() == 1, "Deleted source not detected");
    fs::rename(root / "include", root / "headers");
    require(observe(watcher).size() == 2, "Directory rename lost contained headers");

    for (const auto& file : {".git/index.cpp",
             ".vs/hidden.hpp",
             "build/generated.cpp",
             ".cpplivelab/build-Debug/CMakeLists.txt",
             "out/build/generated.cmake",
             "cmake-build-debug/test.cpp",
             "CMakeFiles/system.cmake",
             "notes.txt",
             "CMakeCache.txt",
             "cmake_install.cmake",
             "src/main.cpp.tmp",
             "src/~backup.cpp"})
    {
        write_file(root / file, "ignored\n");
    }
    require(observe(watcher, 300).empty(), "Generated/irrelevant file caused a source change");
    write_file(root / "custom-generated/CMakeCache.txt", "# generated build tree\n");
    write_file(root / "custom-generated/compiler.cpp", "generated source\n");
    require(observe(watcher, 300).empty(), "Custom CMake build directory was watched");
    write_file(root / "src/main.cpp", "int main() { return 0; }\n");
    require(observe(watcher).empty(), "Duplicate content notification not suppressed");
    for (const auto& extension : {".c", ".cc", ".cxx", ".h", ".hpp", ".hxx", ".cmake"})
    {
        require(cpplivelab::ProjectWatcher::relevant(fs::path("source") += extension),
            "Required extension not watched");
    }

    const auto second = projects.root / "Second";
    write_file(second / "main.cpp", "int main() {}\n");
    watcher.start(second);
    write_file(root / "src/main.cpp", "old project changed\n");
    require(observe(watcher, 250).empty(), "Old project was still watched");
    write_file(second / "main.cpp", "new project changed\n");
    require(observe(watcher).size() == 1, "New active project was not watched");
    write_file(second / "main.cpp.tmp", "atomic editor replacement\n");
    require(MoveFileExW((second / "main.cpp.tmp").c_str(),
                (second / "main.cpp").c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0,
        "Atomic editor save fixture failed");
    const auto atomic_change = observe(watcher);
    require(atomic_change.size() == 1 && atomic_change.front().relative_path == L"main.cpp",
        "Atomic editor save did not produce one source change");
    write_file(second / L"\u8a08\u7b97.hpp", "// Unicode header\n");
    require(observe(watcher).size() == 1, "Unicode source filename was not detected");

    write_file(second / "main.cpp", "save while editor holds the file\n");
    cpplivelab::windows_support::UniqueHandle editor_lock;
    editor_lock.reset(CreateFileW(
        (second / "main.cpp").c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr));
    require(editor_lock.value != INVALID_HANDLE_VALUE, "Editor lock fixture failed");
    bool retry_required = false;
    try
    {
        observe(watcher);
    }
    catch (const std::exception&)
    {
        retry_required = true;
    }
    require(retry_required, "A locked source was not retained for retry");
    editor_lock.reset();
    require(watcher.consume_changes().size() == 1, "Retry lost the saved source change");
    watcher.stop();
    write_file(second / "main.cpp", "after stop\n");
    require(!watcher.poll(), "Stopped watcher still notified");

    DWORD before = 0, after = 0;
    GetProcessHandleCount(GetCurrentProcess(), &before);
    for (int iteration = 0; iteration < 20; ++iteration)
    {
        cpplivelab::ProjectWatcher temporary;
        temporary.start(second);
    }
    GetProcessHandleCount(GetCurrentProcess(), &after);
    require(after <= before + 1, "Watcher shutdown leaked Windows handles");
}

void test_scheduler()
{
    using cpplivelab::ProjectWatcher;
    const std::map<fs::path, std::uint64_t> inputs{{L"main.cpp", 1}, {L"CMakeLists.txt", 2}};
    auto changed = inputs;
    changed[L"main.cpp"] = 3;
    require(!ProjectWatcher::configuration_changed(inputs, changed),
        "Source-only replacement incorrectly requested CMake reconfiguration");
    for (const auto* file :
        {L"CMakeLists.txt", L"cmake/extra.CMAKE", L"CMakePresets.json", L"CMakeUserPresets.json"})
    {
        auto configuration = changed;
        configuration[file] = 4;
        require(ProjectWatcher::configuration_changed(inputs, configuration) &&
                    ProjectWatcher::configuration_changed(configuration, inputs),
            "Changed/added/removed configuration input lost reconfiguration");
    }
    cpplivelab::AutoBuildScheduler scheduler;
    scheduler.notify(0);
    require(!scheduler.pending(), "Disabled scheduler queued work");
    scheduler.enable(true);
    scheduler.set_debounce(400);
    scheduler.notify(100);
    scheduler.notify(300);
    require(!scheduler.settled(699) && scheduler.settled(700), "Rapid saves were not debounced");
    scheduler.confirm(true, false);
    require(!scheduler.ready(700, true, true), "Overlapping build was allowed");
    require(!scheduler.ready(700, false, false), "Locked executable was ignored");
    require(scheduler.ready(700, false, true), "Settled build was not ready");
    scheduler.begin_build();
    require(!scheduler.pending(), "Beginning build failed to consume known changes");
    scheduler.notify(800);
    require(scheduler.settled(1200), "Change during build lost");
    scheduler.confirm(true, true);
    require(scheduler.pending() && scheduler.needs_reconfigure(), "Follow-up CMake build lost");
    require(!scheduler.ready(1200, true, true), "Follow-up overlapped ongoing build");
    scheduler.defer(1200);
    require(!scheduler.ready(2199, false, true) && scheduler.ready(2200, false, true),
        "Deferred retry not throttled");
    scheduler.begin_build();
    require(!scheduler.ready(9999, false, true), "Failure/no changes caused infinite retry");
    scheduler.notify(10000);
    scheduler.confirm(false, false);
    require(!scheduler.pending(), "Duplicate event queued a build");
    scheduler.notify(11000);
    scheduler.enable(false);
    require(!scheduler.pending() && !scheduler.ready(20000, false, true),
        "OFF did not cancel pending work");
    scheduler.enable(true);
    require(!scheduler.pending(), "Re-enabling unexpectedly rebuilt old changes");
    scheduler.notify(22000);
    scheduler.cancel();
    require(!scheduler.pending(), "Project switch did not cancel old work");
}

void test_prompts()
{
    using cpplivelab::MessageKind;
    cpplivelab::ConsoleHistory history;
    history.append(MessageKind::ProgramOutput, L"Earlier output\nAnother completed line\nNa");
    history.append(MessageKind::ProgramOutput, L"me");
    history.append(MessageKind::ProgramOutput, L": ");
    require(history.unfinished_stdout_line() == L"Name: ", "Split stdout prompt was not retained");
    history.append(MessageKind::Input, L"> Ada\n");
    require(history.unfinished_stdout_line().empty(), "Input echo retained an answered prompt");
    history.append(MessageKind::ProgramOutput, L"Second number: ");
    require(history.unfinished_stdout_line() == L"Second number: ",
        "Next unfinished stdout line was lost");
    history.clear();
    require(history.unfinished_stdout_line().empty(), "Full clear retained old stream state");
    history.append(MessageKind::ProgramOutput, L"Finished\r");
    history.append(MessageKind::ProgramOutput, L"\n");
    require(
        history.unfinished_stdout_line().empty(), "Split CRLF was mistaken for an unfinished line");
    history.append(MessageKind::ProgramOutput, L"\u540d\u524d: ");
    require(history.unfinished_stdout_line() == L"\u540d\u524d: ", "Unicode stdout line changed");
    history.append(MessageKind::ProgramError, L"stderr without newline");
    require(history.unfinished_stdout_line().empty(), "Mixed stdout/stderr line was guessed");
    history.append(MessageKind::ProgramError, L"\n");
    history.append(MessageKind::ProgramOutput, L"New stdout: ");
    require(history.unfinished_stdout_line() == L"New stdout: ",
        "Newline did not reset stream ambiguity");
    history.append(MessageKind::ProgramOutput, L"\b");
    require(history.unfinished_stdout_line().empty(),
        "Backspace editing was reconstructed as a prompt");
    history.append(MessageKind::ProgramOutput, L"\nName: \x1b[0m");
    require(history.unfinished_stdout_line().empty(), "ANSI editing was reconstructed as a prompt");
    cpplivelab::ConsoleHistory bounded(16, 2);
    bounded.append(MessageKind::ProgramOutput, std::wstring(40, L'x'));
    require(
        bounded.unfinished_stdout_line().empty(), "Truncated long line was treated as complete");
    bounded.append(MessageKind::ProgramOutput, L"\nName: ");
    require(bounded.unfinished_stdout_line() == L"Name: ", "Bounded line tracking did not recover");
}

void test_console_model()
{
    using cpplivelab::MessageKind;
    cpplivelab::ConsoleHistory program(10000, 200), system(10000, 200);
    program.set_active(true);
    program.append(MessageKind::ProgramOutput, L"stdout remains neutral\n");
    system.append(MessageKind::Error, L"example.cpp(12): error C2143\n");
    require(system.unread() && system.unread_error(), "Background error indicator missing");
    require(!program.unread(), "Active program tab incorrectly marked unread");
    system.set_active(true);
    program.set_active(false);
    program.append(MessageKind::ProgramOutput, L"background stdout\n");
    require(program.unread(), "Inactive program tab did not receive output");
    const auto program_size = program.characters();
    system.clear();
    require(program.characters() == program_size, "Independent clear destroyed another console");
    require(cpplivelab::classify_build_line(L"example.cpp(12): error C2143: missing token") ==
                MessageKind::Error,
        "Compiler error not classified");
    require(
        cpplivelab::classify_build_line(
            L"example.cpp(7): fatal error C1189: #error Executable is running or not writable") ==
            MessageKind::Error,
        "Compiler error text was mistaken for a deferred-build warning");
    require(cpplivelab::classify_build_line(
                L"CppLiveLab build deferred: This configuration is already building or running") ==
                MessageKind::Warning,
        "Explicit deferred-build result lost its warning classification");
    require(cpplivelab::classify_build_line(L"example.cpp(12): warning C4101: unused") ==
                MessageKind::Warning,
        "Compiler warning not classified");
    require(cpplivelab::classify_build_line(L"[1/2] Building src/error.cpp") == MessageKind::Build,
        "A filename was confused with a diagnostic severity");
    require(program.entries().front().kind == MessageKind::ProgramOutput,
        "stdout was classified as success");
    for (int chunk = 0; chunk < 500; ++chunk)
    {
        program.append(MessageKind::ProgramOutput, std::wstring(100, L'x'));
    }
    program.append(MessageKind::ProgramError, L"recent stderr\n");
    require(program.characters() <= 10000 && program.entries().size() <= 200,
        "Console history was unbounded");
    require(program.entries().back().kind == MessageKind::ProgramError,
        "Recent relevant error was discarded");
    require(cpplivelab::message_label(MessageKind::Success) == L"[SUCCESS] " &&
                cpplivelab::message_label(MessageKind::Error) == L"[ERROR] ",
        "Severity text labels absent");
}

void test_channels(const fs::path& cli)
{
    const auto fixture = cli.parent_path() / L"session_fixture.exe";
    cpplivelab::ConsoleSession session;
    session.start_program(fixture, {L"interactive"}, fixture.parent_path(), {});
    std::string standard_output, standard_error;
    const auto collect = [&]
    {
        for (const auto& chunk : session.poll_output())
        {
            (chunk.stream == cpplivelab::OutputStream::StandardError ? standard_error
                                                                     : standard_output) +=
                chunk.bytes;
        }
    };
    const auto await = [&](const std::string& prompt)
    {
        const auto start = GetTickCount64();
        while (standard_output.find(prompt) == std::string::npos)
        {
            collect();
            if (GetTickCount64() - start > 5000)
            {
                throw std::runtime_error("Separated console prompt absent");
            }
            Sleep(10);
        }
    };
    await("Name: ");
    require(session.send_line("Ada Lovelace"), "Separated name input failed");
    await("First: ");
    require(session.send_line("2.5"), "Separated first number failed");
    await("Second: ");
    require(session.send_line("3.75"), "Separated second number failed");
    const auto start = GetTickCount64();
    while (session.running() || session.output_pending())
    {
        collect();
        if (GetTickCount64() - start > 5000)
        {
            throw std::runtime_error("Separated program did not finish");
        }
        Sleep(10);
    }
    collect();
    require(
        standard_output.find("sum 6.25") != std::string::npos, "Separated program output missing");
    require(standard_error.find("stderr visible") != std::string::npos,
        "stderr was not distinguishable");
    require(
        standard_output.find("stderr visible") == std::string::npos, "stderr leaked into stdout");
    require(standard_error.find("Name:") == std::string::npos, "stdout leaked into stderr");
    require(session.exit_code() == 7, "Separated program exit code was lost");
}

void test_appearance()
{
    using cpplivelab::ThemeChoice;
    require(
        std::abs(theme_test::contrast_ratio(RGB(0, 0, 0), RGB(255, 255, 255)) - 21) < 0.000001 &&
            theme_test::contrast_ratio(RGB(100, 100, 100), RGB(100, 100, 100)) == 1,
        "WCAG contrast calculation failed black/white or equal-color reference values");
    std::string contrast_failures;
    for (const auto choice : {ThemeChoice::Dark, ThemeChoice::Light})
    {
        const auto palette = cpplivelab::theme_palette(choice);
        for (const auto kind : {cpplivelab::MessageKind::ProgramOutput,
                 cpplivelab::MessageKind::System,
                 cpplivelab::MessageKind::Success,
                 cpplivelab::MessageKind::Warning,
                 cpplivelab::MessageKind::Error})
        {
            const auto description =
                theme_test::contrast_description(choice, kind, palette.console);
            std::cout << description << '\n';
            // Compare the unrounded value; printed rounding must never admit a failure.
            if (theme_test::contrast_ratio(cpplivelab::message_color(palette.console, kind),
                    palette.console.background) < 4.5)
            {
                contrast_failures += "\n" + description;
            }
        }
    }
    require(contrast_failures.empty(), "Console contrast failures:" + contrast_failures);
    for (const auto choice : {ThemeChoice::Dark, ThemeChoice::Light})
    {
        const auto palette = cpplivelab::theme_palette(choice);
        require(palette.dark == (choice == ThemeChoice::Dark), "Explicit theme resolution failed");
        require(cpplivelab::message_color(palette.console,
                    cpplivelab::MessageKind::ProgramOutput) == palette.console.neutral,
            "Theme made stdout success-colored");
        const auto colors = std::array<COLORREF, 4>{palette.console.error,
            palette.console.success,
            palette.console.information,
            palette.console.warning};
        const auto kinds = std::array{cpplivelab::MessageKind::Error,
            cpplivelab::MessageKind::Success,
            cpplivelab::MessageKind::System,
            cpplivelab::MessageKind::Warning};
        const auto identity = [&](size_t index, bool valid, const char* reason)
        {
            require(valid,
                theme_test::context(choice, kinds[index]) + " " + theme_test::rgb(colors[index]) +
                    ": " + reason);
        };
        identity(0,
            GetRValue(colors[0]) > GetGValue(colors[0]) + 30 &&
                GetRValue(colors[0]) > GetBValue(colors[0]) + 30,
            "red must exceed green and blue by more than 30");
        identity(1,
            GetGValue(colors[1]) > GetRValue(colors[1]) + 30 &&
                GetGValue(colors[1]) > GetBValue(colors[1]) + 30,
            "green must exceed red and blue by more than 30");
        identity(2,
            GetBValue(colors[2]) > GetRValue(colors[2]) + 30 &&
                GetBValue(colors[2]) > GetGValue(colors[2]) + 30,
            "blue must exceed red and green by more than 30");
        identity(3,
            GetRValue(colors[3]) > GetGValue(colors[3]) + 20 &&
                GetGValue(colors[3]) > GetBValue(colors[3]) + 40,
            "orange requires red > green + 20 and green > blue + 40");
        for (size_t first = 0; first < colors.size(); ++first)
        {
            for (size_t second = first + 1; second < colors.size(); ++second)
            {
                const auto hue = [](COLORREF color)
                {
                    const double red = GetRValue(color), green = GetGValue(color),
                                 blue = GetBValue(color);
                    const double high = (std::max)({red, green, blue});
                    const double low = (std::min)({red, green, blue});
                    const double range = high - low;
                    const double sector = high == red     ? (green - blue) / range
                                          : high == green ? (blue - red) / range + 2
                                                          : (red - green) / range + 4;
                    return std::fmod(sector * 60 + 360, 360);
                };
                // Hue separation expresses color identity; a raw RGB-distance
                // cutoff can reject distinct dark red/orange colors arbitrarily.
                const auto difference = std::abs(hue(colors[first]) - hue(colors[second]));
                const double separation = (std::min)(difference, 360 - difference);
                require(separation >= 30,
                    theme_test::context(choice, kinds[first]) + " " +
                        theme_test::rgb(colors[first]) + " and " +
                        theme_test::semantic_name(kinds[second]) + " " +
                        theme_test::rgb(colors[second]) + ": hue separation " +
                        theme_test::decimal(separation) + " degrees (minimum 30)");
            }
        }
    }
    require(cpplivelab::theme_palette(ThemeChoice::FollowWindows).dark ==
                cpplivelab::windows_uses_dark_theme(),
        "Follow Windows did not resolve the current OS preference");
    const auto followed = cpplivelab::theme_palette(ThemeChoice::FollowWindows);
    const auto explicit_palette = cpplivelab::theme_palette(
        cpplivelab::windows_uses_dark_theme() ? ThemeChoice::Dark : ThemeChoice::Light);
    require(followed.console.error == explicit_palette.console.error &&
                followed.console.success == explicit_palette.console.success &&
                followed.console.information == explicit_palette.console.information &&
                followed.console.warning == explicit_palette.console.warning,
        "Follow Windows selected a mixed semantic palette");
    for (bool dark : {false, true})
    {
        const auto resolved = cpplivelab::theme_palette(ThemeChoice::FollowWindows, dark);
        const auto explicit_theme =
            cpplivelab::theme_palette(dark ? ThemeChoice::Dark : ThemeChoice::Light);
        require(resolved.dark == dark && resolved.console.error == explicit_theme.console.error &&
                    resolved.console.success == explicit_theme.console.success &&
                    resolved.console.information == explicit_theme.console.information &&
                    resolved.console.warning == explicit_theme.console.warning,
            "Follow Windows transition used the wrong semantic palette");
    }
    require(cpplivelab::scale_pixels(100, 96) == 100 && cpplivelab::scale_pixels(100, 144) == 150 &&
                cpplivelab::scale_pixels(100, 192) == 200,
        "DPI scaling incorrect");

    const std::wstring test_key =
        L"Software\\CppLiveLab\\Stage4Tests\\" + std::to_wstring(GetCurrentProcessId());

    struct RemoveTestKey
    {
        std::wstring key;

        ~RemoveTestKey()
        {
            RegDeleteTreeW(HKEY_CURRENT_USER, key.c_str());
        }
    } cleanup{test_key};

    const auto defaults = cpplivelab::load_preferences(test_key);
    require(defaults.theme == ThemeChoice::FollowWindows && !defaults.auto_build &&
                defaults.debounce_ms == 400,
        "New-user preference defaults incorrect");
    cpplivelab::Preferences selected;
    selected.theme = ThemeChoice::Dark;
    selected.auto_build = true;
    selected.debounce_ms = 450;
    require(cpplivelab::save_preferences(selected, test_key), "Preferences did not save");
    const auto restored = cpplivelab::load_preferences(test_key);
    require(restored.theme == selected.theme && restored.auto_build && restored.debounce_ms == 450,
        "Theme/Auto Build/debounce persistence failed");
    selected.theme = ThemeChoice::Light;
    selected.auto_build = false;
    require(cpplivelab::save_preferences(selected, test_key), "Light preference did not save");
    require(cpplivelab::load_preferences(test_key).theme == ThemeChoice::Light,
        "Light persistence failed");
}

std::string finish(cpplivelab::ConsoleSession& session)
{
    const auto started = GetTickCount64();
    std::string output;
    while (session.running() || session.output_pending())
    {
        output += session.poll();
        if (GetTickCount64() - started > 60000)
        {
            throw std::runtime_error("Build timed out\n" + output);
        }
        Sleep(10);
    }
    output += session.poll();
    return output;
}

void test_auto_build(const fs::path& cli)
{
    TemporaryProjects projects;
    const auto root = projects.root / "Auto Calculator";
    cpplivelab::create_project(root, true);
    const auto configuration_before = contents(root / "CMakeLists.txt");
    cpplivelab::ProjectWatcher watcher;
    watcher.start(root);
    cpplivelab::ConsoleSession build;
    const auto build_current = [&](const wchar_t* configuration)
    {
        build.start_worker(
            cli, {L"build", root.wstring(), L"--config", configuration, L"--incremental"}, root);
        return finish(build);
    };
    auto output = build_current(L"Debug");
    require(build.exit_code() == 0, output.c_str());
    const auto program = root / ".cpplivelab/build-Debug/app.exe";
    const auto successful_executable = contents(program);
    write_file(root / "src/calculator.cpp", "#error Stage4 intentional failure\n");
    require(!observe(watcher).empty(), "Error save not detected");
    output = build_current(L"Debug");
    require(build.exit_code() != 0 && output.find("error C1189") != std::string::npos,
        "Compilation failure missing");
    require(contents(program) == successful_executable,
        "Compile error destroyed previous successful program");
    write_file(root / "src/calculator.cpp",
        "#include \"calculator.hpp\"\ndouble add(double a, double b) { return a * b; }\n");
    require(!observe(watcher).empty(), "Corrected save not detected");
    output = build_current(L"Debug");
    require(build.exit_code() == 0, output.c_str());
    output = build_current(L"Release");
    require(
        build.exit_code() == 0 && fs::is_regular_file(root / ".cpplivelab/build-Release/app.exe"),
        "Auto Release build failed");

    cpplivelab::ConsoleSession program_session;
    program_session.start_worker(cli, {L"run", root.wstring()}, root);
    std::string program_output;
    const auto started = GetTickCount64();
    while (program_output.find("Your name: ") == std::string::npos)
    {
        program_output += program_session.poll();
        if (!program_session.running() || GetTickCount64() - started > 30000)
        {
            throw std::runtime_error("Input-waiting program failed to launch\n" + program_output);
        }
        Sleep(10);
    }
    write_file(root / "src/calculator.cpp",
        "#include \"calculator.hpp\"\ndouble add(double a, double b) { return a + b; }\n");
    require(!observe(watcher).empty(), "Running-program save not detected");
    require(!cpplivelab::build_outputs_available(root / ".cpplivelab/build-Debug"),
        "Running program was not deferred");
    require(program_session.running(), "Source change killed the running program");
    require(program_session.send_line("Ada Lovelace"), "Name input failed");
    Sleep(50);
    program_session.poll();
    require(program_session.send_line("2.5"), "First number input failed");
    Sleep(50);
    program_session.poll();
    require(program_session.send_line("3.75"), "Second number input failed");
    program_output += finish(program_session);
    require(program_output.find("Sum: 9.375") != std::string::npos,
        "Running program was silently replaced/restarted");
    require(cpplivelab::build_outputs_available(root / ".cpplivelab/build-Debug"),
        "Exited program retained build lock");
    output = build_current(L"Debug");
    require(build.exit_code() == 0, "Deferred build did not recover");
    require(contents(root / "CMakeLists.txt") == configuration_before,
        "Auto builds changed user configuration");
    require(observe(watcher, 250).empty(), "Build outputs created an automatic rebuild loop");

    write_file(root / "CMakeLists.txt",
        configuration_before + "\nadd_custom_target(stage4_delay ALL\n"
                               " COMMAND ${CMAKE_COMMAND} -E echo Stage4Delay\n"
                               " COMMAND ${CMAKE_COMMAND} -E sleep 2)\n");
    const auto configuration_changes = observe(watcher);
    require(configuration_changes.size() == 1 && configuration_changes.front().affects_cmake,
        "Real CMake save did not request configuration");
    build.start_worker(cli, {L"build", root.wstring()}, root);
    output.clear();
    const auto build_started = GetTickCount64();
    while (output.find("Stage4Delay") == std::string::npos)
    {
        output += build.poll();
        require(build.running() && GetTickCount64() - build_started < 30000,
            "Delayed CMake build did not start");
        Sleep(10);
    }
    cpplivelab::AutoBuildScheduler scheduler;
    scheduler.enable(true);
    for (int save = 0; save < 3; ++save)
    {
        write_file(root / "src/calculator.cpp",
            "#include \"calculator.hpp\"\ndouble add(double a, double b) { return a + b; }\n// "
            "save " +
                std::to_string(save) + "\n");
        scheduler.notify(GetTickCount64());
    }
    const auto during_build = observe(watcher);
    require(during_build.size() == 1, "Rapid saves were not coalesced");
    scheduler.confirm(true, false);
    require(!scheduler.ready(GetTickCount64(), true, true), "Overlapping build was permitted");
    output += finish(build);
    require(build.exit_code() == 0, output.c_str());
    require(scheduler.ready(GetTickCount64(), false, true), "Follow-up build was lost");
    scheduler.begin_build();
    output = build_current(L"Debug");
    require(build.exit_code() == 0 && !scheduler.pending(),
        "Exactly one follow-up build did not complete");
    require(observe(watcher, 250).empty(), "Follow-up build caused a watcher loop");
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    if (argc != 3)
    {
        return 2;
    }
    try
    {
        const std::wstring test_case = argv[1];
        if (test_case == L"watcher")
        {
            test_watcher();
        }
        else if (test_case == L"scheduler")
        {
            test_scheduler();
        }
        else if (test_case == L"auto_build")
        {
            test_auto_build(argv[2]);
        }
        else if (test_case == L"console_model")
        {
            test_console_model();
        }
        else if (test_case == L"prompts")
        {
            test_prompts();
        }
        else if (test_case == L"channels")
        {
            test_channels(argv[2]);
        }
        else if (test_case == L"appearance")
        {
            test_appearance();
        }
        else
        {
            return 2;
        }
        std::wcout << L"Stage 4 " << test_case << L": " << checks << L" checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Stage 4 failure: " << error.what() << '\n';
        return 1;
    }
}
