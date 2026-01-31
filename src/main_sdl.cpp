/*
    3D Graphics Demo - SDL2 + ImGui Version
    Based on OneLoneCoder.com 3D Graphics Tutorial
    Clean version: SDL/ImGui encapsulated in SDLApp.h
*/

#include "SDLApp.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <list>
#include <cmath>
#include <algorithm>

using namespace std;

// ============== 3D Data Structures ==============
struct vec2d { float u = 0, v = 0, w = 1; };
struct vec3d { float x = 0, y = 0, z = 0, w = 1; };
struct mat4x4 { float m[4][4] = { 0 }; };

struct triangle {
    vec3d p[3];
    vec2d t[3];
    Color color;
};

struct mesh {
    vector<triangle> tris;
    bool LoadFromObjFile(const string& filename) {
        ifstream f(filename);
        if (!f.is_open()) return false;
        vector<vec3d> verts;
        string line;
        while (getline(f, line)) {
            if (line.empty()) continue;
            istringstream ss(line);
            char type;
            if (line[0] == 'v' && line[1] == ' ') {
                vec3d v; ss >> type >> v.x >> v.y >> v.z;
                verts.push_back(v);
            }
            if (line[0] == 'f') {
                int a, b, c; ss >> type >> a >> b >> c;
                tris.push_back({ verts[a-1], verts[b-1], verts[c-1] });
            }
        }
        return true;
    }
};

