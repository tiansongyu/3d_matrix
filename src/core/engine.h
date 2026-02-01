/*
    engine.h - 3D Graphics Engine
    Clean and minimal 3D rendering engine for learning projection concepts
*/

#pragma once

#include "../SDLApp.h"
#include "../math3d/math3d.h"
#include <vector>
#include <list>
#include <algorithm>

// ============== Triangle Structure ==============
struct triangle {
    vec3d p[3];     // Three vertices
    Color color;    // Face color
};

// ============== Triangle Clipping ==============

// Calculate intersection point of line with plane
inline vec3d Vec_IntersectPlane(vec3d& plane_p, vec3d& plane_n, vec3d& start, vec3d& end, float& t) {
    plane_n = Vec_Norm(plane_n);
    float pd = -Vec_Dot(plane_n, plane_p);
    float ad = Vec_Dot(start, plane_n);
    float bd = Vec_Dot(end, plane_n);
    t = (-pd - ad) / (bd - ad);
    return Vec_Add(start, Vec_Mul(Vec_Sub(end, start), t));
}

// Clip triangle against a plane, returns 0-2 triangles
inline int ClipTriangle(vec3d plane_p, vec3d plane_n, triangle& in, triangle& out1, triangle& out2) {
    plane_n = Vec_Norm(plane_n);
    auto dist = [&](vec3d& p) { return Vec_Dot(plane_n, p) - Vec_Dot(plane_n, plane_p); };

    vec3d* inside[3];  int nIn = 0;
    vec3d* outside[3]; int nOut = 0;

    float d0 = dist(in.p[0]), d1 = dist(in.p[1]), d2 = dist(in.p[2]);
    if (d0 >= 0) inside[nIn++] = &in.p[0]; else outside[nOut++] = &in.p[0];
    if (d1 >= 0) inside[nIn++] = &in.p[1]; else outside[nOut++] = &in.p[1];
    if (d2 >= 0) inside[nIn++] = &in.p[2]; else outside[nOut++] = &in.p[2];

    if (nIn == 0) return 0;
    if (nIn == 3) { out1 = in; return 1; }

    if (nIn == 1) {
        out1.color = in.color;
        out1.p[0] = *inside[0];
        float t;
        out1.p[1] = Vec_IntersectPlane(plane_p, plane_n, *inside[0], *outside[0], t);
        out1.p[2] = Vec_IntersectPlane(plane_p, plane_n, *inside[0], *outside[1], t);
        return 1;
    }

    if (nIn == 2) {
        out1.color = in.color; out2.color = in.color;
        out1.p[0] = *inside[0]; out1.p[1] = *inside[1];
        float t;
        out1.p[2] = Vec_IntersectPlane(plane_p, plane_n, *inside[0], *outside[0], t);
        out2.p[0] = *inside[1]; out2.p[1] = out1.p[2];
        out2.p[2] = Vec_IntersectPlane(plane_p, plane_n, *inside[1], *outside[0], t);
        return 2;
    }
    return 0;
}

// ============== 3D Engine Class ==============
class Engine3D {
public:
    SDLApp app;
    std::vector<triangle> cubeTris;  // Cube triangles
    mat4x4 matProj;                  // Projection matrix

    // Camera parameters
    vec3d camera = {0, 0, 0};
    vec3d lookDir;
    float camRotX = 0, camRotY = 0;

    // Object parameters
    float rotX = 0, rotZ = 0;
    bool autoRotate = true;
    float rotSpeed = 1.0f;
    float objDist = 5.0f;

    // Light direction
    vec3d light = {0, 0, -1};

    // Display options
    bool showWireframe = true;
    bool showFilled = true;
    Color fillColor = Color::Blue();

    // Projection parameters
    float fov = 90.0f, zNear = 0.1f, zFar = 1000.0f;

