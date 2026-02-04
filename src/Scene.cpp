// Scene.cpp
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>
#include "Scene.h"
#include <iostream>
#include <fstream>
#include <GLFW/glfw3.h>
#include <chrono>
#include <imgui.h>

#include "Model.h"

using Clock = std::chrono::high_resolution_clock;

auto start = Clock::now();

void setBasisVectors(const glm::vec3& forward, glm::vec3& up, glm::vec3& right) {
    constexpr glm::vec3 world_up(0, 1, 0);
    right = glm::normalize(glm::cross(forward, world_up));
    up = glm::normalize(glm::cross(right, forward));
}

glm::mat4 composeTransform(const glm::vec3& position,
                           const glm::vec3& rotation, // Euler angles in radians
                           const glm::vec3& scale) {
    glm::mat4 I(1.0f);

    // Scale
    glm::mat4 S = glm::scale(I, scale);

    // Rotation (order: Z * Y * X)
    glm::mat4 Rx = glm::rotate(I, rotation.x, glm::vec3(1, 0, 0));
    glm::mat4 Ry = glm::rotate(I, rotation.y, glm::vec3(0, 1, 0));
    glm::mat4 Rz = glm::rotate(I, rotation.z, glm::vec3(0, 0, 1));
    glm::mat4 R = Rz * Ry * Rx;

    // Translation
    glm::mat4 T = glm::translate(I, position);

    // Final TRS matrix
    return T * R * S;
}
inline bool DragFloat3(const char* label, glm::vec3& v,
                       float speed = 0.01f, float min = 0.0f, float max = 0.0f) {
    return ImGui::DragFloat3(label, glm::value_ptr(v), speed, min, max);
}
inline bool ColorEdit3(const char* label, glm::vec3& v) {
    return ImGui::ColorEdit3(label, glm::value_ptr(v));
}
inline bool ColorEdit3(const char* label, glm::vec4& v) {
    auto v3 = glm::vec3(v.x, v.y, v.z);
    const bool out = ImGui::ColorEdit3(label, glm::value_ptr(v3));
    v.x = v3.x;
    v.y = v3.y;
    v.z = v3.z;
    return out;
}

Scene::Scene() {
    samples = 1;
    aa = 1;
    bounceLim = 8;

    width = 2560;
    height = 1440;

    frameCount = 0;

    lock = false;
}

Scene::Scene(const int width, const int height, const int samples, const int aa, const int bounceLim)
    : samples(samples), aa(aa), bounceLim(bounceLim), frameCount(0), width(width), height(height){
    camForward = glm::vec3(0, 0, -1);
    setBasisVectors(camForward, camUp, camRight);

    cameraPos = glm::vec3(0, 0, 0);

    lock = false;
}

void Scene::addModel(const std::string& filename, const glm::vec3 position, const glm::vec3 scale, const glm::vec3 color, const float smoothness, const glm::vec3 specularColor, const float specularProb, const float transparency, const float ior, const float emission) {
    Model model(filename);

    addModel(model, position, scale, color, smoothness, specularColor, specularProb, transparency, ior, emission);
}

