# ⚡ Real-Time Raytracer

> High-Performance BVH-Accelerated Renderer with OpenGL Backend

[![Millions of Triangles](https://img.shields.io/badge/Triangles-Millions-purple)](https://github.com)
[![Real-Time](https://img.shields.io/badge/Performance-Real--Time-blue)](https://github.com)
[![Interactive](https://img.shields.io/badge/Controls-Interactive-green)](https://github.com)

## ✨ Features

### 🌲 Full BVH Acceleration
Bounding Volume Hierarchy for efficient ray-triangle intersection testing, enabling real-time performance even with complex scenes.

### 🔺 Million Triangle Scenes
Render complex models with millions of polygons in real-time without sacrificing performance.

### ⚙️ OpenGL Backend
Hardware-accelerated rendering pipeline leveraging modern GPU capabilities for maximum performance.

### 🎮 IMGUI Interface
Intuitive GUI controls for adjusting position, rotation, and material properties in real-time.

---

## 🚀 Usage

### Adding Models

Simply use the `addModel()` function in `main.cpp`:

```cpp
// Add models in main.cpp
addModel("models/bunny.obj");
addModel("models/dragon800K.obj");
addModel("models/sponza.obj");
```

---

## 🎮 Controls

### Camera Controls
- **W/A/S/D** - Move forward/left/backward/right
- **Q/E** - Move down/up
- **Mouse** - Look around (point direction)

### IMGUI Panel
- Position adjustment sliders
- Rotation controls (X, Y, Z axes)
- Color & material properties
- Per-model transformation tools

---

## 🎨 Sample Scenes

### Stanford Bunny
**144,000 Triangles**

```cpp
addModel("models/bunny.obj");
```

![Stanford Bunny](https://github.com/user-attachments/assets/27dd7cc8-0da3-4275-bf8b-428c7ca3c0c4)

---

### Dragon High-Poly
**3.6 Million Triangles**

```cpp
addModel("models/dragon800K.obj");
```

![Dragon](https://github.com/user-attachments/assets/3ac3f259-c193-438b-8f7b-7a77fbb2d656)

---

### Dragon + Sponza Palace
**4.1 Million Triangles**

```cpp
addModel("models/dragon800K.obj");
addModel("models/sponza.obj");
```

![Dragon and Sponza](https://github.com/user-attachments/assets/44e7d337-e166-4283-b095-181cd4c8e0ea)

---

### Dragon + Suzanne (Blender Monkey)
**880,000 Triangles**

```cpp
addModel("models/dragon800K.obj");
addModel("models/suzanne.obj");
```

![Dragon and Suzanne](https://github.com/user-attachments/assets/c1242dc8-1644-48ae-ac7c-6fa8cee91f60)

---

### Dragon + Primitives Scene
**890,000 Triangles**

```cpp
addModel("models/dragon800K.obj");
addModel("models/sphere.obj");
addModel("models/cube/cube.obj");
```

![Dragon with Primitives](https://github.com/user-attachments/assets/2aca1800-fce0-4c0b-9561-e57925719d76)

---

### Bugatti Veyron
**1.5 Million Triangles**

```cpp
addModel("models/bugatti/bugatti.obj");
```

![Bugatti Veyron](https://github.com/user-attachments/assets/cdff6480-75de-497c-b0a0-e32f4a416e26)

---

## 🛠️ Technical Details

### Architecture
- **Language**: C++
- **Graphics API**: OpenGL
- **GUI Framework**: Dear ImGui
- **Acceleration Structure**: Bounding Volume Hierarchy (BVH)
- **Model Format**: Wavefront OBJ

### Performance
- Real-time rendering of scenes with 4+ million triangles
- Interactive frame rates maintained through efficient BVH traversal
- Hardware-accelerated ray-triangle intersections

---

## 📊 Scene Complexity

| Scene | Triangle Count | Models |
|-------|----------------|--------|
| Stanford Bunny | 144K | 1 |
| Dragon High-Poly | 3.6M | 1 |
| Dragon + Sponza | 4.1M | 2 |
| Dragon + Suzanne | 880K | 2 |
| Dragon + Primitives | 890K | 3 |
| Bugatti Veyron | 1.5M | 1 |

---

## 🎯 Key Capabilities

- ✅ Real-time raytracing with millions of triangles
- ✅ Interactive camera controls (WASD + mouse)
- ✅ Per-model transformations via IMGUI
- ✅ Efficient BVH-accelerated ray-triangle intersection
- ✅ Multiple model loading and scene composition
- ✅ Dynamic color and material adjustments

---

## 📝 License

Built with C++, OpenGL, and Dear ImGui | Powered by BVH Acceleration Structure

---

*Rendering the impossible, in real-time.* ⚡
