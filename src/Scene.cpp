// Scene.cpp
#include "Scene.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../external/stb_image_write.h"


#define BVH_MAX_DEPTH 16
#define BVH_TESTS_PER_AXIS 5


using Clock = std::chrono::high_resolution_clock;

auto start = Clock::now();

void setBasisVectors(const vec3& forward, vec3& up, vec3& right) {
    constexpr vec3 world_up(0, 1, 0);
    right = normalize(cross(forward, world_up));
    up = normalize(cross(right, forward));
}

inline float nodeCost(const BVHnode &node) {
    int numModels = node.getEndIdx()-node.getStartIdx();
    const vec3 size = node.getMax()-node.getMin();
    const float halfArea = size.x * (size.y + size.z) + size.y * size.z;
    return halfArea * float(numModels);
}

Scene::Scene() {
    bloom.init(window.getGLSLVersion());

    raytracer.enableFeedback();
    raytracer.enableAOVs(2); // 0 = albedo, 1 = normals
    raytracer.enableFeedback();

    window.addPass(&raytracer);
    window.addPass(&bloom);
    window.addPass(&postProcessing);


    samples = 1;
    aa = 1;
    bounceLim = 8;

    frameCount = 0;
    sampleCount = 0;

    camera = Camera();

    textureScales.fill(0.0f);

    isBusy = false;
    newData = false;
    statusIsError = false;
}

