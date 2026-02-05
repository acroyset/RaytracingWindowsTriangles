//
// Created by acroy on 7/26/2025.
//

#include "Model.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <sstream>
#include <unordered_map>

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
void growToInclude(glm::vec3& min, glm::vec3& max, const glm::vec3 tMin, const glm::vec3 tMax) {
    min.x = std::min(min.x, tMin.x);
    min.y = std::min(min.y, tMin.y);
    min.z = std::min(min.z, tMin.z);
    max.x = std::max(max.x, tMax.x);
    max.y = std::max(max.y, tMax.y);
    max.z = std::max(max.z, tMax.z);
}
void growToInclude(glm::vec4& min, glm::vec4& max, const glm::vec3 tMin, const glm::vec3 tMax) {
    min.x = std::min(min.x, tMin.x);
    min.y = std::min(min.y, tMin.y);
    min.z = std::min(min.z, tMin.z);
    max.x = std::max(max.x, tMax.x);
    max.y = std::max(max.y, tMax.y);
    max.z = std::max(max.z, tMax.z);
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
    const auto biggestDiff = float(fmax(fmax(max.x-min.x, max.y-min.y), max.z-min.z));
    const float scaler = 2/biggestDiff;

    for (glm::vec3 &point : points) {
        point -= offset;
        point *= scaler;
    }
}

float nodeCost(const glm::vec3 min, const glm::vec3 max, const int numTris) {
    const glm::vec3 size = max-min;
    const float halfArea = size.x * (size.y + size.z) + size.y * size.z;
    return halfArea * float(numTris);
}

inline float fast_strtof(const char* str, char** endptr) {
    return std::strtof(str, endptr);
    // Or use a faster implementation like:
    // return fast_float::from_chars(str, str + strlen(str), result).ptr;
}
inline int fast_strtoi(const char* str, char** endptr) {
    return std::strtol(str, endptr, 10);
}

std::string dirOf(const std::string& path) {
    size_t p = path.find_last_of("/\\");
    return (p == std::string::npos) ? std::string() : path.substr(0, p+1);
}

void loadMTL(
    const std::string& mtlPath,
    std::unordered_map<std::string, int>& nameToIndex,
    std::vector<glm::vec4>& colors,
    std::vector<glm::vec4>& specularColors,
    std::vector<glm::vec4>& glassLightSettings
    ) {
    std::ifstream f(mtlPath);
    if (!f) { std::cerr << "WARN: could not open MTL: " << mtlPath << "\n"; return; }

    std::string line, curName;
    glm::vec3 Kd, Ks, Ke;
    float smoothness, specularProb, transparency, ior, emission;

    auto flushMaterial = [&](){
        if (curName.empty()) return;
        if (nameToIndex.find(curName) == nameToIndex.end()) {
            int idx = int(colors.size());
            nameToIndex[curName] = idx;
            auto luminance = [](glm::vec3 c) {
                return 0.2126f*c.r + 0.7152f*c.g + 0.0722f*c.b;
            };

            float Ld = luminance(Kd);
            float Ls = luminance(Ks);

            specularProb = (Ld + Ls > 0.0f) ? (Ls / (Ld + Ls)) : 0.0f;

            emission = glm::length(Ke);
            colors.emplace_back(emission == 0 ? Kd : Ke, smoothness);
            specularColors.emplace_back(Ks, specularProb);
            glassLightSettings.emplace_back(transparency, ior, emission, 0);
        }
    };

    while (std::getline(f, line)) {
        if (line.empty() || line[0]=='#') continue;
        std::istringstream iss(line);
        std::string key; iss >> key;
        if (key == "newmtl") {
            flushMaterial();
            iss >> curName;
        } else if (key == "Kd") {
            iss >> Kd.r >> Kd.g >> Kd.b;
        } else if (key == "Ks") {
            iss >> Ks.r >> Ks.g >> Ks.b;
        } else if (key == "Ke") {
            iss >> Ke.r >> Ke.g >> Ke.b;
        } else if (key == "Ns") {
            float ns;
            iss >> ns;
            float roughness = sqrtf(2.0f / (ns + 2.0f));
            smoothness = 1.0f - roughness;
        } else if (key == "Ni") {
            iss >> ior;
        } else if (key == "d") {
            iss >> transparency;
            transparency = 1.0f - transparency;
        }
    }
    flushMaterial();
}

Model::Model() = default;

