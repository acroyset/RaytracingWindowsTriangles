//
// Created by acroy on 2/16/2026.
//

#ifndef MODEL_H
#define MODEL_H

struct  Model {
    std::string filename;
    std::string name = "Null";

    BaseModel base;
    Transformation transformation;
    std::vector<Material> materials;
    std::vector<std::string> materialNames;

    Model(const std::string &name, const BaseModel& baseModel, const Transformation &transformation = {vec3(0), vec3(-1)}){
        this->filename = baseModel.filename;
        this->name = name;
        this->base = baseModel;
        this->transformation = transformation.scale == vec3(-1) ? baseModel.baseTransform : transformation;
        this->materials = baseModel.materials;
        this->materialNames = baseModel.materialNames;
    }

    Model(const Model& other){
        this->filename = other.filename;
        this->name = other.name;
        this->base = other.base;
        this->transformation = other.transformation;
        this->materials = other.materials;
        this->materialNames = other.materialNames;
    }
};

#endif //MODEL_H