// ============== Vector Math ==============
vec3d Vec_Add(const vec3d& a, const vec3d& b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
vec3d Vec_Sub(const vec3d& a, const vec3d& b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
vec3d Vec_Mul(const vec3d& v, float k) { return {v.x*k, v.y*k, v.z*k}; }
vec3d Vec_Div(const vec3d& v, float k) { return {v.x/k, v.y/k, v.z/k}; }
float Vec_Dot(const vec3d& a, const vec3d& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
vec3d Vec_Cross(const vec3d& a, const vec3d& b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}
vec3d Vec_Norm(const vec3d& v) {
    float len = sqrtf(Vec_Dot(v, v));
    return len > 0 ? Vec_Div(v, len) : v;
}

// ============== Matrix Operations ==============
vec3d Mat_MulVec(const mat4x4& m, const vec3d& v) {
    vec3d o;
    o.x = v.x*m.m[0][0] + v.y*m.m[1][0] + v.z*m.m[2][0] + v.w*m.m[3][0];
    o.y = v.x*m.m[0][1] + v.y*m.m[1][1] + v.z*m.m[2][1] + v.w*m.m[3][1];
    o.z = v.x*m.m[0][2] + v.y*m.m[1][2] + v.z*m.m[2][2] + v.w*m.m[3][2];
    o.w = v.x*m.m[0][3] + v.y*m.m[1][3] + v.z*m.m[2][3] + v.w*m.m[3][3];
    return o;
}

mat4x4 Mat_Identity() {
    mat4x4 m; m.m[0][0]=1; m.m[1][1]=1; m.m[2][2]=1; m.m[3][3]=1; return m;
}

mat4x4 Mat_RotX(float a) {
    mat4x4 m; m.m[0][0]=1; m.m[1][1]=cosf(a); m.m[1][2]=sinf(a);
    m.m[2][1]=-sinf(a); m.m[2][2]=cosf(a); m.m[3][3]=1; return m;
}

mat4x4 Mat_RotY(float a) {
    mat4x4 m; m.m[0][0]=cosf(a); m.m[0][2]=sinf(a); m.m[1][1]=1;
    m.m[2][0]=-sinf(a); m.m[2][2]=cosf(a); m.m[3][3]=1; return m;
}

mat4x4 Mat_RotZ(float a) {
    mat4x4 m; m.m[0][0]=cosf(a); m.m[0][1]=sinf(a);
    m.m[1][0]=-sinf(a); m.m[1][1]=cosf(a); m.m[2][2]=1; m.m[3][3]=1; return m;
}

mat4x4 Mat_Trans(float x, float y, float z) {
    mat4x4 m = Mat_Identity(); m.m[3][0]=x; m.m[3][1]=y; m.m[3][2]=z; return m;
}

mat4x4 Mat_Proj(float fov, float aspect, float znear, float zfar) {
    float f = 1.0f / tanf(fov * 0.5f / 180.0f * 3.14159f);
    mat4x4 m;
    m.m[0][0] = aspect * f; m.m[1][1] = f;
    m.m[2][2] = zfar / (zfar - znear);
    m.m[3][2] = (-zfar * znear) / (zfar - znear);
    m.m[2][3] = 1.0f;
    return m;
}

mat4x4 Mat_Mul(const mat4x4& a, const mat4x4& b) {
    mat4x4 m;
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++)
            m.m[r][c] = a.m[r][0]*b.m[0][c] + a.m[r][1]*b.m[1][c] + 
                        a.m[r][2]*b.m[2][c] + a.m[r][3]*b.m[3][c];
    return m;
}

mat4x4 Mat_PointAt(const vec3d& pos, const vec3d& target, const vec3d& up) {
    vec3d fwd = Vec_Norm(Vec_Sub(target, pos));
    vec3d newUp = Vec_Norm(Vec_Sub(up, Vec_Mul(fwd, Vec_Dot(up, fwd))));
    vec3d right = Vec_Cross(newUp, fwd);
    mat4x4 m;
    m.m[0][0]=right.x; m.m[0][1]=right.y; m.m[0][2]=right.z;
    m.m[1][0]=newUp.x; m.m[1][1]=newUp.y; m.m[1][2]=newUp.z;
    m.m[2][0]=fwd.x;   m.m[2][1]=fwd.y;   m.m[2][2]=fwd.z;
    m.m[3][0]=pos.x;   m.m[3][1]=pos.y;   m.m[3][2]=pos.z; m.m[3][3]=1;
    return m;
}

mat4x4 Mat_QuickInv(const mat4x4& m) {
    mat4x4 r;
    r.m[0][0]=m.m[0][0]; r.m[0][1]=m.m[1][0]; r.m[0][2]=m.m[2][0];
    r.m[1][0]=m.m[0][1]; r.m[1][1]=m.m[1][1]; r.m[1][2]=m.m[2][1];
    r.m[2][0]=m.m[0][2]; r.m[2][1]=m.m[1][2]; r.m[2][2]=m.m[2][2];
    r.m[3][0]=-(m.m[3][0]*r.m[0][0] + m.m[3][1]*r.m[1][0] + m.m[3][2]*r.m[2][0]);
    r.m[3][1]=-(m.m[3][0]*r.m[0][1] + m.m[3][1]*r.m[1][1] + m.m[3][2]*r.m[2][1]);
    r.m[3][2]=-(m.m[3][0]*r.m[0][2] + m.m[3][1]*r.m[1][2] + m.m[3][2]*r.m[2][2]);
    r.m[3][3]=1; return r;
}

// ============== Triangle Clipping ==============
vec3d Vec_IntersectPlane(vec3d& plane_p, vec3d& plane_n, vec3d& start, vec3d& end, float& t) {
    plane_n = Vec_Norm(plane_n);
    float pd = -Vec_Dot(plane_n, plane_p);
    float ad = Vec_Dot(start, plane_n);
    float bd = Vec_Dot(end, plane_n);
    t = (-pd - ad) / (bd - ad);
    return Vec_Add(start, Vec_Mul(Vec_Sub(end, start), t));
}

int ClipTriangle(vec3d plane_p, vec3d plane_n, triangle& in, triangle& out1, triangle& out2) {
    plane_n = Vec_Norm(plane_n);
    auto dist = [&](vec3d& p) { return Vec_Dot(plane_n, p) - Vec_Dot(plane_n, plane_p); };

    vec3d* inside[3]; int nIn = 0;
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
    mesh meshCube;
    mat4x4 matProj;

    // Camera (controllable via ImGui)
    vec3d camera = {0, 0, 0};
    vec3d lookDir;
    float camRotX = 0, camRotY = 0, camRotZ = 0;

    // Object rotation (controllable via ImGui)
    float rotX = 0, rotZ = 0;
    bool autoRotate = true;
    float rotSpeed = 1.0f;
    float objDist = 5.0f;

    // Light (controllable via ImGui)
    vec3d light = {0, 0, -1};

    // Display options
    bool showWireframe = true;
    bool showFilled = true;
    Color fillColor = Color::Blue();

    // Projection
    float fov = 90.0f, zNear = 0.1f, zFar = 1000.0f;

    void CreateCube() {
        meshCube.tris.clear();
        // Helper to add triangle
        auto addTri = [&](vec3d a, vec3d b, vec3d c) {
            triangle t;
            t.p[0] = a; t.p[1] = b; t.p[2] = c;
            meshCube.tris.push_back(t);
        };
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
        if (!app.Init("3D Demo - SDL2 + ImGui", 1024, 960)) return false;
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
            if (ImGui::Button("Reset Camera")) { camera = {0,0,0}; camRotX = camRotY = camRotZ = 0; }
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

        // Camera matrix
        mat4x4 camRot = Mat_Mul(Mat_RotX(camRotX), Mat_RotY(camRotY));
        lookDir = Mat_MulVec(camRot, target);
        target = Vec_Add(camera, lookDir);
        mat4x4 matView = Mat_QuickInv(Mat_PointAt(camera, target, up));

        // World matrix
        mat4x4 matWorld = Mat_Mul(Mat_Mul(Mat_RotZ(rotZ), Mat_RotX(rotX)), Mat_Trans(0, 0, objDist));

        vector<triangle> trisToRaster;

        for (auto& tri : meshCube.tris) {
            triangle triTrans, triView, triProj;

            // World transform
            for (int i = 0; i < 3; i++) triTrans.p[i] = Mat_MulVec(matWorld, tri.p[i]);

            // Calculate normal
            vec3d n = Vec_Norm(Vec_Cross(Vec_Sub(triTrans.p[1], triTrans.p[0]),
                                          Vec_Sub(triTrans.p[2], triTrans.p[0])));

            // Backface culling
            if (Vec_Dot(n, Vec_Sub(triTrans.p[0], camera)) < 0) {
                // Lighting
                float dp = max(0.1f, Vec_Dot(Vec_Norm(light), n));
                triProj.color = fillColor * dp;

                // View transform
                for (int i = 0; i < 3; i++) triView.p[i] = Mat_MulVec(matView, triTrans.p[i]);
                triView.color = triProj.color;

                // Clip against near plane
                triangle clipped[2];
                int nClip = ClipTriangle({0,0,0.1f}, {0,0,1}, triView, clipped[0], clipped[1]);

                for (int c = 0; c < nClip; c++) {
                    // Project
                    for (int i = 0; i < 3; i++) {
                        triProj.p[i] = Mat_MulVec(matProj, clipped[c].p[i]);
                        triProj.p[i] = Vec_Div(triProj.p[i], triProj.p[i].w);
                        triProj.p[i].x *= -1; triProj.p[i].y *= -1;
                        triProj.p[i] = Vec_Add(triProj.p[i], offset);
                        triProj.p[i].x *= 0.5f * app.screenWidth;
                        triProj.p[i].y *= 0.5f * app.screenHeight;
                    }
                    triProj.color = clipped[c].color;
                    trisToRaster.push_back(triProj);
                }
            }
        }

        // Clip against screen edges and draw
        for (auto& tri : trisToRaster) {
            triangle clipped[2];
            list<triangle> triList; triList.push_back(tri);
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
            Render();      // Render 3D scene first
            RenderUI();    // Then render ImGui on top
            app.EndFrame();
        }
        app.Cleanup();
    }
};

// ============== Main ==============
int main(int argc, char* argv[]) {
    Engine3D engine;
    if (engine.Init()) engine.Run();
    return 0;
}