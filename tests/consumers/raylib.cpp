#include <raylib.h>
#include <iostream>

int main()
{
    InitWindow(320, 200, "CppLiveLab Raylib integration");
    if (!IsWindowReady())
    {
        return 1;
    }
    SetTargetFPS(60);
    for (int frame = 0; frame < 90 && !WindowShouldClose(); ++frame)
    {
        BeginDrawing();
        ClearBackground(DARKBLUE);
        DrawText("Raylib integration", 20, 80, 20, WHITE);
        EndDrawing();
    }
    CloseWindow();
    std::cout << "CPPLIVELAB_CONSUMER_OK\n";
}
