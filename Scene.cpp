// Scene.cpp

#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>
#include "Scene.h"
#include <iostream>
#include <fstream>
#include <GLFW/glfw3.h>
#include <chrono>
#include "BaseModel.h"

using Clock = std::chrono::high_resolution_clock;

auto start = Clock::now();

void setBasisVectors(const glm::vec3& forward, glm::vec3& up, glm::vec3& right) {
    constexpr glm::vec3 world_up(0, 1, 0);
    right = glm::normalize(glm::cross(forward, world_up));
    up = glm::normalize(glm::cross(right, forward));
}

Scene::Scene() {
    samples = 1;
    aa = 1;
    bounceLim = 8;

    width = 2560;
    height = 1440;

    frameCount = 0;

    lock = false;
}

Scene::Scene(const int width, const int height, const int samples, const int aa, const int bounceLim)
    : samples(samples), aa(aa), bounceLim(bounceLim), frameCount(0), width(width), height(height){
    camForward = glm::vec3(0, 0, -1);
    setBasisVectors(camForward, camUp, camRight);

    lock = false;
}

void Scene::addModel(const std::string& filename, const glm::vec3 position, const glm::vec3 scale, const glm::vec3 color, const float smoothness, const float emission) {
    BaseModel model(filename);

    addModel(model, position, scale, color, smoothness, emission);
}

void Scene::addModel(BaseModel& model, glm::vec3 position, glm::vec3 scale, glm::vec3 color, float smoothness, float emission) {
    int Voffset = int(vertices.size());
    int Toffset = int(triangles.size());
    int BBoffset = int(boundingBoxMin.size());

    models.emplace_back(BBoffset);

    for (glm::vec3 vertex : model.vertices) {
        vertex *= scale;
        vertex += position;
        vertices.emplace_back(vertex, 0);
    }
    for (glm::ivec3 triangle : model.triangles) {
        triangle += Voffset;
        triangles.emplace_back(triangle, colors.size());
    }
    for (glm::vec4 bboxMin : model.boundingBoxMin) {
        bboxMin *= glm::vec4(scale,1);
        int offset = bboxMin.w <= 0 ? Toffset : BBoffset;
        bboxMin += glm::vec4(position,offset);
        boundingBoxMin.emplace_back(bboxMin);
    }
    for (glm::vec4 bboxMax : model.boundingBoxMax) {
        bboxMax *= glm::vec4(scale,1);
        int offset = bboxMax.w <= 0 ? 0 : BBoffset;
        bboxMax += glm::vec4(position,offset);
        boundingBoxMax.emplace_back(bboxMax);
    }

    colors.emplace_back(color, smoothness);
    this->emission.push_back(emission);
}

void Scene::set_ssbo() const {
    GLuint ssboVertices;
    glGenBuffers(1, &ssboVertices);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboVertices);
    glBufferData(GL_SHADER_STORAGE_BUFFER, int(vertices.size() * sizeof(glm::vec4)), vertices.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboVertices);

    GLuint ssboTriangles;
    glGenBuffers(1, &ssboTriangles);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboTriangles);
    glBufferData(GL_SHADER_STORAGE_BUFFER, int(triangles.size() * sizeof(glm::ivec4)), triangles.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboTriangles);

    GLuint ssboColors;
    glGenBuffers(1, &ssboColors);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboColors);
    glBufferData(GL_SHADER_STORAGE_BUFFER, int(colors.size() * sizeof(glm::vec4)), colors.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssboColors);

    GLuint ssboEmission;
    glGenBuffers(1, &ssboEmission);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboEmission);
    glBufferData(GL_SHADER_STORAGE_BUFFER, int(emission.size() * sizeof(float)), emission.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, ssboEmission);

    GLuint ssboBoundingBoxMin;
    glGenBuffers(1, &ssboBoundingBoxMin);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboBoundingBoxMin);
    glBufferData(GL_SHADER_STORAGE_BUFFER, int(boundingBoxMin.size() * sizeof(glm::vec4)), boundingBoxMin.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, ssboBoundingBoxMin);

    GLuint ssboBoundingBoxMax;
    glGenBuffers(1, &ssboBoundingBoxMax);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboBoundingBoxMax);
    glBufferData(GL_SHADER_STORAGE_BUFFER, int(boundingBoxMax.size() * sizeof(glm::vec4)), boundingBoxMax.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, ssboBoundingBoxMax);

    GLuint ssboModels;
    glGenBuffers(1, &ssboModels);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboModels);
    glBufferData(GL_SHADER_STORAGE_BUFFER, int(models.size() * sizeof(int)), models.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, ssboModels);
}

