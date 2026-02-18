//
// Created by acroy on 2/12/2026.
//

#ifndef MATERIAL_H
#define MATERIAL_H
#include <glm/vec3.hpp>

using namespace glm;

enum MaterialType : int {Opaque=0, Specular=1, Transparent=2, Emissive=3};

class Material {

    // Opaque

    vec4 diffuseColor{};
    vec4 specularColor{};

    float diffuseRoughness;
    float specularRoughness;
    float specularProbability;

    // Transparent

    float transparency;
    float indexOfRefraction;
    float absorption;

    // absorb color = diffuse color
    // roughness = diffuse roughness

    // specular color = same
    // specular roughness = same
    // specular prob = same

    // Emissive

    float emissionStrength;

    MaterialType type;

public:

    Material() {
        type = Opaque;

        diffuseColor = vec4(vec3(0.95), 0);
        diffuseRoughness = 1;

        specularColor = vec4(0);
        specularRoughness = 0;
        specularProbability = 0;

        transparency = 0;
        indexOfRefraction = 1;
        absorption = 0;

        emissionStrength = 0;
    }

    Material(
        MaterialType type,
        vec3 diffuseColor,
        float diffuseRoughness,
        vec3 specularColor = vec3(-1),
        float specularRoughness = 0,
        float specularProbability = 0.0f,
        float transparency = 0.0f,
        float ior = 1.0f,
        float absorption = 0.0f)
    {
        this->type = type;

        this->diffuseColor = vec4(diffuseColor, 0);
        this->diffuseRoughness = diffuseRoughness;

        this->specularColor = vec4(specularColor, 0);
        this->specularRoughness = specularRoughness;
        this->specularProbability = specularProbability;

        this->transparency = transparency;
        this->indexOfRefraction = ior;
        this->absorption = absorption;

        this->emissionStrength = 0;
    }

    Material(
        float emissionStrength,
        vec3 color)
    {
        type = Emissive;
        this->diffuseColor = vec4(color, 0);
        this->emissionStrength = emissionStrength;
    }

    [[nodiscard]] MaterialType getType() const { return type; }

    [[nodiscard]] vec3 getDiffuseColor() const { return xyz(diffuseColor); }
    [[nodiscard]] float getDiffuseRoughness() const { return diffuseRoughness; }

    [[nodiscard]] vec3 getSpecularColor() const { return xyz(specularColor); }
    [[nodiscard]] float getSpecularRoughness() const { return specularRoughness; }
    [[nodiscard]] float getSpecularProbability() const { return specularProbability; }

    [[nodiscard]] float getTransparency() const { return transparency; }
    [[nodiscard]] float getIndexOfRefraction() const { return indexOfRefraction; }
    [[nodiscard]] float getAbsorption() const { return absorption; }

    [[nodiscard]] float getEmissionStrength() const { return emissionStrength; }


    void setType(MaterialType type) { this->type = type; }

    void setDiffuseColor(vec3 color) { this->diffuseColor = vec4(color, 0); }
    void setDiffuseRoughness(float roughness) { this->diffuseRoughness = roughness; }

    void setSpecularColor(vec3 color) { this->specularColor = vec4(color, 0); }
    void setSpecularRoughness(float roughness) { this->specularRoughness = roughness; }
    void setSpecularProbability(float probability) { this->specularProbability = probability; }

    void setTransparency(float transparency) { this->transparency = transparency; }
    void setIndexOfRefraction(float ior) { this->indexOfRefraction = ior; }
    void setAbsorption(float absorption) { this->absorption = absorption; }

    void setEmissionStrength(float e) { this->emissionStrength = e; }

};


#endif //MATERIAL_H
