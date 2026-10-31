#include <fmt/format.h>
#include <iostream>

int main()
{
    if (fmt::format("{} + {} = {}", 20, 22, 42) != "20 + 22 = 42")
    {
        return 1;
    }
    std::cout << "CPPLIVELAB_CONSUMER_OK\n";
}
