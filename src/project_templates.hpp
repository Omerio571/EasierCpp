#pragma once

#include <string_view>

namespace cpplivelab::project_templates
{
// Kept as readable source text rather than densely escaped C++ string fragments.
// These templates are written only when creating a new or empty user project.
inline constexpr std::string_view cmake_header = R"cmake(cmake_minimum_required(VERSION 3.20)
project(UserProject LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
)cmake";

inline constexpr std::string_view calculator_header = R"cpp(#pragma once

double add(double left, double right);
)cpp";

inline constexpr std::string_view calculator_implementation = R"cpp(#include "calculator.hpp"

double add(double left, double right)
{
    return left + right;
}
)cpp";

inline constexpr std::string_view calculator_main = R"cpp(#include "calculator.hpp"

#include <iostream>
#include <string>

int main()
{
    std::string user_name;
    double first_number = 0;
    double second_number = 0;

    std::cout << "Your name: ";

    if (!std::getline(std::cin, user_name))
    {
        return 1;
    }

    std::cout << "First number: ";

    if (!(std::cin >> first_number))
    {
        std::cerr << "Invalid number.\n";
        return 1;
    }

    std::cout << "Second number: ";

    if (!(std::cin >> second_number))
    {
        std::cerr << "Invalid number.\n";
        return 1;
    }

    std::cout << "Hello, " << user_name << "! Sum: " << add(first_number, second_number) << '\n';
}
)cpp";

inline constexpr std::string_view hello_main = R"cpp(#include <iostream>

int main()
{
    std::cout << "Hello from C++20!\n";
}
)cpp";
} // namespace cpplivelab::project_templates
