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

std::string bytesToReadable(long long bytes, int sigFigs = 3) {
    if (bytes == 0) return "0 Bytes";
    if (sigFigs < 1) sigFigs = 1;

    std::vector<std::string> units = {"Bytes", "KB", "MB", "GB", "TB", "PB"};
    const int unitCount = int(units.size());

    auto value = static_cast<double>(bytes);
    int unitIndex = 0;

    while (value >= 1024.0 && unitIndex < unitCount - 1) {
        value /= 1024.0;
        ++unitIndex;
    }

    // determine decimal places needed for desired significant figures
    int digitsBeforeDecimal = (value > 0) ? static_cast<int>(std::floor(std::log10(value))) + 1 : 1;
    int decimals = sigFigs - digitsBeforeDecimal;
    if (decimals < 0) decimals = 0;

    std::ostringstream out;
    out << std::fixed << std::setprecision(decimals) << value << " " << units[unitIndex];
    return out.str();
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

void Scene::addModel(const std::string& filename, const Transformation &transformation, const Material &material, const std::string& texturePath) {
    Model model(filename);

    addModel(model, transformation, material, texturePath);
}
void Scene::addModel(const Model& model, const Transformation& transformation, const Material& material, const std::string& texturePath) {

    bool useTexture = !model.texCoords.empty() && !texturePath.empty();
    int textureID = int(textures.size());
    if (useTexture) {
        textures.emplace_back(window.createTexture("textures[" + std::to_string(textureID) + "]", texturePath));
        textures.back().setWrap(TextureWrap::REPEAT, TextureWrap::REPEAT);
        const std::string& name = texturePath;
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

    int Toffset = int(triangles.size())/3;
    int Voffset = int(vertices.size());
    int TXoffset = int(texCoords.size());
    int Noffset = int(normals.size());
    int BBoffset = int(BVHnodes.size());
    int Moffset = int(materials.size());

    bool reuse = false;

    if (modelOffsets.find(model.filename) != modelOffsets.end()) {
        std::vector<int> offsets = modelOffsets[model.filename];
        //Toffset need to work out material
        Voffset = offsets[1];
        TXoffset = offsets[2];
        Noffset = offsets[3];
        //BBoffsets relies on triangle idx so have to do Toffset first

        reuse = true;
        std::cout << "Reuse " << model.filename << std::endl;
    } else {
        modelOffsets[model.filename] = {Toffset, Voffset, TXoffset, Noffset, BBoffset};
    }

    models.emplace_back(BBoffset);

    for (int i = 0; i < model.triangles.size()/3; i++) {
        ivec4 triangle1 = model.triangles[i*3+0];
        ivec4 triangle2 = model.triangles[i*3+1];
        ivec4 triangle3 = model.triangles[i*3+2];

        ivec3 offsets = ivec3(Voffset, TXoffset, Noffset);

        triangle1 += ivec4(offsets, Moffset);
        triangle2 += ivec4(offsets, useTexture ? textureID : -1);
        triangle3 += ivec4(offsets, 0);

        triangles.emplace_back(triangle1);
        triangles.emplace_back(triangle2);
        triangles.emplace_back(triangle3);
    }

    if (!reuse) {
        for (vec3 vertex : model.vertices) {
            vertices.emplace_back(vertex, 0);
        }
        for (vec2 texCoord : model.texCoords) {
            texCoords.emplace_back(texCoord);
        }
        for (vec3 normal : model.normals) {
            normals.emplace_back(normal, 0);
        }
    }

    for (auto node : model.BVHnodes) {

        BVHnode newNode;
        newNode.setMin(node.getMin());
        newNode.setMax(node.getMax());
        if (node.leaf()) {
            newNode.setTriStart(node.getTriStart()+Toffset);
            newNode.setNumTri(node.getNumTri());
        } else {
            newNode.setChildA(node.getChildA()+BBoffset);
            newNode.setChildB(node.getChildB()+BBoffset);
        }

        BVHnodes.emplace_back(newNode);
    }

    if (model.materials.empty()) {
        materials.emplace_back(material);
    } else {
        for (Material m : model.materials) {
            materials.emplace_back(m);
        }
    }

    modelTransforms.emplace_back(transformation.matrix);
    modelInvTransforms.emplace_back(inverse(modelTransforms.back()));

    modelPos.emplace_back(transformation.position);
    modelRot.emplace_back(transformation.rotation);
    modelScale.emplace_back(transformation.scale);

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

    int Mcount = int(materials.size()) - Moffset;
    modelsMaterialsIdx.emplace_back(Moffset, Moffset + Mcount);

}

int Scene::getNumBVHNodes() const {
    return int(BVHnodes.size());
}

int Scene::getNumTris() const {
    return int(triangles.size()/3);
}

void Scene::createUniforms() {
    uNumModels = window.createUniform<int>("numModels");

    uCameraPos          = window.createUniform<vec3>("cameraPos");
    uCameraForward      = window.createUniform<vec3>("camForward");
    uCameraUp           = window.createUniform<vec3>("camUp");
    uCameraRight        = window.createUniform<vec3>("camRight");
    uFovDeg             = window.createUniform<float>("fovDeg");
    uAperture           = window.createUniform<float>("aperture");
    uFocusDistance      = window.createUniform<float>("focusDistance");
    uFocusDistancePlane = window.createUniform<bool>("focusDistancePlane");

    uResolution     = window.createUniform<uvec2>("resolution");
    uFrameCount     = window.createUniform<int>("frameCount");
    uTimeSinceStart = window.createUniform<float>("timeSinceStart");

    uNumNodes  = window.createUniform<int>("numNodes");
    uSamples   = window.createUniform<int>("samples");
    uAA        = window.createUniform<int>("aa");
    uBounceLim = window.createUniform<int>("bounceLim");

    uSkyActive = window.createUniform<bool>("skyActive");
    uSkyColor  = window.createUniform<vec3>("skyColor");
    uSunDir    = window.createUniform<vec3>("sunDir");
    uSunColor  = window.createUniform<vec3>("sunColor");

    uFloorActive        = window.createUniform<bool>("floorActive");
    uFloorDiffuseColor  = window.createUniform<vec4>("floorDiffuseColor");
    uFloorSpecularColor = window.createUniform<vec4>("floorSpecularColor");

    uDebugView     = window.createUniform<bool>("debugView");
    uDebugMode     = window.createUniform<int>("debugMode");
    uTriThreshold  = window.createUniform<int>("triTh");
    uAABBThreshold = window.createUniform<int>("aabbTh");
    uDepthScale    = window.createUniform<float>("depthScale");

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
    uAperture.set(aperture);
    uFocusDistance.set(focusDistance);
    uFocusDistancePlane.set(focusDistancePlane);

    uResolution.set(window.size());
    uFrameCount.set(frameCount);
    uTimeSinceStart.set(window.getTimeSinceStart());

    uNumNodes.set(getNumBVHNodes());
    uSamples.set(samples);
    uAA.set(aa);
    uBounceLim.set(bounceLim);

    uSkyActive.set(skyActive);
    uSkyColor.set(skyColor);
    uSunDir.set(sunDir);
    uSunColor.set(sunColor*sunStrength);

    uFloorActive.set(floorActive);
    uFloorDiffuseColor.set(floorDiffuseColor);
    uFloorSpecularColor.set(floorSpecularColor);

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
    ssboMaterials.set(materials, 4, true);
    ssboBVHnodes.set(BVHnodes, 5);
    ssboModels.set(models, 6);
    ssboModelTransformations.set(modelTransforms, 7, true);
    ssboModelInvTransformations.set(modelInvTransforms, 8, true);

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
    Timer t;
    window.start();

    if (hud) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    float dt = window.getDeltaTime();
    updateItemSmooth(totalTime, dt);

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

    updateItemSmooth(cpuTime, t.reset());

    GLuint query;
    glGenQueries(1, &query);
    glBeginQuery(GL_TIME_ELAPSED, query);

    window.render();

    glEndQuery(GL_TIME_ELAPSED);
    GLuint64 time;
    glGetQueryObjectui64v(query, GL_QUERY_RESULT, &time);
    double s = double(time) / 1e9;
    updateItemSmooth(gpuTime, s);

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
        ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_Once);
        ImGui::Begin("Scene");

        ui_resetAccum |= ImGui::Checkbox("Sky Active", &skyActive);
        if (skyActive) {
            ImGui::Indent();
            ui_resetAccum |= ColorEdit3("Sun Color", sunColor);
            ui_resetAccum |= DragFloat3("Sun Direction", sunDir);
            sunDir = normalize(sunDir);

            ui_resetAccum |= ImGui::SliderFloat("Sun Strength", &sunStrength, 0, 2000);
            ImGui::Unindent();
        }

        ui_resetAccum |= ImGui::Checkbox("Floor Active", &floorActive);
        if (floorActive) {
            ImGui::Indent();

            vec3 diffuseColor = vec3(floorDiffuseColor);
            if (ColorEdit3("Floor Diffuse Color", diffuseColor)) {
                floorDiffuseColor = vec4(diffuseColor, floorDiffuseColor.w);
                ui_resetAccum = true;
            }

            bool specularFloor = floorSpecularColor.w != -1.0f;
            if (ImGui::Checkbox("Specular Floor", &specularFloor)) {
                if (specularFloor) floorSpecularColor.w = 0.0f;
                else floorSpecularColor.w = -1.0f;
                ui_resetAccum = true;
            }

            if (specularFloor) {
                vec3 specularColor = vec3(floorSpecularColor);
                if (ColorEdit3("Floor Specular Color", specularColor)) {
                    floorSpecularColor = vec4(specularColor, floorSpecularColor.w);
                    ui_resetAccum = true;
                }
            }

             ui_resetAccum |= ImGui::SliderFloat("Floor Smoothness", &floorDiffuseColor.w, 0.0f, 1.0f);

            if (specularFloor) ui_resetAccum |= ImGui::SliderFloat("Floor Specular Probability", &floorSpecularColor.w, 0.0f, 1.0f);

            ImGui::Unindent();
        }

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
                        selectedColor = modelsMaterialsIdx[i].x;
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (selectedModel >= 0) {
                ImGui::Indent();
                bool changedPRS = false;
                bool changedM = false;

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
                    for (int i = modelsMaterialsIdx[selectedModel].x; i < modelsMaterialsIdx[selectedModel].y; ++i) {
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
                    Material& m = materials[selectedColor];
                    vec4 DC = m.getDC();
                    vec4 SC = m.getSC();
                    vec4 GLS = m.getGLS();

                    bool emissive = GLS.z > 0;
                    if (ImGui::Checkbox("Emissive", &emissive)) {
                        if (emissive) GLS.z = 1;
                        else GLS.z = 0;
                        changedM = true;
                    }

                    if (emissive) {
                        if (modelsTextureID[selectedModel] == -1) changedM |= ColorEdit3("Color", DC);
                        changedM |= ImGui::SliderFloat("Emission Strength", &GLS.z, 0.01f, 1000.0f);
                    } else {

                        if (modelsTextureID[selectedModel] == -1) changedM |= ColorEdit3("Diffuse Color", DC);

                        bool specular = SC.w >= 0;
                        if (ImGui::Checkbox("Specular", &specular)) {
                            if (specular) SC.w = 0;
                            else SC.w = -1;
                            changedM = true;
                        }

                        if (specular) changedM |= ColorEdit3("Specular Color", SC);

                        ImGui::Text("Material Properties");

                        changedM |= ImGui::SliderFloat("Smoothness", &DC.w, 0.0f, 1.0f);
                        if (specular) changedM |= ImGui::SliderFloat("Specular Probability", &SC.w, 0.0f, 1.0f);
                        changedM |= ImGui::SliderFloat("Transparency", &GLS.x, 0.0f, 1.0f);
                        if (GLS.x > 0) changedM |= ImGui::SliderFloat("Index of Refraction", &GLS.y, 0.0f, 3.0f);
                    }

                    m.setDC(DC);
                    m.setSC(SC);
                    m.setGLS(GLS);
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

                    Transformation t(P, S, R);
                    modelTransforms[selectedModel] = t.matrix;
                    modelInvTransforms[selectedModel] = inverse(modelTransforms[selectedModel]);

                    ssboModelTransformations.update(selectedModel, modelTransforms[selectedModel]);
                    ssboModelInvTransformations.update(selectedModel, modelInvTransforms[selectedModel]);

                    ui_resetAccum = true;
                }
                // All materials for this model share the same contiguous range:
                const int start = modelsMaterialsIdx[selectedModel][0];   // inclusive
                const int end   = modelsMaterialsIdx[selectedModel][1];   // exclusive

                if (changedM) {
                    ssboMaterials.update(start, end, materials.data() + start);
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
        ImGui::SetNextWindowSize(ImVec2(300, 350), ImGuiCond_Once);
        ImGui::Begin("Settings");

        ImGui::Checkbox("Lock", &lock);

        ImGui::SliderInt("Samples", &samples, 1, 25);
        ui_resetAccum |= ImGui::SliderInt("Antialiasing", &aa, 1, 5);
        ui_resetAccum |= ImGui::SliderInt("Bounces", &bounceLim, 1, 16);

        ImGui::Separator();

        ImGui::Text("Camera");

        ui_resetAccum |= ImGui::SliderFloat("FOV", &fovDeg, 20, 140);
        bool dof = aperture > 0;
        if (ImGui::Checkbox("Depth of Field", &dof)) {
            if (dof) aperture = 0.001;
            else aperture = 0.0;
            ui_resetAccum = true;
        }

        if (dof) {
            ImGui::Indent();
            ui_resetAccum |= ImGui::SliderFloat("Aperture", &aperture, 0.001f, 0.5f);
            ui_resetAccum |= ImGui::SliderFloat("Focus Distance", &focusDistance, 0.0f, 1000.0f);
            ui_resetAccum |= ImGui::Checkbox("Focus Distance Plane", &focusDistancePlane);
            ImGui::Unindent();
        }

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
        ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_Once);
        ImGui::Begin("Stats");

        ImGui::Text("Samples: %d", sampleCount);
        ImGui::Text("Frame: %d", frameCount);

        ImGui::Separator();

        ImGui::Text("FPS: %.2f", 1/totalTime);
        ImGui::Text("Frame Time (ms): %.2f", totalTime*1000.0f);
        ImGui::Text("CPU Time (ms): %.2f", cpuTime*1000.0f);
        ImGui::Text("GPU Time (ms): %.2f", gpuTime*1000.0f);

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


void Scene::displayStats() {
    DataPackageSize package = dataSentSize();

    std::cout << "Triangles: "       << triangles.size()       << " (" << bytesToReadable(package.triangleDataSize)  << ")" << std::endl;
    std::cout << "Vertices: "        << vertices.size()        << " (" << bytesToReadable(package.vertexDataSize)    << ")" << std::endl;
    std::cout << "Texture Coords: "  << texCoords.size()       << " (" << bytesToReadable(package.texCoordDataSize)  << ")" << std::endl;
    std::cout << "Normals: "         << normals.size()         << " (" << bytesToReadable(package.normalDataSize)    << ")" << std::endl;
    std::cout                                                                                                               << std::endl;
    std::cout << "Models: "          << models.size()                                                                       << std::endl;
    std::cout << "Materials: "       << materials.size()       << " (" << bytesToReadable(package.materialDataSize)  << ")" << std::endl;
    std::cout << "Textures: "        << textures.size()        << " (" << bytesToReadable(package.textureDataSize)   << ")" << std::endl;
    std::cout << "BVH Nodes: "       << BVHnodes.size()        << " (" << bytesToReadable(package.BVHnodesDataSize)  << ")" << std::endl;
    std::cout << "Transformations: " << modelTransforms.size() << " (" << bytesToReadable(package.transformDataSize) << ")" << std::endl;
    std::cout << "Total Data Sent: " << bytesToReadable(package.totalSize) << std::endl;
    std::cout << std::endl;
}

DataPackageSize Scene::dataSentSize() const {
    DataPackageSize result{};

    result.triangleDataSize  =   int(triangles.size()       * sizeof(ivec4));
    result.vertexDataSize    =   int(vertices.size()        * sizeof(vec4));
    result.texCoordDataSize  =   int(texCoords.size()       * sizeof(vec2));
    result.normalDataSize    =   int(normals.size()         * sizeof(vec4));
    result.materialDataSize  =   int(materials.size()       * sizeof(Material));
    result.BVHnodesDataSize  =   int(BVHnodes.size()        * sizeof(BVHnode));
    result.transformDataSize = 2*int(modelTransforms.size() * sizeof(mat4));

    result.textureDataSize = 0;
    for (const Texture& t : textures) {
        result.textureDataSize += int(t.gpuSizeBytes());
    }

    result.totalSize =
        result.triangleDataSize +
        result.vertexDataSize +
        result.texCoordDataSize +
        result.normalDataSize +
        result.materialDataSize +
        result.BVHnodesDataSize +
        result.transformDataSize +
        result.textureDataSize;

    return result;
}

/*
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
*/