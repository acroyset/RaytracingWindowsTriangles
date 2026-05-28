#include <iostream>
#include <fstream>
#include "Scene.h"

int main() {
    Scene scene(1,5, 8);

    scene.set_ssbo();

    // render loop
    while (scene.open()) {
        scene.updateFrame();
    }

    return 0;
}


