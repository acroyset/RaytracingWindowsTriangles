//
// Created by acroy on 7/26/2025.
//

#include "BaseModel.h"

#include <fstream>
#include <iostream>

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
float nodeCost(const glm::vec4 min, const glm::vec4 max) {
    const glm::vec3 size = xyz(max-min);
    const float halfArea = size.x * (size.y + size.z) + size.y * size.z;
    return halfArea * -max.w;
}

BaseModel::BaseModel() = default;

void BaseModel::parse(const std::string& nfilename, std::vector<glm::vec3>& vertices, std::vector<glm::ivec3>& triangles) {
    const std::string filename = "" + nfilename;
    std::ifstream model(filename, std::ios::binary | std::ios::ate);

    if (!model.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    std::streamsize size = model.tellg();
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
        if (i.x < 0) i.x += int(vertices.size())+1;
        if (i.y < 0) i.y += int(vertices.size())+1;
        if (i.z < 0) i.z += int(vertices.size())+1;
        i.x --;
        i.y --;
        i.z --;
        if (i.x > vertices.size()) std::cout << "Error" << std::endl;
        //if (i.y > vertices.size()) std::cout << "Error" << std::endl;
        //if (i.z > vertices.size()) std::cout << "Error" << std::endl;
    }

    center(vertices);
}

BaseModel::BaseModel(const std::string &filename) {
    this->filename = filename;
    std::vector<glm::vec3> tempVertices;
    std::vector<glm::ivec3> tempTriangles;

    parse(filename, tempVertices, tempTriangles);

    std::cout << filename << std::endl;
    std::cout << tempVertices.size() << std::endl;
    std::cout << tempTriangles.size()/3 << std::endl;

    int triStart = int(triangles.size());
    int numTris = int(tempTriangles.size())/3;

    for (glm::vec3 tempVertice : tempVertices) {
        vertices.emplace_back(tempVertice);
    }
    for (int i = 0; i < numTris; ++i) {
        triangles.emplace_back(tempTriangles[i*3+0].x, tempTriangles[i*3+1].x, tempTriangles[i*3+2].x);
    }

    createBVH(32, 5, triStart, numTris);
}

void BaseModel::createBVH(const int depth, const int numTestsPerAxis, int triStart, int numTris) {

    auto min = glm::vec3(1000000000.0f);
    auto max = glm::vec3(-1000000000.0f);

    for (int i = triStart; i < triStart + numTris; ++i) {
        glm::ivec3 tri = triangles[i];
        glm::vec3 v1 = vertices[tri.x];
        glm::vec3 v2 = vertices[tri.y];
        glm::vec3 v3 = vertices[tri.z];
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

float BaseModel::evaluateSplit(glm::vec4 min, glm::vec4 max, int axis, float pos) const {
    auto minA = glm::vec4(1000000000.0f), maxA = glm::vec4(-1000000000.0f);
    auto minB = glm::vec4(1000000000.0f), maxB = glm::vec4(-1000000000.0f);
    maxA.w = 0; maxB.w = 0;

    int triStart = -int(min.w);
    int numTri = -int(max.w);

    for (int i = triStart; i < numTri+triStart; ++i) {
        glm::ivec3 tri = triangles[i];
        glm::vec3 v1 = vertices[tri.x];
        glm::vec3 v2 = vertices[tri.y];
        glm::vec3 v3 = vertices[tri.z];
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

void BaseModel::chooseSplit(const int numTestsPerAxis, glm::vec4 min, glm::vec4 max, int& bestAxis, float& bestPos, float& bestCost) const {

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

void BaseModel::split(int numTestsPerAxis, glm::vec4& bboxMin, glm::vec4& bboxMax, int depth) {
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
        glm::ivec3 tri = triangles[i];
        glm::vec3 v1 = vertices[tri.x];
        glm::vec3 v2 = vertices[tri.y];
        glm::vec3 v3 = vertices[tri.z];
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
