//
// Created by acroy on 2/26/2026.
//

#ifndef DEBUGVIEW_H
#define DEBUGVIEW_H

#include "Rendering/Uniform.h"

enum DebugMode {
    Normals,
    Heatmap,
    Depth
};

struct DebugView {
    bool enable = false;
    DebugMode mode = Normals;
    int triTh = 5;
    int aabbTh = 100;
    float depthScale = 1500;
};



template<>
struct UniformFields<DebugView> {
    Uniform<bool> uEnable;
    Uniform<int> uMode;
    Uniform<int> uTriTh;
    Uniform<int> uAABBTh;
    Uniform<float> uDepthScale;

    UniformFields() = default;

    UniformFields(GLuint program, const std::string& name) :
        uEnable(program, name + ".enable"),
        uMode(program, name + ".mode"),
        uTriTh(program, name + ".triTh"),
        uAABBTh(program, name + ".aabbTh"),
        uDepthScale(program, name + ".depthScale")
    {}

    void set(const DebugView& d) const {
        uEnable.set(d.enable);
        uMode.set(d.mode);
        uTriTh.set(d.triTh);
        uAABBTh.set(d.aabbTh);
        uDepthScale.set(d.depthScale);
    }
};

#endif //DEBUGVIEW_H