void Scene::addModel(
    Model& model,
    glm::vec3 position,
    glm::vec3 scale,
    glm::vec3 color,
    float smoothness,
    glm::vec3 specularColor,
    float specularProb,
    const float transparency,
    const float ior,
    float emission) {
    if (specularColor == glm::vec3(-1)) specularProb = -1;
    int Voffset = int(vertices.size());
    int Toffset = int(triangles.size());
    int BBoffset = int(boundingBoxMin.size());
    int Coffset = int(colors.size());
    int Noffset = int(normalsList.size());

    models.emplace_back(BBoffset);

    for (glm::vec3 vertex : model.vertices) {
        vertices.emplace_back(vertex, 0);
    }
    for (glm::ivec4 triangle : model.triangles) {
        triangle += glm::vec4(Voffset, Voffset, Voffset, Coffset);
        triangles.emplace_back(triangle);
    }
    for (int i = 0; i < model.boundingBoxMin.size(); i++) {
        glm::vec3 bboxMin = model.boundingBoxMin[i];
        glm::vec3 bboxMax = model.boundingBoxMax[i];
        int childA = model.childA[i];
        int childB = model.childB[i];

        int offsetA = childA <= 0 ? -Toffset : BBoffset;
        int offsetB = childA <= 0 ? 0 : BBoffset;
        boundingBoxMin.emplace_back(bboxMin, 0);
        boundingBoxMax.emplace_back(bboxMax, 0);
        this->childA.push_back(childA+offsetA);
        this->childB.push_back(childB+offsetB);
    }
    for (auto & i : model.normalsList) {
        normalsList.emplace_back(i, 0);
    }
    for (glm::ivec3 normal : model.normals) {
        normal += glm::ivec3(Noffset, Noffset, Noffset);
        normals.emplace_back(normal, 0);
    }

    if (model.colors.empty()) {
        colors.emplace_back(color, smoothness);
        specularColors.emplace_back(specularColor, specularProb);
        glassLightSettings.emplace_back(transparency, ior, emission, 0);
    } else {
        for (glm::vec4 tempColor : model.colors) {
            colors.emplace_back(tempColor);
        }
        for (glm::vec4 tempSpecularColor : model.specularColors) {
            specularColors.emplace_back(tempSpecularColor);
        }
        for (glm::vec4 tempGlassLightSetting : model.glassLightSettings) {
            glassLightSettings.emplace_back(tempGlassLightSetting);
        }
    }

    modelTransforms.emplace_back(composeTransform(
        position,
        glm::vec3(0, 0, 0),
        scale
        ));
    modelInvTransforms.emplace_back(glm::inverse(modelTransforms.back()));

    modelPos.emplace_back(position);
    modelRot.emplace_back(0.0f, 0.0f, 0.0f);
    modelScale.emplace_back(scale);

    // a friendly label (filename stem or anything you want)
    size_t slash = model.filename.find_last_of("/\\");
    std::string stem = (slash == std::string::npos) ? model.filename : model.filename.substr(slash + 1);
    modelLabels.emplace_back(stem);

    int Ccount = int(colors.size()) - Coffset;      // how many entries were added for this model
    modelsColors.emplace_back(Coffset, Coffset + Ccount);

}

void Scene::set_ssbo() {
    GLuint ssboVertices;
    glGenBuffers(1, &ssboVertices);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboVertices);
    glBufferData(GL_SHADER_STORAGE_BUFFER, int(vertices.size() * sizeof(glm::vec4)), vertices.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboVertices);

    GLuint ssboTriangles;
    glGenBuffers(1, &ssboTriangles);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboTriangles);
    glBufferData(GL_SHADER_STORAGE_BUFFER, int(triangles.size() * sizeof(glm::ivec4)), triangles.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboTriangles);

    glGenBuffers(1, &ssboColors);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboColors);
    glBufferData(GL_SHADER_STORAGE_BUFFER, int(colors.size() * sizeof(glm::vec4)), colors.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssboColors);

    glGenBuffers(1, &ssboSpecularColors);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboSpecularColors);
    glBufferData(GL_SHADER_STORAGE_BUFFER, int(specularColors.size() * sizeof(glm::vec4)), specularColors.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssboSpecularColors);

    glGenBuffers(1, &ssboGlassLightSettings);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboGlassLightSettings);
    glBufferData(GL_SHADER_STORAGE_BUFFER, int(glassLightSettings.size() * sizeof(glm::vec4)), glassLightSettings.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, ssboGlassLightSettings);

    GLuint ssboBoundingBoxMin;
    glGenBuffers(1, &ssboBoundingBoxMin);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboBoundingBoxMin);
    glBufferData(GL_SHADER_STORAGE_BUFFER, int(boundingBoxMin.size() * sizeof(glm::vec4)), boundingBoxMin.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, ssboBoundingBoxMin);

    GLuint ssboBoundingBoxMax;
    glGenBuffers(1, &ssboBoundingBoxMax);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboBoundingBoxMax);
    glBufferData(GL_SHADER_STORAGE_BUFFER, int(boundingBoxMax.size() * sizeof(glm::vec4)), boundingBoxMax.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, ssboBoundingBoxMax);

    GLuint ssboChildA;
    glGenBuffers(1, &ssboChildA);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboChildA);
    glBufferData(GL_SHADER_STORAGE_BUFFER, int(childA.size() * sizeof(int)), childA.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, ssboChildA);

    GLuint ssboChildB;
    glGenBuffers(1, &ssboChildB);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboChildB);
    glBufferData(GL_SHADER_STORAGE_BUFFER, int(childB.size() * sizeof(int)), childB.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, ssboChildB);

    GLuint ssboModels;
    glGenBuffers(1, &ssboModels);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboModels);
    glBufferData(GL_SHADER_STORAGE_BUFFER, int(models.size() * sizeof(int)), models.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, ssboModels);

    GLuint ssboNormalsList;
    glGenBuffers(1, &ssboNormalsList);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboNormalsList);
    glBufferData(GL_SHADER_STORAGE_BUFFER, int(normalsList.size() * sizeof(glm::vec4)), normalsList.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, ssboNormalsList);

    GLuint ssboNormals;
    glGenBuffers(1, &ssboNormals);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboNormals);
    glBufferData(GL_SHADER_STORAGE_BUFFER, int(normals.size() * sizeof(glm::ivec4)), normals.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, ssboNormals);

    glGenBuffers(1, &ssboModelTransformations);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboModelTransformations);
    glBufferData(GL_SHADER_STORAGE_BUFFER, int(modelTransforms.size() * sizeof(glm::mat4)),modelTransforms.data(),GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 12, ssboModelTransformations);

    glGenBuffers(1, &ssboModelInvTransformations);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboModelInvTransformations);
    glBufferData(GL_SHADER_STORAGE_BUFFER, int(modelInvTransforms.size() * sizeof(glm::mat4)),modelInvTransforms.data(),GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 13, ssboModelInvTransformations);
}

