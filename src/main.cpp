#include <ctime>
#include <iostream>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Scene.h"

int main() {
    Scene scene(1,5, 8);

    Timer t;


    Model sphere("assets/models/sphere.obj");
    Model bull("assets/models/bull.obj");
    Model dragon("assets/models/dragon800K.obj");

    scene.addModel(
        dragon,
        {vec3(-100, 70, 0), vec3(100)},
        {vec3(0.5), 1, vec3(1), 0.1}
        );

    scene.addModel(
        bull,
        {vec3(100, 50, 0), vec3(100)},
        {vec3(0), 1, vec3(1), 0.1}
        );


    scene.addModel(
        "assets/models/cube.obj",
        {vec3(0, -20, 0), vec3(300, 20, 300)},
        {vec3(1), 0}
        );


    scene.addModel(
        sphere,
        {vec3(-100, 20, 200), vec3(20)},
        {},
        "assets/textures/grass.png"
        );

    scene.addModel(
        sphere,
        {vec3(-33.33, 20, 200), vec3(20)},
        {},
        "assets/textures/rock.png"
    );

    scene.addModel(
        sphere,
        {vec3(33.33, 20, 200), vec3(20)},
        {vec3(0), 1, vec3(1), 0.1},
        "assets/textures/wood.png"
        );

    scene.addModel(
        sphere,
        {vec3(100, 20, 200), vec3(20)},
        {vec3(0), 1, vec3(1), 0.1},
        "assets/textures/marble.png"
        );


    std::cout << std::endl;
    std::cout << "Total build time: " << t.reset() << std::endl;

    scene.set_ssbo();

    std::cout << "SSBO build time: " << t.reset() << std::endl;

    scene.displayStats();

    // render loop
    while (scene.open()) {
        scene.updateFrame();
    }

    return 0;
}


