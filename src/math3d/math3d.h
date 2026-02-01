/*
    math3d.h - 3D Mathematics Library
    Core data structures and operations for 3D graphics
    
    Contains:
    - vec3d: 3D vector with homogeneous coordinate w
    - mat4x4: 4x4 transformation matrix
    - Vector operations: add, sub, mul, div, dot, cross, normalize
    - Matrix operations: multiply, identity, rotation, translation, projection
*/

#pragma once
#include <cmath>

// ============== 3D Vector ==============
struct vec3d {
    float x = 0, y = 0, z = 0, w = 1;
};

// ============== 4x4 Matrix ==============
struct mat4x4 {
    float m[4][4] = { 0 };
};

// ============== Vector Operations ==============

// Vector addition: a + b
inline vec3d Vec_Add(const vec3d& a, const vec3d& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

// Vector subtraction: a - b
inline vec3d Vec_Sub(const vec3d& a, const vec3d& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

// Scalar multiplication: v * k
inline vec3d Vec_Mul(const vec3d& v, float k) {
    return {v.x * k, v.y * k, v.z * k};
}

// Scalar division: v / k
inline vec3d Vec_Div(const vec3d& v, float k) {
    return {v.x / k, v.y / k, v.z / k};
}

// Dot product: a · b
inline float Vec_Dot(const vec3d& a, const vec3d& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Cross product: a × b
inline vec3d Vec_Cross(const vec3d& a, const vec3d& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

// Normalize vector to unit length
inline vec3d Vec_Norm(const vec3d& v) {
    float len = sqrtf(Vec_Dot(v, v));
    return len > 0 ? Vec_Div(v, len) : v;
}

// ============== Matrix Operations ==============

// Matrix × Vector multiplication
inline vec3d Mat_MulVec(const mat4x4& m, const vec3d& v) {
    vec3d o;
    o.x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + v.w * m.m[3][0];
    o.y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + v.w * m.m[3][1];
    o.z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + v.w * m.m[3][2];
    o.w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + v.w * m.m[3][3];
    return o;
}

// Identity matrix
inline mat4x4 Mat_Identity() {
    mat4x4 m;
    m.m[0][0] = 1; m.m[1][1] = 1; m.m[2][2] = 1; m.m[3][3] = 1;
    return m;
}

// Rotation around X axis
inline mat4x4 Mat_RotX(float a) {
    mat4x4 m;
    m.m[0][0] = 1;
    m.m[1][1] = cosf(a);  m.m[1][2] = sinf(a);
    m.m[2][1] = -sinf(a); m.m[2][2] = cosf(a);
    m.m[3][3] = 1;
    return m;
}

// Rotation around Y axis
inline mat4x4 Mat_RotY(float a) {
    mat4x4 m;
    m.m[0][0] = cosf(a);  m.m[0][2] = sinf(a);
    m.m[1][1] = 1;
    m.m[2][0] = -sinf(a); m.m[2][2] = cosf(a);
    m.m[3][3] = 1;
    return m;
}

// Rotation around Z axis
inline mat4x4 Mat_RotZ(float a) {
    mat4x4 m;
    m.m[0][0] = cosf(a);  m.m[0][1] = sinf(a);
    m.m[1][0] = -sinf(a); m.m[1][1] = cosf(a);
    m.m[2][2] = 1;
    m.m[3][3] = 1;
    return m;
}

// Translation matrix
inline mat4x4 Mat_Trans(float x, float y, float z) {
    mat4x4 m = Mat_Identity();
    m.m[3][0] = x; m.m[3][1] = y; m.m[3][2] = z;
    return m;
}

// Perspective projection matrix
inline mat4x4 Mat_Proj(float fov, float aspect, float znear, float zfar) {
    float f = 1.0f / tanf(fov * 0.5f / 180.0f * 3.14159f);
    mat4x4 m;
    m.m[0][0] = aspect * f;
    m.m[1][1] = f;
    m.m[2][2] = zfar / (zfar - znear);
    m.m[3][2] = (-zfar * znear) / (zfar - znear);
    m.m[2][3] = 1.0f;
    return m;
}

// Matrix × Matrix multiplication
inline mat4x4 Mat_Mul(const mat4x4& a, const mat4x4& b) {
    mat4x4 m;
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++)
            m.m[r][c] = a.m[r][0] * b.m[0][c] + a.m[r][1] * b.m[1][c] +
                        a.m[r][2] * b.m[2][c] + a.m[r][3] * b.m[3][c];
    return m;
}

// "Point At" matrix - creates view orientation
inline mat4x4 Mat_PointAt(const vec3d& pos, const vec3d& target, const vec3d& up) {
    vec3d fwd = Vec_Norm(Vec_Sub(target, pos));
    vec3d newUp = Vec_Norm(Vec_Sub(up, Vec_Mul(fwd, Vec_Dot(up, fwd))));
    vec3d right = Vec_Cross(newUp, fwd);
    mat4x4 m;
    m.m[0][0] = right.x; m.m[0][1] = right.y; m.m[0][2] = right.z;
    m.m[1][0] = newUp.x; m.m[1][1] = newUp.y; m.m[1][2] = newUp.z;
    m.m[2][0] = fwd.x;   m.m[2][1] = fwd.y;   m.m[2][2] = fwd.z;
    m.m[3][0] = pos.x;   m.m[3][1] = pos.y;   m.m[3][2] = pos.z;
    m.m[3][3] = 1;
    return m;
}

// Quick inverse for rotation/translation matrices
inline mat4x4 Mat_QuickInv(const mat4x4& m) {
    mat4x4 r;
    r.m[0][0] = m.m[0][0]; r.m[0][1] = m.m[1][0]; r.m[0][2] = m.m[2][0];
    r.m[1][0] = m.m[0][1]; r.m[1][1] = m.m[1][1]; r.m[1][2] = m.m[2][1];
    r.m[2][0] = m.m[0][2]; r.m[2][1] = m.m[1][2]; r.m[2][2] = m.m[2][2];
    r.m[3][0] = -(m.m[3][0] * r.m[0][0] + m.m[3][1] * r.m[1][0] + m.m[3][2] * r.m[2][0]);
    r.m[3][1] = -(m.m[3][0] * r.m[0][1] + m.m[3][1] * r.m[1][1] + m.m[3][2] * r.m[2][1]);
    r.m[3][2] = -(m.m[3][0] * r.m[0][2] + m.m[3][1] * r.m[1][2] + m.m[3][2] * r.m[2][2]);
    r.m[3][3] = 1;
    return r;
}