int Scene::getNumBVHNodes() const {
    return int(boundingBoxMin.size());
}

int Scene::getNumTris() const {
    return int(triangles.size());
}

void Scene::setUniforms(const GLuint shaderProgram) const {
    const auto end = Clock::now();
    const glm::uint duration = static_cast<glm::uint>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()) % (width*height);

    glUniform1i(glGetUniformLocation(shaderProgram, "numModels"), int(models.size()));
    glUniform3f(glGetUniformLocation(shaderProgram, "cameraPos"), cameraPos.x, cameraPos.y, cameraPos.z);
    glUniform3f(glGetUniformLocation(shaderProgram, "camForward"), camForward.x, camForward.y, camForward.z);
    glUniform3f(glGetUniformLocation(shaderProgram, "camUp"), camUp.x, camUp.y, camUp.z);
    glUniform3f(glGetUniformLocation(shaderProgram, "camRight"), camRight.x, camRight.y, camRight.z);
    glUniform2ui(glGetUniformLocation(shaderProgram, "resolution"), width, height);
    glUniform1i(glGetUniformLocation(shaderProgram, "frameCount"), frameCount);
    glUniform1i(glGetUniformLocation(shaderProgram, "numNodes"), getNumBVHNodes());
    glUniform1i(glGetUniformLocation(shaderProgram, "samples"), samples);
    glUniform1i(glGetUniformLocation(shaderProgram, "aa"), aa);
    glUniform1i(glGetUniformLocation(shaderProgram, "bounceLim"), bounceLim);
    glUniform1ui(glGetUniformLocation(shaderProgram, "time"), duration);
    glUniform3f(glGetUniformLocation(shaderProgram, "skyColor"), skyColor.x, skyColor.y, skyColor.z);
    glUniform3f(glGetUniformLocation(shaderProgram, "sunDir"), sunDir.x, sunDir.y, sunDir.z);
    glUniform3f(glGetUniformLocation(shaderProgram, "sunColor"), sunStrength*sunColor.x, sunStrength*sunColor.y, sunStrength*sunColor.z);
    glUniform1i(glGetUniformLocation(shaderProgram, "debugView"), debugView ? 1 : 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "triTh"), triTh);
    glUniform1i(glGetUniformLocation(shaderProgram, "aabbTh"), aabbTh);
}