    void CreateCube() {
        cubeTris.clear();
        auto addTri = [&](vec3d a, vec3d b, vec3d c) {
            triangle t; t.p[0] = a; t.p[1] = b; t.p[2] = c;
            cubeTris.push_back(t);
        };
        // 6 faces × 2 triangles = 12 triangles
        // SOUTH
        addTri({0,0,0,1}, {0,1,0,1}, {1,1,0,1});
        addTri({0,0,0,1}, {1,1,0,1}, {1,0,0,1});
        // EAST
        addTri({1,0,0,1}, {1,1,0,1}, {1,1,1,1});
        addTri({1,0,0,1}, {1,1,1,1}, {1,0,1,1});
        // NORTH
        addTri({1,0,1,1}, {1,1,1,1}, {0,1,1,1});
        addTri({1,0,1,1}, {0,1,1,1}, {0,0,1,1});
        // WEST
        addTri({0,0,1,1}, {0,1,1,1}, {0,1,0,1});
        addTri({0,0,1,1}, {0,1,0,1}, {0,0,0,1});
        // TOP
        addTri({0,1,0,1}, {0,1,1,1}, {1,1,1,1});
        addTri({0,1,0,1}, {1,1,1,1}, {1,1,0,1});
        // BOTTOM
        addTri({1,0,1,1}, {0,0,1,1}, {0,0,0,1});
        addTri({1,0,1,1}, {0,0,0,1}, {1,0,0,1});
    }

    bool Init() {
        if (!app.Init("3D Demo - Understanding 3D to 2D Projection", 1024, 960)) return false;
        CreateCube();
        matProj = Mat_Proj(fov, (float)app.screenHeight / app.screenWidth, zNear, zFar);
        return true;
    }

    void RenderUI() {
        ImGui::Begin("Control Panel");

        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Cam X", &camera.x, -10, 10);
            ImGui::SliderFloat("Cam Y", &camera.y, -10, 10);
            ImGui::SliderFloat("Cam Z", &camera.z, -10, 10);
            ImGui::SliderFloat("Look X", &camRotX, -3.14f, 3.14f);
            ImGui::SliderFloat("Look Y", &camRotY, -3.14f, 3.14f);
            if (ImGui::Button("Reset Camera")) { camera = {0,0,0}; camRotX = camRotY = 0; }
        }

        if (ImGui::CollapsingHeader("Object", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Auto Rotate", &autoRotate);
            ImGui::SliderFloat("Speed", &rotSpeed, 0.1f, 5.0f);
            ImGui::SliderFloat("Rot X", &rotX, -3.14f, 3.14f);
            ImGui::SliderFloat("Rot Z", &rotZ, -3.14f, 3.14f);
            ImGui::SliderFloat("Distance", &objDist, 2.0f, 20.0f);
        }

        if (ImGui::CollapsingHeader("Light")) {
            ImGui::SliderFloat("Light X", &light.x, -1, 1);
            ImGui::SliderFloat("Light Y", &light.y, -1, 1);
            ImGui::SliderFloat("Light Z", &light.z, -1, 1);
        }

        if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Wireframe", &showWireframe);
            ImGui::Checkbox("Filled", &showFilled);
            float c[3] = {fillColor.r/255.f, fillColor.g/255.f, fillColor.b/255.f};
            if (ImGui::ColorEdit3("Color", c)) {
                fillColor = Color((Uint8)(c[0]*255), (Uint8)(c[1]*255), (Uint8)(c[2]*255));
            }
        }

        if (ImGui::CollapsingHeader("Projection")) {
            bool changed = ImGui::SliderFloat("FOV", &fov, 30, 120);
            changed |= ImGui::SliderFloat("Near", &zNear, 0.01f, 1.0f);
            changed |= ImGui::SliderFloat("Far", &zFar, 100, 2000);
            if (changed) matProj = Mat_Proj(fov, (float)app.screenHeight/app.screenWidth, zNear, zFar);
        }

