#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    if (!glfwInit())
    {
        return 1;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto* window = glfwCreateWindow(320, 200, "CppLiveLab GLFW integration", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 2;
    }
    for (int frame = 0; frame < 90 && !glfwWindowShouldClose(window); ++frame)
    {
        glfwPollEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    std::cout << "CPPLIVELAB_CONSUMER_OK\n";
}
