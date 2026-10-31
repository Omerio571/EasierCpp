#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <iostream>

int main(int argc, char* argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << SDL_GetError() << '\n';
        return 1;
    }
    auto* window = SDL_CreateWindow("CppLiveLab SDL3 integration", 320, 200, 0);
    if (!window)
    {
        std::cerr << SDL_GetError() << '\n';
        SDL_Quit();
        return 2;
    }
    for (int frame = 0; frame < 90; ++frame)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
        }
        SDL_Delay(16);
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    std::cout << "CPPLIVELAB_CONSUMER_OK\n";
    return 0;
}
