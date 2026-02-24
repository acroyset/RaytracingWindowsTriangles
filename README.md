# ⚡ Real-Time GPU Raytracer

> High-Performance BVH-Accelerated Path Tracer with OpenGL Compute Shaders

![C++](https://img.shields.io/badge/C++-17-blue.svg)
![OpenGL](https://img.shields.io/badge/OpenGL-4.3-green.svg)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)

A physically-based path tracer running entirely on the GPU, capable of rendering millions of triangles in real-time with full global illumination, refractions, and physically-based materials.

---

## 🎨 Gallery

### Model Family (7.2M Triangles)
*Multiple models showcasing different material properties*

<div align="center">
  <img width="2560" height="1440" alt="Base Profile Screenshot 2026 02 21 - 00 35 55 24" src="https://github.com/user-attachments/assets/20ce0e74-eba9-406e-adf1-0fe404323b76" />


</div>

### Bull Family (2.1M Triangles)
*Complex textures with different materials*

<div align="center">
  <img width="2560" height="1440" alt="Base Profile Screenshot 2026 02 11 - 18 16 50 59" src="https://github.com/user-attachments/assets/1ebc96a7-e2e2-4de3-9365-908545cddc64" />
</div>

### Glass Dragon (870K Triangles)
*High-gloss material demonstrating reflections and refractions*

<div align="center">
  <img width="2560" height="1440" alt="Base Profile Screenshot 2026 02 16 - 23 23 55 58" src="https://github.com/user-attachments/assets/d89a0d0a-425d-4d5f-b2ab-8fce070098ee" />


</div>

### Bugatti Veyron (1.5M Triangles)
*Automotive rendering with complex geometry and materials*

<div align="center">
  <img width="2560" height="1440" alt="Base Profile Screenshot 2026 02 21 - 16 52 35 52" src="https://github.com/user-attachments/assets/298440c4-8a96-482b-ad56-72b51d68a529" />

</div>


---

## ✨ Key Features

### 🚀 Performance
- **Real-Time Path Tracing**: 30-60 FPS on complex scenes with millions of triangles
- **BVH Acceleration**: Custom-built Bounding Volume Hierarchy for efficient ray-triangle intersection
- **GPU-Accelerated**: All rendering performed in fragment shaders using OpenGL 4.3
- **Next Event Estimation** (NEE) Calculate direct and indirect light separately and acount for bias
- **Multi-threaded BVH Construction**: Parallel tree building for fast scene initialization
- **Russian Roulette Path Termination**: Unbiased variance reduction

### 🎨 Rendering Features
- **Physically-Based Materials**: Support for diffuse, specular, metallic, glass, and emissive materials
- **Textures**: Supports png files, uses .obj uv coords
- **Global Illumination**: Full indirect lighting simulation via path tracing
- **Refractions & Reflections**: Accurate glass and mirror materials with Fresnel equations
- **HDR Environment Maps**: Equirectangular environment map support for realistic lighting
- **Progressive Accumulation**: Temporal anti-aliasing with progressive sample accumulation
- **Post Processing**: Multi-level bloom rendering with ACES tonemap

### 🛠️ Technical Features
- **OBJ Model Loading**: Fast, optimized parser with MTL material support
- **Per-Model Transformations**: Independent position, rotation, and scale for each model
- **Live Material Editing**: Real-time material property adjustments via ImGui interface
- **Save/Load JSON**: Save and load scenes with JSON format
- **Debug Visualization**: BVH traversal heatmaps for performance analysis
- **Reuse Model Data**: Reuse per model BVH and trianlges, and don't send duplicate to GPU

---

## 🏗️ Architecture

### Rendering Pipeline

```
User Input → Scene Setup → BVH Construction → GPU Upload → Shader Raytracing → Accumulation → Post Processing → Display
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
- **Next Event Estimation**: Choose random emissive triangle or sun and compute direct light

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
- **nlohmann/json** (save/load)

## 🎮 Usage

### Basic Scene Setup

```cpp
#include "Scene.h"

int main() {
    // Create scene (samples per frame, AA level, max bounces)
    Scene scene(1, 3, 16);
    
    // add models in editor, save/load json files
    
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
| `L`       | Lock / Unlock cursor    |
| `F`       | Toggle Fullscreen       |
| `Esc`     | Exit application        |

#### UI Controls
- **Model Selection**: left hand panel to select active model
- **Inspector**: right hand panel after selecting model
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
vec3 diffuseColor = vec3(0.9);
float smoothness = 0.7;             // 70% mirror-like
vec3 specularColor = vec3(1.0);
float specularProb = 0.5;           // 50% specular lobes
```

#### Glass (Dielectric)
```cpp
float transparency = 1.0;
float ior = 1.5;                    // glass index of refraction
float transparentSmoothness = 1.0f  // frosted glass
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
| Model Family | 7.2M | 14.3M | 30 | 14.9s |
| 4 Dragons | 3.6M | 8.2M | 45 | 4.2s |
| Stanford Bunny | 144K | 288K | 180 | 0.7s |
| Bugatti Veyron | 1.5M | 2.9M | 60 | 7.7s |
| Lucy | 100K | 199K | 180 | 0.5s |

*Settings: 2560×1440, 1 sample/frame, 5x5 AA, 16 bounces, NEE enabled*

---

## 🔬 Technical Deep Dive

### BVH Construction Algorithm

The renderer uses a Surface Area Heuristic (SAH) based BVH builder with the following optimizations:

1. **Spatial Binning**: Instead of testing every triangle position, we discretize space into bins
2. **Parallel Split Evaluation**: Multi-threaded cost evaluation for all split candidates
3. **Early Termination**: Stop splitting when SAH cost doesn't improve
4. **Tight Bounds**: Per-triangle min/max precomputation for fast bounding box updates

```cpp
float nodeCost(const BVHnode &node) {
    int numTris = node.getNumTri();
    const vec3 size = node.getMax()-node.getMin();
    const float halfArea = size.x * (size.y + size.z) + size.y * size.z;
    return halfArea * float(numTris);
}
```

### Ray-Triangle Intersection

Using the Möller-Trumbore algorithm for watertight intersections:

```glsl
Hit rayTriangleIntersect(Ray ray, vec3 v1, vec3 v2, vec3 v3){
    Hit h;
    h.hit = false;

    const float EPS = 1e-10;

    vec3 e1 = v2 - v1;
    vec3 e2 = v3 - v1;

    vec3 p  = cross(ray.dir, e2);

    float det = dot(e1, p);

    if (abs(det) < EPS) return h;

    float invDet = 1.0/det;
    vec3 tv = ray.pos - v1;

    float u = dot(tv, p) * invDet; if (u < 0.0 || u > 1.0) return h;

    vec3 q = cross(tv, e1);

    float v = dot(ray.dir, q) * invDet; if (v < 0.0 || (u+v) > 1.0) return h;

    float t = dot(e2, q) * invDet;
    if (t < 0.0005) return h;

    h.hit = true;
    h.t = t;
    h.u = u;
    h.v = v;
    h.w = 1-u-v;
    return h;

}
```
---

## 🐛 Known Limitations

- **Single-Level BVH**: No TLAS/BLAS hierarchy for instancing
- **CPU BVH Build**: Scene loading can be slow for very large models
- **No Denoising**: Pure path tracing without ML denoising

---

## 🗺️ Roadmap

- [ ] Two-level BVH for instancing
- [ ] GPU BVH construction
- [ ] OptiX/DXR acceleration
- [ ] OIDN/OptiX denoising integration
- [ ] Volumetric rendering (fog, smoke)
- [ ] glTF model loading

---
