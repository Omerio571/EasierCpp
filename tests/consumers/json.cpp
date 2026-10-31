#include <nlohmann/json.hpp>
#include <iostream>

int main()
{
    const auto value = nlohmann::json::parse("{\"answer\":42}");
    if (value.at("answer") != 42 || nlohmann::json::parse(value.dump()) != value)
    {
        return 1;
    }
    std::cout << "CPPLIVELAB_CONSUMER_OK\n";
}
