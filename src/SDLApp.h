/*
    SDLApp.h - SDL2 + ImGui Application Framework
    Encapsulates all SDL2 and ImGui initialization, rendering, and cleanup
*/

#pragma once

#include <SDL.h>
#include <functional>
#include <string>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

// ============== Color Structure ==============
struct Color {
    Uint8 r = 255, g = 255, b = 255, a = 255;
    Color() = default;
    Color(Uint8 r_, Uint8 g_, Uint8 b_, Uint8 a_ = 255) : r(r_), g(g_), b(b_), a(a_) {}
    
    static Color White()  { return Color(255, 255, 255); }
    static Color Black()  { return Color(0, 0, 0); }
    static Color Red()    { return Color(255, 0, 0); }
    static Color Green()  { return Color(0, 255, 0); }
    static Color Blue()   { return Color(0, 0, 255); }
    static Color Yellow() { return Color(255, 255, 0); }
    static Color Cyan()   { return Color(0, 255, 255); }
    static Color Gray()   { return Color(128, 128, 128); }
    
    Color operator*(float f) const {
        return Color((Uint8)(r * f), (Uint8)(g * f), (Uint8)(b * f), a);
    }
};

// ============== SDL Application Class ==============
class SDLApp {
public:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    int screenWidth = 1024;
    int screenHeight = 960;
    float deltaTime = 0.0f;
    bool running = true;
    
    // Keyboard state
    const Uint8* keyState = nullptr;
    
    bool Init(const std::string& title, int width = 1024, int height = 960) {
        screenWidth = width;
        screenHeight = height;
        
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            SDL_Log("SDL_Init Error: %s", SDL_GetError());
            return false;
        }
        
        window = SDL_CreateWindow(title.c_str(),
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            screenWidth, screenHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        if (!window) return false;
        
        renderer = SDL_CreateRenderer(window, -1, 
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) return false;
        
        // Initialize ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();
        ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
        ImGui_ImplSDLRenderer2_Init(renderer);
        
        keyState = SDL_GetKeyboardState(NULL);
        return true;
    }
    
    void ProcessEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_WINDOWEVENT && 
                event.window.event == SDL_WINDOWEVENT_RESIZED) {
                screenWidth = event.window.data1;
                screenHeight = event.window.data2;
            }
        }
    }
    
    void BeginFrame() {
        static Uint64 lastTime = SDL_GetPerformanceCounter();
        Uint64 currentTime = SDL_GetPerformanceCounter();
        deltaTime = (float)(currentTime - lastTime) / SDL_GetPerformanceFrequency();
        lastTime = currentTime;
        
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
    }
    
    void EndFrame() {
        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }
    
    void Cleanup() {
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
    
    // Drawing functions
    void DrawPixel(int x, int y, const Color& c) {
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
        SDL_RenderDrawPoint(renderer, x, y);
    }
    
    void DrawLine(int x1, int y1, int x2, int y2, const Color& c) {
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
    }
    
    void DrawTriangle(int x1, int y1, int x2, int y2, int x3, int y3, const Color& c) {
        DrawLine(x1, y1, x2, y2, c);
        DrawLine(x2, y2, x3, y3, c);
        DrawLine(x3, y3, x1, y1, c);
    }

    void FillTriangle(int x1, int y1, int x2, int y2, int x3, int y3, const Color& c) {
        // Sort vertices by y
        if (y2 < y1) { std::swap(y1, y2); std::swap(x1, x2); }
        if (y3 < y1) { std::swap(y1, y3); std::swap(x1, x3); }
        if (y3 < y2) { std::swap(y2, y3); std::swap(x2, x3); }

        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);

        auto drawScanline = [&](int sy, int ax, int bx) {
            if (ax > bx) std::swap(ax, bx);
            SDL_RenderDrawLine(renderer, ax, sy, bx, sy);
        };

        int dy1 = y2 - y1, dx1 = x2 - x1;
        int dy2 = y3 - y1, dx2 = x3 - x1;
        float dax_step = 0, dbx_step = 0;
        if (dy1) dax_step = dx1 / (float)abs(dy1);
        if (dy2) dbx_step = dx2 / (float)abs(dy2);

        if (dy1) {
            for (int i = y1; i <= y2; i++) {
                int ax = x1 + (int)((float)(i - y1) * dax_step);
                int bx = x1 + (int)((float)(i - y1) * dbx_step);
                drawScanline(i, ax, bx);
            }
        }

        dy1 = y3 - y2; dx1 = x3 - x2;
        if (dy1) dax_step = dx1 / (float)abs(dy1);

        if (dy1) {
            for (int i = y2; i <= y3; i++) {
                int ax = x2 + (int)((float)(i - y2) * dax_step);
                int bx = x1 + (int)((float)(i - y1) * dbx_step);
                drawScanline(i, ax, bx);
            }
        }
    }

    // Key checking
    bool IsKeyDown(SDL_Scancode key) const { return keyState[key] != 0; }
};

