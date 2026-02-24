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

inline void to_json(json& j, const MaterialType& t) {
    switch (t) {
        case Specular:
            j = "Specular";
            return;
        case Transparent:
            j = "Transparent";
            return;
        case Emissive:
            j = "Emissive";
    }
}
inline void from_json(const json& j, MaterialType& t) {
    if (j == "Specular") {
        t = Specular;
    } else if (j == "Transparent") {
        t = Transparent;
    } else if (j == "Emissive") {
        t = Emissive;
    }
}

inline void to_json(json& j, const Material& m) {
    j["type"] = m.getType();
    j["diffuseColor"] = m.getDiffuseColor();
    j["diffuseRoughness"] = m.getDiffuseRoughness();
    j["specularColor"] = m.getSpecularColor();
    j["specularRoughness"] = m.getSpecularRoughness();
    j["specularProbability"] = m.getSpecularProbability();
    j["transparency"] = m.getTransparency();
    j["indexOfRefraction"] = m.getIndexOfRefraction();
    j["absorption"] = m.getAbsorption();
    j["emissionStrength"] = m.getEmissionStrength();
}
inline void from_json(const json& j, Material& m) {
    m.setType(j.at("type"));
    m.setDiffuseColor(j.at("diffuseColor"));
    m.setDiffuseRoughness(j.at("diffuseRoughness"));
    m.setSpecularColor(j.at("specularColor"));
    m.setSpecularRoughness(j.at("specularRoughness"));
    m.setSpecularProbability(j.at("specularProbability"));
    m.setTransparency(j.at("transparency"));
    m.setIndexOfRefraction(j.at("indexOfRefraction"));
    m.setAbsorption(j.at("absorption"));
    m.setEmissionStrength(j.at("emissionStrength"));
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
