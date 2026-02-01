/*
    3D Graphics Demo - SDL2 + ImGui Version

    This is a clean demonstration of 3D to 2D projection.

    Code Structure:
    - math3d/math3d.h : Vector and matrix operations
    - core/engine.h   : 3D rendering engine
    - SDLApp.h        : SDL2 + ImGui framework
*/

#include "core/engine.h"

// ============== Main ==============
int main(int argc, char* argv[]) {
    Engine3D engine;
    if (engine.Init()) engine.Run();
    return 0;
}