        ImGui::Separator();
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::End();
    }

    void Update(float dt) {
        if (autoRotate) { rotX += rotSpeed * dt; rotZ += rotSpeed * 0.5f * dt; }

        vec3d fwd = Vec_Mul(lookDir, 8.0f * dt);
        if (app.IsKeyDown(SDL_SCANCODE_W) || app.IsKeyDown(SDL_SCANCODE_UP)) camera = Vec_Add(camera, fwd);
        if (app.IsKeyDown(SDL_SCANCODE_S) || app.IsKeyDown(SDL_SCANCODE_DOWN)) camera = Vec_Sub(camera, fwd);
        if (app.IsKeyDown(SDL_SCANCODE_A)) camRotY += dt;
        if (app.IsKeyDown(SDL_SCANCODE_D)) camRotY -= dt;
    }

    void Render() {
        vec3d up = {0, 1, 0}, target = {0, 0, 1}, offset = {1, 1, 0};

        // Step 1: Build Camera Matrix (View Matrix)
        mat4x4 camRot = Mat_Mul(Mat_RotX(camRotX), Mat_RotY(camRotY));
        lookDir = Mat_MulVec(camRot, target);
        target = Vec_Add(camera, lookDir);
        mat4x4 matView = Mat_QuickInv(Mat_PointAt(camera, target, up));

        // Step 2: Build World Matrix (Model Transform)
        mat4x4 matWorld = Mat_Mul(Mat_Mul(Mat_RotZ(rotZ), Mat_RotX(rotX)), Mat_Trans(0, 0, objDist));

        std::vector<triangle> trisToRaster;

        for (auto& tri : cubeTris) {
            triangle triTrans, triView, triProj;

            // Step 3: Apply World Transform
            for (int i = 0; i < 3; i++) triTrans.p[i] = Mat_MulVec(matWorld, tri.p[i]);

            // Step 4: Calculate Normal (for lighting and culling)
            vec3d n = Vec_Norm(Vec_Cross(Vec_Sub(triTrans.p[1], triTrans.p[0]),
                                          Vec_Sub(triTrans.p[2], triTrans.p[0])));

            // Step 5: Backface Culling
            if (Vec_Dot(n, Vec_Sub(triTrans.p[0], camera)) < 0) {
                // Step 6: Calculate Lighting
                float dp = std::max(0.1f, Vec_Dot(Vec_Norm(light), n));
                triProj.color = fillColor * dp;

                // Step 7: Apply View Transform (World -> Camera Space)
                for (int i = 0; i < 3; i++) triView.p[i] = Mat_MulVec(matView, triTrans.p[i]);
                triView.color = triProj.color;

                // Step 8: Clip against near plane
                triangle clipped[2];
                int nClip = ClipTriangle({0,0,0.1f}, {0,0,1}, triView, clipped[0], clipped[1]);

                for (int c = 0; c < nClip; c++) {
                    // Step 9: Apply Projection Transform (Camera -> NDC)
                    for (int i = 0; i < 3; i++) {
                        triProj.p[i] = Mat_MulVec(matProj, clipped[c].p[i]);
                        // Perspective division: divide by w to get normalized coords
                        triProj.p[i] = Vec_Div(triProj.p[i], triProj.p[i].w);
                        // Invert X and Y (screen coordinate convention)
                        triProj.p[i].x *= -1; triProj.p[i].y *= -1;
                        // Step 10: Transform to Screen Space
                        triProj.p[i] = Vec_Add(triProj.p[i], offset);
                        triProj.p[i].x *= 0.5f * app.screenWidth;
                        triProj.p[i].y *= 0.5f * app.screenHeight;
                    }
                    triProj.color = clipped[c].color;
                    trisToRaster.push_back(triProj);
                }
            }
        }

        // Step 11: Clip against screen edges and draw
        for (auto& tri : trisToRaster) {
            triangle clipped[2];
            std::list<triangle> triList; triList.push_back(tri);
            int nNew = 1;

            for (int p = 0; p < 4; p++) {
                while (nNew > 0) {
                    triangle t = triList.front(); triList.pop_front(); nNew--;
                    int nAdd = 0;
                    switch (p) {
                        case 0: nAdd = ClipTriangle({0,0,0}, {0,1,0}, t, clipped[0], clipped[1]); break;
                        case 1: nAdd = ClipTriangle({0,(float)app.screenHeight-1,0}, {0,-1,0}, t, clipped[0], clipped[1]); break;
                        case 2: nAdd = ClipTriangle({0,0,0}, {1,0,0}, t, clipped[0], clipped[1]); break;
                        case 3: nAdd = ClipTriangle({(float)app.screenWidth-1,0,0}, {-1,0,0}, t, clipped[0], clipped[1]); break;
                    }
                    for (int w = 0; w < nAdd; w++) triList.push_back(clipped[w]);
                }
                nNew = (int)triList.size();
            }

            for (auto& t : triList) {
                if (showFilled)
                    app.FillTriangle((int)t.p[0].x, (int)t.p[0].y, (int)t.p[1].x, (int)t.p[1].y,
                                     (int)t.p[2].x, (int)t.p[2].y, t.color);
                if (showWireframe)
                    app.DrawTriangle((int)t.p[0].x, (int)t.p[0].y, (int)t.p[1].x, (int)t.p[1].y,
                                     (int)t.p[2].x, (int)t.p[2].y, Color::White());
            }
        }
    }

    void Run() {
        while (app.running) {
            app.ProcessEvents();
            app.BeginFrame();
            Update(app.deltaTime);
            Render();
            RenderUI();
            app.EndFrame();
        }
        app.Cleanup();
    }
};