bool Scene::updateCamera(GLFWwindow* window, float speed, float sensitivity, float dt) {
    double xpos, ypos;
    bool moved = false;
    glfwGetCursorPos(window, &xpos, &ypos);
    glm::vec2 center = glm::vec2(float(width)/2, float(height)/2);
    glm::vec2 delta = glm::vec2(xpos - center.x, -(ypos - center.y));
    if (delta.x*delta.x + delta.y*delta.y > 0 and !lock) {
        delta *= 2.0f/float(height) * sensitivity;
        camForward += delta.x * camRight + delta.y * camUp;
        camForward = glm::normalize(camForward);
        moved = true;
        setBasisVectors(camForward, camUp, camRight);
        glfwSetCursorPos(window, center.x, center.y);
    }

    glm::vec3 change = glm::vec3(0, 0, 0);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        change += camForward;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        change -= camForward;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        change -= camRight;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        change += camRight;
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        change += camUp;
    }
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        change -= camUp;
    }
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
        lock = true;
    }
    if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS) {
        lock = false;
        glfwSetCursorPos(window, center.x, center.y);
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
       glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
        speed *= 2;
       }
    if (pow(change.x, 2) + pow(change.y, 2) + pow(change.z, 2) > 0 and !lock) {
        change = glm::normalize(change);
        cameraPos += change*speed*dt;
        moved = true;
    }
    return moved;
}

