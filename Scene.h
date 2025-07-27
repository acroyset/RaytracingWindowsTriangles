//
// Created by acroy on 7/20/2025.
//

#ifndef SCENE_H
#define SCENE_H
#include <glm/glm.hpp>
#include <string>
#include <GLFW/glfw3.h>


class Scene {
    std::vector<glm::vec4> vertices;
    std::vector<glm::ivec4> triangles;
    std::vector<glm::vec4> colors;
    std::vector<float> emission;

    std::vector<glm::vec4> boundingBoxMin;
    std::vector<glm::vec4> boundingBoxMax;

    std::vector<int> models;

    int samples;
    int aa;
    int bounceLim;

    glm::vec3 cameraPos{};
    glm::vec3 camForward{};
    glm::vec3 camUp{};
    glm::vec3 camRight{};

    bool lock;

    int frameCount;
    int width, height;

    glm::vec3 skyColor = glm::vec3(0.5,0.7,0.9);
    glm::vec3 sunDir = glm::vec3(-0.1, 1, 0.1);
    float sunStrength = 10;
    glm::vec3 sunColor = glm::vec3(100, 70, 30);

    public:
    Scene();
    Scene(int width, int height, int samples, int aa, int bounceLim);

    static void parse(const std::string& nfilename, glm::vec3 position, glm::vec3 scale, std::vector<glm::vec3>& vertices, std::vector<glm::ivec3>& triangles);

    void addModel(const ::std::string &filename, glm::vec3 position, glm::vec3 scale, glm::vec3 color, float smoothness, float emission);

    [[nodiscard]] float evaluateSplit(glm::vec4 min, glm::vec4 max, int axis, float pos) const;

    void chooseSplit(int numTestsPerAxis, glm::vec4 min, glm::vec4 max, int& bestAxis, float& bestPos, float& bestCost) const;

    void split(int numTestsPerAxis, glm::vec4& bboxMin, glm::vec4& bboxMax, int depth);

    void createBVH(int depth, int numTestsPerAxis, int triStart, int numTris);

    void set_ssbo() const;

    [[nodiscard]] int getNumBVHNodes() const;

    [[nodiscard]] int getNumTris() const;

    void setUniforms(GLuint shaderProgram) const;

    bool updateCamera(GLFWwindow& window, float speed, float sensitivity, float dt);

    void updateFrame(GLuint shaderProgram, GLFWwindow& window, float dt);

    int numTriBelow(int index);

    void get_BVH_stats(int index, int& leafNodes, int& depth, int& minDepth, int& maxDepth, int& triPerLeaf, int& minTriPerLeaf, int& maxTriPerLeaf, int current_depth);

    void displayBVH(int index, std::string prefix);
};

#endif //SCENE_H
