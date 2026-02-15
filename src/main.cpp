#include <ctime>
#include <iostream>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Scene.h"

int main() {
    Scene scene(1,5, 8);

    Timer t;


    //Model sphere("assets/models/sphere.obj");
    Model dragon("assets/models/dragon800K.obj");

    scene.addModel(
        "assets/models/cube.obj",
        {vec3(0, -10, 0), vec3(300, 10, 300)},
        {vec3(0.9), 0}
        );


    scene.addModel(
        dragon,
        {vec3(0, 70, 0), vec3(100)},
        {vec3(0.9), 1, vec3(0.95), 1, 1, 1.3}
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


