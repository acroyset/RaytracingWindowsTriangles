//
// Created by acroy on 2/12/2026.
//

#ifndef MATERIAL_H
#define MATERIAL_H
#include <glm/vec3.hpp>

using namespace glm;

class Material {

    vec4 diffuseColor{}; // diffuse color, smoothness
    vec4 specularColor{}; // specular color, specular probability
    vec4 glassLightSettings{}; // transparency, ior, emission strength, (padding)

public:

    Material() = default;

    Material(
        vec3 diffuseColor,
        float smoothness,
        vec3 specularColor = vec3(-1),
        float specularProbability = 0.0f,
        float transparency = 0.0f,
        float ior = 1.0f)
    {
        this->diffuseColor = vec4(diffuseColor, smoothness);
        if (specularColor == vec3(-1)) {
            this->specularColor = vec4(0, 0, 0, -1);
        } else {
            this->specularColor = vec4(specularColor, specularProbability);
        }
        this->glassLightSettings = vec4(transparency, ior, 0, 0);
    }

    Material(
        float emissionStrength,
        vec3 color)
    {
        this->diffuseColor = vec4(color, 0);
        this->specularColor = vec4(0, 0, 0, -1);
        this->glassLightSettings = vec4(0, 1, emissionStrength, 0);
    }

    [[nodiscard]] vec3 getDiffuseColor() const {
        return xyz(diffuseColor);
    }
    [[nodiscard]] float getSmoothness() const {
        return diffuseColor.w;
    }
    [[nodiscard]] vec3 getSpecularColor() const {
        return xyz(specularColor);
    }
    [[nodiscard]] float getSpecularProbability() const {
        return specularColor.w;
    }
    [[nodiscard]] float getTransparency() const {
        return glassLightSettings.x;
    }
    [[nodiscard]] float getIOR() const {
        return glassLightSettings.y;
    }
    [[nodiscard]] float getEmissionStrength() const {
        return glassLightSettings.z;
    }

    [[nodiscard]] vec4 getDC() const {
        return diffuseColor;
    }
    [[nodiscard]] vec4 getSC() const {
        return specularColor;
    }
    [[nodiscard]] vec4 getGLS() const {
        return glassLightSettings;
    }

    void setDiffuseColor(const vec3 color) {
        diffuseColor = vec4(color, diffuseColor.w);
    }
    void setSmoothness(const float smoothness) {
        diffuseColor.w = smoothness;
    }
    void setSpecularColor(const vec3 color) {
        specularColor = vec4(color, specularColor.w);
    }
    void setSpecularProbability(const float probability) {
        specularColor.w = probability;
    }
    void setTransparency(const float transparency) {
        glassLightSettings.x = transparency;
    }
    void setIOR(const float ior) {
        glassLightSettings.y = ior;
    }
    void setEmissionStrength(const float emissionStrength) {
        glassLightSettings.z = emissionStrength;
    }

    void setDC(const vec4 dc) {
        diffuseColor = dc;
    }
    void setSC(const vec4 sc) {
        specularColor = sc;
    }
    void setGLS(const vec4 gls) {
        glassLightSettings = gls;
    }
};


#endif //MATERIAL_H
