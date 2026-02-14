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

    std::vector<
    vec3> boundingBoxMin;
    std::vector<
    vec3> boundingBoxMax;
    std::vector<int> childA;
    std::vector<int> childB;

    std::vector<
    vec3> triangleCenters;
    std::vector<
    vec3> triangleMin, triangleMax;

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

    [[nodiscard]] float evaluateSplit(int childA, int childB, int axis, float pos) const;

    void chooseSplit(int numTestsPerAxis,
    vec3 min, int childA,
    vec3 max, int childB, int& bestAxis, float& bestPos, float& bestCost);

    void split(int numTestsPerAxis,
    vec3 bboxMin, int& childA,
    vec3 bboxMax, int& childB, int depth);

    void createBVH(int depth, int numTestsPerAxis, int triStart, int numTris);
};



#endif //BASEMODEL_H
