//
// Created by acroy on 7/20/2025.
//

#ifndef SCENE_H
#define SCENE_H
#define GLFW_INCLUDE_NONE

#include <deque>
#include <future>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Rendering/ShaderWindow.h"
#include "Rendering/Uniform.h"
#include <glm/gtc/type_ptr.hpp>
#include "Transformation.h"
#include "Material.h"
#include <glm/glm.hpp>
#include <string>
#include "BaseModel.h"
#include <iomanip>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "Rendering/SSBO.h"
#include <map>
#include "Model.h"
#include "JSONextensions.h"
#include "SceneUI.h"
#include <OpenImageDenoise/oidn.hpp>

#include "BloomPass.h"
#include "Camera.h"
#include "Rendering/Shader.h"
#include "Camera.h"

#define GLAD_GL_IMPLEMENTATION


struct DataPackage {
    int totalSize;

    int trianglesSent;
    int triangles;
    int triangleBytes;

    int verticesSent;
    int vertices;
    int verticesBytes;

    int texCoordsSent;
    int texCoords;
    int texCoordsBytes;

    int normalsSent;
    int normals;
    int normalsBytes;

    int BVHNodesSent;
    int BVHNodes;
    int BVHNodesBytes;

    int models;
    int modelsBytes;

    int emissiveModels;
    int emissiveModelsBytes;

    int emissiveTriangles;
    int emissiveTrianglesBytes;

    int materials;
    int materialsBytes;

    int transforms;
    int transformsBytes;

    int textures;
    int texturesBytes;
};

struct ModelOffset {
    int triangle;
    int vertex;
    int texCoord;
    int normal;
    int BVHnodes;
    int material;
    int textureID;

    int padding = 0;

    ModelOffset(int triangle, int vertex, int texCoord, int normal, int BVHnodes, int material, int textureID) {
        this->triangle = triangle;
        this->vertex = vertex;
        this->texCoord = texCoord;
        this->normal = normal;
        this->BVHnodes = BVHnodes;
        this->material = material;
        this->textureID = textureID;
    }
};

enum DebugMode {
    Normals,
    Heatmap,
    Depth
};

class SceneUI;


class Scene {
    friend SceneUI;

    ShaderWindow window{"Raytracer"};
    Shader raytracer{"shaders/fullscreen.vert", "shaders/raytracer.frag", window.getGLSLVersion(), {"shaders/structs.glsl"}};
    BloomPass bloom;
    Shader postProcessing{"shaders/fullscreen.vert", "shaders/postProcessing.frag", window.getGLSLVersion()};

    SceneUI ui{};
    bool isOpen = true;

    std::vector<ivec4> triangles;

    std::vector<vec4> vertices;
    std::vector<vec2> texCoords;
    std::vector<vec4> normals;

    // Model Info

    std::map<std::string, BaseModel> precomputedModels;
    std::vector<Model> models;

    // Model Info

    std::vector<ModelOffset> modelOffsets;

    // BVH

    std::vector<BVHnode> BVHnodes;

    // NEE

    bool NEE = true;
    std::vector<ivec2> emissiveTris;
    std::vector<ivec2> emissiveModelTriangleNum;
    std::atomic<bool> emissiveTrisStale = false;

    // Camera
    Camera camera;

    bool lock = true;
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
    std::deque<float> fpsData;

    // Sky

    bool skyActive = true;
    vec3 skyColor = vec3(0.5,0.7,0.9);
    vec3 sunDir = normalize(vec3(0.867, 0.498, 0.01));
    float sunStrength = 1000;
    vec3 sunColor = vec3(1, 0.93, 0.31);

    // Floor
    bool floorActive = true;
    Material floorMaterial;

    // Debug

    bool debugView = false;
    DebugMode debugMode = Normals;
    int triTh = 5;
    int aabbTh = 100;
    float depthScale = 1500;


    // SSBO

    SSBO<ivec4> ssboTriangles;
    SSBO<vec4> ssboVertices;
    SSBO<vec2> ssboTexCoords;
    SSBO<vec4> ssboNormals;
    SSBO<Material> ssboMaterials;
    SSBO<BVHnode> ssboBVHnodes;
    SSBO<ModelOffset> ssboModelOffsets;
    SSBO<mat4> ssboModelTransformations;
    SSBO<mat4> ssboModelInvTransformations;
    SSBO<ivec2> ssboEmissiveTris;
    SSBO<ivec2> ssboEmissiveModelTriangleNum;

    DataPackage lastSentPackage{};


    // Uniforms

    Uniform<int> uNumModels;
    Uniform<bool> uNEE;
    Uniform<int> uNumEmissiveModels;
    Uniform<int> uNumEmissiveTris;

    UniformBlock<Camera> uCamera;

    Uniform<uvec2> uResolutionRTX;
    Uniform<uvec2> uResolutionPP;
    Uniform<int> uFrameCount;
    Uniform<float> uTimeSinceStart;
    Uniform<int> uSampleCount;

    Uniform<int> uNumNodes;
    Uniform<int> uSamples;
    Uniform<int> uAA;
    Uniform<int> uBounceLim;

    Uniform<bool> uSkyActive;
    Uniform<vec3> uSkyColor;
    Uniform<vec3> uSunDir;
    Uniform<vec3> uSunColor;

    Uniform<bool> uFloorActive;
    UniformBlock<Material> uFloorMaterial;

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

    std::vector<std::pair<std::string, int>> pendingTextures;
    bool pendingClearTextures = false;

    // Scene save / load / add

    std::future<void> job;
    std::atomic<bool> isBusy{};

    std::string busyLabel;
    std::string statusMsg;
    bool statusIsError;

    std::atomic<bool> newData{};
    std::atomic<float> progress{};
    std::atomic<float> progressMax{};

    std::thread::id mainThreadID = std::this_thread::get_id();

    // Keys

    std::map<GLuint, bool> trackedKeysPressed{};

    public:

    Scene();
    Scene(int samples, int aa, int bounceLim);

    ~Scene() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void addModel(const std::string &filename, const Transformation& transformation, const std::string& texturePath = "");
    void addModel(const Model& model, const std::string& texturePath = "");

    void removeModel(int index);

    [[nodiscard]] int getNumBVHNodes() const;

    [[nodiscard]] int getNumTris() const;

    [[nodiscard]] int getNumMaterials() const;

    void createUniforms();

    void setUniformsRTX() const;
    void setUniformsPP() const;

    void set_ssbo();

    bool inputHandling(float speed, float sensitivity, float dt);

    void updateFrame();


    void displayStats() const;

    [[nodiscard]] DataPackage dataSent() const;


    void saveJSON(const std::string& filename) const;
    void loadJSON(const std::string& filename);

    int numTriBelow(int index);

    void get_BVH_stats(int index, int& leafNodes, int& depth, int& minDepth, int& maxDepth, int& triPerLeaf, int& minTriPerLeaf, int& maxTriPerLeaf, int current_depth);

    void displayBVH();

    void displayBVH(int index, std::string prefix);

    [[nodiscard]] bool open() const {
        return window.open() && isOpen;
    }

    void resetAccumulation() {
        frameCount = 0;
        sampleCount = 0;
    }

    void updateItemSmooth(float& item, const float value) const {
        item = smoothing * item + (1.0f - smoothing) * value;
    }

    void startAddJob(const std::string& path, const std::string& texturePath);

    void startLoadJob(const std::string& path);

    void reloadEmissiveTris();

    void reloadShaders() {
        window.reload();
        createUniforms();
        resetAccumulation();
    }

    void savePNG(const std::string& filename);
};

#endif //SCENE_H
