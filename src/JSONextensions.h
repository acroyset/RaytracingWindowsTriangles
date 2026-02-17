//
// Created by acroy on 2/16/2026.
//

#ifndef JSONEXTENSIONS_H
#define JSONEXTENSIONS_H

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Material.h"
#include "Transformation.h"
#include "../external/json.hpp"

using json = nlohmann::json;


namespace glm {
    inline void to_json(nlohmann::json& j, const vec3& v) {
        j = nlohmann::json::array({ v.x, v.y, v.z });
    }
    inline void from_json(const nlohmann::json& j, vec3& v) {
        v.x = j.at(0).get<float>();
        v.y = j.at(1).get<float>();
        v.z = j.at(2).get<float>();
    }

    inline void to_json(nlohmann::json& j, const vec4& v) {
        j = nlohmann::json::array({ v.x, v.y, v.z, v.w });
    }
    inline void from_json(const nlohmann::json& j, vec4& v) {
        v.x = j.at(0).get<float>();
        v.y = j.at(1).get<float>();
        v.z = j.at(2).get<float>();
        v.w = j.at(3).get<float>();
    }
}

inline void to_json(json& j, const Material& m) {
    j["diffuseColor"] = m.getDC();
    j["specularColor"] = m.getSC();
    j["glassLightSettings"] = m.getGLS();
}
inline void from_json(const json& j, Material& m) {
    m.setDC(j.at("diffuseColor"));
    m.setSC(j.at("specularColor"));
    m.setGLS(j.at("glassLightSettings"));
}

inline void to_json(json& j, const Transformation& t) {
    j["position"] = t.position;
    j["rotation"] = t.rotation;
    j["scale"] = t.scale;
}
inline void from_json(const json& j, Transformation& t) {
    t.position = j.at("position");
    t.rotation = j.at("rotation");
    t.scale = j.at("scale");
    t.setMatrix();
}


#endif //JSONEXTENSIONS_H
