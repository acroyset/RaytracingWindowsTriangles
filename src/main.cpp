#include <ctime>
#include <iostream>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Scene.h"

int main() {
    Scene scene(1,5, 8);

    Timer t;

    //scene.loadJSON("scenes/scene3.json");
    scene.addModel("assets/models/sphere.obj", Transformation(), "assets/textures/wood.png");

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