int Scene::getNumBVHNodes() const {
    return int(boundingBoxMin.size());
}

int Scene::getNumTris() const {
    return int(triangles.size());
}

void Scene::setUniforms(const GLuint shaderProgram) const {
    const auto end = Clock::now();
    const glm::uint duration = static_cast<glm::uint>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());

    glUniform1i(glGetUniformLocation(shaderProgram, "numModels"), int(models.size()));
    glUniform3f(glGetUniformLocation(shaderProgram, "cameraPos"), cameraPos.x, cameraPos.y, cameraPos.z);
    glUniform3f(glGetUniformLocation(shaderProgram, "camForward"), camForward.x, camForward.y, camForward.z);
    glUniform3f(glGetUniformLocation(shaderProgram, "camUp"), camUp.x, camUp.y, camUp.z);
    glUniform3f(glGetUniformLocation(shaderProgram, "camRight"), camRight.x, camRight.y, camRight.z);
    glUniform2f(glGetUniformLocation(shaderProgram, "resolution"), static_cast<float>(width), static_cast<float>(height));
    glUniform1i(glGetUniformLocation(shaderProgram, "frameCount"), frameCount);
    glUniform1i(glGetUniformLocation(shaderProgram, "numNodes"), getNumBVHNodes());
    glUniform1i(glGetUniformLocation(shaderProgram, "samples"), samples);
    glUniform1i(glGetUniformLocation(shaderProgram, "aa"), aa);
    glUniform1i(glGetUniformLocation(shaderProgram, "bounceLim"), bounceLim);
    glUniform1ui(glGetUniformLocation(shaderProgram, "time"), duration);
    glUniform3f(glGetUniformLocation(shaderProgram, "skyColor"), skyColor.x, skyColor.y, skyColor.z);
    glUniform3f(glGetUniformLocation(shaderProgram, "sunDir"), sunDir.x, sunDir.y, sunDir.z);
    glUniform3f(glGetUniformLocation(shaderProgram, "sunColor"), sunStrength*sunColor.x, sunStrength*sunColor.y, sunStrength*sunColor.z);
}

