//
// Created by acroy on 7/20/2025.
//

#ifndef SCENE_H
#define SCENE_H
#include <glm/glm.hpp>
#include <string>
#include <GLFW/glfw3.h>
#include "BaseModel.h"

class Scene {
    std::vector<glm::vec4> vertices;
    std::vector<glm::ivec4> triangles;
    std::vector<glm::ivec4> normals;
    std::vector<glm::vec4> normalsList;
    std::vector<glm::vec4> colors;
    std::vector<glm::vec4> specularColors;
    std::vector<glm::vec4> glassLightSettings;

    std::vector<glm::mat4> modelTransforms;
    int selectedModel = -1;
    int selectedColor = -1;

    std::vector<glm::vec3> modelPos;
    std::vector<glm::vec3> modelRot;   // radians (x,y,z)
    std::vector<glm::vec3> modelScale;
    std::vector<std::string> modelLabels;

    std::vector<glm::vec4> boundingBoxMin;
    std::vector<glm::vec4> boundingBoxMax;
    std::vector<int> childA;
    std::vector<int> childB;

    std::vector<int> models;
    std::vector<glm::ivec2> modelsColors;

    glm::vec3 cameraPos{};
    glm::vec3 camForward{};
    glm::vec3 camUp{};
    glm::vec3 camRight{};

    bool lock;

    glm::vec3 skyColor = glm::vec3(0.5,0.7,0.9);
    glm::vec3 sunDir = glm::normalize(glm::vec3(0.28, 0.33, 0.2));
    float sunStrength = 100;
    glm::vec3 sunColor = glm::vec3(1, .7, .3);

    GLuint ssboColors;
    GLuint ssboSpecularColors;
    GLuint ssboGlassLightSettings;
    GLuint ssboModelTransformations = 0;

    bool debugView = false;
    int triTh = 75;
    int aabbTh = 400;

    public:

    int frameCount;
    int width, height;
    int samples;
    int aa;
    int bounceLim;

    Scene();
    Scene(int width, int height, int samples, int aa, int bounceLim);

    void addModel(const std::string &filename, glm::vec3 position, glm::vec3 scale, glm::vec3 color, float smoothness, glm::vec3 specularColor = glm::vec3(-1), float specularProb = 1, float transparency = 0, float ior = 1, float emission = 0);

    void addModel(BaseModel& model, glm::vec3 position, glm::vec3 scale, glm::vec3 color, float smoothness, glm::vec3 specularColor = glm::vec3(-1), float specularProb = 1, float transparency = 0, float ior = 1, float emission = 0);

    void set_ssbo();

    [[nodiscard]] int getNumBVHNodes() const;

    [[nodiscard]] int getNumTris() const;

    void setUniforms(GLuint shaderProgram) const;

    bool updateCamera(GLFWwindow& window, float speed, float sensitivity, float dt);

    void updateFrame(GLuint shaderProgram, GLFWwindow& window, float dt);

    int numTriBelow(int index);

    void get_BVH_stats(int index, int& leafNodes, int& depth, int& minDepth, int& maxDepth, int& triPerLeaf, int& minTriPerLeaf, int& maxTriPerLeaf, int current_depth);

    void displayBVH();

    void displayBVH(int index, std::string prefix);
};

#endif //SCENE_H
