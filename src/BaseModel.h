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
#include <future>
#include <iostream>
#include <mutex>

#include "Material.h"
#include "Transformation.h"


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

    template <class F, class... Args>
    std::future<std::invoke_result_t<F, Args...>> enqueue(F&& f, Args&&... args) {
        using R = std::invoke_result_t<F, Args...>;

        auto job = std::make_shared<std::packaged_task<R()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<R> fut = job->get_future();

        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }

            tasks.emplace_back([job]() { (*job)(); });
            ++total_enqueued;
        }

        cv.notify_one();
        return fut;
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
    float minX, minY, minZ;// xyz: bbox min
    int childA_TriStart; // childA, -TriStart
    float maxX, maxY, maxZ;// xyz: bbox max
    int childB_NumTri; // childB, -NumTri

public:

    BVHnode() {
        minX = std::numeric_limits<float>::infinity();
        minY = std::numeric_limits<float>::infinity();
        minZ = std::numeric_limits<float>::infinity();
        maxX = -std::numeric_limits<float>::infinity();
        maxY = -std::numeric_limits<float>::infinity();
        maxZ = -std::numeric_limits<float>::infinity();
        childA_TriStart = 0;
        childB_NumTri = 0;
    }

    [[nodiscard]] bool leaf() const {
        return childA_TriStart <= 0;
    }

    [[nodiscard]] vec3 getMin() const {
        return {minX, minY, minZ};
    }
    [[nodiscard]] vec3 getMax() const {
        return {maxX, maxY, maxZ};
    }
    [[nodiscard]] int getChildA() const {
        if (childA_TriStart <= 0) std::cerr << "Tried to access child A of leaf node" << std::endl;
        return childA_TriStart;
    }
    [[nodiscard]] int getChildB() const {
        if (childB_NumTri <= 0) std::cerr << "Tried to access child B of leaf node" << std::endl;
        return childB_NumTri;
    }
    [[nodiscard]] int getTriStart() const {
        if (childA_TriStart > 0) std::cerr << "Tried to access triStart of non leaf node" << std::endl;
        return -childA_TriStart;
    }
    [[nodiscard]] int getNumTri() const {
        if (childB_NumTri > 0) std::cerr << "Tried to access numTri of non leaf node" << std::endl;
        return -childB_NumTri;
    }

    [[nodiscard]] vec3 getCenter() const {
        return 0.5f*(getMin() + getMax());
    }

    void setChildA(const int a) { childA_TriStart = a; }
    void setChildB(const int b) { childB_NumTri = b; }

    void setTriStart(const int triStart) { childA_TriStart = -triStart; }
    void setNumTri(const int numTri) { childB_NumTri = -numTri; }

    void setMin(const vec3& min) {
        minX = min.x;
        minY = min.y;
        minZ = min.z;
    }
    void setMax(const vec3& max) {
        maxX = max.x;
        maxY = max.y;
        maxZ = max.z;
    }

    void growToInclude(const vec3& tMin, const vec3& tMax) {
        minX = std::min(minX, tMin.x);
        minY = std::min(minY, tMin.y);
        minZ = std::min(minZ, tMin.z);
        maxX = std::max(maxX, tMax.x);
        maxY = std::max(maxY, tMax.y);
        maxZ = std::max(maxZ, tMax.z);
    }
};

struct Split {
    int axis = -1;
    float pos = 0.0f;
    float cost = std::numeric_limits<float>::infinity();
};

class BaseModel {
    public:

    std::string filename;
    bool valid = false;
    Transformation baseTransform;

    std::vector<ivec4> triangles;
    std::vector<vec3> vertices;
    std::vector<vec2> texCoords;
    std::vector<vec3> normals;
    std::vector<Material> materials;

    std::vector<BVHnode> BVHnodes;

    std::vector<vec3> triangleCenters;
    std::vector<vec3> triangleMin, triangleMax;

    ThreadPool pool{std::thread::hardware_concurrency()};

    BaseModel();

    BaseModel(const BaseModel& model) {
        filename = model.filename;
        valid = model.valid;
        baseTransform = model.baseTransform;

        triangles = model.triangles;
        vertices = model.vertices;
        texCoords = model.texCoords;
        normals = model.normals;
        materials = model.materials;

        BVHnodes = model.BVHnodes;

        triangleCenters = model.triangleCenters;
        triangleMin = model.triangleMin;
        triangleMax = model.triangleMax;
    }

    BaseModel& operator = (const BaseModel& model) {
        filename = model.filename;
        valid = model.valid;
        baseTransform = model.baseTransform;

        triangles = model.triangles;
        vertices = model.vertices;
        texCoords = model.texCoords;
        normals = model.normals;
        materials = model.materials;

        BVHnodes = model.BVHnodes;

        triangleCenters = model.triangleCenters;
        triangleMin = model.triangleMin;
        triangleMax = model.triangleMax;
        return *this;
    }

    bool parse(const std::string& filename);

    explicit BaseModel(const std::string& filename);

    void precomputeTriangleData();

    [[nodiscard]] float evaluateSplit(int triStart, int numTri, int axis, float pos) const;

    Split chooseSplit(int numTestsPerAxis, const BVHnode &node);

    void split(int numTestsPerAxis, int BVHindex, int depth);

    void createBVH(int depth, int numTestsPerAxis, int triStart, int numTris);
};



#endif //BASEMODEL_H
