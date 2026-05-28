//
// Created by acroy on 2/26/2026.
//

#ifndef UNIFORMBLOCKS_H
#define UNIFORMBLOCKS_H

#include "../Rendering/Uniform.h"

struct Camera {
    vec3  pos;
    vec3  forward;
    vec3  up;
    vec3  right;
    float fovDeg = 60;
    float aperture = 0;
    float focusDistance = 1000;
    bool  focusDistancePlane = false;
};

template<>
struct UniformFields<Camera> {
    Uniform<vec3> uPos;
    Uniform<vec3> uForward;
    Uniform<vec3> uUp;
    Uniform<vec3> uRight;
    Uniform<float> uFovDeg;
    Uniform<float> uAperture;
    Uniform<float> uFocusDistance;
    Uniform<bool> uFocusDistancePlane;

    UniformFields() = default;

    UniformFields(GLuint program, const std::string& name) :
        uPos(               program, name + ".pos"               ),
        uForward(           program, name + ".forward"           ),
        uUp(                program, name + ".up"                ),
        uRight(             program, name + ".right"             ),
        uFovDeg(            program, name + ".fovDeg"            ),
        uAperture(          program, name + ".aperture"          ),
        uFocusDistance(     program, name + ".focusDistance"     ),
        uFocusDistancePlane(program, name + ".focusDistancePlane")
    {}

    void set(const Camera& d) const {
        uPos.set(d.pos);
        uForward.set(d.forward);
        uUp.set(d.up);
        uRight.set(d.right);
        uFovDeg.set(d.fovDeg);
        uAperture.set(d.aperture);
        uFocusDistance.set(d.focusDistance);
        uFocusDistancePlane.set(d.focusDistancePlane);
    }
};

#endif //UNIFORMBLOCKS_H
