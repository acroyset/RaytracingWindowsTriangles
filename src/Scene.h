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
#include "ShaderWindow.h"
#include "Uniform.h"
#include <glm/gtc/type_ptr.hpp>

#define GLAD_GL_IMPLEMENTATION
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "SSBO.h"


class Scene {
    ShaderWindow window{};

    std::vector<vec4> vertices;
    std::vector<ivec4> triangles;
    std::vector<ivec4> normals;
    std::vector<vec4> normalsList;
    std::vector<vec4> colors;
    std::vector<vec4> specularColors;
    std::vector<vec4> glassLightSettings;

    std::vector<mat4> modelTransforms;
    std::vector<mat4> modelInvTransforms;
    int selectedModel = -1;
    int selectedColor = -1;

    std::vector<vec3> modelPos;
    std::vector<vec3> modelRot;   // radians (x,y,z)
    std::vector<vec3> modelScale;
    std::vector<std::string> modelLabels;

    std::vector<vec4> boundingBoxMin;
    std::vector<vec4> boundingBoxMax;
    std::vector<int> childA;
    std::vector<int> childB;

    std::vector<int> models;
    std::vector<ivec2> modelsColors;

    vec3 cameraPos{};
    vec3 camForward{};
    vec3 camUp{};
    vec3 camRight{};

    bool lock;

    vec3 skyColor = vec3(0.5,0.7,0.9);
    vec3 sunDir = normalize(vec3(0.28, 0.33, 0.2));
    float sunStrength = 100;
    vec3 sunColor = vec3(1, .7, .3);

    SSBO<vec4> ssboVertices;
    SSBO<ivec4> ssboTriangles;
    SSBO<vec4> ssboColors;
    SSBO<vec4> ssboSpecularColors;
    SSBO<vec4> ssboGlassLightSettings;
    SSBO<vec4> ssboBoundingBoxMin;
    SSBO<vec4> ssboBoundingBoxMax;
    SSBO<int> ssboChildA;
    SSBO<int> ssboChildB;
    SSBO<int> ssboModels;
    SSBO<vec4> ssboNormalsList;
    SSBO<ivec4> ssboNormals;
    SSBO<mat4> ssboModelTransformations;
    SSBO<mat4> ssboModelInvTransformations;

    bool debugView = false;
    int triTh = 75;
    int aabbTh = 400;

    public:

    int frameCount;
    int sampleCount;
    int width, height;
    int samples;
    int aa;
    int bounceLim;

    Uniform<int> uNumModels;
    Uniform<vec3> uCameraPos;
    Uniform<vec3> uCameraForward;
    Uniform<vec3> uCameraUp;
    Uniform<vec3> uCameraRight;
    Uniform<uvec2> uResolution;
    Uniform<int> uFrameCount;
    Uniform<int> uNumNodes;
    Uniform<int> uSamples;
    Uniform<int> uAA;
    Uniform<int> uBounceLim;
    Uniform<vec3> uSkyColor;
    Uniform<vec3> uSunDir;
    Uniform<vec3> uSunColor;
    Uniform<int> uDebugView;
    Uniform<int> uTriThreshold;
    Uniform<int> uAABBThreshold;

    GLuint skyTex = 0;
    Uniform<int> uEnvLatLong{};
    Uniform<float> uEnvYaw{};

    Scene();
    Scene(int samples, int aa, int bounceLim);

    ~Scene() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void createUniforms();

    void addModel(const std::string &filename, vec3 position, vec3 scale, vec3 color, float smoothness, vec3 specularColor = vec3(-1), float specularProb = 1, float transparency = 0, float ior = 1, float emission = 0);

    void addModel(Model& model, vec3 position, vec3 scale, vec3 color, float smoothness, vec3 specularColor = vec3(-1), float specularProb = 1, float transparency = 0, float ior = 1, float emission = 0);

    void set_ssbo();

    [[nodiscard]] int getNumBVHNodes() const;

    [[nodiscard]] int getNumTris() const;

    void setUniforms(bool moved) const;

    bool updateCamera(GLFWwindow* window, float speed, float sensitivity, float dt);

    void updateFrame();

    void ImGuiRender(float dt);

    int numTriBelow(int index);

    void get_BVH_stats(int index, int& leafNodes, int& depth, int& minDepth, int& maxDepth, int& triPerLeaf, int& minTriPerLeaf, int& maxTriPerLeaf, int current_depth);

    void displayBVH();

    void displayBVH(int index, std::string prefix);

    bool open() {
        return window.open();
    }
};

#endif //SCENE_H
