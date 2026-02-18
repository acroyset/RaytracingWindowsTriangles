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

Scene::Scene() {
    samples = 1;
    aa = 1;
    bounceLim = 8;

    frameCount = 0;
    sampleCount = 0;

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
        if (!baseModel.valid) return;
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
void Scene::addModel(const Model& model, const std::string& texturePath) {

    int Toffset = int(triangles.size())/3;
    int Voffset = int(vertices.size());
    int TXoffset = int(texCoords.size());
    int Noffset = int(normals.size());
    int BBoffset = int(BVHnodes.size());
    int Moffset = getNumMaterials();

    bool useTexture = !model.base.texCoords.empty() && !texturePath.empty();
    int textureID = int(textures.size()) + int(pendingTextures.size());

    bool reuse = false;

    auto loc = std::find_if(models.begin(), models.end(),
    [&model](const Model& a) {
        return a.filename == model.filename;
    });

    if (loc != models.end()) {
        int index = int(loc - models.begin());
        std::vector<int> offsets = modelOffsets[index];
        Toffset = offsets[0];
        Voffset = offsets[1];
        TXoffset = offsets[2];
        Noffset = offsets[3];
        BBoffset = offsets[4];

        reuse = true;
    }

    models.push_back(model);
    modelOffsets.push_back({Toffset, Voffset, TXoffset, Noffset, BBoffset, Moffset});

    if (!reuse) {
        for (int i = 0; i < model.base.triangles.size()/3; i++) {
            ivec4 triangle1 = model.base.triangles[i*3+0];
            ivec4 triangle2 = model.base.triangles[i*3+1];
            ivec4 triangle3 = model.base.triangles[i*3+2];

            ivec4 offsets1 = ivec4(Voffset, triangle1.y == -1 ? 0 : TXoffset, triangle1.z == -1 ? 0 : Noffset, 0);
            ivec4 offsets2 = ivec4(Voffset, triangle2.y == -1 ? 0 : TXoffset, triangle2.z == -1 ? 0 : Noffset, 0);
            ivec4 offsets3 = ivec4(Voffset, triangle3.y == -1 ? 0 : TXoffset, triangle3.z == -1 ? 0 : Noffset, 0);

            triangle1 += offsets1;
            triangle2 += offsets2;
            triangle3 += offsets3;

            triangles.emplace_back(triangle1);
            triangles.emplace_back(triangle2);
            triangles.emplace_back(triangle3);
        }

        for (vec3 vertex : model.base.vertices) {
            vertices.emplace_back(vertex, 0);
        }
        for (vec2 texCoord : model.base.texCoords) {
            texCoords.emplace_back(texCoord);
        }
        for (vec3 normal : model.base.normals) {
            normals.emplace_back(normal, 0);
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
    }

    if (model.materials.empty()) {
        models.back().materials.emplace_back();
    }

    if (useTexture) {
        models.back().textureID = textureID;
        if (std::this_thread::get_id() == mainThreadID) {
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
        } else pendingTextures.emplace_back(texturePath, textureID);
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

    uEnvYaw.set(0.0f);

    skyTexture.bind();

    for (const Texture &tex : textures) {
        tex.bind();
    }
}

void Scene::set_ssbo() {

    lastSentPackage = dataSent();

    std::vector<Material> materials;
    std::vector<mat4> modelTransforms;
    std::vector<mat4> modelInvTransforms;
    std::vector<ModelOffset> modelOffsetsTemp;
    for (int i = 0; i < models.size(); i++) {
        std::vector<int> offsets = modelOffsets[i];
        Model model = models[i];

        for (const Material& m : model.materials) materials.emplace_back(m);
        modelTransforms.emplace_back(model.transformation.matrix);
        modelInvTransforms.emplace_back(model.transformation.inverseMatrix);

        modelOffsetsTemp.emplace_back(offsets[4], offsets[5], model.textureID);
    }

    ssboTriangles.set(triangles, 0);
    ssboVertices.set(vertices, 1);
    ssboTexCoords.set(texCoords, 2);
    ssboNormals.set(normals, 3);
    ssboMaterials.set(materials, 4);
    ssboBVHnodes.set(BVHnodes, 5);
    ssboModelOffsets.set(modelOffsetsTemp, 6);
    ssboModelTransformations.set(modelTransforms, 7);
    ssboModelInvTransformations.set(modelInvTransforms, 8);

}

bool Scene::inputHandling(float speed, float sensitivity, float dt) {
    if (ui.typing) return false;

    double mx, my;
    glfwGetCursorPos(window.getWindow(), &mx, &my);
    vec2 mousePos((float)mx, (float)my);

    // Convert ImGui viewport center to GLFW client space
    int winX, winY;
    glfwGetWindowPos(window.getWindow(), &winX, &winY);
    vec2 imguiCenter = ui.getCenter();
    vec2 center = imguiCenter - vec2(winX, winY);  // now in GLFW client space

    bool moved = false;

    if (!ui.skipMouseFrame) {
        if (!lock) {
            vec2 delta = vec2(mousePos.x - center.x,
                              -(mousePos.y - center.y));

            if (delta.x*delta.x + delta.y*delta.y > 0.25f) {
                delta *= (2.0f / float(window.size().y)) * sensitivity * fovDeg;

                camForward += delta.x * camRight + delta.y * camUp;
                camForward = normalize(camForward);
                setBasisVectors(camForward, camUp, camRight);
                moved = true;

                window.setMousePos(center);
            }
        }
    } else {
        ui.skipMouseFrame = false;
        window.setMousePos(center);
    }

    if (window.keyPressed(GLFW_KEY_L)) {
        if (!trackedKeysPressed[GLFW_KEY_L]) {
            lock = !lock;
            if (!lock) window.setMousePos(center);
        }

        trackedKeysPressed[GLFW_KEY_L] = true;
    } else trackedKeysPressed[GLFW_KEY_L] = false;
    if (window.keyPressed(GLFW_KEY_F)) {
        if (!trackedKeysPressed[GLFW_KEY_F]) {
            ui.viewportFullscreen = !ui.viewportFullscreen;
        }
        if (!lock) {
            ui.skipMouseFrame = true;
        }
        trackedKeysPressed[GLFW_KEY_F] = true;
    } else trackedKeysPressed[GLFW_KEY_F] = false;

    if (lock) return false;

    vec3 change = vec3(0, 0, 0);
    if (window.keyPressed(GLFW_KEY_W)) change += camForward;
    if (window.keyPressed(GLFW_KEY_S)) change -= camForward;
    if (window.keyPressed(GLFW_KEY_A)) change -= camRight;
    if (window.keyPressed(GLFW_KEY_D)) change += camRight;
    if (window.keyPressed(GLFW_KEY_E)) change += camUp;
    if (window.keyPressed(GLFW_KEY_Q)) change -= camUp;

    if (window.keyPressed(GLFW_KEY_LEFT_SHIFT) || window.keyPressed(GLFW_KEY_RIGHT_SHIFT)) speed *= 2;

    if (pow(change.x, 2) + pow(change.y, 2) + pow(change.z, 2) > 0) {
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
        if (!pendingTextures.empty()) {
            for (const auto& [path, id] : pendingTextures) {
                textures.emplace_back(window.createTexture("textures[" + std::to_string(id) + "]", path));
                textures.back().setWrap(TextureWrap::REPEAT, TextureWrap::REPEAT);

                std::string label = path;
                int count = 0;
                for (const std::string& i : textureLabels) {
                    if (i == label) { count++; label = path + " " + std::to_string(count); }
                }
                textureLabels.emplace_back(label);
            }
            pendingTextures.clear();
        }

        set_ssbo();
        newData = false;
    }
    if (busy) {
        lastSentPackage = dataSent();
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    float dt = window.getDeltaTime();
    updateItemSmooth(totalTime, dt);
    dtData.emplace_back(dt);

    if (inputHandling(speed, sensitivity, dt)) resetAccumulation();

    setUniforms();

    frameCount++;
    sampleCount += samples;

    // --- Controls window ---
    ui.ImGuiRender(*this);

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

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


    glfwSwapBuffers(window.getWindow());
    glfwPollEvents();
}

void Scene::displayStats() const {
    DataPackage package = lastSentPackage;

    std::cout << "Triangles: "       << lastSentPackage.triangles  << " (" << bytesToReadable(package.triangleBytes)   << ")" << std::endl;
    std::cout << "Vertices: "        << lastSentPackage.vertices   << " (" << bytesToReadable(package.verticesBytes)   << ")" << std::endl;
    std::cout << "Texture Coords: "  << lastSentPackage.texCoords  << " (" << bytesToReadable(package.texCoordsBytes)  << ")" << std::endl;
    std::cout << "Normals: "         << lastSentPackage.normals    << " (" << bytesToReadable(package.normalsBytes)    << ")" << std::endl;
    std::cout << "BVH Nodes: "       << lastSentPackage.BVHNodes   << " (" << bytesToReadable(package.BVHNodesBytes)   << ")" << std::endl;
    std::cout                                                                                                                 << std::endl;
    std::cout << "Models: "          << models.size()                                                                         << std::endl;
    std::cout << "Materials: "       << lastSentPackage.materials  << " (" << bytesToReadable(package.materialsBytes)  << ")" << std::endl;
    std::cout << "Textures: "        << lastSentPackage.textures   << " (" << bytesToReadable(package.texturesBytes)   << ")" << std::endl;
    std::cout << "Transformations: " << lastSentPackage.transforms << " (" << bytesToReadable(package.transformsBytes) << ")" << std::endl;
    std::cout << "Total Data Sent: " << bytesToReadable(package.totalSize) << std::endl;
    std::cout << std::endl;
}

DataPackage Scene::dataSent() const {
    DataPackage result{};

    for (const Model& model : models) {
        result.triangles += int(model.base.triangles.size())/3;
        result.vertices += int(model.base.vertices.size());
        result.texCoords += int(model.base.texCoords.size());
        result.normals += int(model.base.normals.size());
        result.materials += int(model.materials.size());
        result.BVHNodes += int(model.base.BVHnodes.size());
        result.transforms ++;
    }
    result.textures = int(textures.size());

    result.trianglesSent = int(triangles.size())/3;
    result.verticesSent = int(vertices.size());
    result.texCoordsSent = int(texCoords.size());
    result.normalsSent = int(normals.size());
    result.BVHNodesSent = int(BVHnodes.size());

    result.triangleBytes = result.trianglesSent * int(sizeof(ivec4)) * 3;
    result.verticesBytes = result.verticesSent * int(sizeof(vec4));
    result.texCoordsBytes = result.texCoordsSent * int(sizeof(vec2));
    result.normalsBytes = result.normalsSent * int(sizeof(vec4));
    result.materialsBytes = result.materials * int(sizeof(Material));
    result.BVHNodesBytes = result.BVHNodesSent * int(sizeof(BVHnode));
    result.transformsBytes = result.transforms * int(sizeof(Transformation));

    result.texturesBytes = 0;
    for (const Texture& t : textures) {
        result.texturesBytes += int(t.gpuSizeBytes());
    }

    result.totalSize =
        result.triangleBytes +
        result.verticesBytes +
        result.texCoordsBytes +
        result.normalsBytes +
        result.materialsBytes +
        result.BVHNodesBytes +
        result.transformsBytes +
        result.texturesBytes;

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

    resetAccumulation();
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