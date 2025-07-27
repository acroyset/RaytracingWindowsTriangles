// Scene.cpp

#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>
#include "Scene.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <limits>
#include <GLFW/glfw3.h>
#include <chrono>

using Clock = std::chrono::high_resolution_clock;

auto start = Clock::now();

void splitSlash(const std::string& s, std::string tokens[3]) {
    std::string token;
    int i = 0;
    int loc = 0;
    int size = int(s.size());
    while (i < size) {
        token.clear();
        char c = s[i];
        bool empty = true;
        while (i < size and c != '/') {
            empty = false;
            token += c;
            i++;
            c = s[i];
        }
        if (!empty) {
            tokens[loc] = token;
        }
        loc++;
        i++;
    }
}
void splitSpace3(const std::string& s, std::string words[3]) {
    std::string word;
    int loc = 0;
    int i = 0;
    const int size = int(s.size());
    while (i < size) {
        word.clear();
        char c = s[i];
        while (i < size and c != ' ') {
            word += c;
            i++;
            c = s[i];
        }
        if (!word.empty()) {
            words[loc] = word;
            loc++;
        }
        i++;
    }
}
void splitSpace4(const std::string& s, std::string words[4], int& num) {
    std::string word;
    num = 0;
    int i = 0;
    const int size = int(s.size());
    while (i < size) {
        word.clear();
        char c = s[i];
        while (i < size and c != ' ') {
            word += c;
            i++;
            c = s[i];
        }
        if (!word.empty()) {
            words[num] = word;
            num++;
        }
        i++;
    }
}
void growToInclude(glm::vec3& min, glm::vec3& max, const glm::vec3 point) {
    if (point.x < min.x) min.x = point.x;
    if (point.y < min.y) min.y = point.y;
    if (point.z < min.z) min.z = point.z;
    if (point.x > max.x) max.x = point.x;
    if (point.y > max.y) max.y = point.y;
    if (point.z > max.z) max.z = point.z;
}
void growToInclude(glm::vec4& min, glm::vec4& max, const glm::vec3 point) {
    if (point.x < min.x) min.x = point.x;
    if (point.y < min.y) min.y = point.y;
    if (point.z < min.z) min.z = point.z;
    if (point.x > max.x) max.x = point.x;
    if (point.y > max.y) max.y = point.y;
    if (point.z > max.z) max.z = point.z;
}
void makeBoundingBox(glm::vec3& min, glm::vec3& max, const std::vector<glm::vec3>& vertices) {
    for (const auto & vertice : vertices) {
        growToInclude(min, max, vertice);
    }
}
void makeBoundingBox(glm::vec3& min, glm::vec3& max, const std::vector<glm::vec4>& vertices) {
    for (glm::vec4 vertice : vertices) {
        growToInclude(min, max, xyz(vertice));
    }
}
void center(std::vector<glm::vec3>& points) {
    auto min = glm::vec3(1000000000.0f), max = glm::vec3(-1000000000.0f);
    makeBoundingBox(min, max, points);

    const glm::vec3 offset = {(max.x+min.x)/2, (max.y+min.y)/2, (max.z+min.z)/2};
    const float biggestDiff = fmax(fmax(max.x-min.x, max.y-min.y), max.z-min.z);
    const float scaler = 2/biggestDiff;

    for (glm::vec3 &point : points) {
        point -= offset;
        point *= scaler;
    }
}
void createNormals(const std::vector<glm::vec3>& vertices, std::vector<glm::vec3>& normals, std::vector<glm::ivec3>& triangles) {
    normals.resize(vertices.size());
    std::vector<int> num(vertices.size());

    for (int i = 0; i < triangles.size()/3; ++i) {
        const glm::ivec3 v = {triangles[3*i+0].x, triangles[3*i+1].x, triangles[3*i+2].x};
        const glm::vec3 a = vertices[v.x];
        const glm::vec3 b = vertices[v.y];
        const glm::vec3 c = vertices[v.z];
        triangles[3*i+0].z = v.x;
        triangles[3*i+1].z = v.y;
        triangles[3*i+2].z = v.z;
        const glm::vec3 AB = b-a;
        const glm::vec3 AC = c-a;
        glm::vec3 normal = {
            AB.y * AC.z - AB.z * AC.y,
            AB.z * AC.x - AB.x * AC.z,
            AB.x * AC.y - AB.y * AC.x
        };
        normal = glm::normalize(normal);

        normals[v.x] += normal;
        num[v.x] ++;
        normals[v.y] += normal;
        num[v.y] ++;
        normals[v.z] += normal;
        num[v.z] ++;
    }

    for (int i = 0; i < normals.size(); ++i) {
        normals[i] /= num[i];
        normals[i] = glm::normalize(normals[i]);
    }
    }
