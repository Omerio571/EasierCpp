#pragma once

#include "process/windows_support.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

namespace cpplivelab
{
// Opt-in numeric diagnostics only. Never persist or print environment blocks.
class BuildProfile
{
public:
    static bool enabled()
    {
        wchar_t flag[2]{};
        return GetEnvironmentVariableW(L"EASIERCPP_PROFILE_BUILD", flag, 2) == 1 && flag[0] == L'1';
    }

    static void worker_start()
    {
        if (enabled())
        {
            std::cout << "EASIERCPP_PROFILE worker_start_tick=" << GetTickCount64() << '\n'
                      << std::flush;
        }
    }

    BuildProfile() : active_(enabled()), started_(Clock::now())
    {
        if (active_)
        {
            std::cout << "EASIERCPP_PROFILE managed_entry_tick=" << GetTickCount64() << '\n';
        }
    }

    void checkpoint(const char* phase)
    {
        if (!active_)
        {
            return;
        }
        const auto now = Clock::now();
        value(phase, std::chrono::duration<double, std::milli>(now - started_).count());
        started_ = Clock::now();
    }

    void value(const char* name, double amount) const
    {
        if (active_)
        {
            std::ostringstream line;
            line << "EASIERCPP_PROFILE " << name << '=' << std::fixed << std::setprecision(3)
                 << amount;
            std::cout << line.str() << '\n' << std::flush;
        }
    }

    void ninja_begin(const std::filesystem::path& directory)
    {
        if (!active_)
        {
            return;
        }
        log_ = directory / L".ninja_log";
        std::ifstream input(log_, std::ios::binary | std::ios::ate);
        offset_ = input ? input.tellg() : std::streampos{0};
        if (offset_ > 0)
        {
            const auto length = std::min<std::streamoff>(128, offset_);
            input.seekg(offset_ - length);
            tail_.resize(static_cast<size_t>(length));
            input.read(tail_.data(), length);
        }
    }

    void ninja_end() const
    {
        if (!active_)
        {
            return;
        }
        std::ifstream input(log_, std::ios::binary | std::ios::ate);
        if (!input || input.tellg() < offset_)
        {
            value("ninja_log_available", 0);
            return;
        }
        input.seekg(0);
        std::string header;
        std::getline(input, header);
        if (header != "# ninja log v5" && header != "# ninja log v6")
        {
            value("ninja_log_available", 0);
            return;
        }
        if (offset_ > 0)
        {
            input.seekg(offset_ - static_cast<std::streamoff>(tail_.size()));
            std::string tail(tail_.size(), '\0');
            input.read(tail.data(), static_cast<std::streamsize>(tail.size()));
            if (tail != tail_)
            {
                value("ninja_log_available", 0);
                return;
            }
            input.seekg(offset_);
        }

        // A link edge can record both an import library and a DLL. Count it once
        // as a link, regardless of which output Ninja records first.
        struct Command
        {
            double duration;
            int kind;
        };

        std::map<std::string, Command> commands;
        for (std::string line; std::getline(input, line);)
        {
            std::istringstream fields(line);
            std::string begin, end, mtime, output, hash;
            if (!std::getline(fields, begin, '\t') || !std::getline(fields, end, '\t') ||
                !std::getline(fields, mtime, '\t') || !std::getline(fields, output, '\t') ||
                !std::getline(fields, hash))
            {
                continue;
            }
            try
            {
                const bool object = output.ends_with(".obj") || output.ends_with(".o");
                const bool binary = output.ends_with(".exe") || output.ends_with(".dll");
                const bool library = output.ends_with(".lib") || output.ends_with(".a");
                if (!object && !binary && !library)
                {
                    continue;
                }
                const auto duration = std::stod(end) - std::stod(begin);
                if (duration < 0)
                {
                    continue;
                }
                const auto identity = begin + "/" + end + "/" + hash;
                const int kind = binary ? 3 : object ? 2 : 1;
                const auto found = commands.find(identity);
                if (found == commands.end() || found->second.kind < kind)
                {
                    commands[identity] = {duration, kind};
                }
            }
            catch (const std::exception&)
            {
                value("ninja_log_available", 0);
                return;
            }
        }
        // Command durations can overlap. Do not present their sum as wall time.
        double compile = 0, link = 0, archive = 0;
        for (const auto& [identity, command] : commands)
        {
            if (command.kind == 3)
            {
                link += command.duration;
            }
            else if (command.kind == 2)
            {
                compile += command.duration;
            }
            else
            {
                archive += command.duration;
            }
        }
        value("ninja_log_available", 1);
        value("compile_commands_ms", compile);
        value("link_commands_ms", link);
        value("archive_commands_ms", archive);
        value("ninja_commands", static_cast<double>(commands.size()));
    }

private:
    using Clock = std::chrono::steady_clock;
    bool active_;
    Clock::time_point started_;
    std::filesystem::path log_;
    std::streampos offset_{0};
    std::string tail_;
};
} // namespace cpplivelab