void Scene::updateFrame(const GLuint shaderProgram, GLFWwindow* window, float dt) {
    const bool moved = updateCamera(window, 500, 2, dt);

    setUniforms(shaderProgram);

    frameCount++;

    bool ui_resetAccum = false;

    if (moved) frameCount = 0;

    // --- Controls window ---
    {
        bool changed = false;

        ImGui::Begin("Controls");
        ImGui::Text("Renderer");
        ImGui::Separator();

        bool check = lock;
        ImGui::Checkbox("Lock", &lock);
        if (check && !lock) {
            glm::vec2 center = glm::vec2(float(width)/2, float(height)/2);
            glfwSetCursorPos(window, center.x, center.y);
        }

        changed |= ColorEdit3("Sun Color", sunColor);
        changed |= DragFloat3("Sun Direction", sunDir);
        sunDir = glm::normalize(sunDir);

        changed |= ImGui::SliderFloat("Sun Strength", &sunStrength, 0, 300);

        changed |= ImGui::SliderInt("Samples", &samples, 1, 25);
        changed |= ImGui::SliderInt("Antialiasing", &aa, 1, 5);
        changed |= ImGui::SliderInt("Bounces", &bounceLim, 1, 16);

        changed |= ImGui::Checkbox("Debug View" , &debugView);
        if (debugView) {
            changed |= ImGui::SliderInt("Triangle Threshhold", &triTh, 0, 1000);
            changed |= ImGui::SliderInt("AABB Threshhold", &aabbTh, 0, 1000);
        }

        if (ImGui::Button("Reset accumulation") || changed) {
            ui_resetAccum = true;
        }

        ImGui::Separator();
        ImGui::Text("Models");

        const bool hasModels = !modelLabels.empty();
        if (!hasModels) {
            ImGui::TextDisabled("(no models)");
        } else {
            // Current label
            const char* preview = (selectedModel >= 0) ? modelLabels[selectedModel].c_str() : "(select)";
            if (ImGui::BeginCombo("Model", preview)) {
                for (int i = 0; i < (int)modelLabels.size(); ++i) {
                    bool sel = (selectedModel == i);
                    if (ImGui::Selectable(modelLabels[i].c_str(), sel)) {
                        selectedModel = i;
                        frameCount = 0; // reset accumulation on selection change
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (selectedModel >= 0) {
                bool changedPRS = false;
                bool changedC = false;
                bool changedSC = false;
                bool changedGLS = false;

                // Local aliases
                glm::vec3& P = modelPos[selectedModel];
                glm::vec3& R = modelRot[selectedModel];   // radians
                glm::vec3& S = modelScale[selectedModel];

                // Rotation UI in degrees (convert to/from radians for nicer UX)
                glm::vec3 rotDeg = glm::degrees(R);
                changedPRS |= DragFloat3("Position", P, 3.0f);                     // world units
                changedPRS |= DragFloat3("Scale",    S, 1.0f, 0.0f, 1e36);
                changedPRS |= DragFloat3("Rotation (deg)", rotDeg, 0.2f);

                if (ImGui::BeginCombo("Color", std::to_string(selectedColor).c_str())) {
                    for (int i = modelsColors[selectedModel].x; i < modelsColors[selectedModel].y; ++i) {
                        bool sel = (selectedColor == i); 
                        if (ImGui::Selectable(std::to_string(i).c_str(), sel)) {
                            selectedColor = i;
                            frameCount = 0; // reset accumulation on selection change
                        }
                        if (sel) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if (selectedColor != -1){
                    glm::vec4& C = colors[selectedColor];
                    glm::vec4& SC = specularColors[selectedColor];
                    glm::vec4& GLS = glassLightSettings[selectedColor];
                    ImGui::Text(std::to_string(selectedColor).c_str());
                    changedC |= ColorEdit3("Color", C);
                    changedC |= ImGui::SliderFloat("Smoothness", &C.w, 0.0f, 1.0f);
                    changedSC |= ColorEdit3("Specular Color", SC);
                    changedSC |= ImGui::SliderFloat("Specualar Probability", &SC.w, 0.0f, 1.0f);
                    changedGLS |= ImGui::SliderFloat("Transparency", &GLS.x, 0.0f, 1.0f);
                    changedGLS |= ImGui::SliderFloat("Index of Refraction", &GLS.y, 0.0f, 3.0f);
                    changedGLS |= ImGui::SliderFloat("Emission Strength", &GLS.z, 0.0f, 10.0f);
                    colors[selectedColor] = C;
                    specularColors[selectedColor] = SC;
                    glassLightSettings[selectedColor] = GLS;
                }

                if (changedPRS) {
                    R = glm::radians(rotDeg);

                    // Rebuild the matrix and overwrite just this model's slice in the SSBO
                    modelTransforms[selectedModel] = composeTransform(P, R, S);

                    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboModelTransformations);
                    const GLsizeiptr offset = (GLsizeiptr)selectedModel * sizeof(glm::mat4);
                    glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset,
                                    sizeof(glm::mat4),
                                    &modelTransforms[selectedModel]);

                    modelInvTransforms[selectedModel] = glm::inverse(modelTransforms[selectedModel]);

                    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboModelInvTransformations);
                    glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset,
                                    sizeof(glm::mat4),
                                    &modelInvTransforms[selectedModel]);

                    frameCount = 0; // nuke accumulation so the new transform converges cleanly
                }
                // All materials for this model share the same contiguous range:
                const int start = modelsColors[selectedModel][0];   // inclusive
                const int end   = modelsColors[selectedModel][1];   // exclusive
                const GLsizeiptr count = (GLsizeiptr)(end - start);
                const GLsizeiptr byteOff = (GLsizeiptr)start * sizeof(glm::vec4);
                const GLsizeiptr byteSize = count * sizeof(glm::vec4);

                if (changedC) {
                    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboColors);
                    glBufferSubData(GL_SHADER_STORAGE_BUFFER, byteOff, byteSize, colors.data() + start);
                    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssboColors);
                    frameCount = 0;
                }

                if (changedSC) {
                    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboSpecularColors);
                    glBufferSubData(GL_SHADER_STORAGE_BUFFER, byteOff, byteSize, specularColors.data() + start);
                    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssboSpecularColors);
                    frameCount = 0;
                }

                if (changedGLS) {
                    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboGlassLightSettings);
                    glBufferSubData(GL_SHADER_STORAGE_BUFFER, byteOff, byteSize, glassLightSettings.data() + start);
                    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, ssboGlassLightSettings);
                    frameCount = 0;
                }
            }
        }

        ImGui::Text("Frame: %d", frameCount * samples);
        ImGui::End();
    }

    if (ui_resetAccum) {
        frameCount = 0;
    }
}

