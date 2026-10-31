#pragma once

#include <chrono>
#include <string_view>

class PerfomanceTimer
{
public:
	using Clock = std::chrono::steady_clock();

	explicit PerfomanceTimer(std::string_view name)
		: name_(name), start_(Clock::now())
	{
	}

	double elapsed_ms() const
	{
        const auto elapsed = Clock::now() - start_;

		return 
			std::chrono::duration<double, 
			std::milli>(elapsed).count();
	}

	std::string_view name() const
	{
        return name_;
	}

private:
    std::string_view name;
    Clock::time_point start_;
}