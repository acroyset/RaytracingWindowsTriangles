#include <ctime>
#include <iostream>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Scene.h"

class Timer {
    std::clock_t start;
    std::clock_t pause{};
    bool paused = false;

public:
    explicit Timer(const bool paused = false) {
        start = std::clock();
        this->paused = paused;
        if (paused) {
            pause = std::clock();
        }
    }

    float reset() {
        const std::clock_t end = std::clock();
        const float t = (float)(end - start) / CLOCKS_PER_SEC;
        start = end;
        return t;
    }
    [[nodiscard]] float elapsed() const {
        const std::clock_t offset = paused ? std::clock() - pause : 0;
        const std::clock_t end = std::clock();
        const float t = (float)(end - start - offset) / CLOCKS_PER_SEC;
        return t;
    }
    void start_stop() {
        if (paused) {
            paused = false;
            const std::clock_t elapsed = std::clock() - pause;
            start += elapsed;
        } else {
            paused = true;
            pause = std::clock();
        }
    }
};

int main() {
    Scene scene(1,3, 16);

    Timer t;

    Model bull("assets/models/bull.obj");
    scene.addModel(bull, vec3(-100, 50, 0), vec3(100), vec3(1, 0, 0), 1, vec3(1), 0.1);
    scene.addModel(bull, vec3(100, 50, 0), vec3(100), vec3(0, 0, 0), 1, vec3(1), 0.1);
    scene.addModel("assets/models/cube/cube.obj", vec3(0, -20, 0), vec3(300, 20, 300), "assets/textures/wood.png", 1, vec3(1), 0.1);
    //scene.addModel("models/dragon800K.obj", vec3(0, 70.498, 0), vec3(100), vec3(0.01), 0.99, vec3(0.15), 0.4);
    //scene.addModel("models/sphere.obj", vec3(200, 50, 0), vec3(50), vec3(1, 0.7, 0.3), 0, vec3(0), 0, 0, 1, 2);

    //scene.addModel("models/quad.txt", vec3(0,4999, 0), vec3(1000), vec3(0.95), 0, vec3(0), 0, 0, 1, 20);
    //scene.addModel("models/cubeInternal.txt", vec3(0,2000, 0), vec3(3000), vec3(0.95), 0);
    //scene.addModel(dragon, vec3(0,762.30, 0), vec3(2500), vec3(0.01), 0.99, vec3(0.95), 0.15);


    float duration = t.reset();

    //scene.displayBVH();

    scene.set_ssbo();

    // display bvh stats
    if (true) {
        int leafNodes = 0, depth = 0, triPerLeaf = 0;
        int minTriPerLeaf = 100000000, maxTriPerLeaf = 0;
        int minDepth = 100000000, maxDepth = 0;
        scene.get_BVH_stats(0, leafNodes, depth, minDepth, maxDepth, triPerLeaf, minTriPerLeaf, maxTriPerLeaf, 1);
        std::cout << std::endl;
        std::cout << "Time (ms): " << duration*1000.0f << std::endl;
        std::cout << "Triangles: " << scene.getNumTris() << std::endl;
        std::cout << "Node Count: " << scene.getNumBVHNodes() << std::endl;
        std::cout << "Leaf Count: " << leafNodes << std::endl;
        std::cout << "Leaf Depth: " << std::endl;
        std::cout << "  -  Min: " << minDepth << std::endl;
        std::cout << "  -  Max: " << maxDepth << std::endl;
        std::cout << "  -  Mean: " << float(depth)/float(leafNodes) << std::endl;
        std::cout << "Leaf Tris: " << std::endl;
        std::cout << "  -  Min: " << minTriPerLeaf << std::endl;
        std::cout << "  -  Max: " << maxTriPerLeaf << std::endl;
        std::cout << "  -  Mean: " << float(triPerLeaf)/float(leafNodes) << std::endl;
    }

    // render loop
    while (scene.open()) {
        scene.updateFrame();
    }

    return 0;
}