bool Scene::updateCamera(GLFWwindow& window, float speed, float sensitivity, float dt) {
    double xpos, ypos;
    bool moved = false;
    glfwGetCursorPos(&window, &xpos, &ypos);
    glm::vec2 center = glm::vec2(float(width)/2, float(height)/2);
    glm::vec2 delta = glm::vec2(xpos - center.x, -(ypos - center.y));
    if (delta.x*delta.x + delta.y*delta.y > 0 and !lock) {
        delta *= 2.0f/float(height) * sensitivity;
        camForward += delta.x * camRight + delta.y * camUp;
        camForward = glm::normalize(camForward);
        moved = true;
        setBasisVectors(camForward, camUp, camRight);
        glfwSetCursorPos(&window, center.x, center.y);
    }

    glm::vec3 change = glm::vec3(0, 0, 0);
    if (glfwGetKey(&window, GLFW_KEY_W) == GLFW_PRESS) {
        change += camForward;
    }
    if (glfwGetKey(&window, GLFW_KEY_S) == GLFW_PRESS) {
        change -= camForward;
    }
    if (glfwGetKey(&window, GLFW_KEY_A) == GLFW_PRESS) {
        change -= camRight;
    }
    if (glfwGetKey(&window, GLFW_KEY_D) == GLFW_PRESS) {
        change += camRight;
    }
    if (glfwGetKey(&window, GLFW_KEY_E) == GLFW_PRESS) {
        change += camUp;
    }
    if (glfwGetKey(&window, GLFW_KEY_Q) == GLFW_PRESS) {
        change -= camUp;
    }
    if (glfwGetKey(&window, GLFW_KEY_L) == GLFW_PRESS) {
        lock = true;
    }
    if (glfwGetKey(&window, GLFW_KEY_U) == GLFW_PRESS) {
        lock = false;
        glfwSetCursorPos(&window, center.x, center.y);
    }
    if (glfwGetKey(&window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
       glfwGetKey(&window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
        speed *= 2;
       }
    if (pow(change.x, 2) + pow(change.y, 2) + pow(change.z, 2) > 0 and !lock) {
        change = glm::normalize(change);
        cameraPos += change*speed*dt;
        moved = true;
    }
    return moved;
}

void Scene::updateFrame(const GLuint shaderProgram, GLFWwindow& window, float dt) {
    const bool moved = updateCamera(window, 500, 2, dt);

    setUniforms(shaderProgram);

    frameCount++;

    if (moved) frameCount = 0;
}

int Scene::numTriBelow(int index) {
    const glm::vec4 bboxMin = boundingBoxMin[index];
    const glm::vec4 bboxMax = boundingBoxMax[index];
    if (int(bboxMax.w) > index and int(bboxMin.w) > index) {
        return numTriBelow(int(bboxMin.w)) + numTriBelow(int(bboxMax.w));
    }
    return -int(bboxMax.w);
}

void Scene::get_BVH_stats(int index, int& leafNodes, int& depth, int& minDepth, int& maxDepth, int& triPerLeaf, int& minTriPerLeaf, int& maxTriPerLeaf, int current_depth) {
    glm::vec4 bboxMin = boundingBoxMin[index];
    glm::vec4 bboxMax = boundingBoxMax[index];
    if (bboxMin.w > 0.0f) {
        get_BVH_stats(int(bboxMin.w), leafNodes, depth, minDepth, maxDepth, triPerLeaf, minTriPerLeaf, maxTriPerLeaf, current_depth+1);
        get_BVH_stats(int(bboxMax.w), leafNodes, depth, minDepth, maxDepth, triPerLeaf, minTriPerLeaf, maxTriPerLeaf, current_depth+1);
        return;
    }
    int triStart = -int(bboxMin.w);
    int numTris = -int(bboxMax.w);
    leafNodes++;
    depth += current_depth;
    triPerLeaf += numTris;
    if (current_depth > maxDepth) maxDepth = current_depth;
    if (current_depth < minDepth) minDepth = current_depth;
    if (numTris > maxTriPerLeaf) maxTriPerLeaf = numTris;
    if (numTris < minTriPerLeaf) minTriPerLeaf = numTris;
}

void Scene::displayBVH() {
    for (const int model : models) {
        const std::string prefix;
        displayBVH(model, prefix);
    }
}

void Scene::displayBVH(int index, std::string prefix) {
    glm::vec4 bboxMin = boundingBoxMin[index];
    glm::vec4 bboxMax = boundingBoxMax[index];
    int numTris = numTriBelow(index);
    if (numTris < 5000) return;
    std::cout << prefix << "Index: " << index << "  -  Tris: " << numTris << std::endl;
    std::cout << prefix << "Bounding Box Min: " << bboxMin.x << ", " << bboxMin.y << ", " << bboxMin.z << ", " << bboxMin.w << std::endl;
    std::cout << prefix << "Bounding Box Max: " << bboxMax.x << ", " << bboxMax.y << ", " << bboxMax.z << ", " << bboxMax.w << std::endl;
    if (bboxMax.w > index and bboxMin.w > index) {
        prefix += "  ";
        displayBVH(int(bboxMin.w), prefix);
        std::cout << std::endl;
        displayBVH(int(bboxMax.w), prefix);
        return;
    }
    int triStart = -int(bboxMin.w);
    std::cout << prefix << "Triangles: " << numTris << std::endl;
    return;
    prefix += "   ";
    for (int i = triStart; i < numTris+triStart; i++) {
        glm::vec3 v1 = vertices[triangles[i*3+0].x];
        glm::vec3 v2 = vertices[triangles[i*3+1].x];
        glm::vec3 v3 = vertices[triangles[i*3+2].x];
        bool check =
            v1.x >= bboxMin.x && v1.x <= bboxMax.x &&
            v2.x >= bboxMin.x && v2.x <= bboxMax.x &&
            v3.x >= bboxMin.x && v3.x <= bboxMax.x &&
            v1.y >= bboxMin.y && v1.y <= bboxMax.y &&
            v2.y >= bboxMin.y && v2.y <= bboxMax.y &&
            v3.y >= bboxMin.y && v3.y <= bboxMax.y &&
            v1.z >= bboxMin.z && v1.z <= bboxMax.z &&
            v2.z >= bboxMin.z && v2.z <= bboxMax.z &&
            v3.z >= bboxMin.z && v3.z <= bboxMax.z;
        std::cout << prefix << (check ? "In" : "--Out--") << " ";
        std::cout << v1.x << " " << v1.y << " " << v1.z << "  -  ";
        std::cout << v2.x << " " << v2.y << " " << v2.z << "  -  ";
        std::cout << v3.x << " " << v3.y << " " << v3.z << std::endl;
    }
}
