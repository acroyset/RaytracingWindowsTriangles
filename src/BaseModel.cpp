//
// Created by acroy on 7/26/2025.
//

#include "BaseModel.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <dinput.h>
#include <sstream>
#include <unordered_map>

#include "Transformation.h"
#include "Rendering/Timer.h"

#define BVH_MAX_DEPTH 48
#define BVH_TESTS_PER_AXIS 5

inline void splitSlash(const std::string& s, std::string tokens[3]) {
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
inline void splitSpace3(const std::string& s, std::string words[3]) {
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
inline void splitSpace4(const std::string& s, std::string words[4], int& num) {
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

inline void growToInclude(vec3& min, vec3& max, const vec3 p) {
    min.x = std::min(min.x, p.x);
    min.y = std::min(min.y, p.y);
    min.z = std::min(min.z, p.z);
    max.x = std::max(max.x, p.x);
    max.y = std::max(max.y, p.y);
    max.z = std::max(max.z, p.z);
}

void makeBoundingBox(vec3& min, vec3& max, const std::vector<vec3>& vertices) {
    for (const auto & vertice : vertices) {
        growToInclude(min, max, vertice);
    }
}

Transformation center(std::vector<vec3>& points) {
    auto min = vec3(std::numeric_limits<float>::infinity()), max = vec3(-std::numeric_limits<float>::infinity());
    makeBoundingBox(min, max, points);

    const vec3 offset = {(max.x+min.x)/2, (max.y+min.y)/2, (max.z+min.z)/2};
    const auto biggestDiff = float(fmax(fmax(max.x-min.x, max.y-min.y), max.z-min.z));
    const float scaler = 2/biggestDiff;

    for (vec3 &point : points) {
        point -= offset;
        point *= scaler;
    }

    return {-offset, vec3(biggestDiff/2.0f)};
}

inline float nodeCost(const BVHnode &node) {
    int numTris = node.getNumTri();
    const vec3 size = node.getMax()-node.getMin();
    const float halfArea = size.x * (size.y + size.z) + size.y * size.z;
    return halfArea * float(numTris);
}

inline float fast_strtof(const char* str, char** endptr) {
    return std::strtof(str, endptr);
    // return fast_float::from_chars(str, str + strlen(str), result).ptr;
}
inline int fast_strtoi(const char* str, char** endptr) {
    return std::strtol(str, endptr, 10);
}

std::string dirOf(const std::string& path) {
    size_t p = path.find_last_of("/\\");
    return (p == std::string::npos) ? std::string() : path.substr(0, p+1);
}

std::vector<vec3> createNormals(std::vector<ivec3>& triangles, const std::vector<vec3>& vertices) {
    std::vector<vec3> normals(triangles.size()/3);
    for (int i = 0; i < triangles.size()/3; ++i) {
        ivec3 tri1 = triangles[3*i+0];
        ivec3 tri2 = triangles[3*i+1];
        ivec3 tri3 = triangles[3*i+2];

        vec3 v1 = vertices[tri1.x];
        vec3 v2 = vertices[tri2.x];
        vec3 v3 = vertices[tri3.x];

        vec3 e1 = v2-v1;
        vec3 e2 = v3-v1;

        vec3 normal = normalize(cross(e1, e2));
        triangles[3*i+0].z = i;
        triangles[3*i+1].z = i;
        triangles[3*i+2].z = i;
        normals[i] = normal;
    }

    return normals;
}


BaseModel::BaseModel() = default;

void BaseModel::loadMTL(const std::string& filename) {
    std::ifstream f(filename);
    if (!f) { std::cerr << "WARN: could not open MTL: " << filename << "\n"; return; }

    std::string line, curName;
    vec3 Kd, Ks, Ke;
    float roughness, specularProb, transparency, ior, emission;

    auto flushMaterial = [&](){
        if (curName.empty()) return;
        auto luminance = [](vec3 c) {
            return 0.2126f*c.r + 0.7152f*c.g + 0.0722f*c.b;
        };

        float Ld = luminance(Kd);
        float Ls = luminance(Ks);

        specularProb = (Ld + Ls > 0.0f) ? (Ls / (Ld + Ls)) : 0.0f;

        emission = length(Ke);

        Material material;


        if (emission > 0) material.setType(Emissive);
        else if (transparency > 0) material.setType(Transparent);
        else material.setType(Specular);

        material.setDiffuseColor(emission == 0 ? Kd : Ke);
        material.setDiffuseRoughness(1);
        material.setSpecularColor(Ks);
        material.setSpecularRoughness(roughness);
        material.setSpecularProbability(specularProb);
        material.setTransparency(transparency);
        material.setIndexOfRefraction(ior);
        material.setEmissionStrength(emission);

        materials.push_back(material);
        materialNames.push_back(curName);

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
            roughness = sqrtf(2.0f / (ns + 2.0f));
        } else if (key == "Ni") {
            iss >> ior;
        } else if (key == "d") {
            iss >> transparency;
            transparency = 1.0f - transparency;
        }
    }
    flushMaterial();
}

bool BaseModel::parse(const std::string& filename) {
    std::vector<ivec3> tempTriangles;
    std::vector<int> tempTriMatIndex;

    std::ifstream model(filename, std::ios::in | std::ios::binary);

    if (!model.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }

    int currentMaterial = 0;
    bool mtlLoaded = false;
    std::string baseDir = dirOf(filename);

    // Get file size for reservations
    model.seekg(0, std::ios::end);
    std::streamsize fileSize = model.tellg();
    model.seekg(0, std::ios::beg);

    // Reserve space
    size_t estimatedVertices = fileSize / 50;
    size_t estimatedTriangles = fileSize / 80;
    tempTriangles.reserve(estimatedTriangles * 3);
    triangles.reserve(estimatedTriangles * 3);
    vertices.reserve(estimatedVertices);
    texCoords.reserve(estimatedVertices);
    normals.reserve(estimatedVertices);

    // Process in chunks to avoid massive memory allocation
    constexpr size_t CHUNK_SIZE = 256 * 1024 * 1024; // 256MB chunks
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
            } // vertices
            else if (*ptr == 'v' && *(ptr + 1) == 't' && *(ptr + 2) == ' ') {
                ptr += 3; // Skip "vt "

                // Fast float parsing
                float x, y;
                x = fast_strtof(ptr, &ptr);
                while (*ptr == ' ' || *ptr == '\t') ptr++; // Skip whitespace
                y = fast_strtof(ptr, &ptr);
                while (*ptr == ' ' || *ptr == '\t') ptr++; // Skip whitespace

            texCoords.emplace_back(x, y);
        } // texCoords
            else if (*ptr == 'v' && *(ptr + 1) == 'n' && *(ptr + 2) == ' ') {
                ptr += 3; // Skip "vn "

                // Fast float parsing
                float x, y, z;
                x = fast_strtof(ptr, &ptr);
                while (*ptr == ' ' || *ptr == '\t') ptr++; // Skip whitespace
                y = fast_strtof(ptr, &ptr);
                while (*ptr == ' ' || *ptr == '\t') ptr++; // Skip whitespace
                z = fast_strtof(ptr, &ptr);

                normals.emplace_back(x, y, z);
            } // normals
            else if (*ptr == 'f' && *(ptr + 1) == ' ') {
                ptr += 2; // Skip "f "
                int vertexCount = 0;

                // Parse face vertices
                while (ptr < end && *ptr != '\n' && *ptr != '\r') {
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

                    vertexCount++;

                    if (vertexCount >= 4) {
                        tempTriangles.push_back(tempTriangles[tempTriangles.size() - vertexCount + 1]);
                        tempTriangles.push_back(tempTriangles[tempTriangles.size() - 2]);
                        tempTriMatIndex.push_back(currentMaterial);
                    }

                    tempTriangles.emplace_back(v, vt, vn);

                    // Skip to next vertex or end of line
                    while (ptr < end && *ptr != ' ' && *ptr != '\t' && *ptr != '\n' && *ptr != '\r') {
                        ptr++;
                    }
                }
                tempTriMatIndex.push_back(currentMaterial);

            } // faces
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
                    loadMTL(full);
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

                auto it = std::find_if(materialNames.begin(), materialNames.end(), [matName](const std::string& name) {return matName == name;});
                if (it != materialNames.end()) currentMaterial = int(it - materialNames.begin());
                else {
                    // unseen name: push a default color and remember it
                    int idx = int(materials.size());
                    materialNames.push_back(matName);
                    materials.emplace_back();
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
        std::cerr << "leftover (add newline to end of obj file)" << std::endl;
        std::cerr << leftover << std::endl;
        // Process final incomplete line if needed
        // (similar parsing logic as above)
    }

    // Convert negative indices and adjust for 0-based indexing
    for (ivec3& i : tempTriangles) {
        if (i.x < 0) i.x += int(vertices.size()) + 1;
        if (i.y < 0) i.y += int(texCoords.size()) + 1;
        if (i.z < 0) i.z += int(normals.size()) + 1;

        i.x--;
        i.y--;
        i.z--;

        if (i.x >= vertices.size() || i.x < 0) {
            std::cout << "Error: Invalid vertex index " << i.x << std::endl;
        }
    }

    baseTransform = center(vertices);

    if (normals.empty()) {
        normals = createNormals(tempTriangles, vertices);
    }

    int numTris = int(tempTriangles.size())/3;

    for (int i = 0; i < numTris; ++i) {
        triangles.emplace_back(tempTriangles[i*3+0].x, tempTriangles[i*3+0].y, tempTriangles[i*3+0].z, tempTriMatIndex[i]);
        triangles.emplace_back(tempTriangles[i*3+1].x, tempTriangles[i*3+1].y, tempTriangles[i*3+1].z, 0);
        triangles.emplace_back(tempTriangles[i*3+2].x, tempTriangles[i*3+2].y, tempTriangles[i*3+2].z, 0);
    }

    model.close();

    return true;
}

BaseModel::BaseModel(const std::string &filename) {
    this->filename = filename;

    std::cout << filename << std::endl;
    Timer t;
    if (parse(filename)) valid = true;
    std::cout << "   Parse Time: " << t.reset() << std::endl;

    std::cout << "   Triangles: " << triangles.size()/3 << std::endl;
    std::cout << "   Vertices: " << vertices.size() << std::endl;
    std::cout << "   TexCoords: " << texCoords.size() << std::endl;
    std::cout << "   Normals: " << normals.size() << std::endl;

    t.reset();
    createBVH(BVH_MAX_DEPTH, BVH_TESTS_PER_AXIS, 0, int(triangles.size())/3);
    std::cout << "   BVH Nodes: " << BVHnodes.size() << std::endl;
    std::cout << "   BVH Construction Time: " << t.reset() << std::endl;
}

void BaseModel::createBVH(const int depth, const int numTestsPerAxis, int triStart, int numTris) {

    BVHnode root;

    precomputeTriangleData();

    for (int i = triStart; i < triStart + numTris; ++i) {
        const vec3& tMin = triangleMin[i];
        const vec3& tMax = triangleMax[i];

        root.growToInclude(tMin, tMax);
    }

    root.setTriStart(triStart);
    root.setNumTri(numTris);

    BVHnodes.emplace_back(root);

    split(numTestsPerAxis, 0, depth-1);


}

void BaseModel::precomputeTriangleData() {
    triangleCenters.resize(triangles.size()/3);
    triangleMin.resize(triangles.size()/3);
    triangleMax.resize(triangles.size()/3);

    for (size_t i = 0; i < triangles.size()/3; ++i) {
        const ivec3& tri1 = triangles[3*i+0];
        const ivec3& tri2 = triangles[3*i+1];
        const ivec3& tri3 = triangles[3*i+2];

        const vec3& v1 = vertices[tri1.x];
        const vec3& v2 = vertices[tri2.x];
        const vec3& v3 = vertices[tri3.x];

        triangleCenters[i] = (v1 + v2 + v3) / 3.0f;
        triangleMin[i] = min(min(v1, v2), v3);
        triangleMax[i] = max(max(v1, v2), v3);
    }
}

float BaseModel::evaluateSplit(const int triStart, const int numTri, const int axis, const float pos) const {
    BVHnode nodeA;
    BVHnode nodeB;

    int numA = 0;
    int numB = 0;

    for (int i = triStart; i < numTri+triStart; ++i) {
        const vec3& tMin = triangleMin[i];
        const vec3& tMax = triangleMax[i];

        vec3 center = triangleCenters[i];

        if (center[axis] < pos) {
            nodeA.growToInclude(tMin, tMax);
            numA++;
        } else {
            nodeB.growToInclude(tMin, tMax);
            numB++;
        }
    }

    nodeA.setNumTri(numA);
    nodeB.setNumTri(numB);

    return nodeCost(nodeA) + nodeCost(nodeB);
}

Split BaseModel::chooseSplit(const int numTestsPerAxis, const BVHnode &node) {

    int triStart = node.getTriStart();
    int numTri = node.getNumTri();

    auto best = Split();
    std::mutex bestLock;

    for (int axis = 0; axis < 3; ++axis) {

        float centerMin = std::numeric_limits<float>::max();
        float centerMax = -std::numeric_limits<float>::max();

        for (int i = triStart; i < triStart + numTri; ++i) {
            float c = triangleCenters[i][axis];
            centerMin = std::min(centerMin, c);
            centerMax = std::max(centerMax, c);
        }

        for (int i = 0; i < numTestsPerAxis; ++i) {
            float splitT = float(i+1) / float(numTestsPerAxis+1);
            float pos = centerMin + (centerMax - centerMin) * splitT;

            if (numTri > 200) {

                pool.enqueue([this, &best, triStart, numTri, &bestLock](int a, float p) {
                  float cost = evaluateSplit(triStart, numTri, a, p);

                  std::lock_guard lock(bestLock);
                  if (cost < best.cost) {
                    best.cost = cost;
                    best.pos  = p;
                    best.axis = a;
                  }
                }, axis, pos);


            } else {
                float cost = evaluateSplit(triStart, numTri, axis, pos);

                if (cost < best.cost) {
                    best.pos = pos;
                    best.cost = cost;
                    best.axis = axis;
                }
            }
        }
    }

    pool.wait_for_tasks();

    return best;

}

void BaseModel::split(int numTestsPerAxis, int BVHindex, int depth) {

    BVHnode& node = BVHnodes[BVHindex];
    int triStart = node.getTriStart();
    int numTris = node.getNumTri();

    if (depth <= 1) {
        if (numTris > 10) {
            std::cerr << "Hit depth limit with " << numTris << " tri" << std::endl;
        }
        return;
    }

    if (numTris <= 1) {return;}

    BVHnode childA;
    BVHnode childB;

    int numA = 0, numB = 0;
    int startA = triStart, startB = triStart;

    Split bestSplit = chooseSplit(numTestsPerAxis, node);
    if (bestSplit.cost >= nodeCost(node)) {return;}

    if (bestSplit.axis == -1) {
        vec3 min = node.getMin();
        vec3 max = node.getMax();
        vec3 size = max - min;

        if (size.x > size.y && size.x > size.z) {
            bestSplit.axis = 0;
        } else if (size.y > size.z && size.y > size.x) {
            bestSplit.axis = 1;
        } else {
            bestSplit.axis = 2;
        }

        bestSplit.pos = (min[bestSplit.axis]+max[bestSplit.axis])/2;

        std::cerr << "Fallback to center split" << std::endl;
    }
    //std::cout << depth << ' ' << splitAxix << ' ' << splitPos << std::endl;

    for (int i = triStart; i < triStart+numTris; i++) {
        const vec3& tMin = triangleMin[i];
        const vec3& tMax = triangleMax[i];

        vec3 center = triangleCenters[i];
        bool triInA = center[bestSplit.axis] < bestSplit.pos;
        if (triInA) {
            childA.growToInclude(tMin, tMax);
            numA++;
            startB++;
            int swap = startA + numA - 1;
            std::swap(triangles[i*3+0], triangles[swap*3+0]);
            std::swap(triangles[i*3+1], triangles[swap*3+1]);
            std::swap(triangles[i*3+2], triangles[swap*3+2]);
            std::swap(triangleCenters[i], triangleCenters[swap]);
            std::swap(triangleMin[i], triangleMin[swap]);
            std::swap(triangleMax[i], triangleMax[swap]);
        } else {
            childB.growToInclude(tMin, tMax);
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
        childA.setTriStart(startA);
        childA.setNumTri(numA);
        childB.setTriStart(startB);
        childB.setNumTri(numB);

        int indexA = int(BVHnodes.size());
        int indexB = indexA + 1;

        node.setChildA(indexA);
        node.setChildB(indexB);

        BVHnodes.emplace_back(childA);
        BVHnodes.emplace_back(childB);

        split(numTestsPerAxis, indexA, depth-1);
        split(numTestsPerAxis, indexB, depth-1);

    } else if (numA > 10) {
        std::cerr << "Big Leaf Node  " << numA << " tri" << std::endl;
    } else if (numB > 10) {
        std::cerr << "Big Leaf Node  " << numB << " tri" << std::endl;
    }
}
