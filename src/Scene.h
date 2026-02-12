//
// Created by acroy on 7/20/2025.
//

#ifndef SCENE_H
#define SCENE_H
#include <glm/glm.hpp>
#include <string>
#include "Model.h"
#define GLFW_INCLUDE_NONE  // Prevent GLFW from including OpenGL headers
#include <glad/glad.h>      // Include glad FIRST
#include <GLFW/glfw3.h>     // Then GLFW
#include "Rendering/ShaderWindow.h"
#include "Rendering/Uniform.h"
#include <glm/gtc/type_ptr.hpp>

#define GLAD_GL_IMPLEMENTATION
#include <map>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "Rendering/SSBO.h"

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
    std::vector<vec4> colors;
    std::vector<vec4> specularColors;
    std::vector<vec4> glassLightSettings;

    std::vector<mat4> modelTransforms;
    std::vector<mat4> modelInvTransforms;

    std::vector<int> models;
    std::vector<vec3> modelPos;
    std::vector<vec3> modelRot;   // radians (x,y,z)
    std::vector<vec3> modelScale;

    std::vector<std::string> modelLabels;
    std::vector<ivec2> modelsColors;
    std::vector<int> modelsTextureID;

    std::vector<vec4> boundingBoxMin;
    std::vector<vec4> boundingBoxMax;
    std::vector<int> childA;
    std::vector<int> childB;


    vec3 cameraPos{};
    vec3 camForward{};
    vec3 camUp{};
    vec3 camRight{};
    float fovDeg = 60;

    bool lock;
    float sensitivity = 0.03;
    float speed = 500;

    int frameCount;
    int sampleCount;
    int samples;
    int aa;
    int bounceLim;

    bool skyActive = true;
    vec3 skyColor = vec3(0.5,0.7,0.9);
    vec3 sunDir = normalize(vec3(0.867, 0.498, 0.01));
    float sunStrength = 200;
    vec3 sunColor = vec3(1, 0.93, 0.31);

    bool debugView = false;
    DebugMode debugMode = Normals;
    int triTh = 5;
    int aabbTh = 100;
    float depthScale = 1500;

    int selectedModel = 0;
    int selectedColor = 0;

    bool hud = true;

    float fps = 0;
    float smoothing = 0.9;

    SSBO<ivec4> ssboTriangles;
    SSBO<vec4> ssboVertices;
    SSBO<vec2> ssboTexCoords;
    SSBO<vec4> ssboNormals;
    SSBO<vec4> ssboColors;
    SSBO<vec4> ssboSpecularColors;
    SSBO<vec4> ssboGlassLightSettings;
    SSBO<vec4> ssboBoundingBoxMin;
    SSBO<vec4> ssboBoundingBoxMax;
    SSBO<int> ssboChildA;
    SSBO<int> ssboChildB;
    SSBO<int> ssboModels;
    SSBO<mat4> ssboModelTransformations;
    SSBO<mat4> ssboModelInvTransformations;

    Uniform<int> uNumModels;

    Uniform<vec3> uCameraPos;
    Uniform<vec3> uCameraForward;
    Uniform<vec3> uCameraUp;
    Uniform<vec3> uCameraRight;
    Uniform<float> uFovDeg;

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

    Uniform<bool> uDebugView;
    Uniform<int> uDebugMode;
    Uniform<int> uTriThreshold;
    Uniform<int> uAABBThreshold;
    Uniform<float> uDepthScale;

    Uniform<float> uTextureScales;


    Texture skyTexture;
    Uniform<float> uEnvYaw{};

    std::vector<Texture> textures;
    std::array<float, 64> textureScales{};
    int selectedTexture = 0;
    std::vector<std::string> textureLabels;

    std::map<GLuint, bool> trackedKeysPressed{{GLFW_KEY_L , false}, {GLFW_KEY_H , false}};

    public:

    Scene();
    Scene(int samples, int aa, int bounceLim);

    ~Scene() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void addModel(const std::string &filename, vec3 position, vec3 scale, vec3 color, float smoothness, vec3 specularColor = vec3(-1), float specularProb = 1, float transparency = 0, float ior = 1, float emission = 0);
    void addModel(const std::string &filename, vec3 position, vec3 scale, const std::string &textureFilename, float smoothness, vec3 specularColor = vec3(-1), float specularProb = 1, float transparency = 0, float ior = 1, float emission = 0);
    void addModel(Model& model, vec3 position, vec3 scale, const std::string &textureFilename, float smoothness, vec3 specularColor = vec3(-1), float specularProb = 1, float transparency = 0, float ior = 1, float emission = 0);

    void addModel(Model& model, vec3 position, vec3 scale, vec3 color, float smoothness, vec3 specularColor = vec3(-1), float specularProb = 1, float transparency = 0, float ior = 1, float emission = 0, const std::string& textureFilename = "");

    [[nodiscard]] int getNumBVHNodes() const;

    [[nodiscard]] int getNumTris() const;

    void createUniforms();

    void setUniforms() const;

    void set_ssbo();

    bool inputHandling(float speed, float sensitivity, float dt);

    void updateFrame();

    void ImGuiRender();


    int numTriBelow(int index);

    void get_BVH_stats(int index, int& leafNodes, int& depth, int& minDepth, int& maxDepth, int& triPerLeaf, int& minTriPerLeaf, int& maxTriPerLeaf, int current_depth);

    void displayBVH();

    void displayBVH(int index, std::string prefix);

    [[nodiscard]] bool open() const {
        return window.open();
    }

    void updateFPS(const float dt)
    {
        float current = 1.0f / dt;
        fps = smoothing * fps + (1.0f - smoothing) * current;
    }
};

#endif //SCENE_H
