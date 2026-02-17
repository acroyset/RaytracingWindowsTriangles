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

    int textureID = -1;

    Model(const std::string &name, const BaseModel& baseModel, const Transformation &transformation){
        this->filename = baseModel.filename;
        this->name = name;
        this->base = baseModel;
        this->transformation = transformation;
        this->materials = baseModel.materials;
        textureID = -1;
    }

    Model(const Model& other){
        this->filename = other.filename;
        this->name = other.name;
        this->base = other.base;
        this->transformation = other.transformation;
        this->materials = other.materials;
        this->textureID = other.textureID;
    }
};

#endif //MODEL_H
