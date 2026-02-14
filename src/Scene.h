//
// Created by acroy on 7/20/2025.
//

#ifndef SCENE_H
#define SCENE_H
#define GLFW_INCLUDE_NONE

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Rendering/ShaderWindow.h"
#include "Rendering/Uniform.h"
#include <glm/gtc/type_ptr.hpp>
#include "Transformation.h"
#include "Material.h"
#include <glm/glm.hpp>
#include <string>
#include "Model.h"
#include <iomanip>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "Rendering/SSBO.h"
#include <map>

#define GLAD_GL_IMPLEMENTATION


struct DataPackageSize {
    int totalSize;
    int triangleDataSize;
    int vertexDataSize;
    int texCoordDataSize;
    int normalDataSize;
    int materialDataSize;
    int BVHnodesDataSize;
    int transformDataSize;
    int textureDataSize;
};

enum DebugMode {
    Normals,
    Heatmap,
    Depth
};


class Scene {
    ShaderWindow window{};

    std::vector<ivec4> triangles;

    std::vector<vec4> vertices;
    std::vector<vec2> texCoords;
    std::vector<vec4> normals;
    std::vector<Material> materials;

    // Model Info

    std::vector<mat4> modelTransforms;
    std::vector<mat4> modelInvTransforms;

    std::map<std::string, std::vector<int>> modelOffsets;

    std::vector<int> models;
    std::vector<vec3> modelPos;
    std::vector<vec3> modelRot;   // radians (x,y,z)
    std::vector<vec3> modelScale;

    std::vector<std::string> modelLabels;
    std::vector<ivec2> modelsMaterialsIdx;
    std::vector<int> modelsTextureID;

    // BVH

    std::vector<BVHnode> BVHnodes;

    // Camera

    vec3 cameraPos{};
    vec3 camForward{};
    vec3 camUp{};
    vec3 camRight{};
    float fovDeg = 60;
    float aperture = 0.0;
    float focusDistance = 1000;
    bool focusDistancePlane = false;

    bool lock;
    float sensitivity = 0.03;
    float speed = 500;

    // Stats

    int frameCount;
    int sampleCount;
    int samples;
    int aa;
    int bounceLim;

    float totalTime = 0;
    float gpuTime = 0;
    float cpuTime = 0;
    float smoothing = 0.9;

    // Sky

    bool skyActive = true;
    vec3 skyColor = vec3(0.5,0.7,0.9);
    vec3 sunDir = normalize(vec3(0.867, 0.498, 0.01));
    float sunStrength = 1000;
    vec3 sunColor = vec3(1, 0.93, 0.31);

    // Floor

    bool floorActive = true;
    vec4 floorDiffuseColor = vec4(1, 1, 1, 0);
    vec4 floorSpecularColor = vec4(0, 0, 0, -1);

    // Debug

    bool debugView = false;
    DebugMode debugMode = Normals;
    int triTh = 5;
    int aabbTh = 100;
    float depthScale = 1500;


    int selectedModel = 0;
    int selectedColor = 0;

    bool hud = true;

    // SSBO

    SSBO<ivec4> ssboTriangles;
    SSBO<vec4> ssboVertices;
    SSBO<vec2> ssboTexCoords;
    SSBO<vec4> ssboNormals;
    SSBO<Material> ssboMaterials;
    SSBO<BVHnode> ssboBVHnodes;
    SSBO<int> ssboModels;
    SSBO<mat4> ssboModelTransformations;
    SSBO<mat4> ssboModelInvTransformations;


    // Uniforms

    Uniform<int> uNumModels;

    Uniform<vec3> uCameraPos;
    Uniform<vec3> uCameraForward;
    Uniform<vec3> uCameraUp;
    Uniform<vec3> uCameraRight;
    Uniform<float> uFovDeg;
    Uniform<float> uAperture;
    Uniform<float> uFocusDistance;
    Uniform<bool> uFocusDistancePlane;

    Uniform<uvec2> uResolution;
    Uniform<int> uFrameCount;
    Uniform<float> uTimeSinceStart;

    Uniform<int> uNumNodes;
    Uniform<int> uSamples;
    Uniform<int> uAA;
    Uniform<int> uBounceLim;

    Uniform<bool> uSkyActive;
    Uniform<vec3> uSkyColor;
    Uniform<vec3> uSunDir;
    Uniform<vec3> uSunColor;

    Uniform<bool> uFloorActive;
    Uniform<vec4> uFloorDiffuseColor;
    Uniform<vec4> uFloorSpecularColor;

    Uniform<bool> uDebugView;
    Uniform<int> uDebugMode;
    Uniform<int> uTriThreshold;
    Uniform<int> uAABBThreshold;
    Uniform<float> uDepthScale;

    Uniform<float> uTextureScales;

    // Textures

    Texture skyTexture;
    Uniform<float> uEnvYaw{};

    std::vector<Texture> textures;
    std::array<float, 64> textureScales{};
    int selectedTexture = 0;
    std::vector<std::string> textureLabels;

    // Keys

    std::map<GLuint, bool> trackedKeysPressed{{GLFW_KEY_L , false}, {GLFW_KEY_H , false}};

    public:

    Scene();
    Scene(int samples, int aa, int bounceLim);

    ~Scene() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void addModel(const std::string &filename, const Transformation& transformation, const Material& material, const std::string& texturePath = "");
    void addModel(const Model& model, const Transformation& transformation, const Material& material, const std::string& texturePath = "");

    [[nodiscard]] int getNumBVHNodes() const;

    [[nodiscard]] int getNumTris() const;

    void createUniforms();

    void setUniforms() const;

    void set_ssbo();

    bool inputHandling(float speed, float sensitivity, float dt);

    void updateFrame();

    void ImGuiRender();


    void displayStats();

    DataPackageSize dataSentSize() const;

    int numTriBelow(int index);

    void get_BVH_stats(int index, int& leafNodes, int& depth, int& minDepth, int& maxDepth, int& triPerLeaf, int& minTriPerLeaf, int& maxTriPerLeaf, int current_depth);

    void displayBVH();

    void displayBVH(int index, std::string prefix);

    [[nodiscard]] bool open() const {
        return window.open();
    }

    void updateItemSmooth(float& item, const float value) const {
        item = smoothing * item + (1.0f - smoothing) * value;
    }
};

#endif //SCENE_H
