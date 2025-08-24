//
// Created by acroy on 7/26/2025.
//

#include "BaseModel.h"
#include <fstream>
#include <iostream>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <mutex>

class ThreadPool {
    std::vector<std::thread> workers;
    std::vector<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable cv;
    std::condition_variable finished_cv;
    bool stop = false;
    size_t active_tasks = 0;  // Tracks how many tasks are currently running
    std::atomic<size_t> total_enqueued{0};
    std::atomic<size_t> total_completed{0};

public:
    explicit ThreadPool(const size_t threads) {
        for (size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->cv.wait(lock, [this] {
                            return this->stop || !this->tasks.empty();
                        });

                        if (this->stop && this->tasks.empty())
                            return;

                        task = std::move(this->tasks.back());
                        this->tasks.pop_back();
                        ++active_tasks;
                    }

                    task();

                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        --active_tasks;
                        ++total_completed;
                        if (tasks.empty() && active_tasks == 0) {
                            finished_cv.notify_all();
                        }
                    }
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        cv.notify_all();
        for (auto &worker : workers)
            worker.join();
    }

    void enqueue(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.push_back(std::move(task));
            ++total_enqueued;
        }
        cv.notify_one();
    }

    // Wait for all tasks to finish
    void wait_for_tasks() {
        std::unique_lock<std::mutex> lock(queue_mutex);
        finished_cv.wait(lock, [this] {
            return tasks.empty() && active_tasks == 0;
        });
    }

    size_t tasks_left() {
        std::unique_lock<std::mutex> lock(queue_mutex);
        return total_enqueued - total_completed;
    }

};

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
float nodeCost(const glm::vec4 min, const glm::vec4 max) {
    const glm::vec3 size = xyz(max-min);
    const float halfArea = size.x * (size.y + size.z) + size.y * size.z;
    return halfArea * -max.w;
}
inline float fast_strtof(const char* str, char** endptr) {
    return std::strtof(str, endptr);
    // Or use a faster implementation like:
    // return fast_float::from_chars(str, str + strlen(str), result).ptr;
}
inline int fast_strtoi(const char* str, char** endptr) {
    return std::strtol(str, endptr, 10);
}

BaseModel::BaseModel() = default;

void BaseModel::parse(const std::string& nfilename, std::vector<glm::vec3>& vertices, std::vector<glm::ivec3>& triangles) {
    const std::string filename = "" + nfilename;
    std::ifstream model(filename, std::ios::in | std::ios::binary);

    if (!model.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    // Get file size for reservations
    model.seekg(0, std::ios::end);
    std::streamsize fileSize = model.tellg();
    model.seekg(0, std::ios::beg);

    // Reserve space
    size_t estimatedVertices = fileSize / 50;
    size_t estimatedTriangles = fileSize / 80;
    vertices.reserve(estimatedVertices);
    triangles.reserve(estimatedTriangles * 3);

    // Process in chunks to avoid massive memory allocation
    constexpr size_t CHUNK_SIZE = 64 * 1024 * 1024; // 64MB chunks
    std::vector<char> buffer(CHUNK_SIZE + 1);
    std::string leftover; // Handle lines split across chunks

    int quads = 0;

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
                    triangles.emplace_back(faceVertices[0]);
                    triangles.emplace_back(faceVertices[1]);
                    triangles.emplace_back(faceVertices[2]);

                    // Handle quads and n-gons
                    for (int i = 3; i < vertexCount; i++) {
                        quads++;
                        triangles.emplace_back(faceVertices[0]);
                        triangles.emplace_back(faceVertices[i-1]);
                        triangles.emplace_back(faceVertices[i]);
                    }
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
        // Process final incomplete line if needed
        // (similar parsing logic as above)
    }

    // Convert negative indices and adjust for 0-based indexing
    for (glm::ivec3& i : triangles) {
        if (i.x < 0) i.x += int(vertices.size()) + 1;
        if (i.y < 0) i.y += int(vertices.size()) + 1;
        if (i.z < 0) i.z += int(vertices.size()) + 1;
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

BaseModel::BaseModel(const std::string &filename) {
    this->filename = filename;
    std::vector<glm::vec3> tempVertices;
    std::vector<glm::ivec3> tempTriangles;

    parse(filename, tempVertices, tempTriangles);

    std::cout << filename << std::endl;
    std::cout << tempVertices.size() << std::endl;
    std::cout << tempTriangles.size()/3 << std::endl;

    int triStart = 0;
    int numTris = int(tempTriangles.size())/3;

    for (glm::vec3 tempVertice : tempVertices) {
        vertices.emplace_back(tempVertice);
    }
    for (int i = 0; i < numTris; ++i) {
        triangles.emplace_back(tempTriangles[i*3+0].x, tempTriangles[i*3+1].x, tempTriangles[i*3+2].x);
    }

    createBVH(64, 5, triStart, numTris);
}

void BaseModel::createBVH(const int depth, const int numTestsPerAxis, int triStart, int numTris) {

    auto min = glm::vec3(1000000000.0f);
    auto max = glm::vec3(-1000000000.0f);

    precomputeTriangleData();

    for (int i = triStart; i < triStart + numTris; ++i) {
        const glm::vec3& tMin = triangleMin[i];
        const glm::vec3& tMax = triangleMax[i];

        growToInclude(min, max, tMin, tMax);
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

void BaseModel::precomputeTriangleData() {
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

float BaseModel::evaluateSplit(glm::vec4 min, glm::vec4 max, int axis, float pos) const {
    auto minA = glm::vec4(1000000000.0f), maxA = glm::vec4(-1000000000.0f);
    auto minB = glm::vec4(1000000000.0f), maxB = glm::vec4(-1000000000.0f);
    maxA.w = 0; maxB.w = 0;

    int triStart = -int(min.w);
    int numTri = -int(max.w);

    for (int i = triStart; i < numTri+triStart; ++i) {
        const glm::vec3& tMin = triangleMin[i];
        const glm::vec3& tMax = triangleMax[i];

        glm::vec3 center = triangleCenters[i];

        if (center[axis] < pos) {
            growToInclude(minA, maxA, tMin, tMax);
            maxA.w --;
        } else {
            growToInclude(minB, maxB, tMin, tMax);
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
    float splitPos = 0;
    float bestCost = 1000000000000.0f;
    chooseSplit(numTestsPerAxis, bboxMin, bboxMax, splitAxis, splitPos, bestCost);
    if (bestCost >= nodeCost(bboxMin, bboxMax)) {return;}

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
