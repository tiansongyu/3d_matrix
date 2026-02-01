# 3D 到 2D 屏幕投影原理详解

## 目录
1. [概述](#1-概述)
2. [坐标系统](#2-坐标系统)
3. [变换流水线](#3-变换流水线)
4. [向量与矩阵基础](#4-向量与矩阵基础)
5. [模型变换](#5-模型变换世界变换)
6. [视图变换](#6-视图变换)
7. [透视投影](#7-透视投影核心)
8. [透视除法与NDC](#8-透视除法与ndc)
9. [屏幕映射](#9-屏幕映射)
10. [完整代码对照](#10-完整代码对照)
11. [CS2 视图矩阵深入解析](#11-cs2-视图矩阵深入解析)
12. [ViewMatrix 实战应用](#12-viewmatrix-实战应用)

---

## 1. 概述

3D渲染的核心问题：**如何将三维空间中的点 (x, y, z) 映射到二维屏幕上的像素 (px, py)?**

```
    3D世界                              2D屏幕
     ┌─────────┐                      ┌──────────┐
    /         /│                      │          │
   /    P    / │    ─────────────>    │    P'    │
  ┌─────────┐  │      投影变换        │    ●     │
  │         │  │                      │          │
  │         │ /                       └──────────┘
  └─────────┘/                        
```

这个过程经历多个变换阶段，我们称之为**图形流水线 (Graphics Pipeline)**。

---

## 2. 坐标系统

### 2.1 右手坐标系 vs 左手坐标系

```
    右手坐标系               左手坐标系
        Y                      Y
        ↑                      ↑
        │                      │
        │                      │
        ●───→ X                ●───→ X
       /                        \
      ↙                          ↘
     Z (指向观察者)              Z (远离观察者)
```

本项目使用**左手坐标系**（与Direct3D相同）：
- X 轴：向右
- Y 轴：向上
- Z 轴：向屏幕内（深度方向）

### 2.2 齐次坐标

在3D图形学中，我们使用4D向量表示3D点：

```
      ┌   ┐
      │ x │
  P = │ y │    其中 w = 1 表示点，w = 0 表示方向向量
      │ z │
      │ w │
      └   ┘
```

**为什么需要齐次坐标？**
- 可以用矩阵乘法统一表示平移、旋转、缩放
- 透视投影需要用到 w 分量进行透视除法

---

## 3. 变换流水线

3D点到2D屏幕的完整变换流程：

```
┌───────────┐    ┌───────────┐    ┌───────────┐    ┌───────────┐    ┌───────────┐
│  模型空间  │───>│  世界空间  │───>│  相机空间  │───>│   裁剪空间  │───>│   屏幕空间  │
│ (Local)   │    │ (World)   │    │ (View)    │    │ (Clip/NDC)│    │ (Screen)  │
└───────────┘    └───────────┘    └───────────┘    └───────────┘    └───────────┘
      │               │                 │                │                │
      │    模型矩阵    │     视图矩阵     │    投影矩阵     │   视口变换     │
      └───────────────┴─────────────────┴────────────────┴────────────────┘
                              M_model × M_view × M_proj × M_viewport
```

**每个变换的作用：**

| 变换 | 输入空间 | 输出空间 | 作用 |
|------|---------|---------|------|
| 模型变换 | 模型空间 | 世界空间 | 物体的位置、旋转、缩放 |
| 视图变换 | 世界空间 | 相机空间 | 以摄像机为原点重建坐标 |
| 投影变换 | 相机空间 | 裁剪空间 | 实现透视效果 |
| 视口变换 | NDC空间 | 屏幕空间 | 映射到像素坐标 |

---

## 4. 向量与矩阵基础

### 4.1 向量运算

**向量加法：**
```
A + B = (Ax + Bx, Ay + By, Az + Bz)
```

**向量点积（内积）：**
```
A · B = Ax×Bx + Ay×By + Az×Bz = |A| × |B| × cos(θ)
```
- 结果是标量
- 用于计算夹角、投影

**向量叉积（外积）：**
```
A × B = (Ay×Bz - Az×By, Az×Bx - Ax×Bz, Ax×By - Ay×Bx)
```
- 结果是向量，垂直于A和B
- 用于计算法向量

### 4.2 矩阵与向量相乘

4×4矩阵乘以4×1向量：

```
┌                    ┐   ┌   ┐     ┌                                            ┐
│ m00  m10  m20  m30 │   │ x │     │ x×m00 + y×m10 + z×m20 + w×m30 │
│ m01  m11  m21  m31 │ × │ y │  =  │ x×m01 + y×m11 + z×m21 + w×m31 │
│ m02  m12  m22  m32 │   │ z │     │ x×m02 + y×m12 + z×m22 + w×m32 │
│ m03  m13  m23  m33 │   │ w │     │ x×m03 + y×m13 + z×m23 + w×m33 │
└                    ┘   └   ┘     └                                            ┘
```

**代码实现：**
```cpp
vec3d Mat_MulVec(const mat4x4& m, const vec3d& v) {
    vec3d o;
    o.x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + v.w * m.m[3][0];
    o.y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + v.w * m.m[3][1];
    o.z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + v.w * m.m[3][2];
    o.w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + v.w * m.m[3][3];
    return o;
}
```

---

## 5. 模型变换（世界变换）

模型变换将物体从**模型空间（局部坐标）** 变换到**世界空间**。

### 5.1 平移矩阵

将点移动 (tx, ty, tz)：

```
┌                    ┐   ┌   ┐     ┌        ┐
│  1   0   0   0  │   │ x │     │ x + tx │
│  0   1   0   0  │ × │ y │  =  │ y + ty │
│  0   0   1   0  │   │ z │     │ z + tz │
│  tx  ty  tz  1  │   │ 1 │     │   1    │
└                    ┘   └   ┘     └        ┘
```

**代码实现：**
```cpp
mat4x4 Mat_Trans(float x, float y, float z) {
    mat4x4 m = Mat_Identity();
    m.m[3][0] = x; m.m[3][1] = y; m.m[3][2] = z;
    return m;
}
```

### 5.2 旋转矩阵

**绕X轴旋转θ角：**

```
         Y                      Y'
         ↑                      ↑
         │    旋转θ             │ ╲
         ●───→ Z    ───────>    ●───→ Z'
        /                      /
       X (不变)               X (不变)

旋转矩阵：
┌                          ┐
│  1      0       0     0  │
│  0    cos(θ)  sin(θ)  0  │
│  0   -sin(θ)  cos(θ)  0  │
│  0      0       0     1  │
└                          ┘
```

**绕Y轴旋转θ角：**
```
┌                          ┐
│  cos(θ)   0   sin(θ)  0  │
│    0      1     0     0  │
│ -sin(θ)   0   cos(θ)  0  │
│    0      0     0     1  │
└                          ┘
```

**绕Z轴旋转θ角：**
```
┌                          ┐
│  cos(θ)  sin(θ)   0   0  │
│ -sin(θ)  cos(θ)   0   0  │
│    0       0      1   0  │
│    0       0      0   1  │
└                          ┘
```

### 5.3 组合变换

多个变换可以通过矩阵相乘组合：

```
M_world = M_rotation × M_translation
P_world = M_world × P_local
```

**代码示例：**
```cpp
// 先绕Z轴旋转，再绕X轴旋转，最后平移到距离相机5单位的位置
mat4x4 matWorld = Mat_Mul(Mat_Mul(Mat_RotZ(rotZ), Mat_RotX(rotX)), Mat_Trans(0, 0, 5));
```

---

## 6. 视图变换

视图变换将世界坐标转换为**相机坐标（观察者视角）**。

### 6.1 概念理解

想象相机在世界中的位置和朝向：

```
         世界空间                          相机空间
           ↑ Y                              ↑ Y
           │                                │
    物体 ● │                                │ ● 物体（相对相机位置）
           │      ──────────────────>       │
           ●───→ X                          ●───→ X
          /  相机                          / 相机在原点
         Z                                Z
```

**核心思想**：不是移动相机，而是移动整个世界，使相机位于原点且面向Z轴正方向。

### 6.2 Look-At 矩阵构建

构建相机坐标系需要三个向量：
- **前向向量 (Forward)**：相机看向的方向
- **上向向量 (Up)**：相机的"上"方向
- **右向向量 (Right)**：由前向和上向叉积得到

```
            Up (newUp)
              ↑
              │
              │
    Right ←───●───→ Forward
              │
              │
              ↓
```

**代码实现：**
```cpp
mat4x4 Mat_PointAt(const vec3d& pos, const vec3d& target, const vec3d& up) {
    // 1. 计算前向向量
    vec3d fwd = Vec_Norm(Vec_Sub(target, pos));

    // 2. 重新计算正交的上向向量（消除倾斜）
    vec3d newUp = Vec_Norm(Vec_Sub(up, Vec_Mul(fwd, Vec_Dot(up, fwd))));

    // 3. 计算右向向量
    vec3d right = Vec_Cross(newUp, fwd);

    // 4. 构建变换矩阵
    mat4x4 m;
    m.m[0][0] = right.x; m.m[0][1] = right.y; m.m[0][2] = right.z;
    m.m[1][0] = newUp.x; m.m[1][1] = newUp.y; m.m[1][2] = newUp.z;
    m.m[2][0] = fwd.x;   m.m[2][1] = fwd.y;   m.m[2][2] = fwd.z;
    m.m[3][0] = pos.x;   m.m[3][1] = pos.y;   m.m[3][2] = pos.z;
    m.m[3][3] = 1;
    return m;
}
```

### 6.3 视图矩阵（逆矩阵）

视图矩阵是 PointAt 矩阵的**逆矩阵**：

```cpp
mat4x4 matView = Mat_QuickInv(Mat_PointAt(camera, target, up));
```

快速求逆（仅适用于旋转+平移矩阵）：
- 旋转部分：转置即可
- 平移部分：需要重新计算

---

## 7. 透视投影（核心！）

这是3D到2D最关键的一步！

### 7.1 透视现象

日常生活中的透视：
- 远处的物体看起来更小
- 平行的铁轨在远处汇聚成一点

```
    近处                     远处
    ┌───┐                    ┌─┐
    │   │   ──────────────>  │ │
    │   │                    └─┘
    └───┘                    更小！

    火车铁轨俯视图：

    眼睛 ●
          \╲
           ╲ ╲
            ╲  ╲
             ╲   ╲
    ══════════════════════  铁轨
    ══════════════════════  铁轨
```

### 7.2 投影原理：相似三角形

透视投影的核心是**相似三角形**：

```
侧视图：

         屏幕                    3D点
           │                      ●
           │                    ╱
           │                  ╱
           │ y'             ╱ y
           ├──────        ╱
           │      ╲     ╱
      眼睛 ●────────────────────────────
                d        z

    根据相似三角形:

        y'     y              y × d
       ─── = ───    ==>  y' = ─────
        d     z                 z
```

**核心公式**：

$$
x_{screen} = \frac{x \times d}{z}
$$

$$
y_{screen} = \frac{y \times d}{z}
$$

其中 `d` 是屏幕到眼睛的距离（与视场角FOV相关）。

### 7.3 视锥体 (Frustum)

透视投影定义了一个**视锥体**，只有在这个锥体内的物体才会被渲染：

```
俯视图：
                                    ╱
                        far plane  ╱
           ┌──────────────────────┐
          ╱                        ╲
         ╱                          ╲
        ╱    可视区域 (Frustum)      ╲
       ╱                              ╲
      ╱                                ╲
     ├───────────────────────────────────
    眼睛      near plane
      ╲                                ╱
       ╲                              ╱
        ╲                            ╱
         ╲                          ╱
          ╲                        ╱
           └──────────────────────┘

侧视图：
              ╱╲
             ╱  ╲
            ╱    ╲
           ╱      ╲
          ╱        ╲
    眼睛 ●          ╲ far
         ╲    FOV   ╱
          ╲   ↕    ╱
           ╲      ╱
            ╲    ╱
             ╲  ╱
              ╲╱ near
```

视锥体参数：
- **FOV (Field of View)**：视场角，通常60°-90°
- **Near Plane**：近裁剪面，太近的物体被裁掉
- **Far Plane**：远裁剪面，太远的物体被裁掉
- **Aspect Ratio**：宽高比

### 7.4 透视投影矩阵

将视锥体映射到归一化设备坐标 (NDC) 的立方体 [-1, 1]：

```
视锥体                         NDC立方体
    ╱╲                         ┌───────┐
   ╱  ╲                        │       │
  ╱    ╲      ──────────>      │       │
 ╱      ╲                      │       │
╱────────╲                     └───────┘
                              (-1,-1) to (1,1)
```

**投影矩阵：**

```
设 f = 1 / tan(FOV/2)  （FOV为弧度）

┌                                      ┐
│  f×aspect    0         0         0   │
│     0        f         0         0   │
│     0        0    zf/(zf-zn)     1   │
│     0        0   -zf×zn/(zf-zn)  0   │
└                                      ┘

其中：
- aspect = height / width
- zn = near plane
- zf = far plane
```

**代码实现：**
```cpp
mat4x4 Mat_Proj(float fov, float aspect, float znear, float zfar) {
    float f = 1.0f / tanf(fov * 0.5f / 180.0f * 3.14159f);  // FOV转弧度
    mat4x4 m;
    m.m[0][0] = aspect * f;                        // X缩放
    m.m[1][1] = f;                                 // Y缩放
    m.m[2][2] = zfar / (zfar - znear);             // Z映射
    m.m[3][2] = (-zfar * znear) / (zfar - znear);  // Z偏移
    m.m[2][3] = 1.0f;                              // 将z存入w（用于透视除法）
    return m;
}
```

### 7.5 为什么 w 分量很重要？

注意矩阵中 `m[2][3] = 1`，这意味着：

```
经过投影矩阵后：
    w_new = z_old × 1 + w_old × 0 = z_old

即新的w分量等于原来的z坐标！
```

这正是后面"透视除法"需要的。

---

## 8. 透视除法与NDC

### 8.1 透视除法

投影矩阵变换后，坐标还不是最终的2D坐标，需要进行**透视除法**：

```
              ┌      ┐
              │ x_p  │       x_p / w_p
  投影后 P' = │ y_p  │  ==>  y_p / w_p   （除以w）
              │ z_p  │       z_p / w_p
              │ w_p  │
              └      ┘

  而 w_p = z_original，所以：

  x_ndc = x_p / z    （远处的物体x值变小）
  y_ndc = y_p / z    （远处的物体y值变小）
```

**这就是透视效果的数学本质！远处物体的z值大，除以z后坐标变小！**

**代码实现：**
```cpp
// 应用投影矩阵
triProj.p[i] = Mat_MulVec(matProj, point);

// 透视除法：除以w（此时w=原始z）
triProj.p[i] = Vec_Div(triProj.p[i], triProj.p[i].w);
```

### 8.2 归一化设备坐标 (NDC)

透视除法后得到 **归一化设备坐标 (Normalized Device Coordinates)**：

```
                 y
                 ↑
          ┌──────┼──────┐
          │      │      │
          │  -1  │  +1  │
     ─────┼──────●──────┼─────→ x
          │      │      │
          │      │      │
          └──────┼──────┘
               -1

  NDC范围：x ∈ [-1, 1], y ∈ [-1, 1]
```

---

## 9. 屏幕映射

### 9.1 从NDC到屏幕像素

NDC坐标范围是 [-1, 1]，屏幕坐标范围是 [0, Width] 和 [0, Height]：

```
    NDC空间                          屏幕空间
       y ↑                           (0,0)┌────────────┐
         │                                │            │
    +1 ──┼── +1     ──────────>           │            │
         │                                │            │
    -1 ──┼── -1                           │            │
         ↓                                └────────────┘(W,H)
```

**转换公式**：

$$
x_{screen} = (x_{ndc} + 1) \times 0.5 \times Width
$$

$$
y_{screen} = (y_{ndc} + 1) \times 0.5 \times Height
$$

### 9.2 坐标翻转

屏幕Y轴通常是向下为正，而3D坐标系Y轴向上为正，需要翻转：

```cpp
// 翻转X和Y（取决于坐标系约定）
triProj.p[i].x *= -1;
triProj.p[i].y *= -1;

// 从NDC [-1,1] 映射到屏幕 [0, Width/Height]
triProj.p[i] = Vec_Add(triProj.p[i], {1, 1, 0});  // 移到 [0, 2]
triProj.p[i].x *= 0.5f * screenWidth;             // 缩放到 [0, Width]
triProj.p[i].y *= 0.5f * screenHeight;            // 缩放到 [0, Height]
```

---

## 10. 完整代码对照

### 10.1 完整变换流程

```cpp
void Render() {
    // ============ 步骤 1: 构建视图矩阵 ============
    vec3d up = {0, 1, 0}, target = {0, 0, 1};
    mat4x4 camRot = Mat_Mul(Mat_RotX(camRotX), Mat_RotY(camRotY));
    lookDir = Mat_MulVec(camRot, target);
    target = Vec_Add(camera, lookDir);
    mat4x4 matView = Mat_QuickInv(Mat_PointAt(camera, target, up));

    // ============ 步骤 2: 构建世界矩阵 ============
    mat4x4 matWorld = Mat_Mul(
        Mat_Mul(Mat_RotZ(rotZ), Mat_RotX(rotX)),
        Mat_Trans(0, 0, objDist)
    );

    for (auto& tri : cubeTris) {
        triangle triTrans, triView, triProj;

        // ============ 步骤 3: 模型空间 → 世界空间 ============
        for (int i = 0; i < 3; i++)
            triTrans.p[i] = Mat_MulVec(matWorld, tri.p[i]);

        // ============ 步骤 4: 背面剔除 ============
        vec3d normal = Vec_Norm(Vec_Cross(
            Vec_Sub(triTrans.p[1], triTrans.p[0]),
            Vec_Sub(triTrans.p[2], triTrans.p[0])
        ));

        // 只渲染朝向相机的三角形
        if (Vec_Dot(normal, Vec_Sub(triTrans.p[0], camera)) < 0) {

            // ============ 步骤 5: 世界空间 → 相机空间 ============
            for (int i = 0; i < 3; i++)
                triView.p[i] = Mat_MulVec(matView, triTrans.p[i]);

            // ============ 步骤 6: 近平面裁剪 ============
            // （省略裁剪代码）

            // ============ 步骤 7: 透视投影 ============
            for (int i = 0; i < 3; i++) {
                // 应用投影矩阵
                triProj.p[i] = Mat_MulVec(matProj, triView.p[i]);

                // ★★★ 透视除法：这是实现透视效果的关键！ ★★★
                triProj.p[i] = Vec_Div(triProj.p[i], triProj.p[i].w);

                // ============ 步骤 8: NDC → 屏幕坐标 ============
                triProj.p[i].x *= -1;  // 翻转X
                triProj.p[i].y *= -1;  // 翻转Y
                triProj.p[i] = Vec_Add(triProj.p[i], {1, 1, 0});  // [-1,1] → [0,2]
                triProj.p[i].x *= 0.5f * screenWidth;   // → [0, Width]
                triProj.p[i].y *= 0.5f * screenHeight;  // → [0, Height]
            }

            // ============ 步骤 9: 光栅化绘制 ============
            DrawTriangle(triProj);
        }
    }
}
```

### 10.2 变换流程图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          3D 投影变换流水线                                    │
└─────────────────────────────────────────────────────────────────────────────┘

    模型空间 (Local)                原始顶点定义
         │                         (0,0,0), (1,0,0), (0,1,0)...
         │
         ▼ × M_world (旋转+平移)
         │
    世界空间 (World)                物体在世界中的位置
         │
         ▼ × M_view (相机逆矩阵)
         │
    相机空间 (View)                 以相机为中心
         │
         ▼ × M_proj (投影矩阵)
         │
    裁剪空间 (Clip)                 齐次坐标 (x, y, z, w)
         │
         ▼ ÷ w (透视除法)          ★ 透视效果的核心！
         │
    NDC空间                        归一化坐标 [-1, 1]
         │
         ▼ × 屏幕尺寸
         │
    屏幕空间 (Screen)               像素坐标 (px, py)
         │
         ▼
    ┌──────────────────┐
    │     光栅化        │           填充三角形像素
    └──────────────────┘
```

---

## 总结

### 核心公式回顾

1. **透视投影的本质**：
   $$
   x_{screen} = \frac{x \times f}{z}, \quad y_{screen} = \frac{y \times f}{z}
   $$

2. **为什么远处物体变小**：
   - 远处物体 z 值大
   - 除以 z 后，x 和 y 的绝对值变小
   - 在屏幕上显示更小

3. **变换链**：
   $$
   P_{screen} = ((P_{local} \times M_{world} \times M_{view} \times M_{proj}) / w) \times M_{viewport}
   $$

### 关键代码位置

| 功能 | 文件 | 函数 |
|------|------|------|
| 向量运算 | `math3d/math3d.h` | `Vec_Add`, `Vec_Cross`, `Vec_Norm` |
| 矩阵运算 | `math3d/math3d.h` | `Mat_MulVec`, `Mat_Mul` |
| 投影矩阵 | `math3d/math3d.h` | `Mat_Proj` |
| 视图矩阵 | `math3d/math3d.h` | `Mat_PointAt`, `Mat_QuickInv` |
| 渲染流程 | `core/engine.h` | `Engine3D::Render` |

---

## 11. CS2 视图矩阵深入解析

### 11.1 什么是 dwViewMatrix？

在CS2（Counter-Strike 2）中，`dwViewMatrix` 是一个 **4×4 矩阵**，它将 **世界坐标** 直接转换为 **裁剪空间坐标**。

**关键理解**：CS2的 `dwViewMatrix` 实际上是 **View × Projection 的组合矩阵**！

```
┌─────────────────────────────────────────────────────────────────┐
│  dwViewMatrix = View Matrix × Projection Matrix                 │
│                                                                 │
│  这意味着：一次矩阵乘法就能完成 世界坐标 → 屏幕坐标 的转换！    │
└─────────────────────────────────────────────────────────────────┘
```

### 11.2 矩阵内存布局

CS2中的ViewMatrix是一个 4×4 = 16 个浮点数的数组：

```
内存布局（行主序 Row-Major）：

float viewMatrix[16] = {
    m[0],  m[1],  m[2],  m[3],    // 第1行
    m[4],  m[5],  m[6],  m[7],    // 第2行
    m[8],  m[9],  m[10], m[11],   // 第3行
    m[12], m[13], m[14], m[15]    // 第4行
};

矩阵形式：
┌                          ┐
│ m[0]   m[1]   m[2]   m[3]  │   ← 用于计算 clip.x
│ m[4]   m[5]   m[6]   m[7]  │   ← 用于计算 clip.y
│ m[8]   m[9]   m[10]  m[11] │   ← 用于计算 clip.z
│ m[12]  m[13]  m[14]  m[15] │   ← 用于计算 clip.w
└                          ┘
```

### 11.3 ViewMatrix 的组成部分

ViewMatrix 融合了两个矩阵的功能：

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   View Matrix   │  ×  │ Projection Mat  │  =  │  dwViewMatrix   │
│   (相机变换)     │     │   (透视投影)     │     │   (组合矩阵)     │
└─────────────────┘     └─────────────────┘     └─────────────────┘

        │                       │                       │
        ▼                       ▼                       ▼
   世界→相机空间          相机→裁剪空间            世界→裁剪空间
```

**View Matrix 的作用**：
```
相机在世界中的位置: CameraPos = (Cx, Cy, Cz)
相机的朝向: Forward, Right, Up 三个正交向量

View Matrix 做的事情：
1. 将世界原点移动到相机位置（平移）
2. 将世界坐标轴旋转到相机坐标轴（旋转）

结果：相机变成了新的原点，相机看向的方向变成了Z轴
```

**Projection Matrix 的作用**：
```
将3D相机空间的点投影到2D平面：
- 应用透视缩放（远小近大）
- 将视锥体映射到标准立方体 [-1, 1]
```

### 11.4 View Matrix 详解

#### 11.4.1 相机坐标系

相机有自己的局部坐标系，由三个正交向量定义：

```
                    Up (头顶方向)
                      ↑
                      │
                      │
        Left ←────────●────────→ Right
                     /│\
                    / │ \
                   /  │  \
                  /   │   \
                 /    ↓    \
                    Forward (看向的方向)

相机位置: Eye = (Ex, Ey, Ez)
目标点:   Target = (Tx, Ty, Tz)
上方向:   Up = (0, 1, 0) 通常
```

#### 11.4.2 构建相机坐标系

```
步骤1: 计算前向向量 (Forward/Look)
─────────────────────────────────
    Forward = Normalize(Target - Eye)

              Target ●
                    ╱
                   ╱
                  ╱ Forward
                 ╱
                ╱
         Eye  ●


步骤2: 计算右向向量 (Right)
─────────────────────────────────
    Right = Normalize(Cross(Up, Forward))

    使用叉积：Up × Forward = Right

              Up
               ↑
               │
               │
               ●───────→ Right
              ╱
             ╱
            ↙ Forward

    叉积的右手法则：
    食指指向Up，中指指向Forward，大拇指指向Right


步骤3: 重新计算正交的上向量 (Up')
─────────────────────────────────
    Up' = Cross(Forward, Right)

    确保三个向量完全正交（互相垂直）
```

#### 11.4.3 View Matrix 的数学形式

View Matrix 由两部分组成：**旋转** 和 **平移**

```
View = Rotation × Translation

其中：

Rotation（将世界坐标轴对齐到相机坐标轴）：
┌                          ┐
│ Rx    Ry    Rz    0      │   R = Right 向量
│ Ux    Uy    Uz    0      │   U = Up 向量
│ Fx    Fy    Fz    0      │   F = Forward 向量
│ 0     0     0     1      │
└                          ┘

Translation（将世界原点移动到相机位置）：
┌                          ┐
│ 1     0     0     0      │
│ 0     1     0     0      │
│ 0     0     1     0      │
│ -Ex   -Ey   -Ez   1      │   E = Eye (相机位置)
└                          ┘

组合后的 View Matrix：
┌                                      ┐
│ Rx      Ry      Rz      0            │
│ Ux      Uy      Uz      0            │
│ Fx      Fy      Fz      0            │
│ -E·R    -E·U    -E·F    1            │
└                                      ┘

其中 E·R = Ex×Rx + Ey×Ry + Ez×Rz （点积）
```

#### 11.4.4 为什么是负的点积？

```
想象你站在世界坐标 (10, 0, 0) 的位置看向原点：

    世界空间：

         你(相机)
            ●
            │
            │  10米
            │
            ●───────────→ X
          原点

    相机空间（你的视角）：

    原点在你的左边10米处，所以在相机空间中原点的X坐标是 -10

         ←───────────●
         -10米      你(原点)
         原点

这就是为什么平移部分是负的：
    -E·R = -(10×1 + 0×0 + 0×0) = -10
```

### 11.5 Projection Matrix 详解

#### 11.5.1 透视投影矩阵结构

```
┌                                              ┐
│  f/aspect    0         0              0      │
│     0        f         0              0      │
│     0        0    (zf+zn)/(zf-zn)     1      │
│     0        0   -2×zf×zn/(zf-zn)     0      │
└                                              ┘

其中：
- f = 1 / tan(FOV/2)  视场角因子
- aspect = width / height  宽高比
- zn = near plane  近裁剪面
- zf = far plane   远裁剪面
```

#### 11.5.2 FOV 与 f 的关系

```
FOV (Field of View) 视场角：

         ╱│╲
        ╱ │ ╲
       ╱  │  ╲
      ╱   │   ╲
     ╱    │    ╲
    ╱     │     ╲
   ╱  FOV │      ╲
  ╱───────┼───────╲
 ╱    ↑   │        ╲
●─────────┴──────────
眼睛      d=1

tan(FOV/2) = 屏幕半高 / 距离
f = 1 / tan(FOV/2) = 距离 / 屏幕半高

FOV越大 → tan越大 → f越小 → 物体在屏幕上越小（广角效果）
FOV越小 → tan越小 → f越大 → 物体在屏幕上越大（长焦效果）

常见FOV值：
- CS2 默认: 90°
- 人眼: ~120°
- 狙击镜: 30°-40°
```

### 11.6 ViewProjection 组合矩阵

当 View 和 Projection 相乘后，得到的就是 CS2 的 `dwViewMatrix`：

```
dwViewMatrix = View × Projection

这个 4×4 矩阵可以一步完成：
    世界坐标 (x, y, z, 1) → 裁剪坐标 (clip.x, clip.y, clip.z, clip.w)
```

---

## 12. ViewMatrix 实战应用

### 12.1 世界坐标转屏幕坐标（World to Screen）

这是游戏中最常用的操作：将3D世界中的点转换为2D屏幕像素。

#### 12.1.1 完整算法流程

```
输入: 世界坐标 worldPos = (x, y, z)
输出: 屏幕坐标 screenPos = (sx, sy)

┌─────────────────────────────────────────────────────────────────┐
│  步骤1: 矩阵乘法 - 世界坐标 × ViewMatrix = 裁剪坐标            │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│  步骤2: 透视除法 - 裁剪坐标 / w = NDC坐标                       │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│  步骤3: 视口变换 - NDC坐标 → 屏幕像素坐标                       │
└─────────────────────────────────────────────────────────────────┘
```

#### 12.1.2 步骤1详解：矩阵乘法

```cpp
// 世界坐标
float worldX = 100.0f, worldY = 50.0f, worldZ = 200.0f;

// ViewMatrix (从游戏内存读取的 4×4 矩阵)
float vm[16];  // dwViewMatrix

// 计算裁剪坐标
float clipX = worldX * vm[0] + worldY * vm[1] + worldZ * vm[2]  + vm[3];
float clipY = worldX * vm[4] + worldY * vm[5] + worldZ * vm[6]  + vm[7];
float clipZ = worldX * vm[8] + worldY * vm[9] + worldZ * vm[10] + vm[11];
float clipW = worldX * vm[12]+ worldY * vm[13]+ worldZ * vm[14] + vm[15];
```

**图解矩阵乘法**：

```
                    ViewMatrix
              ┌─────────────────────┐
              │ vm[0]  vm[1]  vm[2]  vm[3]  │
              │ vm[4]  vm[5]  vm[6]  vm[7]  │
              │ vm[8]  vm[9]  vm[10] vm[11] │
              │ vm[12] vm[13] vm[14] vm[15] │
              └─────────────────────┘
                        ×
世界坐标      ┌───┐
              │ x │
              │ y │
              │ z │
              │ 1 │
              └───┘
                        =
裁剪坐标      ┌───────────────────────────────────────┐
              │ clipX = x×vm[0] + y×vm[1] + z×vm[2] + vm[3]   │
              │ clipY = x×vm[4] + y×vm[5] + z×vm[6] + vm[7]   │
              │ clipZ = x×vm[8] + y×vm[9] + z×vm[10]+ vm[11]  │
              │ clipW = x×vm[12]+ y×vm[13]+ z×vm[14]+ vm[15]  │
              └───────────────────────────────────────┘
```

#### 12.1.3 步骤2详解：透视除法

```cpp
// 检查点是否在相机前方
if (clipW < 0.001f) {
    // 点在相机后面，不可见
    return false;
}

// 透视除法：除以 w
float ndcX = clipX / clipW;  // 范围 [-1, 1]
float ndcY = clipY / clipW;  // 范围 [-1, 1]
```

**为什么要检查 clipW？**

```
clipW 本质上代表了点到相机的"深度"

clipW > 0: 点在相机前方 ✓
clipW < 0: 点在相机后方 ✗ (不应该渲染)
clipW ≈ 0: 点在相机平面上 ✗ (会导致除零)

图示：
                    clipW < 0        clipW > 0
                    (后方)           (前方)
                        │               │
        ●───────────────●───────────────●──────→ Z
      后方物体        相机           前方物体
      (不可见)                       (可见)
```

#### 12.1.4 步骤3详解：视口变换

```cpp
// 屏幕尺寸
float screenWidth = 1920.0f;
float screenHeight = 1080.0f;

// NDC [-1, 1] → 屏幕 [0, Width/Height]
float screenX = (ndcX + 1.0f) * 0.5f * screenWidth;
float screenY = (1.0f - ndcY) * 0.5f * screenHeight;  // Y轴翻转！
```

**为什么Y轴要翻转？**

```
NDC坐标系：              屏幕坐标系：
    Y ↑                  (0,0)┌────────────→ X
      │                       │
      │                       │
      ●───→ X                 │
                              ↓ Y

NDC中 Y向上为正          屏幕中 Y向下为正
所以需要翻转：screenY = (1 - ndcY) × ...
```

### 12.2 完整的 WorldToScreen 函数

```cpp
bool WorldToScreen(float worldPos[3], float screenPos[2],
                   float viewMatrix[16], int screenWidth, int screenHeight) {

    // ============ 步骤1: 矩阵乘法 ============
    float clipX = worldPos[0] * viewMatrix[0] +
                  worldPos[1] * viewMatrix[1] +
                  worldPos[2] * viewMatrix[2] +
                  viewMatrix[3];

    float clipY = worldPos[0] * viewMatrix[4] +
                  worldPos[1] * viewMatrix[5] +
                  worldPos[2] * viewMatrix[6] +
                  viewMatrix[7];

    float clipW = worldPos[0] * viewMatrix[12] +
                  worldPos[1] * viewMatrix[13] +
                  worldPos[2] * viewMatrix[14] +
                  viewMatrix[15];

    // ============ 步骤2: 可见性检查 ============
    if (clipW < 0.001f) {
        return false;  // 在相机后面
    }

    // ============ 步骤3: 透视除法 ============
    float ndcX = clipX / clipW;
    float ndcY = clipY / clipW;

    // ============ 步骤4: 视口变换 ============
    screenPos[0] = (ndcX + 1.0f) * 0.5f * screenWidth;
    screenPos[1] = (1.0f - ndcY) * 0.5f * screenHeight;

    // ============ 步骤5: 边界检查 ============
    if (screenPos[0] < 0 || screenPos[0] > screenWidth ||
        screenPos[1] < 0 || screenPos[1] > screenHeight) {
        return false;  // 在屏幕外
    }

    return true;
}
```

### 12.3 图解完整流程

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        World to Screen 完整流程                              │
└─────────────────────────────────────────────────────────────────────────────┘

    世界空间                                              屏幕空间

         Y ↑                                         (0,0)┌──────────────┐
           │    ● 敌人位置                                │              │
           │   (100, 50, 200)                             │    ● (960,   │
           │                                              │      400)    │
           ●───────→ X                                    │              │
          /                                               └──────────────┘
         Z                                                         (1920,1080)

                              │
                              │  × ViewMatrix
                              ▼

    裁剪空间 (Clip Space)

    clipX = 100×vm[0] + 50×vm[1] + 200×vm[2] + vm[3] = 0.5
    clipY = 100×vm[4] + 50×vm[5] + 200×vm[6] + vm[7] = 0.2
    clipW = 100×vm[12]+ 50×vm[13]+ 200×vm[14]+ vm[15]= 200

                              │
                              │  ÷ clipW (透视除法)
                              ▼

    NDC空间 (Normalized Device Coordinates)

    ndcX = 0.5 / 200 = 0.0025
    ndcY = 0.2 / 200 = 0.001

    范围: [-1, 1]

                              │
                              │  视口变换
                              ▼

    屏幕空间 (Screen Space)

    screenX = (0.0025 + 1) × 0.5 × 1920 = 962.4
    screenY = (1 - 0.001) × 0.5 × 1080 = 539.46

    最终像素: (962, 539)
```

### 12.4 ViewMatrix 各行的含义

理解矩阵每一行的作用：

```
ViewMatrix[16] 的结构：

行0: [vm[0], vm[1], vm[2], vm[3]]
     ↓
     用于计算 clipX（水平位置）
     - vm[0], vm[1], vm[2]: 世界坐标如何影响水平位置
     - vm[3]: 水平偏移量

行1: [vm[4], vm[5], vm[6], vm[7]]
     ↓
     用于计算 clipY（垂直位置）
     - vm[4], vm[5], vm[6]: 世界坐标如何影响垂直位置
     - vm[7]: 垂直偏移量

行2: [vm[8], vm[9], vm[10], vm[11]]
     ↓
     用于计算 clipZ（深度，用于深度测试）

行3: [vm[12], vm[13], vm[14], vm[15]]
     ↓
     用于计算 clipW（透视除法的分母）
     ★ 这一行决定了透视效果的强度！
```

### 12.5 dwViewMatrix vs dwProjMatrix

在某些游戏中，View 和 Projection 是分开存储的：

```
┌─────────────────────────────────────────────────────────────────┐
│  方式1: 分离存储 (某些引擎)                                      │
├─────────────────────────────────────────────────────────────────┤
│  dwViewMatrix[16]  - 纯视图矩阵 (相机变换)                       │
│  dwProjMatrix[16]  - 纯投影矩阵 (透视变换)                       │
│                                                                 │
│  使用时需要两次矩阵乘法：                                        │
│  clipPos = worldPos × ViewMatrix × ProjMatrix                   │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│  方式2: 组合存储 (CS2, Source引擎)                               │
├─────────────────────────────────────────────────────────────────┤
│  dwViewMatrix[16]  - 已经是 View × Proj 的组合                   │
│                                                                 │
│  使用时只需一次矩阵乘法：                                        │
│  clipPos = worldPos × ViewMatrix                                │
│                                                                 │
│  ★ 更高效！这就是为什么CS2只暴露一个矩阵                         │
└─────────────────────────────────────────────────────────────────┘
```

### 12.6 常见问题与调试

#### 问题1: 坐标显示在错误位置

```
可能原因：
1. 矩阵行列顺序错误（Row-Major vs Column-Major）
2. Y轴翻转遗漏
3. 使用了错误的屏幕分辨率

调试方法：
- 测试已知点（如地图原点 0,0,0）
- 检查 clipW 的值是否合理（应该约等于距离）
```

#### 问题2: 点在相机后面时显示异常

```cpp
// 错误：没有检查 clipW
float screenX = (clipX / clipW + 1) * 0.5f * width;  // clipW < 0 时会出错！

// 正确：先检查可见性
if (clipW < 0.001f) return false;
float screenX = (clipX / clipW + 1) * 0.5f * width;
```

#### 问题3: 边缘物体闪烁

```
原因：物体在视锥体边缘时，浮点精度问题

解决：添加小的边界容差
if (screenX < -10 || screenX > width + 10) return false;
```

### 12.7 ViewMatrix 的逆向工程

从 ViewMatrix 中提取相机信息：

```cpp
// 从组合矩阵中近似提取相机位置（不完全精确）
// 这需要对矩阵进行逆运算

// 更简单的方法：直接读取游戏中的相机位置
// 通常游戏会单独存储 CameraPos, CameraAngles 等
```

---

## 13. 总结：从3D到2D的完整旅程

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           3D → 2D 投影总结                                   │
└─────────────────────────────────────────────────────────────────────────────┘

1. 模型空间 (Local Space)
   │  物体自身的坐标系，原点在物体中心
   │
   │  × Model Matrix (旋转、缩放、平移)
   ▼

2. 世界空间 (World Space)
   │  所有物体共享的全局坐标系
   │  敌人位置、玩家位置都在这个空间
   │
   │  × View Matrix (相机变换)
   ▼

3. 相机空间 (View Space)
   │  以相机为原点，相机看向Z轴正方向
   │
   │  × Projection Matrix (透视投影)
   ▼

4. 裁剪空间 (Clip Space)
   │  齐次坐标 (x, y, z, w)
   │  w 包含了深度信息
   │
   │  ÷ w (透视除法) ★核心步骤★
   ▼

5. NDC空间 (Normalized Device Coordinates)
   │  标准化坐标 [-1, 1]
   │  与屏幕分辨率无关
   │
   │  × 屏幕尺寸 (视口变换)
   ▼

6. 屏幕空间 (Screen Space)
      最终的像素坐标 (px, py)
      可以直接用于绘制！


核心公式：
─────────
screen.x = ((world · row0) / (world · row3) + 1) × 0.5 × width
screen.y = (1 - (world · row1) / (world · row3)) × 0.5 × height

其中 world · rowN 表示世界坐标与矩阵第N行的点积
```

---

## 参考资源

- [learnopengl.com - Transformations](https://learnopengl.com/Getting-started/Transformations)
- [songho.ca - OpenGL Projection Matrix](http://www.songho.ca/opengl/gl_projectionmatrix.html)
- OneLoneCoder 3D Graphics Tutorial
- Source Engine SDK Documentation

---

*文档最后更新: 2024*

