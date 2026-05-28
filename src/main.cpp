#include <iostream>
#include <fstream>
#include "Core/Scene.h"

int main() {
    Scene scene(1,5, 8);

    scene.set_ssbo();

    // render loop
    while (scene.open()) {
        scene.updateFrame();
    }

    return 0;
}

/*
To Do / Ideas

Volumetrics — fog, smoke, participating media. Relatively straightforward to add to your path loop
Camera path animation — keyframe the camera and render a sequence
Live shader editing — you have reload but could watch files and auto-reload
Lens flares / chromatic aberration
glTF loading — much more common than OBJ for complex scenes, supports PBR materials natively
 /*
