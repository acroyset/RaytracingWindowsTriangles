// Scene.cpp
#include "Scene.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"

using Clock = std::chrono::high_resolution_clock;

auto start = Clock::now();

void setBasisVectors(const vec3& forward, vec3& up, vec3& right) {
    constexpr vec3 world_up(0, 1, 0);
    right = normalize(cross(forward, world_up));
    up = normalize(cross(right, forward));
}

mat4 composeTransform(const vec3& position,
                           const vec3& rotation, // Euler angles in radians
                           const vec3& scale) {
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
    return T * R * S;
}

inline bool DragFloat3(const char* label, vec3& v,
                       float speed = 0.01f, float min = 0.0f, float max = 0.0f) {
    return ImGui::DragFloat3(label, value_ptr(v), speed, min, max);
}
inline bool ColorEdit3(const char* label, vec3& v) {
    return ImGui::ColorEdit3(label, value_ptr(v));
}
inline bool ColorEdit3(const char* label, vec4& v) {
    auto v3 = vec3(v.x, v.y, v.z);
    const bool out = ImGui::ColorEdit3(label, value_ptr(v3));
    v.x = v3.x;
    v.y = v3.y;
    v.z = v3.z;
    return out;
}

static void setDefault2DParams() {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);        // horiz repeat
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // clamp vertically
}
GLuint LoadEnvLatLongTextureAuto(const char* path) {
    stbi_set_flip_vertically_on_load(false); // equirect usually not flipped

    int w=0, h=0, n=0;  // n = channels in file
    GLuint tex=0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    // Alignment fix (prevents rainbow banding on RGB 3-byte rows)
    GLint prevAlign = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevAlign);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    if (stbi_is_hdr(path)) {
        float* data = stbi_loadf(path, &w, &h, &n, 0); // keep original n (3 or 4)
        if (!data) {
            fprintf(stderr, "HDR load failed: %s (%s)\n", path, stbi_failure_reason());
            glBindTexture(GL_TEXTURE_2D, 0);
            glDeleteTextures(1, &tex);
            glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlign);
            return 0;
        }
        GLenum srcFmt = (n == 4) ? GL_RGBA : GL_RGB;
        GLint  dstFmt = (n == 4) ? GL_RGBA16F : GL_RGB16F; // linear HDR
        glTexImage2D(GL_TEXTURE_2D, 0, dstFmt, w, h, 0, srcFmt, GL_FLOAT, data);
        stbi_image_free(data);
    } else {
        unsigned char* data = stbi_load(path, &w, &h, &n, 0); // keep original n (3 or 4)
        if (!data) {
            fprintf(stderr, "LDR load failed: %s (%s)\n", path, stbi_failure_reason());
            glBindTexture(GL_TEXTURE_2D, 0);
            glDeleteTextures(1, &tex);
            glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlign);
            return 0;
        }
        GLenum srcFmt = (n == 4) ? GL_RGBA : GL_RGB;
        // sRGB internal formats → sampling returns LINEAR color automatically
        GLint  dstFmt = (n == 4) ? GL_SRGB8_ALPHA8 : GL_SRGB8;
        glTexImage2D(GL_TEXTURE_2D, 0, dstFmt, w, h, 0, srcFmt, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    }



    setDefault2DParams();
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
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