void Model::parse(
    const std::string& nfilename,
    std::vector<glm::vec3>& vertices,
    std::vector<glm::ivec3>& triangles,
    std::vector<glm::vec3>& normalsList,
    std::vector<int>& tempTriMatIndex,
    std::vector<glm::vec4>& colors,
    std::vector<glm::vec4>& specularColors,
    std::vector<glm::vec4>& glassLightSettings
    ) {
    const std::string filename = "" + nfilename;
    std::ifstream model(filename, std::ios::in | std::ios::binary);

    if (!model.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    std::unordered_map<std::string,int> materialNameToIndex;
    int currentMaterial = 0;                 // -1 = no material; we’ll map it to a default color later if needed
    bool mtlLoaded = false;
    std::string baseDir = dirOf(filename);

    // Get file size for reservations
    model.seekg(0, std::ios::end);
    std::streamsize fileSize = model.tellg();
    model.seekg(0, std::ios::beg);

    // Reserve space
    size_t estimatedVertices = fileSize / 50;
    size_t estimatedTriangles = fileSize / 80;
    vertices.reserve(estimatedVertices);
    triangles.reserve(estimatedTriangles * 3);
    normalsList.reserve(estimatedVertices);

    // Process in chunks to avoid massive memory allocation
    constexpr size_t CHUNK_SIZE = 64 * 1024 * 1024; // 64MB chunks
    std::vector<char> buffer(CHUNK_SIZE + 1);
    std::string leftover; // Handle lines split across chunks

    while (model) {
        model.read(buffer.data(), CHUNK_SIZE);
        std::streamsize bytesRead = model.gcount();

        if (bytesRead == 0) break;

        buffer[bytesRead] = '\0';

        // Combine leftover from previous chunk with current chunk
        std::string chunk = leftover + std::string(buffer.data(), bytesRead);
        leftover.clear();

        // Find last complete line in chunk
        size_t lastNewline = chunk.find_last_of('\n');
        if (lastNewline != std::string::npos && lastNewline < chunk.size() - 1) {
            // Save incomplete line for next chunk
            leftover = chunk.substr(lastNewline + 1);
            chunk = chunk.substr(0, lastNewline + 1);
        }

        // Fast parse the chunk
        char* ptr = chunk.data();
        char* end = ptr + chunk.size();

        while (ptr < end) {
            // Skip to start of line content (skip whitespace/newlines)
            while (ptr < end && (*ptr == '\n' || *ptr == '\r' || *ptr == ' ' || *ptr == '\t')) {
                ptr++;
            }

            if (ptr >= end) break;

            // Quick check for line type
            if (*ptr == 'v' && *(ptr + 1) == ' ') {
                ptr += 2; // Skip "v "

                // Fast float parsing
                float x, y, z;
                x = fast_strtof(ptr, &ptr);
                while (*ptr == ' ' || *ptr == '\t') ptr++; // Skip whitespace
                y = fast_strtof(ptr, &ptr);
                while (*ptr == ' ' || *ptr == '\t') ptr++; // Skip whitespace
                z = fast_strtof(ptr, &ptr);

                vertices.emplace_back(x, y, z);
            }
            else if (*ptr == 'v' && *(ptr + 1) == 'n' && *(ptr + 2) == ' ') {
                ptr += 3; // Skip "vn "

                // Fast float parsing
                float x, y, z;
                x = fast_strtof(ptr, &ptr);
                while (*ptr == ' ' || *ptr == '\t') ptr++; // Skip whitespace
                y = fast_strtof(ptr, &ptr);
                while (*ptr == ' ' || *ptr == '\t') ptr++; // Skip whitespace
                z = fast_strtof(ptr, &ptr);

                normalsList.emplace_back(x, y, z);
            }
            else if (*ptr == 'f' && *(ptr + 1) == ' ') {
                ptr += 2; // Skip "f "

                std::array<glm::ivec3, 4> faceVertices{};
                int vertexCount = 0;

                // Parse face vertices
                while (ptr < end && *ptr != '\n' && *ptr != '\r' && vertexCount < 4) {
                    while (*ptr == ' ' || *ptr == '\t') ptr++; // Skip whitespace
                    if (*ptr == '\n' || *ptr == '\r') break;

                    // Parse vertex indices (v/vt/vn format)
                    int v = fast_strtoi(ptr, &ptr);
                    int vt = 0, vn = 0;

                    if (*ptr == '/') {
                        ptr++;
                        if (*ptr != '/') {
                            vt = fast_strtoi(ptr, &ptr);
                        }
                        if (*ptr == '/') {
                            ptr++;
                            vn = fast_strtoi(ptr, &ptr);
                        }
                    }

                    faceVertices[vertexCount] = glm::ivec3(v, vt, vn);
                    vertexCount++;

                    // Skip to next vertex or end of line
                    while (ptr < end && *ptr != ' ' && *ptr != '\t' && *ptr != '\n' && *ptr != '\r') {
                        ptr++;
                    }
                }

                // Convert to triangles (triangle fan)
                if (vertexCount >= 3) {
                    // base triangle
                    triangles.emplace_back(faceVertices[0]);
                    triangles.emplace_back(faceVertices[1]);
                    triangles.emplace_back(faceVertices[2]);
                    // record material for this emitted tri
                    tempTriMatIndex.push_back(currentMaterial >= 0 ? currentMaterial : 0);

                    // handle quads/n-gons
                    for (int i = 3; i < vertexCount; i++) {
                        triangles.emplace_back(faceVertices[0]);
                        triangles.emplace_back(faceVertices[i-1]);
                        triangles.emplace_back(faceVertices[i]);
                        tempTriMatIndex.push_back(currentMaterial >= 0 ? currentMaterial : 0);
                    }
                }
            }
            else if (*ptr=='m' && (ptr+6) < end && std::memcmp(ptr, "mtllib", 6)==0) {
                // advance to end of token and one space
                while (ptr<end && *ptr!=' ') ptr++;
                while (ptr<end && (*ptr==' ' || *ptr=='\t')) ptr++;

                // read the filename until EOL
                char* start = ptr;
                while (ptr<end && *ptr!='\n' && *ptr!='\r') ptr++;
                std::string mtlName(start, ptr);

                if (!mtlLoaded) {
                    std::string full = baseDir + mtlName;
                    loadMTL(full, materialNameToIndex, colors, specularColors, glassLightSettings);
                    mtlLoaded = true;
                }
            }
            else if (*ptr=='u' && (ptr+6) < end && std::memcmp(ptr, "usemtl", 6)==0) {
                // move to name
                while (ptr<end && *ptr!=' ') ptr++;
                while (ptr<end && (*ptr==' ' || *ptr=='\t')) ptr++;

                // read material name until EOL
                char* start = ptr;
                while (ptr<end && *ptr!='\n' && *ptr!='\r') ptr++;
                std::string matName(start, ptr);

                auto it = materialNameToIndex.find(matName);
                if (it != materialNameToIndex.end()) currentMaterial = it->second;
                else {
                    // unseen name: push a default color and remember it
                    int idx = int(colors.size());
                    materialNameToIndex[matName] = idx;
                    colors.emplace_back(1.0f, 1.0f, 1.0f, 0.0f);
                    specularColors.emplace_back(0.0f);
                    glassLightSettings.emplace_back(0, 1, 0, 0);
                    currentMaterial = idx;
                }
            }

            // Skip to next line
            while (ptr < end && *ptr != '\n' && *ptr != '\r') {
                ptr++;
            }
        }
    }

    // Handle any remaining leftover
    if (!leftover.empty()) {
        std::cerr << "leftover" << std::endl;
        std::cerr << leftover << std::endl;
        // Process final incomplete line if needed
        // (similar parsing logic as above)
    }

    // Convert negative indices and adjust for 0-based indexing
    for (glm::ivec3& i : triangles) {
        if (i.x < 0) i.x += int(vertices.size()) + 1;
        if (i.y < 0) i.y += int(vertices.size()) + 1;
        if (i.z < 0) i.z += int(normalsList.size()) + 1;
        i.x--;

        i.y--;
        i.z--;

        if (i.x >= vertices.size() || i.x < 0) {
            std::cout << "Error: Invalid vertex index " << i.x << std::endl;
        }
    }

    center(vertices);
    model.close();
}

Model::Model(const std::string &filename) {
    this->filename = filename;
    std::vector<glm::vec3> tempVertices;
    std::vector<glm::ivec3> tempTriangles;
    std::vector<glm::vec3> tempNormals;
    std::vector<glm::vec4> tempColors;
    std::vector<glm::vec4> tempSpecularColors;
    std::vector<glm::vec4> tempGlassLightSettings;
    std::vector<int> tempTriMatIndex;

    std::cout << "Parsing " << filename << "..." << std::endl;
    parse(filename, tempVertices, tempTriangles, tempNormals, tempTriMatIndex, tempColors, tempSpecularColors, tempGlassLightSettings);

    std::cout << filename << std::endl;
    std::cout << "Vertices: " << tempVertices.size() << std::endl;
    std::cout << "Triangles: " << tempTriangles.size()/3 << std::endl;

    int triStart = 0;
    int numTris = int(tempTriangles.size())/3;

    std::cout << "Copying vertex data..." << std::endl;
    for (glm::vec3 tempVertice : tempVertices) {
        vertices.emplace_back(tempVertice);
    }

    std::cout << "Copying normal data..." << std::endl;
    for (glm::vec3 tempNormal : tempNormals) {
        normalsList.emplace_back(tempNormal);
    }

    std::cout << "Copying triangle data..." << std::endl;
    for (int i = 0; i < numTris; ++i) {
        triangles.emplace_back(tempTriangles[i*3+0].x, tempTriangles[i*3+1].x, tempTriangles[i*3+2].x, tempTriMatIndex[i]);
        normals.emplace_back(tempTriangles[i*3+0].z, tempTriangles[i*3+1].z, tempTriangles[i*3+2].z);
    }

    for (glm::vec4 tempColor : tempColors) {
        colors.emplace_back(tempColor);
    }

    for (glm::vec4 tempSpecularColor : tempSpecularColors) {
        specularColors.emplace_back(tempSpecularColor);
    }

    for (glm::vec4 tempGlassLightSetting : tempGlassLightSettings) {
        glassLightSettings.emplace_back(tempGlassLightSetting);
    }

    int testPerAxis = 3;

    std::cout << "Starting BVH construction..." << std::endl;
    createBVH(47, testPerAxis, triStart, numTris);
    std::cout << "BVH construction complete!" << std::endl;
}

void Model::createBVH(const int depth, const int numTestsPerAxis, int triStart, int numTris) {

    auto min = glm::vec3(1000000000.0f);
    auto max = glm::vec3(-1000000000.0f);

    precomputeTriangleData();

    for (int i = triStart; i < triStart + numTris; ++i) {
        const glm::vec3& tMin = triangleMin[i];
        const glm::vec3& tMax = triangleMax[i];

        growToInclude(min, max, tMin, tMax);
    }

    int index = int(boundingBoxMin.size());
    boundingBoxMin.emplace_back(min);
    boundingBoxMax.emplace_back(max);
    int indexA = -triStart;
    int indexB = -numTris;
    childA.emplace_back(indexA);
    childB.emplace_back(indexA);

    split(numTestsPerAxis, min, indexA, max, indexB, depth-1);
    childA[index] = indexA;
    childB[index] = indexB;
}

void Model::precomputeTriangleData() {
    triangleCenters.resize(triangles.size());
    triangleMin.resize(triangles.size());
    triangleMax.resize(triangles.size());

    for (size_t i = 0; i < triangles.size(); ++i) {
        const glm::ivec3& tri = triangles[i];

        const glm::vec3& v1 = vertices[tri.x];
        const glm::vec3& v2 = vertices[tri.y];
        const glm::vec3& v3 = vertices[tri.z];

        triangleCenters[i] = (v1 + v2 + v3) / 3.0f;
        triangleMin[i] = glm::min(glm::min(v1, v2), v3);
        triangleMax[i] = glm::max(glm::max(v1, v2), v3);
    }
}

float Model::evaluateSplit(const int childA, const int childB, const int axis, const float pos) const {
    auto minA = glm::vec3(1000000000.0f), maxA = glm::vec3(-1000000000.0f);
    auto minB = glm::vec3(1000000000.0f), maxB = glm::vec3(-1000000000.0f);

    int triStart = -childA;
    int numTri = -childB;

    int numA = 0;
    int numB = 0;

    for (int i = triStart; i < numTri+triStart; ++i) {
        const glm::vec3& tMin = triangleMin[i];
        const glm::vec3& tMax = triangleMax[i];

        glm::vec3 center = triangleCenters[i];

        if (center[axis] < pos) {
            growToInclude(minA, maxA, tMin, tMax);
            numA++;
        } else {
            growToInclude(minB, maxB, tMin, tMax);
            numB++;
        }
    }

    return nodeCost(minA, maxA, numA) + nodeCost(minB, maxB, numB);
}

void Model::chooseSplit(const int numTestsPerAxis, glm::vec3 min, const int childA, glm::vec3 max, const int childB, int& bestAxis, float& bestPos, float& bestCost) {

    for (int axis = 0; axis < 3; ++axis) {
        float bStart = min[axis];
        float bEnd = max[axis];

        for (int i = 0; i < numTestsPerAxis; ++i) {

            if (-childB > 1000) {

                pool.enqueue([numTestsPerAxis, childA, childB, i, bStart, bEnd, axis, &bestPos, &bestCost, &bestAxis, this] {
                    float splitT = float(i+1) / float(numTestsPerAxis+1);

                    float pos = bStart + (bEnd - bStart) * splitT;
                    float cost = evaluateSplit(childA, childB, axis, pos);

                    if (cost < bestCost) {
                        bestPos = pos;
                        bestCost = cost;
                        bestAxis = axis;
                    }
                });

            } else {
                float splitT = float(i+1) / float(numTestsPerAxis+1);

                float pos = bStart + (bEnd - bStart) * splitT;
                float cost = evaluateSplit(childA, childB, axis, pos);

                if (cost < bestCost) {
                    bestPos = pos;
                    bestCost = cost;
                    bestAxis = axis;
                }
            }
        }

        pool.wait_for_tasks();
    }

}

void Model::split(int numTestsPerAxis, glm::vec3 bboxMin, int& childA, glm::vec3 bboxMax, int& childB, int depth) {
    if (depth <= 0) {return;};

    int triStart = -childA;
    int numTris = -childB;

    if (numTris <= 1) {return;}

    auto minA = glm::vec3(1000000000.0f);
    auto minB = glm::vec3(1000000000.0f);
    auto maxA = glm::vec3(-1000000000.0f);
    auto maxB = glm::vec3(-1000000000.0f);

    int numA = 0, numB = 0;
    int startA = triStart, startB = triStart;

    int splitAxis;
    float splitPos = 0;
    float bestCost = std::numeric_limits<float>::max();
    chooseSplit(numTestsPerAxis, bboxMin, childA, bboxMax, childB, splitAxis, splitPos, bestCost);
    if (bestCost >= nodeCost(bboxMin, bboxMax, numTris)) {return;}

    //std::cout << depth << ' ' << splitAxix << ' ' << splitPos << std::endl;

    for (int i = triStart; i < triStart+numTris; i++) {
        const glm::vec3& tMin = triangleMin[i];
        const glm::vec3& tMax = triangleMax[i];

        glm::vec3 center = triangleCenters[i];
        bool triInA = center[splitAxis] < splitPos;
        if (triInA) {
            growToInclude(minA, maxA, tMin, tMax);
            numA++;
            startB ++;
            int swap = startA + numA - 1;
            std::swap(triangles[i], triangles[swap]);
            std::swap(normals[i], normals[swap]);
            std::swap(triangleCenters[i], triangleCenters[swap]);
            std::swap(triangleMin[i], triangleMin[swap]);
            std::swap(triangleMax[i], triangleMax[swap]);
        } else {
            growToInclude(minB, maxB, tMin, tMax);
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
        int childA_A = -startA, childB_A = -numA;
        int childA_B = -startB, childB_B = -numB;

        boundingBoxMin.push_back(minA);
        boundingBoxMax.push_back(maxA);
        int indexA = int(boundingBoxMin.size())-1;
        boundingBoxMin.push_back(minB);
        boundingBoxMax.push_back(maxB);
        int indexB = indexA + 1;

        this->childA.emplace_back(childA_A);
        this->childB.emplace_back(childB_A);
        this->childA.emplace_back(childA_B);
        this->childB.emplace_back(childB_B);

        childA = indexA;
        childB = indexB;

        split(numTestsPerAxis, minA, childA_A, maxA, childB_A, depth-1);
        split(numTestsPerAxis, minB, childA_B, maxB, childB_B, depth-1);

        if (childA_A > 0 && (childA_A == indexA || childB_A == indexA)) {
            std::cerr << "Child A points back to parent indexA=" << indexA << std::endl;
            std::cerr << "childA_A=" << childA_A << " childB_A=" << childB_A << std::endl;
            return;
        }

        if (childA_B > 0 && (childA_B == indexB || childB_B == indexB)) {
            std::cerr << "Child B points back to parent indexA=" << indexB << std::endl;
            std::cerr << "childA_B=" << childA_B << " childB_B=" << childB_B << std::endl;
            return;
        }


        this->childA[indexA] = childA_A;
        this->childB[indexA] = childB_A;
        this->childA[indexB] = childA_B;
        this->childB[indexB] = childB_B;
    }
}