void setBasisVectors(const glm::vec3& forward, glm::vec3& up, glm::vec3& right) {
    constexpr glm::vec3 world_up(0, 1, 0);
    right = glm::normalize(glm::cross(forward, world_up));
    up = glm::normalize(glm::cross(right, forward));
}
float nodeCost(const glm::vec4 min, const glm::vec4 max) {
    const glm::vec3 size = xyz(max-min);
    const float halfArea = size.x * (size.y + size.z) + size.y * size.z;
    return halfArea * -max.w;
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

Scene::Scene(int width, int height, int samples, int aa, int bounceLim)
    : samples(samples), aa(aa), bounceLim(bounceLim), frameCount(0), width(width), height(height){
    camForward = glm::vec3(0, 0, -1);
    setBasisVectors(camForward, camUp, camRight);

    lock = false;
}

void Scene::parse(const std::string& nfilename, const glm::vec3 position, const glm::vec3 scale, std::vector<glm::vec3>& vertices, std::vector<glm::ivec3>& triangles) {
    const std::string filename = "" + nfilename;
    std::ifstream model(filename, std::ios::binary | std::ios::ate);

    if (!model.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    std::streamsize size = model.tellg();
    std::cout << size << std::endl;
    model.seekg(0, std::ios::beg);
    std::vector<char> buffer(size + 1);  // Add 1 for null-terminator
    model.read(buffer.data(), size);

    buffer[size] = '\0';  // Null-terminate for safe parsing

    char* ptr = buffer.data();
    char* end = ptr + size;
    std::string start;
    bool prefix;

    int quads = 0;

    std::string line;
    while (ptr < end) {
        line.clear();
        start.clear();
        prefix = true;
        while (*ptr != '\n' and *ptr != '\r' and ptr < end) {
            if (prefix) {
                start += *ptr;
            }
            else {
                line += *ptr;
            }
            if (*ptr == ' ') {
                prefix = false;
            }
            ptr++;
        }
        ptr++;

        if (start == "v ") {
            std::string parts[3];
            splitSpace3(line, parts);
            float x = std::stof(parts[0]);
            float y = std::stof(parts[1]);
            float z = std::stof(parts[2]);
            auto p = glm::vec3(x, y, z);
            vertices.emplace_back(p);
        } // vertex line
        else if (start == "f ") {
            std::string parts[4];
            int s;
            splitSpace4(line, parts, s);
            for (int i = 0; i < s; ++i) {
                std::string indexGroup[3];
                indexGroup[0] = "0";
                indexGroup[1] = "0";
                indexGroup[2] = "0";
                splitSlash(parts[i], indexGroup);

                int pointIndex = std::stoi(indexGroup[0]);
                int textureIndex = std::stoi(indexGroup[1]);
                int normalIndex = std::stoi(indexGroup[2]);

                // triangle fan for n-gon
                if (i >= 3) {
                    quads++;
                    triangles.emplace_back(triangles[triangles.size() - (3 * i - 6)]);
                    triangles.emplace_back(triangles[triangles.size() - 2]);
                }

                triangles.emplace_back(pointIndex, textureIndex, normalIndex);
            }
        } // face line
    }

    for (glm::ivec3& i : triangles) {
        if (i.x < 0) i.x += vertices.size()+1;
        if (i.y < 0) i.y += vertices.size()+1;
        if (i.z < 0) i.z += vertices.size()+1;
        i.x --;
        i.y --;
        i.z --;
        if (i.x > vertices.size()) std::cout << "Error" << std::endl;
        //if (i.y > vertices.size()) std::cout << "Error" << std::endl;
        //if (i.z > vertices.size()) std::cout << "Error" << std::endl;
    }

    center(vertices);

    for (glm::vec3 &point : vertices) {
        point *= scale;
        point += position;
    }
}

void Scene::addModel(const std::string& filename, glm::vec3 position, glm::vec3 scale, glm::vec3 color, float smoothness, float emission) {
    int Voffset = int(vertices.size());

    auto min = glm::vec3(1000000000.0f);
    auto max = glm::vec3(-1000000000.0f);
    models.emplace_back(boundingBoxMin.size());

    std::vector<glm::vec3> tempVertices;
    std::vector<glm::ivec3> tempTriangles;

    parse(filename, position, scale, tempVertices, tempTriangles);

    int triStart = int(triangles.size());
    int numTris = int(tempTriangles.size())/3;

    for (glm::vec3 tempVertice : tempVertices) {
        growToInclude(min, max, tempVertice);
        vertices.emplace_back(tempVertice, 0);
    }
    for (int i = 0; i < numTris; ++i) {
        triangles.emplace_back(tempTriangles[i*3+0].x+Voffset, tempTriangles[i*3+1].x+Voffset, tempTriangles[i*3+2].x+Voffset, colors.size());
    }

    std::string prefix = "   ";
    std::cout << std::endl;
    std::cout << filename << std::endl;
    std::cout << prefix << tempVertices.size() << std::endl;
    std::cout << prefix << numTris << std::endl;
    std::cout << prefix << min.x << ' ' << min.y << ' ' << min.z << std::endl;
    std::cout << prefix << max.x << ' ' << max.y << ' ' << max.z << std::endl;
    std::cout << std::endl;

    colors.emplace_back(color, smoothness);
    this->emission.push_back(emission);

    createBVH(32, 5, triStart, numTris);
}

void Scene::createBVH(const int depth, const int numTestsPerAxis, int triStart, int numTris) {

    auto min = glm::vec3(1000000000.0f);
    auto max = glm::vec3(-1000000000.0f);

    for (int i = triStart; i < triStart + numTris; ++i) {
        glm::ivec4 tri = triangles[i];
        glm::vec4 v1 = vertices[tri.x];
        glm::vec4 v2 = vertices[tri.y];
        glm::vec4 v3 = vertices[tri.z];
        growToInclude(min, max, v1);
        growToInclude(min, max, v2);
        growToInclude(min, max, v3);
    }

    glm::vec4 bboxMin = glm::vec4(min, -triStart);
    glm::vec4 bboxMax = glm::vec4(max, -numTris);

    int index = int(boundingBoxMin.size());
    boundingBoxMin.emplace_back(bboxMin);
    boundingBoxMax.emplace_back(bboxMax);

    split(numTestsPerAxis, bboxMin, bboxMax, depth-1);
    boundingBoxMin[index] = bboxMin;
    boundingBoxMax[index] = bboxMax;
}

float Scene::evaluateSplit(glm::vec4 min, glm::vec4 max, int axis, float pos) const {
    auto minA = glm::vec4(1000000000.0f), maxA = glm::vec4(-1000000000.0f);
    auto minB = glm::vec4(1000000000.0f), maxB = glm::vec4(-1000000000.0f);
    maxA.w = 0; maxB.w = 0;

    int triStart = -int(min.w);
    int numTri = -int(max.w);

    for (int i = triStart; i < numTri+triStart; ++i) {
        glm::ivec4 tri = triangles[i];
        glm::vec4 v1 = vertices[tri.x];
        glm::vec4 v2 = vertices[tri.y];
        glm::vec4 v3 = vertices[tri.z];
        glm::vec3 center = (v1 + v2 + v3) / 3.0f;

        if (center[axis] < pos) {
            growToInclude(minA, maxA, v1);
            growToInclude(minA, maxA, v2);
            growToInclude(minA, maxA, v3);
            maxA.w --;
        } else {
            growToInclude(minB, maxB, v1);
            growToInclude(minB, maxB, v2);
            growToInclude(minB, maxB, v3);
            maxB.w --;
        }
    }

    return nodeCost(minA, maxA) + nodeCost(minB, maxB);
}

void Scene::chooseSplit(const int numTestsPerAxis, glm::vec4 min, glm::vec4 max, int& bestAxis, float& bestPos, float& bestCost) const {

    for (int axis = 0; axis < 3; ++axis) {
        float bStart = min[axis];
        float bEnd = max[axis];

        for (int i = 0; i < numTestsPerAxis; ++i) {
            float splitT = float(i+1) / float(numTestsPerAxis+1);

            float pos = bStart + (bEnd - bStart) * splitT;
            float cost = evaluateSplit(min, max, axis, pos);

            if (cost < bestCost) {
                bestPos = pos;
                bestCost = cost;
                bestAxis = axis;
            }
        }
    }

}

void Scene::split(int numTestsPerAxis, glm::vec4& bboxMin, glm::vec4& bboxMax, int depth) {
    if (depth <= 0) {return;};

    int triStart = -int(bboxMin.w);
    int numTris = -int(bboxMax.w);

    if (numTris <= 1) {return;}

    auto minA = glm::vec3(1000000000.0f);
    auto minB = glm::vec3(1000000000.0f);
    auto maxA = glm::vec3(-1000000000.0f);
    auto maxB = glm::vec3(-1000000000.0f);

    int numA = 0, numB = 0;
    int startA = triStart, startB = triStart;

    int splitAxis;
    glm::vec4 size = bboxMax - bboxMin;
    if (size.x > size.y && size.x > size.z) {
        splitAxis = 0;
    } else if (size.y > size.z && size.y > size.x) {
        splitAxis = 1;
    } else {
        splitAxis = 2;
    }

    float splitPos = (bboxMin[splitAxis]+bboxMax[splitAxis])/2;

    float bestCost = 1000000000000.0f;
    chooseSplit(numTestsPerAxis, bboxMin, bboxMax, splitAxis, splitPos, bestCost);
    if (bestCost >= nodeCost(bboxMin, bboxMax)) {return;}

    //std::cout << depth << ' ' << splitAxix << ' ' << splitPos << std::endl;

    for (int i = triStart; i < triStart+numTris; i++) {
        glm::ivec4 tri = triangles[i];
        glm::vec4 v1 = vertices[tri.x];
        glm::vec4 v2 = vertices[tri.y];
        glm::vec4 v3 = vertices[tri.z];
        glm::vec3 center = (v1 + v2 + v3)/3.0f;
        bool triInA = center[splitAxis] < splitPos;
        if (triInA) {
            growToInclude(minA, maxA, v1);
            growToInclude(minA, maxA, v2);
            growToInclude(minA, maxA, v3);
            numA++;
            startB ++;
            int swap = startA + numA - 1;
            std::swap(triangles[i], triangles[swap]);
        } else {
            growToInclude(minB, maxB, v1);
            growToInclude(minB, maxB, v2);
            growToInclude(minB, maxB, v3);
            numB++;
        }
    }
    //std::cout << depth << std::endl;
    //std::cout << "  " << numA << ' ' << numB << ' ' << splitAxis << ' ' << splitPos << std::endl;
    //std::cout << "  " << minA.x << ' ' << minA.y << ' ' << minA.z << std::endl;
    //std::cout << "  " << maxA.x << ' ' << maxA.y << ' ' << maxA.z << std::endl;
    //std::cout << "  " << minB.x << ' ' << minB.y << ' ' << minB.z << std::endl;
    //std::cout << "  " << maxB.x << ' ' << maxB.y << ' ' << maxB.z << std::endl;

    if (numA > 0 and numB > 0) {
        auto minAOut = glm::vec4(minA, -startA);
        auto maxAOut = glm::vec4(maxA, -numA);
        auto minBOut = glm::vec4(minB, -startB);
        auto maxBOut = glm::vec4(maxB, -numB);

        boundingBoxMin.emplace_back(0);
        boundingBoxMax.emplace_back(0);
        boundingBoxMin.emplace_back(0);
        boundingBoxMax.emplace_back(0);
        int indexA = int(boundingBoxMin.size())-2;
        int indexB = int(boundingBoxMax.size())-1;

        bboxMin.w = float(indexA);
        bboxMax.w = float(indexB);

        split(numTestsPerAxis, minAOut, maxAOut, depth-1);
        split(numTestsPerAxis, minBOut, maxBOut, depth-1);

        boundingBoxMin[indexA] = minAOut;
        boundingBoxMax[indexA] = maxAOut;
        boundingBoxMin[indexB] = minBOut;
        boundingBoxMax[indexB] = maxBOut;
    }
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
    glm::vec2 delta = glm::vec2(xpos - width/2, -(ypos - height/2));
    if (delta.x*delta.x + delta.y*delta.y > 0 and !lock) {
        delta *= 2.0f/height * sensitivity;
        camForward += delta.x * camRight + delta.y * camUp;
        camForward = glm::normalize(camForward);
        moved = true;
        setBasisVectors(camForward, camUp, camRight);
        glfwSetCursorPos(&window, width/2, height/2);
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
        glfwSetCursorPos(&window, width/2, height/2);
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
    glm::vec4 bboxMin = boundingBoxMin[index];
    glm::vec4 bboxMax = boundingBoxMax[index];
    if (bboxMax.w > index and bboxMin.w > index) {
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

void Scene::displayBVH(int index, std::string prefix) {
    glm::vec4 bboxMin = boundingBoxMin[index];
    glm::vec4 bboxMax = boundingBoxMax[index];
    int numTris = numTriBelow(index);
    if (numTris < 1000) return;
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
