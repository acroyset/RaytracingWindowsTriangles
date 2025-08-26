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
    std::vector<glm::ivec3> normals;
    std::vector<glm::vec3> normalsList;

    std::vector<glm::vec3> boundingBoxMin;
    std::vector<glm::vec3> boundingBoxMax;
    std::vector<int> childA;
    std::vector<int> childB;

    std::vector<glm::vec3> triangleCenters;
    std::vector<glm::vec3> triangleMin, triangleMax;

    BaseModel();

    static void parse(const std::string& nfilename, std::vector<glm::vec3>& vertices, std::vector<glm::ivec3>& triangles, std::vector<glm::vec3>& normalsList);

    explicit BaseModel(const std::string& filename);

    void precomputeTriangleData();

    [[nodiscard]] float evaluateSplit(int childA, int childB, int axis, float pos) const;

    void chooseSplit(int numTestsPerAxis, glm::vec3 min, int childA, glm::vec3 max, int childB, int& bestAxis, float& bestPos, float& bestCost) const;

    void split(int numTestsPerAxis, glm::vec3 bboxMin, int& childA, glm::vec3 bboxMax, int& childB, int depth);

    void createBVH(int depth, int numTestsPerAxis, int triStart, int numTris);
};



#endif //BASEMODEL_H