Scene::Scene(const int samples, const int aa, const int bounceLim)
    : samples(samples), aa(aa), bounceLim(bounceLim), frameCount(0), sampleCount(0){

    bloom.init(window.getGLSLVersion());

    raytracer.enableFeedback();
    raytracer.enableAOVs(2);  // 0 = albedo, 1 = normals
    raytracer.enableFeedback();

    window.addPass(&raytracer);
    window.addPass(&bloom);
    window.addPass(&postProcessing);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();

    const char* glsl_version = "#version 430";
    ImGui_ImplGlfw_InitForOpenGL(window.getWindow(), true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    camera.forward = vec3(0, 0, -1);
    setBasisVectors(camera.forward, camera.up, camera.right);

    camera.pos = vec3(0, 0, 0);

    createUniforms();

    skyTexture = raytracer.createTexture("skyTex", "assets/textures/sky.png");

    textureScales.fill(0.0f);

    isBusy = false;
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
    int BBoffset = int(BVHtriangleNodes.size());
    int Moffset = getNumMaterials();

    bool useTexture = !model.base.texCoords.empty() && !texturePath.empty();
    int textureID = useTexture ? (pendingClearTextures ? 0 : int(textures.size())) + int(pendingTextures.size()) : -1;

    bool reuse = false;

    auto loc = std::find_if(models.begin(), models.end(),
    [&model](const Model& a) {
        return a.filename == model.filename;
    });

    if (loc != models.end()) {
        int index = int(loc - models.begin());
        ModelOffset o = modelOffsets[index];
        Toffset = o.triangle;
        Voffset = o.vertex;
        TXoffset = o.texCoord;
        Noffset = o.normal;
        BBoffset = o.BVHnodes;

        reuse = true;
    }

    models.push_back(model);
    modelOffsets.emplace_back(Toffset, Voffset, TXoffset, Noffset, BBoffset, Moffset, textureID);

    if (!reuse) {
        for (ivec4 t : model.base.triangles) {
            triangles.emplace_back(t);
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
            BVHtriangleNodes.emplace_back(node);
        }
    }

    if (model.materials.empty()) {
        models.back().materials.emplace_back();
        models.back().materialNames.emplace_back("Material " + std::to_string(model.materials.size()));
    }

    if (useTexture) {
        modelOffsets.back().textureID = textureID;
        if (std::this_thread::get_id() == mainThreadID) {
            textures.emplace_back(raytracer.createTexture("textures[" + std::to_string(textureID) + "]", texturePath));
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

    emissiveTrisStale = true;
}

void Scene::removeModel(int index) {
    if (index < 0 || index >= models.size()) return;
    Model model = models[index];

    bool otherModels = false;
    for (int i = 0; i < models.size(); ++i) {
        if (i == index) continue;

        if (model.filename == models[i].filename) {
            otherModels = true;
            break;
        }
    }

    ModelOffset offsets = modelOffsets[index];

    int numTriangles = int(model.base.triangles.size())/3;
    int numVertices = int(model.base.vertices.size());
    int numTexCoords = int(model.base.texCoords.size());
    int numNormals = int(model.base.normals.size());
    int numBVHnodes = int(model.base.BVHnodes.size());
    int numMaterials = int(model.materials.size());

    if (otherModels) {
        for (int i = index+1; i < models.size(); ++i) {
            modelOffsets[i].material -= numMaterials;
        }

    } else {


        triangles.erase(triangles.begin() + offsets.triangle*3,
                        triangles.begin() + offsets.triangle*3 + numTriangles*3);

        vertices.erase(vertices.begin() + offsets.vertex,
                       vertices.begin() + offsets.vertex + numVertices);

        texCoords.erase(texCoords.begin() + offsets.texCoord,
                        texCoords.begin() + offsets.texCoord+ numTexCoords);

        normals.erase(normals.begin() + offsets.normal,
                      normals.begin() + offsets.normal + numNormals);


        BVHtriangleNodes.erase(BVHtriangleNodes.begin() + offsets.BVHnodes,
                       BVHtriangleNodes.begin() + offsets.BVHnodes + numBVHnodes);


        for (int i = index+1; i < models.size(); ++i) {
            ModelOffset& o = modelOffsets[i];

            if (o.triangle > offsets.triangle) o.triangle -= numTriangles;
            if (o.vertex > offsets.vertex) o.vertex -= numVertices;
            if (o.texCoord > offsets.texCoord) o.texCoord -= numTexCoords;
            if (o.normal > offsets.normal) o.normal -= numNormals;
            if (o.BVHnodes > offsets.BVHnodes) o.BVHnodes -= numBVHnodes;
            o.material -= numMaterials;
        }
    }

    models.erase(models.begin()+index);
    modelOffsets.erase(modelOffsets.begin()+index);

    newData = true;
    emissiveTrisStale = true;
}


void Scene::createTLAS() {

    int numModels = int(models.size());

    modelMin.reserve(numModels);
    modelMax.reserve(numModels);
    modelCenter.reserve(numModels);
    for (int i = 0; i < numModels; ++i) {
        std::cout << i << " " << models[i].name << " " << modelOffsets[i].BVHnodes << std::endl;
        modelMin.emplace_back(BVHtriangleNodes[modelOffsets[i].BVHnodes].getMin());
        modelMax.emplace_back(BVHtriangleNodes[modelOffsets[i].BVHnodes].getMax());
        modelCenter.emplace_back((modelMin.back()+modelMax.back())/2.0f);
    }

    BVHnode root;

    for (int i = 0; i < numModels; ++i) {
        const vec3& tMin = modelMin[i];
        const vec3& tMax = modelMax[i];

        root.growToInclude(tMin, tMax);
    }

    root.setStartIdx(0);
    root.setEndIdx(numModels-1);

    BVHmodelNodes.emplace_back(root);

    split(BVH_TESTS_PER_AXIS, 0, BVH_MAX_DEPTH-1);
}

float Scene::evaluateSplit(const int start, const int end, const int axis, const float pos) const {
    BVHnode nodeA;
    BVHnode nodeB;

    int numA = 0;
    int numB = 0;

    for (int i = start; i < end; ++i) {
        const vec3& tMin = modelMin[i];
        const vec3& tMax = modelMax[i];

        vec3 center = modelCenter[i];

        if (center[axis] < pos) {
            nodeA.growToInclude(tMin, tMax);
            numA++;
        } else {
            nodeB.growToInclude(tMin, tMax);
            numB++;
        }
    }

    nodeA.setStartIdx(0);
    nodeA.setEndIdx(numA);
    nodeB.setStartIdx(0);
    nodeB.setEndIdx(numB);

    return nodeCost(nodeA) + nodeCost(nodeB);
}

Split Scene::chooseSplit(const int numTestsPerAxis, const BVHnode &node) {

    int start = node.getStartIdx();
    int end = node.getEndIdx();

    auto best = Split();
    std::mutex bestLock;

    for (int axis = 0; axis < 3; ++axis) {

        float centerMin = std::numeric_limits<float>::max();
        float centerMax = -std::numeric_limits<float>::max();

        for (int i = start; i < end; ++i) {
            float c = modelCenter[i][axis];
            centerMin = std::min(centerMin, c);
            centerMax = std::max(centerMax, c);
        }

        for (int i = 0; i < numTestsPerAxis; ++i) {
            float splitT = float(i+1) / float(numTestsPerAxis+1);
            float pos = centerMin + (centerMax - centerMin) * splitT;

            if (end-start > 200) {

                pool.enqueue([this, &best, start, end, &bestLock](int a, float p) {
                  float cost = evaluateSplit(start, end, a, p);

                  std::lock_guard lock(bestLock);
                  if (cost < best.cost) {
                    best.cost = cost;
                    best.pos  = p;
                    best.axis = a;
                  }
                }, axis, pos);


            } else {
                float cost = evaluateSplit(start, end, axis, pos);

                if (cost < best.cost) {
                    best.pos = pos;
                    best.cost = cost;
                    best.axis = axis;
                }
            }
        }
    }

    pool.wait_for_tasks();

    return best;

}

void Scene::split(int numTestsPerAxis, int BVHindex, int depth) {

    BVHnode& node = BVHmodelNodes[BVHindex];
    int start = node.getStartIdx();
    int end = node.getEndIdx();

    if (depth <= 1) {
        if (end-start > 10) {
            std::cerr << "Hit depth limit with " << end-start << " models" << std::endl;
        }
        return;
    }

    BVHnode childA;
    BVHnode childB;

    int numA = 0, numB = 0;
    int startA = start, startB = start;

    Split bestSplit = chooseSplit(numTestsPerAxis, node);
    if (bestSplit.cost >= nodeCost(node)) {return;}

    if (bestSplit.axis == -1) {
        vec3 min = node.getMin();
        vec3 max = node.getMax();
        vec3 size = max - min;

        if (size.x > size.y && size.x > size.z) {
            bestSplit.axis = 0;
        } else if (size.y > size.z && size.y > size.x) {
            bestSplit.axis = 1;
        } else {
            bestSplit.axis = 2;
        }

        bestSplit.pos = (min[bestSplit.axis]+max[bestSplit.axis])/2;

        std::cerr << "Fallback to center split" << std::endl;
    }
    //std::cout << depth << ' ' << splitAxix << ' ' << splitPos << std::endl;

    for (int i = start; i < end; i++) {
        const vec3& tMin = modelMin[i];
        const vec3& tMax = modelMax[i];

        vec3 center = modelCenter[i];
        bool modelInA = center[bestSplit.axis] < bestSplit.pos;
        if (modelInA) {
            childA.growToInclude(tMin, tMax);
            numA++;
            startB++;
            int swap = startA + numA - 1;
            std::swap(modelOffsets[i], modelOffsets[swap]);
            std::swap(modelCenter[i], modelCenter[swap]);
            std::swap(modelMin[i], modelMin[swap]);
            std::swap(modelMax[i], modelMax[swap]);
        } else {
            childB.growToInclude(tMin, tMax);
            numB++;
        }
    }

    //std::cout << depth << std::endl;
    //std::cout << "  " << numA << ' ' << numB << ' ' << splitAxis << ' ' << splitPos << std::endl;
    //std::cout << "  " << minA.x << ' ' << minA.y << ' ' << minA.z << std::endl;
    //std::cout << "  " << maxA.x << ' ' << maxA.y << ' ' << maxA.z << std::endl;
    //std::cout << "  " << minB.x << ' ' << minB.y << ' ' << minB.z << std::endl;
    //std::cout << "  " << maxB.x << ' ' << maxB.y << ' ' << maxB.z << std::endl;

    if (numA > 0 and numB > 0) {
        childA.setStartIdx(startA);
        childA.setEndIdx(startA+numA);
        childB.setStartIdx(startB);
        childB.setEndIdx(startB+numB);

        int indexA = int(BVHmodelNodes.size());
        int indexB = indexA + 1;

        node.setChildA(indexA);
        node.setChildB(indexB);

        BVHmodelNodes.emplace_back(childA);
        BVHmodelNodes.emplace_back(childB);

        split(numTestsPerAxis, indexA, depth-1);
        split(numTestsPerAxis, indexB, depth-1);

    } else if (numA > 10) {
        std::cerr << "Big Leaf Node  " << numA << " tri" << std::endl;
    } else if (numB > 10) {
        std::cerr << "Big Leaf Node  " << numB << " tri" << std::endl;
    }
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
    uNumModels          = raytracer.createUniform<int>("numModels");

    uNEE                = raytracer.createUniform<bool>("NEE");
    uNumEmissiveModels  = raytracer.createUniform<int>("numEmissiveModels");
    uNumEmissiveTris    = raytracer.createUniform<int>("numEmissiveTris");

    uCamera     = raytracer.createUniformBlock<Camera>("camera");

    uResolutionRTX  = raytracer.createUniform<uvec2>("resolution");
    uFrameCount     = raytracer.createUniform<int>("frameCount");
    uTimeSinceStart = raytracer.createUniform<float>("timeSinceStart");
    uSampleCount    = raytracer.createUniform<int>("sampleCount");

    uSamples   = raytracer.createUniform<int>("samples");
    uAA        = raytracer.createUniform<int>("aa");
    uBounceLim = raytracer.createUniform<int>("bounceLim");

    uSkyActive = raytracer.createUniform<bool>("skyActive");
    uSunDir    = raytracer.createUniform<vec3>("sunDir");
    uSunColor  = raytracer.createUniform<vec3>("sunColor");

    uFloorActive   = raytracer.createUniform<bool>("floorActive");
    uFloorMaterial = raytracer.createUniformBlock<Material>("floorMaterial");

    uDebugView     = raytracer.createUniformBlock<DebugView>("debugView");

    uTextureScales = raytracer.createUniform<float>("textureScales");

    uEnvYaw = raytracer.createUniform<float>("uEnvYaw");
}

void Scene::setUniformsRTX() const {
    raytracer.bind();

    uNumModels.set(int(models.size()));
    uNEE.set(NEE);
    uNumEmissiveModels.set(int(emissiveModelTriangleNum.size()));
    uNumEmissiveTris.set(int(emissiveTris.size()));

    uCamera.set(camera);

    uResolutionRTX.set(window.size());
    uFrameCount.set(frameCount);
    uTimeSinceStart.set(window.getTimeSinceStart());
    uSampleCount.set(sampleCount);

    uSamples.set(samples);
    uAA.set(aa);
    uBounceLim.set(bounceLim);

    uSkyActive.set(skyActive);
    uSunDir.set(sunDir);
    uSunColor.set(sunColor*sunStrength);

    uFloorActive.set(floorActive);
    uFloorMaterial.set(floorMaterial);

    uDebugView.set(debugView);

    uTextureScales.setArray(textureScales.data(), 64);

    uEnvYaw.set(0.0f);

    skyTexture.bind();

    for (const Texture &tex : textures) {
        tex.bind();
    }
}
void Scene::setUniformsPP() const {
    postProcessing.bind();
    uResolutionPP.set(window.size());
}

void Scene::set_ssbo() {

    lastSentPackage = dataSent();

    std::vector<Material> materials;
    std::vector<mat4> modelTransforms;
    std::vector<mat4> modelInvTransforms;
    for (const auto& model : models) {
        for (const Material& m : model.materials) materials.emplace_back(m);
        modelTransforms.emplace_back(model.transformation.matrix);
        modelInvTransforms.emplace_back(model.transformation.inverseMatrix);
    }

    ssboTriangles.set(triangles, 0);
    ssboVertices.set(vertices, 1);
    ssboTexCoords.set(texCoords, 2);
    ssboNormals.set(normals, 3);
    ssboMaterials.set(materials, 4);
    ssboBVHtriangleNodes.set(BVHtriangleNodes, 5);
    ssboBVHmodelNodes.set(BVHmodelNodes, 6);
    ssboModelOffsets.set(modelOffsets, 7);
    ssboModelTransformations.set(modelTransforms, 8);
    ssboModelInvTransformations.set(modelInvTransforms, 9);
    ssboEmissiveTris.set(emissiveTris, 10);
    ssboEmissiveModelTriangleNum.set(emissiveModelTriangleNum, 11);

}

bool Scene::inputHandling(float speed, float sensitivity, float dt) {
    if (window.keyPressed(GLFW_KEY_ESCAPE)) {
        if (!trackedKeysPressed[GLFW_KEY_ESCAPE]) {
            if (ui.viewportFullscreen) ui.viewportFullscreen = false;
            else isOpen = false;
        }

        trackedKeysPressed[GLFW_KEY_ESCAPE] = true;
    } else trackedKeysPressed[GLFW_KEY_ESCAPE] = false;

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
                delta *= (2.0f / float(window.size().y)) * sensitivity * camera.fovDeg;

                camera.forward += delta.x * camera.right + delta.y * camera.up;
                camera.forward = normalize(camera.forward);
                setBasisVectors(camera.forward, camera.up, camera.right);
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
    if (window.keyPressed(GLFW_KEY_V)){
        if (!trackedKeysPressed[GLFW_KEY_V]) {
            ui.promptSavePNG(this);
        }

        trackedKeysPressed[GLFW_KEY_V] = true;
    } else trackedKeysPressed[GLFW_KEY_V] = false;

    if (lock) return false;

    vec3 change = vec3(0, 0, 0);
    if (window.keyPressed(GLFW_KEY_W)) change += camera.forward;
    if (window.keyPressed(GLFW_KEY_S)) change -= camera.forward;
    if (window.keyPressed(GLFW_KEY_A)) change -= camera.right;
    if (window.keyPressed(GLFW_KEY_D)) change += camera.right;
    if (window.keyPressed(GLFW_KEY_E)) change += camera.up;
    if (window.keyPressed(GLFW_KEY_Q)) change -= camera.up;

    if (window.keyPressed(GLFW_KEY_LEFT_SHIFT) || window.keyPressed(GLFW_KEY_RIGHT_SHIFT)) speed *= 2;

    if (pow(change.x, 2) + pow(change.y, 2) + pow(change.z, 2) > 0) {
        change = normalize(change);
        camera.pos += change*speed*dt;
        moved = true;
    }
    return moved;
}

void Scene::updateFrame() {

    Timer t;
    window.start();

    if (!isBusy) reloadEmissiveTris();

    if (newData) {
        if (pendingClearTextures) {
            for (const Texture& tex : textures) {
                raytracer.releaseTexture(tex);
            }
            textures.clear();
            textureLabels.clear();
            pendingClearTextures = false;
        }
        if (!pendingTextures.empty()) {
            for (const auto& [path, id] : pendingTextures) {
                textures.emplace_back(raytracer.createTexture("textures[" + std::to_string(id) + "]", path));
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

        createTLAS();

        resetAccumulation();

        set_ssbo();
        newData = false;
    }


    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    float dt = window.getDeltaTime();
    updateItemSmooth(totalTime, dt);
    fpsData.emplace_back(1.0f/dt);

    if (inputHandling(speed, sensitivity, dt)) resetAccumulation();


    ui.render(*this);

    setUniformsRTX();
    setUniformsPP();

    frameCount++;
    sampleCount += samples;


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

    std::cout << "Triangles: "          << package.triangles         << " (" << bytesToReadable(package.triangleBytes)          << ")" << std::endl;
    std::cout << "Vertices: "           << package.vertices          << " (" << bytesToReadable(package.verticesBytes)          << ")" << std::endl;
    std::cout << "Texture Coords: "     << package.texCoords         << " (" << bytesToReadable(package.texCoordsBytes)         << ")" << std::endl;
    std::cout << "Normals: "            << package.normals           << " (" << bytesToReadable(package.normalsBytes)           << ")" << std::endl;
    std::cout << "BVH Triangle Nodes: " << package.BVHtriangleNodes  << " (" << bytesToReadable(package.BVHtriangleNodesBytes)  << ")" << std::endl;
    std::cout << "BVH Model Nodes: "    << package.BVHmodelNodes     << " (" << bytesToReadable(package.BVHmodelNodesBytes)     << ")" << std::endl;
    std::cout                                                                                                                                  << std::endl;
    std::cout << "Models: "             << package.models            << " (" << bytesToReadable(package.modelsBytes)            << ")" << std::endl;
    std::cout << "Emissive Models"      << package.emissiveModels    << " (" << bytesToReadable(package.emissiveModelsBytes)    << ")" << std::endl;
    std::cout << "Emissive Triangles"   << package.emissiveTriangles << " (" << bytesToReadable(package.emissiveTrianglesBytes) << ")" << std::endl;
    std::cout << "Materials: "          << package.materials         << " (" << bytesToReadable(package.materialsBytes)         << ")" << std::endl;
    std::cout << "Textures: "           << package.textures          << " (" << bytesToReadable(package.texturesBytes)          << ")" << std::endl;
    std::cout << "Transformations: "    << package.transforms        << " (" << bytesToReadable(package.transformsBytes)        << ")" << std::endl;
    std::cout << "Total Data Sent: "    << bytesToReadable(package.totalSize) << std::endl;
    std::cout << std::endl;
}

DataPackage Scene::dataSent() const {
    DataPackage result{};

    for (const Model& model : models) {
        result.triangles        += int(model.base.triangles.size())/3;
        result.vertices         += int(model.base.vertices.size());
        result.texCoords        += int(model.base.texCoords.size());
        result.normals          += int(model.base.normals.size());
        result.BVHtriangleNodes += int(model.base.BVHnodes.size());
        result.models           ++;
        result.materials        += int(model.materials.size());
        result.transforms       ++;
    }
    result.BVHmodelNodes     = int(BVHmodelNodes.size());
    result.emissiveModels    = int(emissiveModelTriangleNum.size());
    result.emissiveTriangles = int(emissiveTris.size());
    result.textures          = int(textures.size());

    result.trianglesSent        = int(triangles.size())/3;
    result.verticesSent         = int(vertices.size());
    result.texCoordsSent        = int(texCoords.size());
    result.normalsSent          = int(normals.size());
    result.BVHtriangleNodesSent = int(BVHtriangleNodes.size());

    result.triangleBytes          = result.trianglesSent        * int(sizeof(ivec4)) * 3;
    result.verticesBytes          = result.verticesSent         * int(sizeof(vec4));
    result.texCoordsBytes         = result.texCoordsSent        * int(sizeof(vec2));
    result.normalsBytes           = result.normalsSent          * int(sizeof(vec4));
    result.BVHtriangleNodesBytes  = result.BVHtriangleNodesSent * int(sizeof(BVHnode));
    result.BVHmodelNodesBytes     = result.BVHmodelNodes        * int(sizeof(BVHnode));

    result.modelsBytes            = result.models               * int(sizeof(ModelOffset));
    result.emissiveModelsBytes    = result.emissiveModels       * int(sizeof(ivec2));
    result.emissiveTrianglesBytes = result.emissiveTriangles    * int(sizeof(ivec2));
    result.materialsBytes         = result.materials            * int(sizeof(Material));
    result.transformsBytes        = result.transforms           * int(sizeof(Transformation));

    result.texturesBytes = 0;
    for (const Texture& t : textures) {
        result.texturesBytes += int(t.gpuSizeBytes());
    }

    result.totalSize =
        result.triangleBytes +
        result.verticesBytes +
        result.texCoordsBytes +
        result.normalsBytes +
        result.BVHtriangleNodesBytes +
        result.BVHmodelNodesBytes +
        result.modelsBytes +
        result.emissiveModelsBytes +
        result.emissiveTrianglesBytes +
        result.materialsBytes +
        result.transformsBytes +
        result.texturesBytes;

    return result;
}


void Scene::saveJSON(const std::string& filename) const {
    json j;

    j["Camera"]["position"] = camera.pos;
    j["Camera"]["forward"] = camera.forward;
    j["Camera"]["fovDeg"] = camera.fovDeg;
    j["Camera"]["aperture"] = camera.aperture;
    j["Camera"]["focusDistance"] = camera.focusDistance;
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
    j["Floor"]["material"] = floorMaterial;

    j["Models"] = json::array();
    for (int i = 0; i < models.size(); ++i) {
        Model m = models[i];
        json jm;
        jm["Filename"] = m.filename;
        jm["Transformation"] = m.transformation;

        jm["Materials"] = json::array();
        for (const Material& mat : m.materials) {
            jm["Materials"].push_back(mat);
        }

        jm["TextureID"] = modelOffsets[i].textureID;

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
    BVHtriangleNodes.clear();
    models.clear();
    modelOffsets.clear();

    pendingClearTextures = true;
    textureScales.fill(0.0f);

    emissiveTris.clear();
    emissiveModelTriangleNum.clear();

    // precomputedModels.clear();

    // --- Camera ---
    if (j.contains("Camera")) {
        const auto& c = j["Camera"];
        if (c.contains("position"))      camera.pos = c["position"];
        if (c.contains("forward"))       camera.forward = c["forward"];
        if (c.contains("fovDeg"))        camera.fovDeg = c["fovDeg"];
        if (c.contains("aperture"))      camera.aperture = c["aperture"];
        if (c.contains("focusDistance")) camera.focusDistance = c["focusDistance"];
        if (c.contains("sensitivity"))   sensitivity = c["sensitivity"];
        if (c.contains("speed"))         speed = c["speed"];

    }
    setBasisVectors(camera.forward, camera.up, camera.right);

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
        if (fl.contains("active"))   floorActive = fl["active"].get<bool>();
        if (fl.contains("material")) floorMaterial = fl["material"].get<Material>();
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
        int numModels = int(j["Models"].size());
        progressMax = float(numModels);
        progress = 0.0f;
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
                for (auto mat : mats) {
                    models.back().materials.push_back(mat);
                    if (mat.contains("Name")) models.back().materialNames.back() = mat["Name"].get<std::string>();
                }
            }
            progress = progress + 1.0f;
        }
    }

    resetAccumulation();
}

void Scene::startAddJob(const std::string& path, const std::string& texturePath) {
    progress = 0.4f;
    progressMax = 1.0f;
    isBusy = true;
    resetAccumulation();
    busyLabel = "Adding Model...";

    job = std::async(std::launch::async, [this, path, texturePath]() {
        try {
            addModel(path, Transformation(vec3(0), vec3(-1)), texturePath);
            statusIsError = false;
            statusMsg = "Added: " + path;
            newData = true;

            progress = 1.0f;
        } catch (const std::exception& e) {
            statusIsError = true;
            statusMsg = std::string("Add failed: ") + e.what();
        } catch (...) {
            statusIsError = true;
            statusMsg = "Add failed: unknown error";
        }
        isBusy = false;
    });
}

void Scene::startLoadJob(const std::string& path){
    isBusy = true;
    resetAccumulation();
    busyLabel = "Loading JSON...";

    job = std::async(std::launch::async, [path, this]() {
        try {
            loadJSON(path);
            statusIsError = false;
            statusMsg = "Loaded: " + path;
            newData = true;
        } catch (const std::exception& e) {
            statusIsError = true;
            statusMsg = std::string("Load failed: ") + e.what();
        } catch (...) {
            statusIsError = true;
            statusMsg = "Load failed: unknown error";
        }
        isBusy = false;
    });
}

void Scene::reloadEmissiveTris() {
    if (!emissiveTrisStale) return;

    emissiveTris.clear();
    emissiveModelTriangleNum.clear();
    for (int m = 0; m < models.size(); ++m) {
        Model model = models[m];
        int start = int(emissiveTris.size());
        int count = 0;
        for (int i = 0; i < model.base.triangles.size()/3; i++) {
            Material mat = model.materials[int(model.base.triangles[i*3].w)];
            if (mat.getType() == Emissive) {
                emissiveTris.emplace_back(i, m);
                count++;
            }
        }
        if (count > 0) emissiveModelTriangleNum.emplace_back(start, count);
    }
    emissiveTrisStale = false;
    newData = true;
}

void Scene::savePNG(const std::string& filename) {
    int width  = int(window.size().x);
    int height = int(window.size().y);
    int n = width * height * 3;

    std::vector<float> beauty(width * height * 4);
    std::vector<float> albedo(width * height * 4);
    std::vector<float> normal(width * height * 4);

    auto readTex = [&](GLuint tex, std::vector<float>& buf) {
        glBindTexture(GL_TEXTURE_2D, tex);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, buf.data());
    };

    readTex(raytracer.outputTexture(),  beauty);
    readTex(raytracer.getAOVTexture(0), albedo);  // albedo
    readTex(raytracer.getAOVTexture(1), normal);  // world normals [0,1]


    std::vector<float> beautyRGB(n), albedoRGB(n), normalRGB(n), outputRGB(n);

    for (int i = 0; i < width * height; i++) {
        beautyRGB[i*3+0] = beauty[i*4+0];
        beautyRGB[i*3+1] = beauty[i*4+1];
        beautyRGB[i*3+2] = beauty[i*4+2];

        albedoRGB[i*3+0] = albedo[i*4+0];
        albedoRGB[i*3+1] = albedo[i*4+1];
        albedoRGB[i*3+2] = albedo[i*4+2];

        // remap normals back from [0,1] to [-1,1] for OIDN
        normalRGB[i*3+0] = normal[i*4+0] * 2.0f - 1.0f;
        normalRGB[i*3+1] = normal[i*4+1] * 2.0f - 1.0f;
        normalRGB[i*3+2] = normal[i*4+2] * 2.0f - 1.0f;
    }


    oidn::DeviceRef device = oidn::newDevice();
    device.commit();

    // Allocate buffers through OIDN's device instead of std::vector
    oidn::BufferRef beautyBuf = device.newBuffer(n * sizeof(float));
    oidn::BufferRef albedoBuf = device.newBuffer(n * sizeof(float));
    oidn::BufferRef normalBuf = device.newBuffer(n * sizeof(float));
    oidn::BufferRef outputBuf = device.newBuffer(n * sizeof(float));

    // Copy packed RGB data into OIDN buffers
    memcpy(beautyBuf.getData(), beautyRGB.data(), n * sizeof(float));
    memcpy(albedoBuf.getData(), albedoRGB.data(), n * sizeof(float));
    memcpy(normalBuf.getData(), normalRGB.data(), n * sizeof(float));

    oidn::FilterRef filter = device.newFilter("RT");
    filter.setImage("color",  beautyBuf, oidn::Format::Float3, width, height);
    filter.setImage("albedo", albedoBuf, oidn::Format::Float3, width, height);
    filter.setImage("normal", normalBuf, oidn::Format::Float3, width, height);
    filter.setImage("output", outputBuf, oidn::Format::Float3, width, height);
    filter.set("hdr", true);
    filter.set("cleanAux", true);
    filter.commit();
    filter.execute();

    const char* err;
    if (device.getError(err) != oidn::Error::None) std::cerr << "OIDN error: " << err << "\n";

    // Read output back from OIDN buffer
    auto* denoised = (float*)outputBuf.getData();

    std::vector<float> denoisedRGBA(width * height * 4);
    for (int i = 0; i < width * height; i++) {
        denoisedRGBA[i*4+0] = denoised[i*3+0];
        denoisedRGBA[i*4+1] = denoised[i*3+1];
        denoisedRGBA[i*4+2] = denoised[i*3+2];
        denoisedRGBA[i*4+3] = 1.0f;
    }

    GLuint denoisedTex;
    glGenTextures(1, &denoisedTex);
    glBindTexture(GL_TEXTURE_2D, denoisedTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, denoisedRGBA.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    bloom.execute(denoisedTex, false);
    postProcessing.execute(bloom.outputTexture(), false);
    glDeleteTextures(1, &denoisedTex);

    std::vector<float> output(width * height * 4);
    glBindTexture(GL_TEXTURE_2D, postProcessing.outputTexture());
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, output.data());


    std::vector<unsigned char> ldr(width * height * 4);
    for (int i = 0; i < width * height * 4; i++) {
        ldr[i] = static_cast<unsigned char>(std::clamp(output[i], 0.0f, 1.0f) * 255.0f);
    }

    stbi_flip_vertically_on_write(true);
    stbi_write_png(filename.c_str(), width, height, 4, ldr.data(), width * 4);
}