Scene::Scene(const int samples, const int aa, const int bounceLim)
    : samples(samples), aa(aa), bounceLim(bounceLim), frameCount(0){

    window.setFeedbackMode(true);
    unsigned int width = window.size().x;
    unsigned int height = window.size().y;
    this->width = width;
    this->height = height;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    const char* glsl_version = "#version 430";
    ImGui_ImplGlfw_InitForOpenGL(window.getWindow(), true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    camForward = vec3(0, 0, -1);
    setBasisVectors(camForward, camUp, camRight);

    cameraPos = vec3(0, 0, 0);

    lock = false;

    createUniforms();

    skyTex = LoadEnvLatLongTextureAuto("sky.png");
}

void Scene::createUniforms() {
    uNumModels = window.createUniform<int>("numModels");
    uCameraPos = window.createUniform<vec3>("cameraPos");
    uCameraForward = window.createUniform<vec3>("camForward");
    uCameraUp = window.createUniform<vec3>("camUp");
    uCameraRight = window.createUniform<vec3>("camRight");
    uResolution = window.createUniform<uvec2>("resolution");
    uFrameCount = window.createUniform<int>("frameCount");
    uNumNodes = window.createUniform<int>("numNodes");
    uSamples = window.createUniform<int>("samples");
    uAA = window.createUniform<int>("aa");
    uBounceLim = window.createUniform<int>("bounceLim");
    uSkyColor = window.createUniform<vec3>("skyColor");
    uSunDir = window.createUniform<vec3>("sunDir");
    uSunColor = window.createUniform<vec3>("sunColor");
    uDebugView = window.createUniform<int>("debugView");
    uTriThreshold = window.createUniform<int>("triTh");
    uAABBThreshold =  window.createUniform<int>("aabbTh");
    uEnvLatLong = window.createUniform<int>("uEnvLatLong");
    uEnvYaw = window.createUniform<float>("uEnvYaw");
}

void Scene::addModel(const std::string& filename, const vec3 position, const vec3 scale, const vec3 color, const float smoothness, const vec3 specularColor, const float specularProb, const float transparency, const float ior, const float emission) {
    Model model(filename);

    addModel(model, position, scale, color, smoothness, specularColor, specularProb, transparency, ior, emission);
}

void Scene::addModel(
    Model& model,
    vec3 position,
    vec3 scale,
    vec3 color,
    float smoothness,
    vec3 specularColor,
    float specularProb,
    const float transparency,
    const float ior,
    float emission) {

    if (specularColor == vec3(-1)) specularProb = -1;
    int Voffset = int(vertices.size());
    int Toffset = int(triangles.size());
    int BBoffset = int(boundingBoxMin.size());
    int Coffset = int(colors.size());
    int Noffset = int(normalsList.size());

    models.emplace_back(BBoffset);

    for (vec3 vertex : model.vertices) {
        vertices.emplace_back(vertex, 0);
    }
    for (ivec4 triangle : model.triangles) {
        triangle += vec4(Voffset, Voffset, Voffset, Coffset);
        triangles.emplace_back(triangle);
    }
    for (int i = 0; i < model.boundingBoxMin.size(); i++) {
        vec3 bboxMin = model.boundingBoxMin[i];
        vec3 bboxMax = model.boundingBoxMax[i];
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
    for (ivec3 normal : model.normals) {
        normal += ivec3(Noffset, Noffset, Noffset);
        normals.emplace_back(normal, 0);
    }

    if (model.colors.empty()) {
        colors.emplace_back(color, smoothness);
        specularColors.emplace_back(specularColor, specularProb);
        glassLightSettings.emplace_back(transparency, ior, emission, 0);
    } else {
        for (vec4 tempColor : model.colors) {
            colors.emplace_back(tempColor);
        }
        for (vec4 tempSpecularColor : model.specularColors) {
            specularColors.emplace_back(tempSpecularColor);
        }
        for (vec4 tempGlassLightSetting : model.glassLightSettings) {
            glassLightSettings.emplace_back(tempGlassLightSetting);
        }
    }

    modelTransforms.emplace_back(composeTransform(
        position,
        vec3(0, 0, 0),
        scale
        ));
    modelInvTransforms.emplace_back(inverse(modelTransforms.back()));

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

    ssboVertices.set(vertices, 0);
    ssboTriangles.set(triangles, 1);
    ssboColors.set(colors, 2, true);
    ssboSpecularColors.set(specularColors, 3, true);
    ssboGlassLightSettings.set(glassLightSettings, 4, true);
    ssboBoundingBoxMin.set(boundingBoxMin, 5);
    ssboBoundingBoxMax.set(boundingBoxMax, 6);
    ssboChildA.set(childA, 7);
    ssboChildB.set(childB, 8);
    ssboModels.set(models, 9);
    ssboNormalsList.set(normalsList, 10);
    ssboNormals.set(normals, 11);
    ssboModelTransformations.set(modelTransforms, 12, true);
    ssboModelInvTransformations.set(modelInvTransforms, 13, true);

}

int Scene::getNumBVHNodes() const {
    return int(boundingBoxMin.size());
}

int Scene::getNumTris() const {
    return int(triangles.size());
}

void Scene::setUniforms() const {
    const auto end = Clock::now();
    const uint duration = static_cast<uint>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()) % (width*height);

    uNumModels.set(int(models.size()));
    uCameraPos.set(cameraPos);
    uCameraForward.set(camForward);
    uCameraUp.set(camUp);
    uCameraRight.set(camRight);
    uResolution.set({width, height});
    uFrameCount.set(frameCount);
    uNumNodes.set(getNumBVHNodes());
    uSamples.set(samples);
    uAA.set(aa);
    uBounceLim.set(bounceLim);
    uSkyColor.set(skyColor);
    uSunDir.set(sunDir);
    uSunColor.set(sunColor*sunStrength);
    uDebugView.set(debugView);
    uTriThreshold.set(triTh);
    uAABBThreshold.set(aabbTh);
}

bool Scene::updateCamera(GLFWwindow* window, float speed, float sensitivity, float dt) {
    double xpos, ypos;
    bool moved = false;
    glfwGetCursorPos(window, &xpos, &ypos);
    vec2 center = vec2(float(width)/2, float(height)/2);
    vec2 delta = vec2(xpos - center.x, -(ypos - center.y));
    if (delta.x*delta.x + delta.y*delta.y > 0 and !lock) {
        delta *= 2.0f/float(height) * sensitivity;
        camForward += delta.x * camRight + delta.y * camUp;
        camForward = normalize(camForward);
        moved = true;
        setBasisVectors(camForward, camUp, camRight);
        glfwSetCursorPos(window, center.x, center.y);
    }

    vec3 change = vec3(0, 0, 0);
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
        change = normalize(change);
        cameraPos += change*speed*dt;
        moved = true;
    }
    return moved;
}

