#include <iostream>
#include <fstream>
#include "Scene.h"

int main() {
    Scene scene(1,5, 8);

    Timer t;

    //scene.startLoadJob("scenes/scene1.json");

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


