# Tank Maze Game — OpenGL 3D Assignment

<div align="center">

![C++](https://img.shields.io/badge/C%2B%2B-11-blue?logo=cplusplus)
![OpenGL](https://img.shields.io/badge/OpenGL-3.3-orange?logo=opengl)
![GLSL](https://img.shields.io/badge/GLSL-330-green)
![GLEW](https://img.shields.io/badge/GLEW-2.2-lightgrey)
![freeglut](https://img.shields.io/badge/freeglut-3.x-lightgrey)
![License](https://img.shields.io/badge/License-Academic-red)

**A real-time 3D tank game built entirely on a modern programmable OpenGL pipeline.**  
Drive a tank across floating maze tiles, aim with the mouse, fire arc-trajectory projectiles,  
and collect tokens before time runs out — all rendered with custom GLSL Blinn-Phong shaders.

</div>

---

## Table of Contents

1. [Gameplay](#gameplay)
2. [Controls](#controls)
3. [Project Structure](#project-structure)
4. [Building & Running](#building--running)
5. [OpenGL Architecture](#opengl-architecture)
6. [GLSL Shader Pipeline](#glsl-shader-pipeline)
7. [Mathematics & Physics](#mathematics--physics)
8. [Feature Reference](#feature-reference)
9. [Assets](#assets)

---

## Gameplay

| Goal | Collect all 7 tokens before the 120-second countdown expires |
|------|-------------------------------------------------------------|
| Movement | Drive on floating maze tiles using WASD |
| Aiming | Mouse movement orbits the camera and aims the turret simultaneously |
| Firing | Left mouse button fires a `ball.obj` projectile on a parabolic arc |
| Danger | Driving off a tile edge triggers realistic gravity — fall too far and it's game over |
| Scoring | 6 tokens sit on the connected outer-ring path; 1 sits on an inner island (projectile-only) |

---

## Controls

| Key / Input | Action |
|-------------|--------|
| `W` | Drive forward (hold for continuous acceleration) |
| `S` | Drive reverse |
| `A` | Turn left |
| `D` | Turn right |
| Mouse move | Orbit camera & aim turret |
| Left click | Fire projectile |
| `ENTER` | Start / Restart game |
| `ESC` | Quit |

---

## Project Structure

```
assignment_tank/
├── common/                          # University-provided library (read-only)
│   ├── Matrix.h / Matrix.cpp        # Column-major 4×4 matrix (perspective, lookAt, rotate…)
│   ├── Vector.h / Vector.cpp        # Vector2f / Vector3f with cross, dot, normalise
│   ├── Mesh.h / Mesh.cpp            # OBJ loader → VAO + VBO pipeline
│   ├── Shader.h / Shader.cpp        # GLSL shader compile & link
│   ├── Texture.h / Texture.cpp      # BMP → GL_TEXTURE_2D loader
│   └── SphericalCameraManipulator.h/.cpp  # Pan/tilt/radius orbit camera
│
├── models/                          # 3D assets
│   ├── chassis.obj                  # Tank body
│   ├── turret.obj                   # Tank turret (independent rotation)
│   ├── front_wheel.obj              # Front wheel pair
│   ├── back_wheel.obj               # Rear wheel pair
│   ├── coin.obj                     # Token / collectible
│   ├── ball.obj                     # Projectile
│   ├── hamvee.bmp                   # Tank texture (mapped to all tank parts)
│   ├── coin.bmp                     # Token texture
│   ├── ball.bmp                     # Projectile texture
│   └── Crate.bmp                    # Maze floor tile texture
│
└── tank_assignment/                 # Application source (this project)
    ├── main.cpp                     # Complete game engine (~660 lines)
    ├── level.txt                    # Maze grid configuration file
    ├── Makefile                     # GNU make build (no qmake required)
    ├── qmake.pro                    # Qt Creator / qmake build
    └── shaders/
        ├── main.vert                # Vertex shader — MVP transform + normal matrix
        └── main.frag                # Fragment shader — Blinn-Phong reflection model
```

---

## Building & Running

### Prerequisites

```bash
sudo apt install libglew-dev freeglut3-dev libgl-dev g++
```

### Build with GNU Make

```bash
cd tank_assignment/
make
./TankAssignment
```

### Build with qmake (Qt Creator)

```bash
cd tank_assignment/
qmake qmake.pro && make
./TankAssignment
```

> **Note:** The executable must be run from inside `tank_assignment/` so that the relative paths  
> `shaders/main.vert`, `shaders/main.frag`, `level.txt`, and `../models/*.{obj,bmp}` resolve correctly.

---

## OpenGL Architecture

This project uses **OpenGL 3.3 Compatibility Profile** with a fully programmable pipeline — no deprecated fixed-function geometry calls (`glBegin`/`glEnd`/`glLightfv`) are used for 3D rendering.

### Rendering Pipeline Overview

```
CPU (C++)                         GPU (GLSL)
─────────────────────────────     ────────────────────────────────────
OBJ → Mesh::loadOBJ()        ─►  Vertex Buffer Objects (VBOs)
  positions  → GL_ARRAY_BUFFER       layout(location=0) vec3 in_Position
  normals    → GL_ARRAY_BUFFER       layout(location=1) vec3 in_Normal
  texcoords  → GL_ARRAY_BUFFER       layout(location=2) vec2 in_TexCoord

Matrix4x4 model / view / proj ─► uniform mat4 modelMatrix / viewMatrix / projMatrix
Normal matrix (CPU computed)   ─► uniform mat3 normalMatrix

BMP → Texture::LoadBMP()      ─►  GL_TEXTURE_2D → sampler2D texSampler
                                   useTexture uniform controls sampling vs solid color

                                   main.vert: world-space transform
                                   main.frag: Blinn-Phong lighting
                                         ↓
                                   Framebuffer → glutSwapBuffers()
                                         ↓
                                   Legacy GL overlay (glWindowPos2i +
                                   glutBitmapCharacter) for 2D HUD text
```

### Key Implementation Details

| Concern | Implementation |
|---------|----------------|
| Shader attributes | Fixed `layout(location = N)` qualifiers — no `glGetAttribLocation()` needed |
| Uniform caching | All `glGetUniformLocation()` calls made once at startup, IDs stored globally |
| Normal matrix | `transpose(inverse(modelMatrix))` computed per draw call on CPU, passed as `mat3` |
| 2D overlay | `glUseProgram(0)` + `glWindowPos2i()` + `glutBitmapCharacter()` — no separate UI shader |
| Physics loop | `glutTimerFunc(10ms, Timer, 0)` — physics at ~100 Hz, decoupled from display FPS |
| Delta time | `glutGet(GLUT_ELAPSED_TIME)` with a 50 ms cap to handle pause/tab-out gracefully |

---

## GLSL Shader Pipeline

### Vertex Shader (`shaders/main.vert`)

```glsl
#version 330

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec2 in_TexCoord;

uniform mat4 modelMatrix, viewMatrix, projMatrix;
uniform mat3 normalMatrix;   // transpose(inverse(model)) upper-3×3

out vec3 fragPos;       // world-space fragment position
out vec3 fragNormal;    // correctly transformed normal
out vec2 fragTexCoord;
```

The vertex shader operates in **world space**: `fragPos = (modelMatrix * position).xyz`.  
This avoids view-space lighting artefacts and keeps the light position intuitive.

### Fragment Shader (`shaders/main.frag`) — Blinn-Phong Model

```glsl
// Ambient
vec3 ambient = ambientStrength * lightColor;

// Diffuse (Lambertian)
float diff   = max(dot(normalize(fragNormal), lightDir), 0.0);
vec3 diffuse = diff * lightColor;

// Specular — Blinn-Phong half-vector (more physically plausible than Phong)
vec3  halfVec  = normalize(lightDir + viewDir);
float spec     = pow(max(dot(norm, halfVec), 0.0), shininess);
vec3  specular = specularStrength * spec * lightColor;

// Combine
vec3 result = (ambient + diffuse + specular) * baseColor;
```

**Why Blinn-Phong over Phong?**  
The half-vector `H = normalize(L + V)` avoids the `dot(R, V)` going negative at grazing  
angles, producing smoother highlights on curved tank surfaces and is cheaper to compute.

---

## Mathematics & Physics

### 1. Tank Kinematics (rubric specification)

The tank uses symplectic Euler integration with explicit acceleration and drag terms:

```
V_new = V_prev + Δt × (a_accel − a_decel)
P_new = P_prev + Δt × V_new
```

Where:
- `a_accel = 8.0 m/s²` when W/S held; `0` otherwise
- `a_decel = 5.0 m/s²` drag (always opposes current velocity direction)
- `MAX_SPEED = 5.5 m/s` clamped after integration

**Turn rate:** `Δheading = ±85°/s × Δt`

**Wheel spin angle** (proportional to actual ground distance):
```
Δθ_wheel = (v × Δt / r_wheel) × (180° / π)    where r_wheel = 0.28
```

### 2. Free-Fall Gravity

When `isOverTile(x, z)` returns false, the tank enters free-fall:

```
v_fall(t + Δt) = v_fall(t) + g × Δt           g = 9.81 m/s²
y_offset(t + Δt) = y_offset(t) − v_fall × Δt
```

Game-over triggers when `y_offset < −20.0`.

### 3. View Matrix — Spherical Orbit Camera

The camera orbits the tank focus point using spherical coordinates:

```
aVec = ( sin(tilt) × sin(pan),   cos(tilt),   sin(tilt) × cos(pan) )
camPos = focus + aVec × radius
viewMatrix = lookAt(camPos, focus, up=(0,1,0))
```

Mouse input updates `pan` and `tilt` (tilt clamped to `[−1.5, −0.05]` rad to keep camera above tank).

### 4. Normal Matrix

For correct lighting under non-uniform scaling (tiles use `scale(0.5, 0.2, 0.5)`),  
the normal matrix is computed each draw call and passed as a `mat3` uniform:

```
normalMatrix = upper3x3( transpose( inverse( modelMatrix ) ) )
```

This ensures normals remain perpendicular to surfaces after non-uniform transforms,  
preventing the "stretched normal" lighting artefact.

### 5. Projectile Trajectory

Fired from the turret with a calculated direction aligned to the camera forward vector:

```
fireDir.x = −sin(tilt) × sin(pan)   // = |sin(tilt)| × sin(pan)
fireDir.z = −sin(tilt) × cos(pan)

vel = normalize(fireDir_xz) × PROJ_SPEED + (0, PROJ_UP_BIAS=3.5, 0)

// Each frame:
pos += vel × Δt
vel.y −= 9.81 × Δt                  // gravity creates parabolic arc
```

### 6. Bounding Sphere Collision

```
distance = √( (px−tx)² + (py−ty)² + (pz−tz)² )

Tank  vs Token:      collision if distance < 0.70
Projectile vs Token: collision if distance < (0.45 + 0.35)
```

Both the tank and projectile perform sphere-sphere intersection tests against every  
live token each physics tick (~100 Hz).

### 7. Model-Space Wheel Rotation

The wheel spin is applied **after** the scale in the model matrix chain:

```
M = Translate × RotateY(heading) × Scale × RotateX(wheelAngle)
```

Because `Matrix4x4::rotate()` right-multiplies (`M = M × R`), calling `rotate(wheelAngle, X)` last  
produces the spin in the wheel's local (model) space — correctly rolling regardless of tank direction.

---

## Feature Reference

| Feature | Status | Notes |
|---------|--------|-------|
| Dynamic maze from file | ✅ | `parseMaze("level.txt")` — any M×N grid |
| Blinn-Phong shading | ✅ | Ambient + diffuse + specular, warm sunlight |
| Normal matrix | ✅ | Correct under non-uniform tile scaling |
| OBJ mesh loading | ✅ | `v`, `vn`, `vt`, `f` parsed; VBO upload |
| BMP texture binding | ✅ | `GL_LINEAR` filtering, `GL_REPEAT` wrapping |
| Tank kinematics | ✅ | Exact V/P equations from rubric spec |
| Key buffering | ✅ | `bool g_keys[256]` — simultaneous inputs |
| Wheel animation | ✅ | Angle proportional to `distance / radius` |
| Edge fall + gravity | ✅ | 9.81 m/s² free-fall, game-over at −20 |
| Spherical orbit camera | ✅ | `glutPassiveMotionFunc` — no button required |
| Turret tracking | ✅ | Turret heading derived from camera pan |
| Projectile arc | ✅ | Initial upward bias + gravity each frame |
| Bounding-sphere collision | ✅ | Tank & projectile vs all live tokens |
| Token bob animation | ✅ | Per-token sine wave, staggered phases |
| Token spin animation | ✅ | 80 °/s Y-axis rotation |
| Start menu | ✅ | Full controls list, pulsing ENTER prompt |
| Game-over overlay | ✅ | Win / lose state, score summary |
| HUD time + score | ✅ | Colour shifts red as time runs low |
| Crosshair | ✅ | Centre-screen text crosshair |

---

## Assets

| File | Format | Usage |
|------|--------|-------|
| `chassis.obj` | Wavefront OBJ | Tank body mesh |
| `turret.obj` | Wavefront OBJ | Tank turret (independently rotated) |
| `front_wheel.obj` | Wavefront OBJ | Front wheel pair |
| `back_wheel.obj` | Wavefront OBJ | Rear wheel pair |
| `coin.obj` | Wavefront OBJ | Collectible token |
| `ball.obj` | Wavefront OBJ | Fired projectile |
| `hamvee.bmp` | 24-bit BMP | Tank surface texture |
| `coin.bmp` | 24-bit BMP | Token texture |
| `ball.bmp` | 24-bit BMP | Projectile texture |
| `Crate.bmp` | 24-bit BMP | Maze floor tile texture |

---

## Maze Layout (`level.txt`)

```
Col →  0  1  2  3  4  5  6  7  8  9
Row 0  .  .  .  .  .  .  .  .  .  .
Row 1  .  ○  ★  ○  ○  ○  ○  ★  ○  .  ← outer ring (tank start at col=1)
Row 2  .  ○  .  .  .  .  .  .  ○  .
Row 3  .  ○  .  ○  ○  ○  ○  .  ○  .  ← inner ring (floating island)
Row 4  .  ○  .  ○  .  ★  ○  .  ○  .
Row 5  .  ★  .  ○  .  .  ○  .  ★  .
Row 6  .  ○  .  ○  ○  ○  ○  .  ○  .
Row 7  .  ○  .  .  .  .  .  .  ○  .
Row 8  .  ○  ★  ○  ○  ○  ○  ★  ○  .
Row 9  .  .  .  .  .  .  .  .  .  .

○ = tile   ★ = tile + token   . = void
```

**6 tokens** on the connected outer-ring path (driveable).  
**1 token** on the inner island — must be destroyed by projectile.

---

*Built for a university Computer Graphics module using C++11 and OpenGL 3.3.*