int Scene::numTriBelow(int index) {
    int childA = this->childA[index];
    int childB = this->childB[index];

    if (childB > index and childA > index) {
        return numTriBelow(childA) + numTriBelow(childB);
    }
    return -childB;
}

void Scene::get_BVH_stats(int index, int& leafNodes, int& depth, int& minDepth, int& maxDepth, int& triPerLeaf, int& minTriPerLeaf, int& maxTriPerLeaf, int current_depth) {
    if (models.empty()) return;
    int childA = this->childA[index];
    int childB = this->childB[index];
    if (childA > 0) {
        get_BVH_stats(childA, leafNodes, depth, minDepth, maxDepth, triPerLeaf, minTriPerLeaf, maxTriPerLeaf, current_depth+1);
        get_BVH_stats(childB, leafNodes, depth, minDepth, maxDepth, triPerLeaf, minTriPerLeaf, maxTriPerLeaf, current_depth+1);
        return;
    }
    int triStart = -childA;
    int numTris = -childB;
    leafNodes++;
    depth += current_depth;
    triPerLeaf += numTris;
    if (current_depth > maxDepth) maxDepth = current_depth;
    if (current_depth < minDepth) minDepth = current_depth;
    if (numTris > maxTriPerLeaf) maxTriPerLeaf = numTris;
    if (numTris < minTriPerLeaf) minTriPerLeaf = numTris;
}

void Scene::displayBVH() {
    for (const int model : models) {
        std::cout << model << " ";
    }
    std::cout << std::endl;
    for (const int model : models) {
        const std::string prefix;
        displayBVH(model, prefix);
    }
}

void Scene::displayBVH(int index, std::string prefix) {
    glm::vec3 bboxMin = boundingBoxMin[index];
    glm::vec3 bboxMax = boundingBoxMax[index];
    int childA = this->childA[index];
    int childB = this->childB[index];
    int numTris = numTriBelow(index);
    //if (numTris < 100000) return;
    std::cout << prefix << "Index: " << index << "  -  Tris: " << numTris << std::endl;
    std::cout << prefix << "Bounding Box Min: " << bboxMin.x << ", " << bboxMin.y << ", " << bboxMin.z << ", " << childA << std::endl;
    std::cout << prefix << "Bounding Box Max: " << bboxMax.x << ", " << bboxMax.y << ", " << bboxMax.z << ", " << childB << std::endl;
    if (childB > index and childA > index) {
        prefix += "  ";
        displayBVH(childA, prefix);
        std::cout << std::endl;
        displayBVH(childB, prefix);
        return;
    }
    int triStart = -childA;
    std::cout << prefix << "Triangles: " << numTris << std::endl;
    return;
    prefix += "   ";
    for (int i = triStart; i < numTris+triStart; i++) {
        glm::vec3 v1 = vertices[triangles[i*3+0].x];
        glm::vec3 v2 = vertices[triangles[i*3+1].x];
        glm::vec3 v3 = vertices[triangles[i*3+2].x];
        bool check =
            v1.x >= bboxMin.x && v1.x <= bboxMax.x &&
            v2.x >= bboxMin.x && v2.x <= bboxMax.x &&
            v3.x >= bboxMin.x && v3.x <= bboxMax.x &&
            v1.y >= bboxMin.y && v1.y <= bboxMax.y &&
            v2.y >= bboxMin.y && v2.y <= bboxMax.y &&
            v3.y >= bboxMin.y && v3.y <= bboxMax.y &&
            v1.z >= bboxMin.z && v1.z <= bboxMax.z &&
            v2.z >= bboxMin.z && v2.z <= bboxMax.z &&
            v3.z >= bboxMin.z && v3.z <= bboxMax.z;
        std::cout << prefix << (check ? "In" : "--Out--") << " ";
        std::cout << v1.x << " " << v1.y << " " << v1.z << "  -  ";
        std::cout << v2.x << " " << v2.y << " " << v2.z << "  -  ";
        std::cout << v3.x << " " << v3.y << " " << v3.z << std::endl;
    }
}