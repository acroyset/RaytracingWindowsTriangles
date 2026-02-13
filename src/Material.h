//
// Created by acroy on 2/12/2026.
//

#ifndef MATERIAL_H
#define MATERIAL_H
#include <glm/vec3.hpp>

struct Material {
    vec3 diffuseColor{};
    float smoothness;
    vec3 specularColor{};
    float specularProbability;
    float transparency;
    float ior;
    float emissionStrength;
    std::string texturePath;

    Material(
        vec3 diffuseColor = vec3(1),
        float smoothness = 0,
        vec3 specularColor = vec3(-1),
        float specularProbability = 0.0f,
        float transparency = 0.0f,
        float ior = 1.0f)
    {
        this->diffuseColor = diffuseColor;
        this->smoothness = smoothness;
        if (specularColor == vec3(-1)) {
            this->specularColor = vec3(0);
            this->specularProbability = -1.0f;
        } else {
            this->specularColor = specularColor;
            this->specularProbability = specularProbability;
        }
        this->transparency = transparency;
        this->ior = ior;
        this->emissionStrength = 0.0f;
    }

    Material(
        const std::string& texturePath,
        float smoothness = 0,
        vec3 specularColor = vec3(-1),
        float specularProbability = 0.0f,
        float transparency = 0.0f,
        float ior = 1.0f)
    {
        this->diffuseColor = vec3(0);
        this->smoothness = smoothness;
        if (specularColor == vec3(-1)) {
            this->specularColor = vec3(0);
            this->specularProbability = -1.0f;
        } else {
            this->specularColor = specularColor;
            this->specularProbability = specularProbability;
        }
        this->transparency = transparency;
        this->ior = ior;
        this->emissionStrength = 0.0f;
        this->texturePath = texturePath;
    }

    Material(
        float emissionStrength,
        vec3 color)
    {
        this->diffuseColor = color;
        this->smoothness = 0.0f;
        this->specularColor = vec3(0);
        this->specularProbability = -1.0f;
        this->transparency = 0.0f;
        this->ior = 1.0f;
        this->emissionStrength = emissionStrength;
    }
};


#endif //MATERIAL_H
