#include "cpplivelab/doctor.hpp"
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        std::cout << "CppLiveLab\nUsage: cpplivelab [--help | --version | doctor]\n";
        return 0;
    }
    if (argc == 2) {
        const std::string_view command(argv[1]);
        if (command == "--version") {
            std::cout << "CppLiveLab " << CPPLIVELAB_VERSION << '\n';
            return 0;
        }
        if (command == "doctor") {
            return cpplivelab::run_doctor(std::cout,
                cpplivelab::discover_toolchain(cpplivelab::system_discovery_inputs()));
        }
    }
    std::cerr << "Invalid arguments. Use cpplivelab --help.\n";
    return 2;
}