void Scene::updateFrame() {
    window.start();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    float dt = window.getDeltaTime();

    const bool moved = updateCamera(window.getWindow(), 500, 2, dt);

    setUniforms();

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
            vec2 center = vec2(float(width)/2, float(height)/2);
            glfwSetCursorPos(window.getWindow(), center.x, center.y);
        }

        changed |= ColorEdit3("Sun Color", sunColor);
        changed |= DragFloat3("Sun Direction", sunDir);
        sunDir = normalize(sunDir);

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
                vec3& P = modelPos[selectedModel];
                vec3& R = modelRot[selectedModel];   // radians
                vec3& S = modelScale[selectedModel];

                // Rotation UI in degrees (convert to/from radians for nicer UX)
                vec3 rotDeg = degrees(R);
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
                    vec4& C = colors[selectedColor];
                    vec4& SC = specularColors[selectedColor];
                    vec4& GLS = glassLightSettings[selectedColor];
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
                    R = radians(rotDeg);

                    modelTransforms[selectedModel] = composeTransform(P, R, S);
                    modelInvTransforms[selectedModel] = inverse(modelTransforms[selectedModel]);

                    ssboModelTransformations.update(selectedModel, modelTransforms[selectedModel]);
                    ssboModelInvTransformations.update(selectedModel, modelInvTransforms[selectedModel]);

                    frameCount = 0; // nuke accumulation so the new transform converges cleanly
                }
                // All materials for this model share the same contiguous range:
                const int start = modelsColors[selectedModel][0];   // inclusive
                const int end   = modelsColors[selectedModel][1];   // exclusive
                const GLsizeiptr count = end - start;
                const GLsizeiptr byteOff = (GLsizeiptr)start * sizeof(vec4);
                const GLsizeiptr byteSize = count * sizeof(vec4);

                if (changedC) {
                    ssboColors.update(start, end, colors.data() + start);
                    frameCount = 0;
                }

                if (changedSC) {
                    ssboSpecularColors.update(start, end, specularColors.data() + start);
                    frameCount = 0;
                }

                if (changedGLS) {
                    ssboGlassLightSettings.update(start, end, glassLightSettings.data() + start);
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

    glActiveTexture(GL_TEXTURE0 + 5); // choose a slot
    glBindTexture(GL_TEXTURE_2D, skyTex);
    uEnvLatLong.set(5);
    uEnvYaw.set(0.0f);

    window.render();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


    glfwSwapBuffers(window.getWindow());
    glfwPollEvents();
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
    vec3 bboxMin = boundingBoxMin[index];
    vec3 bboxMax = boundingBoxMax[index];
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
        vec3 v1 = vertices[triangles[i*3+0].x];
        vec3 v2 = vertices[triangles[i*3+1].x];
        vec3 v3 = vertices[triangles[i*3+2].x];
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