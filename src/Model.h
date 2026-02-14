//
// Created by acroy on 7/26/2025.
//

#ifndef BASEMODEL_H
#define BASEMODEL_H

#include <glm/glm.hpp>
#include <string>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>

#include "Material.h"

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

class BVHnode {
    vec4 min{}; // xyz: bbox min    w: childA, -TriStart
    vec4 max{}; // xyz: bbox max    w: childB, -NumTri

    [[nodiscard]] int minW() const {
        return floatBitsToInt(min.w);
    }
    [[nodiscard]] int maxW() const {
        return floatBitsToInt(max.w);
    }

public:

    BVHnode() {
        min = vec4(vec3(std::numeric_limits<float>::infinity()), 0);
        max = vec4(vec3(-std::numeric_limits<float>::infinity()), 0);
    }

    BVHnode(vec4 min, vec4 max) {
        this->min = min;
        this->max = max;
    }

    [[nodiscard]] bool leaf() const {
        int w = minW();
        return w <= 0;
    }

    [[nodiscard]] vec3 getMin() const {
        return xyz(min);
    }
    [[nodiscard]] vec3 getMax() const {
        return xyz(max);
    }
    [[nodiscard]] int getChildA() const {
        int w = minW();
        if (w <= 0) std::cerr << "Tried to access child A of leaf node" << std::endl;
        return w;
    }
    [[nodiscard]] int getChildB() const {
        int w = maxW();
        if (w <= 0) std::cerr << "Tried to access child B of leaf node" << std::endl;
        return w;
    }
    [[nodiscard]] int getTriStart() const {
        int w = minW();
        if (w > 0) std::cerr << "Tried to access triStart of non leaf node" << std::endl;
        return -w;
    }
    [[nodiscard]] int getNumTri() const {
        int w = maxW();
        if (w > 0) std::cerr << "Tried to access numTri of non leaf node" << std::endl;
        return -w;
    }

    [[nodiscard]] vec3 getCenter() const {
        return 0.5f*(getMin() + getMax());
    }

    void setChildA(const int a) { min.w = intBitsToFloat(a); }
    void setChildB(const int b) { max.w = intBitsToFloat(b); }

    void setTriStart(const int triStart) {min.w = intBitsToFloat(-triStart); }
    void setNumTri(const int numTri) {max.w = intBitsToFloat(-numTri); }

    void setMin(const vec3& min) {this->min = vec4(min, this->min.w);}
    void setMax(const vec3& max) {this->max = vec4(max, this->max.w);}

    void growToInclude(const vec3& tMin, const vec3& tMax) {
        vec3 bmin = xyz(min);
        vec3 bmax = xyz(max);
        bmin = glm::min(bmin, tMin);
        bmax = glm::max(bmax, tMax);
        min = vec4(bmin, min.w);
        max = vec4(bmax, max.w);
    }
};

class Model {
    public:

    std::string filename;

    std::vector<
    ivec4> triangles;
    std::vector<
    vec3> vertices;
    std::vector<
    vec2> texCoords;
    std::vector<
    vec3> normals;
    std::vector<Material> materials;

    std::vector<BVHnode> BVHnodes;

    std::vector<vec3> triangleCenters;
    std::vector<vec3> triangleMin, triangleMax;

    ThreadPool pool{std::thread::hardware_concurrency()};

    Model();

    static void parse(
        const std::string& nfilename,
        std::vector<
        ivec3>& triangles,
        std::vector<
        vec3>& vertices,
        std::vector<
        vec2>& texCoords,
        std::vector<
        vec3>& normals,
        std::vector<int>& tempTriMatIndex,
        std::vector<Material>& materials
    );

    explicit Model(const std::string& filename);

    void precomputeTriangleData();

    [[nodiscard]] float evaluateSplit(int triStart, int numTri, int axis, float pos) const;

    void chooseSplit(int numTestsPerAxis, const BVHnode &node, int& bestAxis, float& bestPos, float& bestCost);

    void split(int numTestsPerAxis, int BVHindex, int depth);

    void createBVH(int depth, int numTestsPerAxis, int triStart, int numTris);
};



#endif //BASEMODEL_H
