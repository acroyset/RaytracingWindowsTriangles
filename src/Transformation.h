//
// Created by acroy on 2/12/2026.
//

#ifndef TRANSFORMATION_H
#define TRANSFORMATION_H
#include <glm/vec3.hpp>
#include <glm/ext/matrix_transform.hpp>

struct Transformation {
    vec3 position{};
    vec3 rotation{}; // rad
    vec3 scale{};
    mat4 matrix{};
    mat4 inverseMatrix{};

    Transformation() = default;

    Transformation(vec3 position, vec3 scale, vec3 rotation = vec3(0)) : position(position), rotation(rotation), scale(scale) {setMatrix();}

    void setMatrix() {
        mat4 I(1.0f);

        // Scale
        mat4 S = glm::scale(I, scale);

        // Rotation (order: Z * Y * X)
        mat4 Rx = rotate(I, rotation.x, vec3(1, 0, 0));
        mat4 Ry = rotate(I, rotation.y, vec3(0, 1, 0));
        mat4 Rz = rotate(I, rotation.z, vec3(0, 0, 1));
        mat4 R = Rz * Ry * Rx;

        // Translation
        mat4 T = translate(I, position);

        // Final TRS matrix
        matrix =  T * R * S;
        inverseMatrix = inverse(matrix);
    }
};

#endif //TRANSFORMATION_H
