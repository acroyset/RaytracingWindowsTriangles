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

mat4 composeTransform(const vec3& position, const vec3& rotation, const vec3& scale) {
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

inline bool DragFloat3(const char* label, vec3& v, float speed = 0.01f, float min = 0.0f, float max = 0.0f) {
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
        GLint  dstFmt = (n == 4) ? GL_RGBA16F : GL_RGB16F;
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
        GLint  dstFmt = (n == 4) ? GL_SRGB8_ALPHA8 : GL_SRGB8;
        glTexImage2D(GL_TEXTURE_2D, 0, dstFmt, w, h, 0, srcFmt, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    }

    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);        // horiz repeat
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // clamp vertically

    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}


Scene::Scene() {
    samples = 1;
    aa = 1;
    bounceLim = 8;

    frameCount = 0;
    sampleCount = 0;

    lock = false;

    textureScales.fill(0.0f);
}

Scene::Scene(const int samples, const int aa, const int bounceLim)
    : samples(samples), aa(aa), bounceLim(bounceLim), frameCount(0), sampleCount(0){

    window.setFeedbackMode(true);

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

    skyTexture = window.createTexture("skyTex", "assets/textures/sky.png");

    textureScales.fill(0.0f);
}

void Scene::addModel(const std::string& filename, const vec3 position, const vec3 scale, const vec3 color, const float smoothness, const vec3 specularColor, const float specularProb, const float transparency, const float ior, const float emission) {
    Model model(filename);

    addModel(model, position, scale, color, smoothness, specularColor, specularProb, transparency, ior, emission, "");
}
void Scene::addModel(const std::string& filename, const vec3 position, const vec3 scale, const std::string &textureFilename, const float smoothness, const vec3 specularColor, const float specularProb, const float transparency, const float ior, const float emission) {
    Model model(filename);

    addModel(model, position, scale, vec3(0), smoothness, specularColor, specularProb, transparency, ior, emission, textureFilename);
}

