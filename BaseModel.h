//
// Created by acroy on 7/26/2025.
//

#ifndef BASEMODEL_H
#define BASEMODEL_H

#include <glm/glm.hpp>
#include <string>
#include <GLFW/glfw3.h>

class BaseModel {
    public:

    std::string filename;

    std::vector<glm::vec3> vertices;
    std::vector<glm::ivec3> triangles;

    std::vector<glm::vec4> boundingBoxMin;
    std::vector<glm::vec4> boundingBoxMax;

    BaseModel();

    static void parse(const std::string& nfilename, std::vector<glm::vec3>& vertices, std::vector<glm::ivec3>& triangles);

    explicit BaseModel(const std::string& filename);

    [[nodiscard]] float evaluateSplit(glm::vec4 min, glm::vec4 max, int axis, float pos) const;

    void chooseSplit(int numTestsPerAxis, glm::vec4 min, glm::vec4 max, int& bestAxis, float& bestPos, float& bestCost) const;

    void split(int numTestsPerAxis, glm::vec4& bboxMin, glm::vec4& bboxMax, int depth);

    void createBVH(int depth, int numTestsPerAxis, int triStart, int numTris);
};



#endif //BASEMODEL_H
