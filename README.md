# ⚡ Real-Time GPU Raytracer

> High-Performance BVH-Accelerated Path Tracer with OpenGL Compute Shaders

![C++](https://img.shields.io/badge/C++-17-blue.svg)
![OpenGL](https://img.shields.io/badge/OpenGL-4.3-green.svg)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)

A physically-based path tracer running entirely on the GPU, capable of rendering millions of triangles in real-time with full global illumination, refractions, and physically-based materials.

---

## 🎨 Gallery

### Dragon Family (Color Variations)
*Multiple dragon renders showcasing different material properties and lighting setups*

<div align="center">
  <img width="2560" height="1440" alt="Base Profile Screenshot 2025 07 25 - 22 14 32 19" src="https://github.com/user-attachments/assets/82968f3c-ffc6-4009-89db-c15fcb04d6b9" />
</div>

### Cathedral Interior
*Complex architectural lighting with millions of triangles*

<div align="center">
  <img width="2560" height="1440" alt="Base Profile Screenshot 2025 07 26 - 19 29 46 83" src="https://github.com/user-attachments/assets/48829c0a-b1e1-45ad-a09f-7704d74701c8" />
</div>

### Glossy Black Dragon
*High-gloss material demonstrating reflections and light transport*

<div align="center">
  <img width="2560" height="1440" alt="Base Profile Screenshot 2025 08 26 - 22 03 01 93" src="https://github.com/user-attachments/assets/92518606-5e52-40c5-8038-d08408294036" />
</div>

### Bugatti Veyron (1.5M Triangles)
*Automotive rendering with complex geometry and materials*

<div align="center">
  <img width="2560" height="1440" alt="Base Profile Screenshot 2025 09 01 - 20 51 40 84" src="https://github.com/user-attachments/assets/5c1ba2f5-7b60-49dc-bf4f-2324d3e5b27c" />
</div>


---

## ✨ Key Features

### 🚀 Performance
- **Real-Time Path Tracing**: 30-60 FPS on complex scenes with millions of triangles
- **BVH Acceleration**: Custom-built Bounding Volume Hierarchy for efficient ray-triangle intersection
- **GPU-Accelerated**: All rendering performed in fragment shaders using OpenGL 4.3
- **Multi-threaded BVH Construction**: Parallel tree building for fast scene initialization

### 🎨 Rendering Features
- **Physically-Based Materials**: Support for diffuse, specular, metallic, glass, and emissive materials
- **Global Illumination**: Full indirect lighting simulation via path tracing
- **Refractions & Reflections**: Accurate glass and mirror materials with Fresnel equations
- **HDR Environment Maps**: Equirectangular environment map support for realistic lighting
- **Progressive Accumulation**: Temporal anti-aliasing with progressive sample accumulation
- **Russian Roulette Path Termination**: Unbiased variance reduction

### 🛠️ Technical Features
- **OBJ Model Loading**: Fast, optimized parser with MTL material support
- **Per-Model Transformations**: Independent position, rotation, and scale for each model
- **Live Material Editing**: Real-time material property adjustments via ImGui interface
- **Debug Visualization**: BVH traversal heatmaps for performance analysis

---

## 🏗️ Architecture

### Rendering Pipeline

```
User Input → Scene Setup → BVH Construction → GPU Upload → Shader Raytracing → Accumulation → Display
     ↓                                                            ↑
     └────────── Camera/Material Updates ─────────────────────────┘
```

### BVH Construction
- **Algorithm**: Surface Area Heuristic (SAH) with spatial binning
- **Parallelization**: Multi-threaded split evaluation using thread pool
- **Storage**: Compressed into GPU-friendly SSBO format

### Ray Traversal
- **Stack-based iteration**: Non-recursive BVH traversal in shader
- **Multi-model support**: Each model has independent transformation matrix
- **Early ray termination**: Distance-based pruning in model space

---

## 📋 Requirements

### Minimum
- **GPU**: OpenGL 4.3 compatible graphics card
- **OS**: Windows 10/11 or Linux (Ubuntu 20.04+)
- **RAM**: 8GB
- **Compiler**: GCC 9+, Clang 10+, or MSVC 2019+

### Recommended
- **GPU**: NVIDIA RTX series or AMD RX 6000 series
- **CPU**: 8+ cores for faster BVH construction
- **RAM**: 16GB for large models

---

## 🚀 Building from Source

### Dependencies

- **GLFW** (window management)
- **GLAD** (OpenGL loader)
- **GLM** (mathematics)
- **ImGui** (UI framework)
- **stb_image** (texture loading)

### Build Instructions

#### Linux
```bash
# Clone repository with submodules
git clone --recursive https://github.com/acroyset/RaytracingWindowsTriangles.git
cd raytracer

# Build
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run
./RaytracingWindowsTriangles
```

#### Windows (Visual Studio)
```bash
# Clone repository
git clone --recursive https://github.com/acroyset/RaytracingWindowsTriangles.git
cd raytracer

# Generate Visual Studio solution
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"

# Open RaytracingWindowsTriangles.sln and build
```

---

## 🎮 Usage

### Basic Scene Setup

```cpp
#include "Scene.h"

int main() {
    // Create scene (samples per frame, AA level, max bounces)
    Scene scene(1, 3, 16);
    
    // Add models (path, position, scale, color, smoothness)
    scene.addModel("models/dragon800K.obj", 
                   vec3(0, 70, 0),      // position
                   vec3(100),           // scale
                   vec3(0.9),           // base color
                   0.3);                // smoothness (0=rough, 1=mirror)
    
    // Optional: Advanced materials
    scene.addModel("models/sphere.obj",
                   vec3(200, 50, 0),
                   vec3(50),
                   vec3(1, 0.7, 0.3),   // emissive color
                   0,                    // smoothness
                   vec3(0),              // specular color
                   0,                    // specular probability
                   0,                    // transparency
                   1,                    // index of refraction
                   2);                   // emission strength
    
    scene.set_ssbo();
    
    // Render loop
    while (scene.open()) {
        scene.updateFrame();
    }
    
    return 0;
}
```

### Controls

#### Camera
| Key       | Action                  |
|-----------|-------------------------|
| `W` / `S` | Move forward / backward |
| `A` / `D` | Move left / right       |
| `Q` / `E` | Move down / up          |
| `Mouse`   | Look around             |
| `Shift`   | Sprint (2x speed)       |
| `L`       | Lock cursor             |
| `U`       | Unlock cursor           |
| `H`       | Turn HUD on             |
| `Esc`     | Exit application        |

#### UI Controls
- **Model Selection**: Dropdown to select active model
- **Position**: 3D slider for model translation
- **Rotation**: Euler angle controls (degrees)
- **Scale**: Uniform or per-axis scaling
- **Material Editor**: Live editing of colors, smoothness, transparency, etc.
- **Render Settings**: Samples, AA, bounce limit, debug view

---

## 🎨 Material System

### Supported Material Types

#### Diffuse (Lambertian)
```cpp
vec3 color = vec3(0.8, 0.1, 0.1);  // red
float smoothness = 0.0;             // pure diffuse
```

#### Glossy (Microfacet)
```cpp
vec3 color = vec3(0.9);
float smoothness = 0.7;             // 70% mirror-like
vec3 specularColor = vec3(1.0);
float specularProb = 0.5;           // 50% specular lobes
```

#### Glass (Dielectric)
```cpp
float transparency = 0.9;
float ior = 1.5;                    // glass index of refraction
vec3 tint = vec3(0.9, 1.0, 0.95);  // slight green tint
```

#### Emissive (Light Source)
```cpp
vec3 emissionColor = vec3(1, 0.8, 0.6);
float emissionStrength = 5.0;
```

### MTL File Support

The renderer automatically parses `.mtl` files referenced in OBJ models:
- `Kd` → Diffuse color
- `Ks` → Specular color
- `Ke` → Emission
- `Ns` → Smoothness (converted from shininess)
- `Ni` → Index of refraction
- `d` → Transparency (inverted opacity)

---

## 📊 Performance

### Benchmark Results

| Scene | Triangles | BVH Nodes | FPS (RTX 4070 Super) | Build Time |
|-------|-----------|-----------|----------------|------------|
| Stanford Bunny | 144K | 71K | 120 | 0.8s |
| Dragon | 3.6M | 1.8M | 45 | 12s |
| Sponza Palace | 500K | 250K | 30 | 3s |
| Bugatti Veyron | 1.5M | 750K | 55 | 8s |

*Settings: 2560×1440, 1 sample/frame, 3×3 AA, 16 bounces*

---

## 🔬 Technical Deep Dive

### BVH Construction Algorithm

The renderer uses a Surface Area Heuristic (SAH) based BVH builder with the following optimizations:

1. **Spatial Binning**: Instead of testing every triangle position, we discretize space into bins
2. **Parallel Split Evaluation**: Multi-threaded cost evaluation for all split candidates
3. **Early Termination**: Stop splitting when SAH cost doesn't improve
4. **Tight Bounds**: Per-triangle min/max precomputation for fast bounding box updates

```cpp
float SAH_cost = surface_area(left) * num_tris(left) + 
                 surface_area(right) * num_tris(right);
```

### Ray-Triangle Intersection

Using the Möller-Trumbore algorithm for watertight intersections:

```glsl
bool rayTriangleIntersect(vec3 ro, vec3 rd, vec3 v0, vec3 v1, vec3 v2, 
                          out float t, out float u, out float v) {
    vec3 e1 = v1 - v0;
    vec3 e2 = v2 - v0;
    vec3 p = cross(rd, e2);
    float det = dot(e1, p);
    
    if (abs(det) < EPS) return false;
    
    float invDet = 1.0 / det;
    vec3 tv = ro - v0;
    u = dot(tv, p) * invDet;
    
    if (u < 0.0 || u > 1.0) return false;
    
    vec3 q = cross(tv, e1);
    v = dot(rd, q) * invDet;
    
    if (v < 0.0 || (u + v) > 1.0) return false;
    
    t = dot(e2, q) * invDet;
    return t > 0.0005;
}
```
---

## 🐛 Known Limitations

- **No Texture Mapping**: Currently only supports per-vertex colors and solid materials
- **Single-Level BVH**: No TLAS/BLAS hierarchy for instancing
- **CPU BVH Build**: Scene loading can be slow for very large models
- **No Denoising**: Pure path tracing without ML denoising

---

## 🗺️ Roadmap

- [ ] Texture mapping (diffuse, normal, roughness)
- [ ] Two-level BVH for instancing
- [ ] GPU BVH construction
- [ ] OptiX/DXR acceleration
- [ ] OIDN/OptiX denoising integration
- [ ] Volumetric rendering (fog, smoke)
- [ ] HDR tone mapping options
- [ ] glTF model loading

---
