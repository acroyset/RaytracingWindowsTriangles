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

static void DrawBusyOverlay(const char* label){
    ImGui::OpenPopup("Working...");
    if (ImGui::BeginPopupModal("Working...", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        auto t = static_cast<float>(ImGui::GetTime());
        float v = 0.5f + 0.5f * sinf(t * 6.0f);
        ImGui::TextUnformatted(label);
        ImGui::ProgressBar(v, ImVec2(250, 0));
        ImGui::TextDisabled("Please wait...");
        ImGui::EndPopup();
    }
}

void DrawDockSpace(){
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    // Optional: remove window padding so dockspace fills the viewport
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("DockSpaceRoot", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::End();
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

    busy = false;
    newData = false;
    statusIsError = false;
}

Scene::Scene(const int samples, const int aa, const int bounceLim)
    : samples(samples), aa(aa), bounceLim(bounceLim), frameCount(0), sampleCount(0){

    window.setFeedbackMode(true);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
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

    busy = false;
    newData = false;
    statusIsError = false;
}

void Scene::addModel(const std::string& filename, const Transformation &transformation, const std::string& texturePath) {
    if (precomputedModels.find(filename) != precomputedModels.end()) {
        const BaseModel baseModel = precomputedModels[filename];

        std::string name = baseModel.filename;
        int count = 0;
        for (const Model& m : models) {
            if (m.filename == name) {
                count++;
            }
        }
        name += (count != 0 ? " " + std::to_string(count) : "");

        Model model(name, baseModel, transformation);

        addModel(model, texturePath);
    } else {
        BaseModel baseModel(filename);
        precomputedModels.emplace(filename, baseModel);

        std::string name = baseModel.filename;
        int count = 0;
        for (const Model& m : models) {
            if (m.filename == name) {
                count++;
            }
        }
        name += (count != 0 ? " " + std::to_string(count) : "");

        Model model(name, baseModel, transformation);

        addModel(model, texturePath);
    }
}
void Scene::addModel(const Model& m, const std::string& texturePath) {
    Model model(m);

    int Toffset = int(triangles.size())/3;
    int Voffset = int(vertices.size());
    int TXoffset = int(texCoords.size());
    int Noffset = int(normals.size());
    int BBoffset = int(BVHnodes.size());
    int Moffset = getNumMaterials();

    bool useTexture = !model.base.texCoords.empty() && !texturePath.empty();
    int textureID = int(textures.size());

    bool reuse = false;

    auto loc = std::find_if(models.begin(), models.end(),
    [&model](const Model& a) {
        return a.filename == model.filename;
    });

    if (loc != models.end()) {
        int index = int(loc - models.begin());
        std::vector<int> offsets = modelOffsets[index];
        //Toffset need to work out material
        Voffset = offsets[1];
        TXoffset = offsets[2];
        Noffset = offsets[3];
        //BBoffsets relies on triangle idx so have to do Toffset first

        reuse = true;
    }

    models.emplace_back(model);
    modelOffsets.push_back({Toffset, Voffset, TXoffset, Noffset, BBoffset, Moffset});

    for (int i = 0; i < model.base.triangles.size()/3; i++) {
        ivec4 triangle1 = model.base.triangles[i*3+0];
        ivec4 triangle2 = model.base.triangles[i*3+1];
        ivec4 triangle3 = model.base.triangles[i*3+2];

        ivec4 offsets1 = ivec4(Voffset, triangle1.y == -1 ? 0 : TXoffset, triangle1.z == -1 ? 0 : Noffset, Moffset);
        ivec4 offsets2 = ivec4(Voffset, triangle2.y == -1 ? 0 : TXoffset, triangle2.z == -1 ? 0 : Noffset, useTexture ? textureID : -1);
        ivec4 offsets3 = ivec4(Voffset, triangle3.y == -1 ? 0 : TXoffset, triangle3.z == -1 ? 0 : Noffset, 0);

        triangle1 += offsets1;
        triangle2 += offsets2;
        triangle3 += offsets3;

        triangles.emplace_back(triangle1);
        triangles.emplace_back(triangle2);
        triangles.emplace_back(triangle3);
    }

    if (!reuse) {
        for (vec3 vertex : model.base.vertices) {
            vertices.emplace_back(vertex, 0);
        }
        for (vec2 texCoord : model.base.texCoords) {
            texCoords.emplace_back(texCoord);
        }
        for (vec3 normal : model.base.normals) {
            normals.emplace_back(normal, 0);
        }
    }

    for (auto node : model.base.BVHnodes) {

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
        models.back().materials.emplace_back();
    }

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
        model.textureID = textureID;
    }
}

int Scene::getNumBVHNodes() const {
    return int(BVHnodes.size());
}

int Scene::getNumTris() const {
    return int(triangles.size()/3);
}

int Scene::getNumMaterials() const {
    int materials = 0;
    for (const Model& m : models) {materials += int(m.materials.size());}
    return materials;
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
    uSampleCount    = window.createUniform<int>("sampleCount");

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
    uSampleCount.set(sampleCount);

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

    lastSentPackage = dataSentSize();

    std::vector<int> modelBVHoffset;
    for (const std::vector<int>& offsets : modelOffsets) {
        modelBVHoffset.push_back(offsets[4]);
    }

    std::vector<Material> materials;
    std::vector<mat4> modelTransforms;
    std::vector<mat4> modelInvTransforms;
    for (const Model& model : models) {
        for (const Material& m : model.materials) materials.emplace_back(m);
        modelTransforms.emplace_back(model.transformation.matrix);
        modelInvTransforms.emplace_back(model.transformation.inverseMatrix);
    }

    ssboTriangles.set(triangles, 0);
    ssboVertices.set(vertices, 1);
    ssboTexCoords.set(texCoords, 2);
    ssboNormals.set(normals, 3);
    ssboMaterials.set(materials, 4);
    ssboBVHnodes.set(BVHnodes, 5);
    ssboModels.set(modelBVHoffset, 6);
    ssboModelTransformations.set(modelTransforms, 7);
    ssboModelInvTransformations.set(modelInvTransforms, 8);

}

bool Scene::inputHandling(float speed, float sensitivity, float dt) {
    if (typing) return false;

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

    if (newData) {
        set_ssbo();
        newData = false;
    }

    if (hud) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    float dt = window.getDeltaTime();
    updateItemSmooth(totalTime, dt);
    dtData.emplace_back(dt);

    if (inputHandling(speed, sensitivity, dt)) {
        frameCount = 0;
        sampleCount = 0;
    }

    setUniforms();

    frameCount++;
    sampleCount += samples;

    // --- Controls window ---
    if (hud) ImGuiRender();

    updateItemSmooth(cpuTime, float(t.reset()));

    GLuint query;
    glGenQueries(1, &query);
    glBeginQuery(GL_TIME_ELAPSED, query);

    window.render();

    glEndQuery(GL_TIME_ELAPSED);
    GLuint64 time;
    glGetQueryObjectui64v(query, GL_QUERY_RESULT, &time);
    float s = float(time) / 1e9f;
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

    DrawDockSpace();

    // scene
    {
        ImGui::Begin("Scene");

        if (ImGui::Button("Reset accumulation")) ui_resetAccum = true;

        if (ImGui::CollapsingHeader("Sky")) {
            ui_resetAccum |= ImGui::Checkbox("Sky Active", &skyActive);
            if (skyActive) {
                ImGui::Indent();
                ui_resetAccum |= ColorEdit3("Sun Color", sunColor);
                ui_resetAccum |= DragFloat3("Sun Direction", sunDir);
                sunDir = normalize(sunDir);

                ui_resetAccum |= ImGui::SliderFloat("Sun Strength", &sunStrength, 0, 500);
                ImGui::Unindent();
            }
        }

        if (ImGui::CollapsingHeader("Floor")) {
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
        }

        ImGui::Separator();
        ImGui::Text("Models");

        ImGui::Indent();

        const bool hasModels = !models.empty();
        if (!hasModels) {
            ImGui::TextDisabled("(no models)");
        }
        else {

            for (int i = 0; i < models.size(); i++) {
                bool sel = (selectedModel == i);
                if (ImGui::Selectable(models[i].name.c_str(), sel)) {
                    if (i == selectedModel) selectedModel = -1;
                    else {
                        selectedModel = i;
                        selectedColor = 0;
                    }
                }
            }
        }

        ImGui::Unindent();


        float buttonHeight = 30.0f;
        float spacing      = ImGui::GetStyle().ItemSpacing.y;

        // total height of the bottom block: 3 buttons + 2 gaps
        float blockHeight = 3.0f * buttonHeight + 2.0f * spacing;

        float availY = ImGui::GetContentRegionAvail().y;

        // Move cursor down so the next items land at the bottom
        float y = ImGui::GetCursorPosY() + (availY - blockHeight);
        if (y > ImGui::GetCursorPosY())  // avoid moving up if not enough room
            ImGui::SetCursorPosY(y);


        static char filename[256] = "";


        if (ImGui::Button("Add Model", ImVec2(-FLT_MIN, buttonHeight))) {
            strcpy(filename, "assets/models/");

            ImGui::OpenPopup("Add Model");
        }

        if (ImGui::Button("Save JSON", ImVec2(-FLT_MIN, buttonHeight))) {
            strcpy(filename, "");

            ImGui::OpenPopup("Save JSON");
        }

        if (ImGui::Button("Load JSON", ImVec2(-FLT_MIN, buttonHeight))) {
            strcpy(filename, "");

            ImGui::OpenPopup("Load JSON");
        }

        if (ImGui::BeginPopupModal("Add Model", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            lock = true;
            typing = true;

            ImGui::Text("Enter file name:");
            ImGui::InputText("##filename", filename, IM_ARRAYSIZE(filename));

            ImGui::Separator();

            if (ImGui::Button("Add")) {
                startAddJob(filename);
                typing = false;

                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Save JSON", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            lock = true;
            typing = true;

            ImGui::Text("Enter file name:");
            ImGui::InputText("##filename", filename, IM_ARRAYSIZE(filename));

            ImGui::Separator();

            if (ImGui::Button("Save")) {
                saveJSON(filename);
                typing = false;

                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Load JSON", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            lock = true;
            typing = true;

            ImGui::Text("Enter file name:");
            ImGui::InputText("##filename", filename, IM_ARRAYSIZE(filename));

            ImGui::Separator();

            if (ImGui::Button("Load")) {
                startLoadJob(filename);
                typing = false;

                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (busy) {
            DrawBusyOverlay(busyLabel.c_str());
        }

        ImGui::End();
    }

    // settings
    {
        ImGui::Begin("Settings");

        ImGui::Checkbox("Lock", &lock);

        ImGui::SliderInt("Samples", &samples, 1, 25);
        ui_resetAccum |= ImGui::SliderInt("Antialiasing", &aa, 1, 5);
        ui_resetAccum |= ImGui::SliderInt("Bounces", &bounceLim, 1, 16);

        if (ImGui::CollapsingHeader("Camera")) {
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
        }

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
        ImGui::Begin("Stats");

        ImGui::Text("Samples: %d", sampleCount);
        ImGui::Text("Frame: %d", frameCount);

        ImGui::Separator();

        char overlay[128];
        sprintf(overlay, "FPS: %.2f (%.2f ms)  CPU: %.2f ms  GPU: %.2f ms", 1/totalTime, totalTime*1000.0f, cpuTime*1000.0f, gpuTime*1000.0f);

        float maxVal = (*std::max_element(dtData.begin(), dtData.end())) * 1.2f;
        ImGui::PlotHistogram(
            "##Frame Time (ms)",
            dtData.data(),
            int(dtData.size()),
            0,
            overlay,
            0.0f,
            maxVal,
            ImVec2(0, 100)
            );
        if (ImGui::Button("Reset")) {
            dtData.clear();
        }
        int maxNum = int(10.0f/totalTime);
        if (int(dtData.size()) > maxNum) {
            dtData.erase(dtData.begin());
        }

        ImGui::Separator();

        ImGui::Text("Width: %d", window.size().x);
        ImGui::Text("Height: %d", window.size().y);

        ImGui::Separator();

        ImGui::Text("Position: %.2f, %.2f, %.2f", cameraPos.x, cameraPos.y, cameraPos.z);
        ImGui::Text("Forward: %.2f, %.2f, %.2f", camForward.x, camForward.y, camForward.z);

        ImGui::Separator();

        if (ImGui::CollapsingHeader("Scene Statistics"))
        {
            DataPackageSize package = lastSentPackage;

            if (ImGui::BeginTable("StatsTable", 3,
                ImGuiTableFlags_SizingStretchProp |
                ImGuiTableFlags_BordersInnerV))
            {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Size",  ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableHeadersRow();

                auto Row = [&](const char* label, int count, const std::string& size)
                {
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(label);

                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%d", count);

                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%s", size.c_str());
                };

                Row("Triangles", int(triangles.size())/3,
                    bytesToReadable(package.triangleDataSize));

                Row("Vertices", int(vertices.size()),
                    bytesToReadable(package.vertexDataSize));

                Row("Texture Coords", int(texCoords.size()),
                    bytesToReadable(package.texCoordDataSize));

                Row("Normals", int(normals.size()),
                    bytesToReadable(package.normalDataSize));

                Row("Materials", getNumMaterials(),
                    bytesToReadable(package.materialDataSize));

                Row("Textures", int(textures.size()),
                    bytesToReadable(package.textureDataSize));

                Row("BVH Nodes", int(BVHnodes.size()),
                    bytesToReadable(package.BVHnodesDataSize));

                Row("Transformations", int(models.size()),
                    bytesToReadable(package.transformDataSize));

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(ImVec4(1,1,0.4f,1), "Total");

                ImGui::TableSetColumnIndex(2);
                ImGui::TextColored(ImVec4(1,1,0.4f,1),
                    "%s", bytesToReadable(package.totalSize).c_str());

                ImGui::EndTable();
            }
        }

        ImGui::End();
    }

    // inspector
    if (selectedModel >= 0) {
        ImGui::Begin("Inspector");

        ImGui::Indent();
        bool changedT = false;
        bool changedM = false;

        Model& model = models[selectedModel];

        // Local aliases
        Transformation& transform = model.transformation;

        ImGui::Text("Transformations");

        // Rotation UI in degrees (convert to/from radians for nicer UX)
        vec3 rotDeg = degrees(transform.rotation);
        changedT |= DragFloat3("Position", transform.position, 3.0f);
        changedT |= DragFloat3("Scale",    transform.scale, 1.0f, 0.0f, 1e36);
        changedT |= DragFloat3("Rotation (deg)", rotDeg, 0.2f);

        ImGui::Separator();

        std::string previewC = "Material " + std::to_string(selectedColor);
        if (ImGui::BeginCombo("##Material", previewC.c_str())) {

            for (int i = 0; i < model.materials.size(); ++i) {
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
            Material& m = model.materials[selectedColor];
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
                if (model.textureID == -1) changedM |= ColorEdit3("Color", DC);
                changedM |= ImGui::SliderFloat("Emission Strength", &GLS.z, 0.01f, 250.0f);
            } else {

                bool isTransparent = (GLS.x > 0);
                if (ImGui::Checkbox("Transparent", &isTransparent)) {
                    if (isTransparent) GLS.x = 1;
                    else GLS.x = 0;
                    changedM = true;
                }

                if (isTransparent) {
                    changedM |= ImGui::SliderFloat("Transparency", &GLS.x, 0.0f, 1.0f);

                    changedM |= ColorEdit3("Color", SC);
                    changedM |= ColorEdit3("Absorb Color", DC);

                    changedM |= ImGui::SliderFloat("Index of Refraction", &GLS.y, 0.0f, 3.0f);
                    changedM |= ImGui::SliderFloat("Smoothness", &DC.w, 0.0f, 1.0f);
                    changedM |= ImGui::SliderFloat("Transparent Smoothness", &GLS.w, 0.0f, 1.0f);
                    changedM |= ImGui::SliderFloat("Absorb Multiplier", &SC.w, 0.0f, 0.1f);
                } else {
                    if (model.textureID == -1) changedM |= ColorEdit3("Diffuse Color", DC);

                    bool specular = SC.w >= 0;
                    if (ImGui::Checkbox("Specular", &specular)) {
                        if (specular) SC.w = 0;
                        else SC.w = -1;
                        changedM = true;
                    }

                    if (specular && !isTransparent) changedM |= ColorEdit3("Specular Color", SC);

                    ImGui::Text("Material Properties");

                    changedM |= ImGui::SliderFloat("Smoothness", &DC.w, 0.0f, 1.0f);
                    if (specular) changedM |= ImGui::SliderFloat("Specular Probability", &SC.w, 0.0f, 1.0f);
                }

            }

            m.setDC(DC);
            m.setSC(SC);
            m.setGLS(GLS);
            ImGui::Unindent();
        }

        int textureID = model.textureID;
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

        if (changedT) {
            transform.rotation = radians(rotDeg);

            transform.setMatrix();

            ssboModelTransformations.update(selectedModel, transform.matrix);
            ssboModelInvTransformations.update(selectedModel, transform.inverseMatrix);

            ui_resetAccum = true;
        }
        // All materials for this model share the same contiguous range:

        if (changedM) {
            int start = modelOffsets[selectedModel][5];
            int end = start + int(model.materials.size());
            ssboMaterials.update(int(start), int(end), model.materials.data());
            ui_resetAccum = true;
        }

        ImGui::End();
    }


    if (ui_resetAccum) {
        frameCount = 0;
        sampleCount = 0;
    }
}


void Scene::displayStats() const {
    DataPackageSize package = lastSentPackage;

    std::cout << "Triangles: "       << triangles.size()/3 << " (" << bytesToReadable(package.triangleDataSize)  << ")" << std::endl;
    std::cout << "Vertices: "        << vertices.size()    << " (" << bytesToReadable(package.vertexDataSize)    << ")" << std::endl;
    std::cout << "Texture Coords: "  << texCoords.size()   << " (" << bytesToReadable(package.texCoordDataSize)  << ")" << std::endl;
    std::cout << "Normals: "         << normals.size()     << " (" << bytesToReadable(package.normalDataSize)    << ")" << std::endl;
    std::cout                                                                                                           << std::endl;
    std::cout << "Models: "          << models.size()                                                                   << std::endl;
    std::cout << "Materials: "       << getNumMaterials()  << " (" << bytesToReadable(package.materialDataSize)  << ")" << std::endl;
    std::cout << "Textures: "        << textures.size()    << " (" << bytesToReadable(package.textureDataSize)   << ")" << std::endl;
    std::cout << "BVH Nodes: "       << BVHnodes.size()    << " (" << bytesToReadable(package.BVHnodesDataSize)  << ")" << std::endl;
    std::cout << "Transformations: " << models.size()      << " (" << bytesToReadable(package.transformDataSize) << ")" << std::endl;
    std::cout << "Total Data Sent: " << bytesToReadable(package.totalSize) << std::endl;
    std::cout << std::endl;
}

DataPackageSize Scene::dataSentSize() const {
    DataPackageSize result{};

    result.triangleDataSize  =   int(triangles.size()  * sizeof(ivec4));
    result.vertexDataSize    =   int(vertices.size()   * sizeof(vec4));
    result.texCoordDataSize  =   int(texCoords.size()  * sizeof(vec2));
    result.normalDataSize    =   int(normals.size()    * sizeof(vec4));
    result.materialDataSize  =   int(getNumMaterials() * sizeof(Material));
    result.BVHnodesDataSize  =   int(BVHnodes.size()   * sizeof(BVHnode));
    result.transformDataSize = 2*int(models.size()     * sizeof(mat4));

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


void Scene::saveJSON(const std::string& filename) const {
    json j;

    j["Camera"]["position"] = cameraPos;
    j["Camera"]["forward"] = camForward;
    j["Camera"]["fovDeg"] = fovDeg;
    j["Camera"]["aperture"] = aperture;
    j["Camera"]["focusDistance"] = focusDistance;
    j["Camera"]["sensitivity"] = sensitivity;
    j["Camera"]["speed"] = speed;

    j["Settings"]["aa"] = aa;
    j["Settings"]["bounceLim"] = bounceLim;

    j["Sky"]["active"] = skyActive;
    j["Sky"]["color"] = skyColor;
    j["Sky"]["sunDir"] = sunDir;
    j["Sky"]["sunStrength"] = sunStrength;
    j["Sky"]["sunColor"] = sunColor;

    j["Floor"]["active"] = floorActive;
    j["Floor"]["diffuseColor"] = floorDiffuseColor;
    j["Floor"]["specularColor"] = floorSpecularColor;

    j["Models"] = json::array();
    for (const Model& m : models) {
        json jm;
        jm["Filename"] = m.filename;
        jm["Transformation"] = m.transformation;

        jm["Materials"] = json::array();
        for (const Material& mat : m.materials) {
            jm["Materials"].push_back(mat);
        }

        jm["TextureID"] = m.textureID;

        j["Models"].push_back(jm);
    }

    j["Textures"] = json::object();
    j["Textures"]["Paths"]  = json::array();
    for (const Texture& t : textures) j["Textures"]["Paths"].push_back(t.getPath());

    j["Textures"]["Scales"] = json::array();
    for (float s : textureScales) j["Textures"]["Scales"].push_back(s);

    std::ofstream f(filename);
    f << j.dump(4);
}

void Scene::loadJSON(const std::string& filename) {
    std::ifstream f(filename);
    if (!f.is_open()) {
        std::cerr << "loadJSON: couldn't open " + filename + "\n";
        return;
    }

    json j;
    f >> j;

    triangles.clear();
    vertices.clear();
    texCoords.clear();
    normals.clear();
    BVHnodes.clear();
    models.clear();
    modelOffsets.clear();

    textures.clear();
    textureLabels.clear();
    textureScales.fill(0.0f);

    // precomputedModels.clear();

    // --- Camera ---
    if (j.contains("Camera")) {
        const auto& c = j["Camera"];
        if (c.contains("position"))      cameraPos = c["position"];
        if (c.contains("forward"))       camForward = c["forward"];
        if (c.contains("fovDeg"))        fovDeg = c["fovDeg"];
        if (c.contains("aperture"))      aperture = c["aperture"];
        if (c.contains("focusDistance")) focusDistance = c["focusDistance"];
        if (c.contains("sensitivity"))   sensitivity = c["sensitivity"];
        if (c.contains("speed"))         speed = c["speed"];

    }
    setBasisVectors(camForward, camUp, camRight);

    // --- Settings ---
    if (j.contains("Settings")) {
        const auto& s = j["Settings"];
        if (s.contains("aa"))        aa = s["aa"].get<int>();
        if (s.contains("bounceLim")) bounceLim = s["bounceLim"].get<int>();
    }

    // --- Sky ---
    if (j.contains("Sky")) {
        const auto& s = j["Sky"];
        if (s.contains("active"))      skyActive = s["active"].get<bool>();
        if (s.contains("color"))       skyColor = s["color"].get<vec3>();
        if (s.contains("sunDir"))      sunDir = normalize(s["sunDir"].get<vec3>());
        if (s.contains("sunStrength")) sunStrength = s["sunStrength"].get<float>();
        if (s.contains("sunColor"))    sunColor = s["sunColor"].get<vec3>();
    }

    // --- Floor ---
    if (j.contains("Floor")) {
        const auto& fl = j["Floor"];
        if (fl.contains("active"))        floorActive = fl["active"].get<bool>();
        if (fl.contains("diffuseColor"))  floorDiffuseColor = fl["diffuseColor"].get<vec4>();
        if (fl.contains("specularColor")) floorSpecularColor = fl["specularColor"].get<vec4>();
    }

    // --- Textures (paths + scales) ---
    std::vector<std::string> texPaths;
    if (j.contains("Textures")) {
        const auto& t = j["Textures"];
        if (t.is_object()) {
            if (t.contains("Paths") && t["Paths"].is_array()) {
                texPaths = t["Paths"].get<std::vector<std::string>>();
            }
            if (t.contains("Scales") && t["Scales"].is_array()) {
                const auto& scales = t["Scales"];
                const int n = std::min<int>(64, (int)scales.size());
                for (int i = 0; i < n; ++i) textureScales[i] = scales[i];
            }
        }
    }

    for (int i = 0; i < (int)texPaths.size(); ++i) {
        const std::string& path = texPaths[i];
        if (path.empty()) {
            continue;
        }

        textures.emplace_back(window.createTexture("textures[" + std::to_string(i) + "]", path));
        textures.back().setWrap(TextureWrap::REPEAT, TextureWrap::REPEAT);

        std::string label = path;
        int count = 0;
        for (const std::string& existing : textureLabels) {
            if (existing == label) {
                count++;
                label = path + " " + std::to_string(count);
            }
        }
        textureLabels.emplace_back(label);
    }

    // --- Models ---
    if (j.contains("Models") && j["Models"].is_array()) {
        for (const auto& m : j["Models"]) {
            std::string mf = m.value("Filename", "");
            if (mf.empty()) continue;

            Transformation t;
            if (m.contains("Transformation")) t = m["Transformation"];

            int texID = m.value("TextureID", -1);
            std::string texturePath = (texID >= 0 && texID < (int)texPaths.size()) ? texPaths[texID] : "";

            addModel(mf, t, texturePath);

            if (m.contains("Materials") && m["Materials"].is_array()) {
                const auto& mats = m["Materials"];

                models.back().materials.clear();
                for (const auto& mat : mats) models.back().materials.push_back(mat);
            }
        }
    }

    // Reset accumulation after loading
    frameCount = 0;
    sampleCount = 0;
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