void Scene::addModel(Model& model, const vec3 position, const vec3 scale, const std::string &textureFilename, const float smoothness, const vec3 specularColor, const float specularProb, const float transparency, const float ior, const float emission) {
    addModel(model, position, scale, vec3(0), smoothness, specularColor, specularProb, transparency, ior, emission, textureFilename);
}
void Scene::addModel(Model& model, vec3 position, vec3 scale, vec3 color, float smoothness, vec3 specularColor, float specularProb, const float transparency, const float ior, float emission, const std::string& textureFilename) {

    bool useTexture = !model.texCoords.empty() && !textureFilename.empty();
    int textureID = int(textures.size());
    if (useTexture) {
        textures.emplace_back(window.createTexture("textures[" + std::to_string(textureID) + "]", textureFilename));
        textures.back().setWrap(TextureWrap::REPEAT, TextureWrap::REPEAT);
        const std::string& name = textureFilename;
        std::string label = name;
        int count = 0;
        for (const std::string& i : textureLabels) {
            if (i == label) {
                count++;
                label = name + (count != 0 ? " " + std::to_string(count) : "");
            }
        }

        textureLabels.emplace_back(label);
    }
    modelsTextureID.emplace_back(useTexture ? textureID : -1);

    if (specularColor == vec3(-1)) specularProb = -1.0f;

    int Toffset = int(triangles.size())/3;
    int Voffset = int(vertices.size());
    int TXoffset = int(texCoords.size());
    int Noffset = int(normals.size());
    int BBoffset = int(boundingBoxMin.size());
    int Coffset = int(colors.size());

    models.emplace_back(BBoffset);

    for (int i = 0; i < model.triangles.size()/3; i++) {
        ivec4 triangle1 = model.triangles[i*3+0];
        ivec4 triangle2 = model.triangles[i*3+1];
        ivec4 triangle3 = model.triangles[i*3+2];

        triangle1 += vec4(Voffset, TXoffset, Noffset, Coffset);
        triangle2 += vec4(Voffset, TXoffset, Noffset, useTexture);
        triangle3 += vec4(Voffset, TXoffset, Noffset, useTexture ? textureID : 0);

        triangles.emplace_back(triangle1);
        triangles.emplace_back(triangle2);
        triangles.emplace_back(triangle3);
    }

    for (vec3 vertex : model.vertices) {
        vertices.emplace_back(vertex, 0);
    }
    for (vec2 texCoord : model.texCoords) {
        texCoords.emplace_back(texCoord);
    }
    for (vec3 normal : model.normals) {
        normals.emplace_back(normal, 0);
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

    std::string name = model.filename;
    std::string label = name;
    int count = 0;
    for (const std::string& i : modelLabels) {
        if (i == label) {
            count++;
            label = name + (count != 0 ? " " + std::to_string(count) : "");
        }
    }

    modelLabels.emplace_back(label);

    int Ccount = int(colors.size()) - Coffset;
    modelsColors.emplace_back(Coffset, Coffset + Ccount);

}

int Scene::getNumBVHNodes() const {
    return int(boundingBoxMin.size());
}

int Scene::getNumTris() const {
    return int(triangles.size()/3);
}

void Scene::createUniforms() {
    uNumModels = window.createUniform<int>("numModels");

    uCameraPos =     window.createUniform<vec3>("cameraPos");
    uCameraForward = window.createUniform<vec3>("camForward");
    uCameraUp =      window.createUniform<vec3>("camUp");
    uCameraRight =   window.createUniform<vec3>("camRight");
    uFovDeg =        window.createUniform<float>("fovDeg");

    uResolution =     window.createUniform<uvec2>("resolution");
    uFrameCount =     window.createUniform<int>("frameCount");
    uTimeSinceStart = window.createUniform<float>("timeSinceStart");

    uNumNodes =  window.createUniform<int>("numNodes");
    uSamples =   window.createUniform<int>("samples");
    uAA =        window.createUniform<int>("aa");
    uBounceLim = window.createUniform<int>("bounceLim");

    uSkyColor = window.createUniform<vec3>("skyColor");
    uSunDir =   window.createUniform<vec3>("sunDir");
    uSunColor = window.createUniform<vec3>("sunColor");

    uDebugView =      window.createUniform<int>("debugView");
    uDebugMode =      window.createUniform<int>("debugMode");
    uTriThreshold =   window.createUniform<int>("triTh");
    uAABBThreshold =  window.createUniform<int>("aabbTh");
    uDepthScale =     window.createUniform<float>("depthScale");

    uTextureScales = window.createUniform<float>("textureScales");

    uEnvYaw = window.createUniform<float>("uEnvYaw");
}

void Scene::setUniforms() const {

    uNumModels.set(int(models.size()));

    uCameraPos.set(cameraPos);
    uCameraForward.set(camForward);
    uCameraUp.set(camUp);
    uCameraRight.set(camRight);
    uFovDeg.set(fovDeg);

    uResolution.set(window.size());
    uFrameCount.set(frameCount);
    uTimeSinceStart.set(window.getTimeSinceStart());

    uNumNodes.set(getNumBVHNodes());
    uSamples.set(samples);
    uAA.set(aa);
    uBounceLim.set(bounceLim);

    uSkyColor.set(skyColor);
    uSunDir.set(sunDir);
    uSunColor.set(sunColor*sunStrength);

    uDebugView.set(debugView);
    uDebugMode.set(debugMode);
    uTriThreshold.set(triTh);
    uAABBThreshold.set(aabbTh);
    uDepthScale.set(depthScale);

    uTextureScales.setArray(textureScales.data(), 64);

    skyTexture.bind();
    uEnvYaw.set(0.0f);

    for (const Texture &tex : textures) {
        tex.bind();
    }
}

void Scene::set_ssbo() {

    ssboTriangles.set(triangles, 0);
    ssboVertices.set(vertices, 1);
    ssboTexCoords.set(texCoords, 2);
    ssboNormals.set(normals, 3);
    ssboColors.set(colors, 4, true);
    ssboSpecularColors.set(specularColors, 5, true);
    ssboGlassLightSettings.set(glassLightSettings, 6, true);
    ssboBoundingBoxMin.set(boundingBoxMin, 7);
    ssboBoundingBoxMax.set(boundingBoxMax, 8);
    ssboChildA.set(childA, 9);
    ssboChildB.set(childB, 10);
    ssboModels.set(models, 11);
    ssboModelTransformations.set(modelTransforms, 12, true);
    ssboModelInvTransformations.set(modelInvTransforms, 13, true);

}

bool Scene::inputHandling(float speed, float sensitivity, float dt) {
    sensitivity *= fovDeg;

    bool moved = false;
    vec2 mousePos = window.getMousePos();
    vec2 center = vec2(window.size())/2.0f;
    vec2 delta = vec2(mousePos.x - center.x, -(mousePos.y - center.y));
    if (delta.x*delta.x + delta.y*delta.y > 0 and !lock) {
        delta *= 2.0f/float(window.size().y) * sensitivity;
        camForward += delta.x * camRight + delta.y * camUp;
        camForward = normalize(camForward);
        moved = true;
        setBasisVectors(camForward, camUp, camRight);
        window.setMousePos(center);
    }

    vec3 change = vec3(0, 0, 0);
    if (window.keyPressed(GLFW_KEY_W)) change += camForward;
    if (window.keyPressed(GLFW_KEY_S)) change -= camForward;
    if (window.keyPressed(GLFW_KEY_A)) change -= camRight;
    if (window.keyPressed(GLFW_KEY_D)) change += camRight;
    if (window.keyPressed(GLFW_KEY_E)) change += camUp;
    if (window.keyPressed(GLFW_KEY_Q)) change -= camUp;

    if (window.keyPressed(GLFW_KEY_L)) {
        if (!trackedKeysPressed[GLFW_KEY_L]) {
            lock = !lock;
            if (!lock) window.setMousePos(center);
        }

        trackedKeysPressed[GLFW_KEY_L] = true;
    } else trackedKeysPressed[GLFW_KEY_L] = false;
    if (window.keyPressed(GLFW_KEY_H)) {
        if (!trackedKeysPressed[GLFW_KEY_H]) hud = !hud;

        trackedKeysPressed[GLFW_KEY_H] = true;
    } else trackedKeysPressed[GLFW_KEY_H] = false;

    if (window.keyPressed(GLFW_KEY_LEFT_SHIFT) || window.keyPressed(GLFW_KEY_RIGHT_SHIFT)) speed *= 2;

    if (pow(change.x, 2) + pow(change.y, 2) + pow(change.z, 2) > 0 and !lock) {
        change = normalize(change);
        cameraPos += change*speed*dt;
        moved = true;
    }
    return moved;
}

void Scene::updateFrame() {
    window.start();

    if (hud) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    float dt = window.getDeltaTime();
    updateFPS(dt);

    const bool moved = inputHandling(speed, sensitivity, dt);
    if (moved) {
        frameCount = 0;
        sampleCount = 0;
    }

    setUniforms();

    frameCount++;
    sampleCount += samples;

    // --- Controls window ---
    if (hud) ImGuiRender();

    window.render();

    if (hud) {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }


    glfwSwapBuffers(window.getWindow());
    glfwPollEvents();
}

void Scene::ImGuiRender() {

    bool ui_resetAccum = false;

    // scene
    {
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(500, 550), ImGuiCond_Once);
        ImGui::Begin("Scene");

        ui_resetAccum |= ColorEdit3("Sun Color", sunColor);
        ui_resetAccum |= DragFloat3("Sun Direction", sunDir);
        sunDir = normalize(sunDir);

        ui_resetAccum |= ImGui::SliderFloat("Sun Strength", &sunStrength, 0, 300);

        ImGui::Separator();
        ImGui::Text("Models");

        const bool hasModels = !modelLabels.empty();
        if (!hasModels) {
            ImGui::TextDisabled("(no models)");
        }
        else {
            // Current label
            const char* preview = (selectedModel >= 0) ? modelLabels[selectedModel].c_str() : "(select)";
            if (ImGui::BeginCombo("Model", preview)) {
                for (int i = 0; i < (int)modelLabels.size(); ++i) {
                    bool sel = (selectedModel == i);
                    if (ImGui::Selectable(modelLabels[i].c_str(), sel)) {
                        selectedModel = i;
                        selectedColor = modelsColors[i].x;
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (selectedModel >= 0) {
                ImGui::Indent();
                bool changedPRS = false;
                bool changedC = false;
                bool changedSC = false;
                bool changedGLS = false;

                // Local aliases
                vec3& P = modelPos[selectedModel];
                vec3& R = modelRot[selectedModel];   // radians
                vec3& S = modelScale[selectedModel];

                ImGui::Text("Transformations");

                // Rotation UI in degrees (convert to/from radians for nicer UX)
                vec3 rotDeg = degrees(R);
                changedPRS |= DragFloat3("Position", P, 3.0f);                     // world units
                changedPRS |= DragFloat3("Scale",    S, 1.0f, 0.0f, 1e36);
                changedPRS |= DragFloat3("Rotation (deg)", rotDeg, 0.2f);

                ImGui::Separator();

                std::string previewC = "Material " + std::to_string(selectedColor);
                if (ImGui::BeginCombo("##Material", previewC.c_str())) {
                    for (int i = modelsColors[selectedModel].x; i < modelsColors[selectedModel].y; ++i) {
                        bool sel = (selectedColor == i);
                        previewC = "Material " + std::to_string(i);
                        if (ImGui::Selectable(previewC.c_str(), sel)) {
                            selectedColor = i;
                        }
                        if (sel) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if (selectedColor != -1){
                    ImGui::Indent();
                    vec4& C = colors[selectedColor];
                    vec4& SC = specularColors[selectedColor];
                    vec4& GLS = glassLightSettings[selectedColor];

                    ImGui::Text("Colors");

                    if (modelsTextureID[selectedModel] == -1) changedC |= ColorEdit3("Diffuse Color", C);
                    changedSC |= ColorEdit3("Specular Color", SC);

                    ImGui::Text("Material Properties");

                    changedC |= ImGui::SliderFloat("Smoothness", &C.w, 0.0f, 1.0f);
                    changedSC |= ImGui::SliderFloat("Specular Probability", &SC.w, 0.0f, 1.0f);
                    changedGLS |= ImGui::SliderFloat("Transparency", &GLS.x, 0.0f, 1.0f);
                    if (GLS.x > 0) changedGLS |= ImGui::SliderFloat("Index of Refraction", &GLS.y, 0.0f, 3.0f);
                    changedGLS |= ImGui::SliderFloat("Emission Strength", &GLS.z, 0.0f, 10.0f);
                    colors[selectedColor] = C;
                    specularColors[selectedColor] = SC;
                    glassLightSettings[selectedColor] = GLS;
                    ImGui::Unindent();
                }

                int textureID = modelsTextureID[selectedModel];
                if (textureID != -1) {
                    ImGui::Separator();
                    ImGui::Text("Texture");
                    ImGui::Text(textureLabels[textureID].c_str());
                    ImGui::Text("ID: %i", textureID);

                    float s = textureScales[textureID];
                    bool wrapTexture = s > 0;;
                    if (ImGui::Checkbox("Wrap Texture", &wrapTexture)) {
                        if (wrapTexture) {
                            textureScales[textureID] = 0.001;
                        } else {
                            textureScales[textureID] = 0;
                        }
                        ui_resetAccum = true;
                    }

                    if (wrapTexture) {
                        if (ImGui::DragFloat("Scale", &s, 0.005f, 0.0001f, 2.0f, "%.4f")) {
                            textureScales[textureID] = s;
                            ui_resetAccum = true;
                        }
                    }
                }
                ImGui::Unindent();

                if (changedPRS) {
                    R = radians(rotDeg);

                    modelTransforms[selectedModel] = composeTransform(P, R, S);
                    modelInvTransforms[selectedModel] = inverse(modelTransforms[selectedModel]);

                    ssboModelTransformations.update(selectedModel, modelTransforms[selectedModel]);
                    ssboModelInvTransformations.update(selectedModel, modelInvTransforms[selectedModel]);

                    ui_resetAccum = true;
                }
                // All materials for this model share the same contiguous range:
                const int start = modelsColors[selectedModel][0];   // inclusive
                const int end   = modelsColors[selectedModel][1];   // exclusive

                if (changedC) {
                    ssboColors.update(start, end, colors.data() + start);
                    ui_resetAccum = true;
                }

                if (changedSC) {
                    ssboSpecularColors.update(start, end, specularColors.data() + start);
                    ui_resetAccum = true;
                }

                if (changedGLS) {
                    ssboGlassLightSettings.update(start, end, glassLightSettings.data() + start);
                    ui_resetAccum = true;
                }
            }
        }

        ImGui::Separator();

        if (ImGui::Button("Reset accumulation")) ui_resetAccum = true;

        ImGui::End();
    }

    // settings
    {
        ImGui::SetNextWindowPos(ImVec2(520, 10), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_Once);
        ImGui::Begin("Settings");

        ImGui::Checkbox("Lock", &lock);

        ImGui::SliderInt("Samples", &samples, 1, 25);
        ui_resetAccum |= ImGui::SliderInt("Antialiasing", &aa, 1, 5);
        ui_resetAccum |= ImGui::SliderInt("Bounces", &bounceLim, 1, 16);

        ImGui::Separator();

        ImGui::Text("Camera");

        ui_resetAccum |= ImGui::SliderFloat("FOV", &fovDeg, 20, 140);

        float s = sensitivity*100;
        if (ImGui::SliderFloat("Sensitivity", &s, 0.1, 10)) sensitivity = s/100;

        ImGui::SliderFloat("Speed", &speed, 0.01, 1000);

        ImGui::Separator();

        ImGui::Text("Debug");

        ui_resetAccum |= ImGui::Checkbox("##Debug View" , &debugView);
        if (debugView) {

            const char* names[] = { "Normals", "Heatmap", "Depth" };

            // Use a custom getter function
            int debugMode = this->debugMode;
            if (ImGui::SliderInt("Debug Mode", &debugMode, 0, 2, names[debugMode])) {
                this->debugMode = static_cast<DebugMode>(debugMode);
                ui_resetAccum = true;
            }

            switch (debugMode) {
                case Normals:
                    break;
                case Heatmap:
                    ui_resetAccum |= ImGui::SliderInt("Triangle Threshold", &triTh, 1, 50);
                    ui_resetAccum |= ImGui::SliderInt("AABB Threshold", &aabbTh, 1, 250);
                    break;
                case Depth:
                    ui_resetAccum |= ImGui::SliderFloat("Depth Scale", &depthScale, 1, 5000);
                default: ;
            }

        }

        ImGui::End();
    }

    // stats
    {
        ImGui::SetNextWindowPos(ImVec2(830, 10), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_Once);
        ImGui::Begin("Stats");

        ImGui::Text("Samples: %d", sampleCount);
        ImGui::Text("Frame: %d", frameCount);

        ImGui::Separator();

        ImGui::Text("FPS: %.2f", fps);
        ImGui::Text("Frame Time (ms): %.2f", 1/fps*1000.0f);

        ImGui::Separator();

        ImGui::Text("Width: %d", window.size().x);
        ImGui::Text("Height: %d", window.size().y);

        ImGui::Separator();

        ImGui::Text("Position: %.2f, %.2f, %.2f", cameraPos.x, cameraPos.y, cameraPos.z);
        ImGui::Text("Forward: %.2f, %.2f, %.2f", camForward.x, camForward.y, camForward.z);

        ImGui::End();
    }


    if (ui_resetAccum) {
        frameCount = 0;
        sampleCount = 0;
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
    std::cout << prefix << "Triangles: " << numTris << std::endl;
}