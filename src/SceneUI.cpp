//
// Created by acroy on 2/17/2026.
//

#include "Scene.h"
#include "SceneUI.h"

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

struct MaterialConstraints {
    bool allowTransparent = true;
    bool allowEmissive    = true;
    bool allowTexture     = true;
};

static const MaterialConstraints FLOOR_CONSTRAINTS = {
    .allowTransparent = false,
    .allowEmissive    = false,
    .allowTexture     = false,
};

static const char* MaterialTypeLabel(MaterialType t) {
    switch (t) {
        case Specular:    return "Specular";
        case Transparent: return "Transparent";
        case Emissive:    return "Emissive";
        default:          return "Unknown";
    }
}

static bool DrawMaterialInspector(Material& m, int matIndex, int textureID, bool& typeChange, const MaterialConstraints& constraints = {}) {
    bool changed = false;

    // Pull values once (edit locals, then write back at end)
    MaterialType type            = m.getType();
    vec3  diffuseColor           = m.getDiffuseColor();
    float diffuseRoughness       = m.getDiffuseRoughness();

    vec3  specularColor          = m.getSpecularColor();
    float specularRoughness      = m.getSpecularRoughness();
    float specularProbability    = m.getSpecularProbability();

    float transparency           = m.getTransparency();
    float ior                    = m.getIndexOfRefraction();
    float absorption             = m.getAbsorption();

    float emissionStrength       = m.getEmissionStrength();

    // Coerce disallowed types to Specular
    if (!constraints.allowTransparent && type == Transparent) { type = Specular; changed = true; typeChange = true; }
    if (!constraints.allowEmissive    && type == Emissive)    { type = Specular; changed = true; typeChange = true; }

    // --- Header ---
    if (matIndex != -1) ImGui::Text("Material #%d", matIndex);
    if (matIndex != -1) ImGui::SameLine();
    if (matIndex != -1) ImGui::TextDisabled("(%s)", MaterialTypeLabel(type));
    if (matIndex != -1) ImGui::Separator();

    // --- Build filtered type list ---
    std::vector<const char*>  items;
    std::vector<MaterialType> itemTypes;

    items.push_back("Specular"); itemTypes.push_back(Specular);
    if (constraints.allowTransparent) { items.push_back("Transparent"); itemTypes.push_back(Transparent); }
    if (constraints.allowEmissive)    { items.push_back("Emissive");    itemTypes.push_back(Emissive); }

    // Find current index in filtered list
    int t = 0;
    for (int i = 0; i < (int)itemTypes.size(); i++)
        if (itemTypes[i] == type) { t = i; break; }

    if (ImGui::Combo("Type", &t, items.data(), (int)items.size())) {
        type = itemTypes[t];
        changed = true;
        typeChange = true;

        // Coerce defaults when switching to reduce “broken” states
        if (type == Emissive) {
            if (emissionStrength <= 0.0f) emissionStrength = 1.0f;
            transparency = 0.0f;
        } else if (type == Transparent) {
            if (transparency <= 0.0f) transparency = 1.0f;
            if (ior <= 1.0f) ior = 1.3f;
            if (specularProbability <= 0.0f) specularProbability = 1.0f; // glass usually specular
            emissionStrength = 0.0f;
        } else if (type == Specular) {
            if (specularProbability < 0.05f) specularProbability = 1.0f;
            transparency = 0.0f;
            emissionStrength = 0.0f;
        } else { // Opaque
            transparency = 0.0f;
            absorption = 0.0f;
            ior = 1.0f;
            emissionStrength = 0.0f;
        }
    }

    ImGui::Spacing();

    // --- Base color / texture ---
    if (constraints.allowTexture && textureID != -1) {
        ImGui::TextDisabled("Texture bound (ID %d).", textureID);
    } else {
        if (type == Emissive) {
            changed |= ImGui::ColorEdit3("Emission Color", &diffuseColor.x);
        } else if (type == Transparent) {
            changed |= ImGui::ColorEdit3("Absorb Color", &diffuseColor.x);
        } else {
            changed |= ImGui::ColorEdit3("Base Color", &diffuseColor.x);
        }
    }

    // --- Type-specific controls ---
    if (type == Emissive) {
        changed |= ImGui::SliderFloat("Emission Strength", &emissionStrength, 0.0f, 250.0f);

    } else if (type == Transparent) {
        changed |= ImGui::SliderFloat("Transparency", &transparency, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Index of Refraction", &ior, 1.0f, 3.0f);

        changed |= ImGui::SliderFloat("Roughness", &diffuseRoughness, 0.0f, 1.0f);

        // Specular for dielectric surface
        changed |= ImGui::ColorEdit3("Specular Color", &specularColor.x);
        changed |= ImGui::SliderFloat("Specular Roughness", &specularRoughness, 0.0f, 1.0f);

        changed |= ImGui::SliderFloat("Absorption", &absorption, 0.0f, 0.1f);

    } else if (type == Specular) {
        ImGui::SeparatorText("Surface");

        changed |= ImGui::SliderFloat("Diffuse Roughness", &diffuseRoughness, 0.0f, 1.0f);

        changed |= ImGui::ColorEdit3("Specular Color", &specularColor.x);
        changed |= ImGui::SliderFloat("Specular Roughness", &specularRoughness, 0.0f, 1.0f);

        changed |= ImGui::SliderFloat("Specular Probability", &specularProbability, 0.0f, 1.0f);
    }

    if (changed) {
        m.setType(type);

        m.setDiffuseColor(diffuseColor);
        m.setDiffuseRoughness(diffuseRoughness);

        m.setSpecularColor(type == Specular || type == Transparent ? specularColor : vec3(0));
        m.setSpecularRoughness(type == Specular || type == Transparent ? specularRoughness : 0.0f);
        m.setSpecularProbability(type == Specular ? specularProbability : 0.0f);

        m.setTransparency(type == Transparent ? transparency : 0.0f);
        m.setIndexOfRefraction(type == Transparent ? ior : 1.0f);
        m.setAbsorption(type == Transparent ? absorption : 0.0f);

        m.setEmissionStrength(type == Emissive ? emissionStrength : 0.0f);
    }

    return changed;
}

static void DrawBusyOverlay(const char* label, float progress){
    ImGui::OpenPopup("Working...");
    if (ImGui::BeginPopupModal("Working...", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::TextUnformatted(label);
        ImGui::ProgressBar(progress, ImVec2(250, 0));
        ImGui::TextDisabled("Please wait...");
        ImGui::EndPopup();
    }
}

void SceneUI::render(Scene& scene) {
    bool ui_resetAccum = false;

    ImGui::DockSpaceOverViewport(
        0,
        ImGui::GetMainViewport(),
        ImGuiDockNodeFlags_PassthruCentralNode
    );

    if (browserMode == BrowserMode::AddTexture && !fileBrowser.open && !showTexturePrompt) {
        showTexturePrompt = true;
        ImGui::OpenPopup("Texture");
    }

    if (ImGui::BeginPopupModal("Texture", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Add a texture to this model?");
        ImGui::Separator();

        if (ImGui::Button("Yes")) {
            showTexturePrompt = false;
            ImGui::CloseCurrentPopup();
            Scene* s = &scene;
            fileBrowser.openAt(PROJECT_DIR "assets/textures", ".png", [this, s](const std::string& texPath) {
                s->startAddJob(pendingModelPath, std::filesystem::relative(texPath, PROJECT_DIR).string());
                browserMode = BrowserMode::None;
            });
        }
        ImGui::SameLine();
        if (ImGui::Button("No")) {
            scene.startAddJob(pendingModelPath, "");
            showTexturePrompt = false;
            browserMode = BrowserMode::None;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    renderViewport(scene);

    // scene
    {
        ImGui::Begin("Scene");

        if (ImGui::Button("Reset accumulation")) ui_resetAccum = true;

        if (ImGui::CollapsingHeader("Camera")) {
            ui_resetAccum |= ImGui::SliderFloat("FOV", &scene.camera.fovDeg, 20, 140);
            bool dof = scene.camera.aperture > 0;
            if (ImGui::Checkbox("Depth of Field", &dof)) {
                if (dof) scene.camera.aperture = 0.001;
                else scene.camera.aperture = 0.0;
                ui_resetAccum = true;
            }

            if (dof) {
                ImGui::Indent();
                ui_resetAccum |= ImGui::SliderFloat("Aperture", &scene.camera.aperture, 0.001f, 0.5f);
                ui_resetAccum |= ImGui::SliderFloat("Focus Distance", &scene.camera.focusDistance, 0.0f, 1000.0f);
                ui_resetAccum |= ImGui::Checkbox("Focus Distance Plane", &scene.camera.focusDistancePlane);
                ImGui::Unindent();
            }

            float s = scene.sensitivity*100;
            if (ImGui::SliderFloat("Sensitivity", &s, 0.1, 10)) scene.sensitivity = s/100;

            ImGui::SliderFloat("Speed", &scene.speed, 0.01, 1000);
        }

        if (ImGui::CollapsingHeader("Sky")) {
            ui_resetAccum |= ImGui::Checkbox("Sky Active", &scene.skyActive);
            if (scene.skyActive) {
                ImGui::Indent();
                ui_resetAccum |= ColorEdit3("Sun Color", scene.sunColor);
                ui_resetAccum |= DragFloat3("Sun Direction", scene.sunDir);
                scene.sunDir = normalize(scene.sunDir);

                ui_resetAccum |= ImGui::SliderFloat("Sun Strength", &scene.sunStrength, 0, 500);
                ImGui::Unindent();
            }
        }

        if (ImGui::CollapsingHeader("Floor")) {
            ui_resetAccum |= ImGui::Checkbox("Floor Active", &scene.floorActive);
            if (scene.floorActive) {
                ImGui::Indent();

                DrawMaterialInspector(scene.floorMaterial, -1, -1, ui_resetAccum, FLOOR_CONSTRAINTS);

                ImGui::Unindent();
            }
        }

        if (ImGui::CollapsingHeader("Bloom")) {
            ImGui::Checkbox("Bloom Active", &scene.bloom.enabled);
            if (scene.bloom.enabled) {

                ImGui::Indent();
                ImGui::SliderFloat("Bloom Threshold", &scene.bloom.threshold, 0.5f, 2.0f);
                ImGui::SliderFloat("Bloom Knee", &scene.bloom.knee, 0.0f, 1.0f);
                ImGui::SliderFloat("Bloom Strength", &scene.bloom.strength, 0.0001f, 0.1f);
                ImGui::SliderInt("Bloom Num Mips", &scene.bloom.numMips, 1, 16);
                ImGui::SliderFloat("Bloom Persistence", &scene.bloom.persistence, 0.0f, 1.0f);
                ImGui::Unindent();
            }
        }

        ImGui::Separator();
        ImGui::Text("Models");

        ImGui::Indent();

        const bool hasModels = !scene.models.empty();
        if (!hasModels) {
            ImGui::TextDisabled("(no models)");
        } else if (scene.isBusy) {
            ImGui::TextDisabled("Loading...");
        } else {

            for (int i = 0; i < scene.models.size(); i++) {
                bool sel = (selectedModel == i);
                if (ImGui::Selectable(scene.models[i].name.c_str(), sel)) {
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

        // total height of the bottom block: 4 buttons + 3 gaps
        float blockHeight = 4.0f * buttonHeight + 3.0f * spacing;

        float availY = ImGui::GetContentRegionAvail().y;

        // Move cursor down so the next items land at the bottom
        float y = ImGui::GetCursorPosY() + (availY - blockHeight);
        if (y > ImGui::GetCursorPosY())  // avoid moving up if not enough room
            ImGui::SetCursorPosY(y);

        if (ImGui::Button("Add Model", ImVec2(-FLT_MIN, buttonHeight))) {
            browserMode = BrowserMode::AddModel;
            fileBrowser.openAt(PROJECT_DIR "assets/models", ".obj", [this](const std::string& path) {
                pendingModelPath = std::filesystem::relative(path, PROJECT_DIR).string();
                browserMode = BrowserMode::AddTexture;
            });
        }

        if (ImGui::Button("Remove Model", ImVec2(-FLT_MIN, buttonHeight))) {
            ImGui::OpenPopup("RemoveModelPopup");
        }

        if (ImGui::Button("Load JSON", ImVec2(-FLT_MIN, buttonHeight))) {
            browserMode = BrowserMode::LoadJSON;
            Scene* s = &scene;
            fileBrowser.openAt(PROJECT_DIR "scenes", ".json", [this, s](const std::string& path) {
                s->startLoadJob(std::filesystem::relative(path, PROJECT_DIR).string());
                selectedModel = -1;
                selectedColor = 0;
                browserMode = BrowserMode::None;
            });
        }

        if (ImGui::Button("Save JSON", ImVec2(-FLT_MIN, buttonHeight))) {
            browserMode = BrowserMode::SaveJSON;
            Scene* s = &scene;
            fileBrowser.openAt(PROJECT_DIR "scenes", ".json", [this, s](const std::string& path) {
                s->saveJSON(std::filesystem::relative(path, PROJECT_DIR).string());
                browserMode = BrowserMode::None;
            });
        }


        if (ImGui::BeginPopupModal("RemoveModelPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure?");
            ImGui::Separator();

            if (ImGui::Button("Yes")) {

                ImGui::CloseCurrentPopup();

                scene.removeModel(selectedModel);
                selectedModel = -1;
                selectedColor = 0;
            }

            ImGui::SameLine();

            if (ImGui::Button("No")) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::End();
    }

    // settings
    {
        ImGui::Begin("Settings");

        ImGui::Checkbox("Lock", &scene.lock);

        ImGui::SliderInt("Samples", &scene.samples, 1, 25);
        ui_resetAccum |= ImGui::SliderInt("Antialiasing", &scene.aa, 1, 5);
        ui_resetAccum |= ImGui::SliderInt("Bounces", &scene.bounceLim, 1, 16);

        ImGui::Separator();

        if (ImGui::Button("Reload Shaders")) {
            scene.reloadShaders();
            ui_resetAccum = true;
        }

        if (ImGui::Button("Save PNG")) {
            browserMode = BrowserMode::SavePNG;
            Scene* s = &scene;
            fileBrowser.openAt(PROJECT_DIR "renders", ".png", [this, s](const std::string& path) {
                s->savePNG(path);
                browserMode = BrowserMode::None;
            });
        }

        ui_resetAccum |= ImGui::Checkbox("Next Event Estimation (NEE)", &scene.NEE);

        ui_resetAccum |= ImGui::Checkbox("Debug Mode" , &scene.debugView.enable);

        ImGui::End();
    }

    // fps stats
    {
        ImGui::Begin("FPS Stats");

        ImGui::Text("Samples: %d", scene.sampleCount);
        ImGui::Text("Frame: %d", scene.frameCount);

        ImGui::Separator();

        char overlay[128];

        const float frameMs = scene.totalTime * 1000.0f;
        const float cpuMs   = scene.cpuTime   * 1000.0f;
        const float gpuMs   = scene.gpuTime   * 1000.0f;
        const float fps     = (scene.totalTime > 0.0f) ? (1.0f / scene.totalTime) : 0.0f;


        sprintf(overlay,"FPS: %.2f (%.2f ms)  CPU: %.2f ms  GPU: %.2f ms", fps, frameMs, cpuMs, gpuMs);

        float maxVal = (*std::max_element(scene.fpsData.begin(), scene.fpsData.end())) * 1.2f;

        if (maxVal < 10.0f) maxVal = 10.0f;

        auto DequeGetter = [](void* data, int idx){
            auto* d = static_cast<std::deque<float> *>(data);
            return (*d)[idx];
        };

        int width = int(ImGui::GetContentRegionAvail().x);

        ImGui::PlotLines(
            "##FPS",
            DequeGetter,
            &scene.fpsData,
            int(scene.fpsData.size()),
            0,
            overlay,
            0.0f,
            maxVal,
            ImVec2(float(width), 100)
        );

        if (ImGui::Button("Reset")) {
            scene.fpsData.clear();
        }
        int maxNum = width/2;
        if (int(scene.fpsData.size()) > maxNum) {
            scene.fpsData.pop_front();
        }

        ImGui::End();
    }

    // scene stats
    {
        ImGui::Begin("Scene Stats");

        ImGui::Text("Width: %d", scene.window.size().x);
        ImGui::Text("Height: %d", scene.window.size().y);

        ImGui::Separator();

        ImGui::Text("Position: %.2f, %.2f, %.2f", scene.camera.pos.x, scene.camera.pos.y, scene.camera.pos.z);
        ImGui::Text("Forward: %.2f, %.2f, %.2f", scene.camera.forward.x, scene.camera.forward.y, scene.camera.forward.z);

        ImGui::Separator();

        ImGui::Text("Pixels: %d", scene.window.size().x * scene.window.size().y);

        ImGui::End();
    }

    // debug settings
    if (scene.debugView.enable) {
        ImGui::Begin("Debug Mode");

        const char* names[] = { "Normals", "Heatmap", "Depth" };

        // Use a custom getter function
        int debugMode = scene.debugView.mode;
        if (ImGui::SliderInt("Debug Mode", &debugMode, 0, 2, names[debugMode])) {
            scene.debugView.mode = static_cast<DebugMode>(debugMode);
            ui_resetAccum = true;
        }

        switch (debugMode) {
            case Normals:
                break;
            case Heatmap:
                ui_resetAccum |= ImGui::SliderInt("Triangle Threshold", &scene.debugView.triTh, 1, 50);
                ui_resetAccum |= ImGui::SliderInt("AABB Threshold", &scene.debugView.aabbTh, 1, 250);
                break;
            case Depth:
                ui_resetAccum |= ImGui::SliderFloat("Depth Scale", &scene.debugView.depthScale, 1, 5000);
            default: ;
        }

        ImGui::End();
    }

    // scene data
    {
        ImGui::Begin("Scene Data");

        DataPackage package = scene.lastSentPackage;

        if (ImGui::BeginTable("##StatsTable", 4,
            ImGuiTableFlags_SizingStretchProp |
            ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Label",  ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Total", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Size",   ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            auto Row = [&](const char* label, int total, int onGpu, const std::string& size)
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(label);

                ImGui::TableSetColumnIndex(1);
                if (onGpu < total) {
                    ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "%d", total);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("On GPU: %d  |  %.0f%% saved by instancing",
                            onGpu, 100.f * float(total - onGpu) / float(total));
                } else {
                    ImGui::Text("%d", total);
                }

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s", size.c_str());
            };

            Row("Triangles",          package.triangles,         package.trianglesSent,     bytesToReadable(package.triangleBytes)          );
            Row("Vertices",           package.vertices,          package.verticesSent,      bytesToReadable(package.verticesBytes)          );
            Row("Tex Coords",         package.texCoords,         package.texCoordsSent,     bytesToReadable(package.texCoordsBytes)         );
            Row("Normals",            package.normals,           package.normalsSent,       bytesToReadable(package.normalsBytes)           );
            Row("BVH Nodes",          package.BVHNodes,          package.BVHNodesSent,      bytesToReadable(package.BVHNodesBytes)          );
            Row("Models",             package.models,            package.models,            bytesToReadable(package.modelsBytes)            );
            Row("Emissive Models",    package.emissiveModels,    package.emissiveModels,    bytesToReadable(package.emissiveModelsBytes)    );
            Row("Emissive Triangles", package.emissiveTriangles, package.emissiveTriangles, bytesToReadable(package.emissiveTrianglesBytes) );
            Row("Materials",          package.materials,         package.materials,         bytesToReadable(package.materialsBytes)         );
            Row("Textures",           package.textures,          package.textures,          bytesToReadable(package.texturesBytes)          );
            Row("Transforms",         package.transforms,        package.transforms,        bytesToReadable(package.transformsBytes)        );

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(ImVec4(1,1,0.4f,1), "Total");

            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(ImVec4(1,1,0.4f,1),
                "%s", bytesToReadable(package.totalSize).c_str());

            ImGui::EndTable();
        }

        ImGui::End();
    }

    // inspector
    if (!scene.isBusy && selectedModel >= 0) {
        ImGui::Begin("Inspector");

        bool changedT = false;
        bool changedM = false;

        Model& model = scene.models[selectedModel];
        ModelOffset offsets = scene.modelOffsets[selectedModel];

        // Local aliases
        Transformation& transform = model.transformation;

        ImGui::Text("Transformations");

        // Rotation UI in degrees (convert to/from radians for nicer UX)
        vec3 rotDeg = degrees(transform.rotation);
        changedT |= DragFloat3("Position", transform.position, 3.0f);

        static bool uniformScale = transform.scale.x == transform.scale.y && transform.scale.x == transform.scale.z;

        if (uniformScale) {
            float s = transform.scale.x;
            if (ImGui::DragFloat("Scale", &s, 1.0f, 0.0f, 1e36f)) {
                transform.scale = vec3(s);
                changedT = true;
            }
        } else {
            changedT |= DragFloat3("Scale", transform.scale, 1.0f, 0.0f, 1e36f);
        }

        ImGui::SameLine();
        ImGui::Checkbox("Uniform Scale", &uniformScale);

        changedT |= DragFloat3("Rotation (deg)", rotDeg, 0.2f);

        ImGui::Separator();

        std::string previewC = model.materialNames[selectedColor];
        if (ImGui::BeginCombo("##Material", previewC.c_str())) {

            for (int i = 0; i < model.materials.size(); ++i) {
                bool sel = (selectedColor == i);
                previewC = model.materialNames[i];
                if (ImGui::Selectable(previewC.c_str(), sel)) {
                    selectedColor = i;
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Indent();
        Material& m = model.materials[selectedColor];

        bool typeChange = false;
        changedM |= DrawMaterialInspector(m, selectedColor, offsets.textureID, typeChange);
        if (typeChange) scene.emissiveTrisStale = true;

        ImGui::Unindent();

        //texture
        if (offsets.textureID != -1) {
            ImGui::Separator();
            ImGui::Text("Texture");
            ImGui::Text(scene.textureLabels[offsets.textureID].c_str());
            ImGui::Text("ID: %i", offsets.textureID);

            float s = scene.textureScales[offsets.textureID];
            bool wrapTexture = s > 0;;
            if (ImGui::Checkbox("Wrap Texture", &wrapTexture)) {
                if (wrapTexture) {
                    scene.textureScales[offsets.textureID] = 0.001;
                } else {
                    scene.textureScales[offsets.textureID] = 0;
                }
                ui_resetAccum = true;
            }

            if (wrapTexture) {
                if (ImGui::DragFloat("Texture Scale", &s, 0.005f, 0.0001f, 2.0f, "%.4f")) {
                    scene.textureScales[offsets.textureID] = s;
                    ui_resetAccum = true;
                }
            }
        }

        ImGui::Separator();

        if (ImGui::BeginTable("##Data", 2,
            ImGuiTableFlags_SizingStretchProp |
            ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthStretch);

            auto Row = [&](const char* label, int count)
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(label);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%d", count);
            };

            Row("Trianlges", int(model.base.triangles.size()));
            ImGui::TableNextRow();
            Row("Vertices", int(model.base.vertices.size()));
            ImGui::TableNextRow();
            Row("Tex Coords", int(model.base.texCoords.size()));
            ImGui::TableNextRow();
            Row("Normals", int(model.base.normals.size()));
            ImGui::TableNextRow();
            Row("BVH Nodes", int(model.base.BVHnodes.size()));
            ImGui::TableNextRow();
            Row("Materials", int(model.base.materials.size()));

            ImGui::EndTable();
        }

        if (changedT) {
            transform.rotation = radians(rotDeg);

            transform.setMatrix();

            scene.ssboModelTransformations.update(selectedModel, transform.matrix);
            scene.ssboModelInvTransformations.update(selectedModel, transform.inverseMatrix);

            ui_resetAccum = true;
        }

        if (changedM) {
            int start = scene.modelOffsets[selectedModel].material;
            int end = start + int(model.materials.size());
            scene.ssboMaterials.update(int(start), int(end), model.materials.data());
            ui_resetAccum = true;
        }

        ImGui::End();
    }

    drawFileBrowser();

    if (scene.isBusy && wasNotBusy) {
        progress = 0.0f;
    }

    progress = progress * smoothing + (1-smoothing)*(scene.progress/scene.progressMax);
    if (scene.isBusy) {
        wasNotBusy = false;
        DrawBusyOverlay(scene.busyLabel.c_str(), progress);
    } else if (progress < 0.99f) {
        progress = progress * 0.8f + 0.2f*(scene.progress/scene.progressMax);
        wasNotBusy = false;
        DrawBusyOverlay(scene.busyLabel.c_str(), progress);
    } else wasNotBusy = true;


    if (ui_resetAccum) scene.resetAccumulation();
}

void SceneUI::renderViewport(Scene& scene){

    static ImGuiID savedDockID = 0;
    static bool prevFullscreen = false;

    ImGuiWindowFlags flags = 0;
    if (viewportFullscreen) {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->Pos);
        ImGui::SetNextWindowSize(vp->Size);
        ImGui::SetNextWindowViewport(vp->ID);
        flags |= ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking;
    }

    if (!viewportFullscreen && prevFullscreen && savedDockID != 0) {
        ImGui::SetNextWindowDockID(savedDockID, ImGuiCond_Always);
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::Begin(viewportFullscreen ? "##Viewport" : "Viewport", nullptr, flags);

    // Save every frame while not fullscreen so we always have a valid ID
    if (!viewportFullscreen) {
        ImGuiID id = ImGui::GetWindowDockID();
        if (id != 0) savedDockID = id;
    }

    prevFullscreen = viewportFullscreen;

    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 drawSize = avail;
    if (scene.window.resizeAll((int)avail.x, (int)avail.y)) scene.resetAccumulation();

    ImVec2 imgMin = ImGui::GetCursorScreenPos();
    ImVec2 imgMax = ImVec2(imgMin.x + drawSize.x, imgMin.y + drawSize.y);

    viewportImgMinScreen = vec2(imgMin.x, imgMin.y);
    viewportImgMaxScreen = vec2(imgMax.x, imgMax.y);

    ImGui::Image(scene.window.outputTexture(), drawSize, ImVec2(0,1), ImVec2(1,0));


    // ---- Safe frame overlay (dims outside) ----
    if (!viewportFullscreen) {
        // Target aspect = fullscreen (monitor) aspect
        ImGuiViewport* vp = ImGui::GetMainViewport();
        float targetAspect = vp->Size.x / vp->Size.y;

        float availW = drawSize.x;
        float availH = drawSize.y;

        float safeW = availW;
        float safeH = safeW / targetAspect;
        if (safeH > availH) {
            safeH = availH;
            safeW = safeH * targetAspect;
        }

        ImVec2 safeMin(
            imgMin.x + 0.5f * (availW - safeW),
            imgMin.y + 0.5f * (availH - safeH)
        );
        ImVec2 safeMax(safeMin.x + safeW, safeMin.y + safeH);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 dimCol    = IM_COL32(0, 0, 0, 120);   // outside opacity
        ImU32 borderCol = IM_COL32(255, 255, 255, 200);

        // Dim outside safe rect
        dl->AddRectFilled(ImVec2(imgMin.x, imgMin.y), ImVec2(imgMax.x, safeMin.y), dimCol);      // top
        dl->AddRectFilled(ImVec2(imgMin.x, safeMax.y), ImVec2(imgMax.x, imgMax.y), dimCol);      // bottom
        dl->AddRectFilled(ImVec2(imgMin.x, safeMin.y), ImVec2(safeMin.x, safeMax.y), dimCol);    // left
        dl->AddRectFilled(ImVec2(safeMax.x, safeMin.y), ImVec2(imgMax.x, safeMax.y), dimCol);    // right

        // Border
        dl->AddRect(safeMin, safeMax, borderCol, 0.0f, 0, 2.0f);

        // Optional label
        dl->AddText(ImVec2(safeMin.x + 6, safeMin.y + 6), IM_COL32(255,255,255,180), "FRAME");
    }


    ImGui::End();
    ImGui::PopStyleVar();
}

void SceneUI::drawFileBrowser() {
    if (!fileBrowser.open) return;

    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("File Browser", &fileBrowser.open);

    // Current path display
    ImGui::TextDisabled("%s", fileBrowser.currentPath.c_str());
    ImGui::Separator();

    // Up button
    if (ImGui::Button("..")) {
        auto parent = std::filesystem::path(fileBrowser.currentPath).parent_path().string();
        fileBrowser.currentPath = parent;
        fileBrowser.refresh();
    }

    ImGui::BeginChild("##files", ImVec2(0, -40), true);

    // Folders first
    for (const std::string& folder : fileBrowser.folders) {
        ImGui::TextDisabled("[folder]");
        ImGui::SameLine();
        if (ImGui::Selectable(folder.c_str(), false)) {
            fileBrowser.currentPath += "/" + folder;
            fileBrowser.refresh();
        }
    }

    // Files
    for (const std::string& file : fileBrowser.files) {
        bool selected = (fileBrowser.selectedFile == file);
        if (ImGui::Selectable(file.c_str(), selected)) {
            fileBrowser.selectedFile = file;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            std::string full = fileBrowser.currentPath + "/" + file;
            fileBrowser.onSelect(full);
            fileBrowser.open = false;
        }
    }

    ImGui::EndChild();

    if (browserMode == BrowserMode::SaveJSON || browserMode == BrowserMode::SavePNG) {
        static char saveName[128] = "";

        // Show current path
        ImGui::Text("Save to: %s/", fileBrowser.currentPath.c_str());

        // Filename input
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("##savename", saveName, IM_ARRAYSIZE(saveName));
        ImGui::SameLine();

        if (ImGui::Button("Save")) {
            std::string full = fileBrowser.currentPath + "/" + std::string(saveName);
            fileBrowser.onSelect(full);
            fileBrowser.open = false;
            browserMode = BrowserMode::None;
        }

        // Clicking an existing file populates the name field
        // (already handled by Selectable in the file list above)
        if (!fileBrowser.selectedFile.empty()) {
            std::string filename = fileBrowser.selectedFile;
            strncpy(saveName, filename.c_str(), IM_ARRAYSIZE(saveName));
            fileBrowser.selectedFile = ""; // consume it
        }
    }
    else {
        // Bottom bar
        ImGui::Separator();
        ImGui::Text("%s", fileBrowser.selectedFile.c_str());
        ImGui::SameLine();

        bool hasSelection = !fileBrowser.selectedFile.empty();
        if (!hasSelection) ImGui::BeginDisabled();
        if (ImGui::Button("Open")) {
            std::string full = fileBrowser.currentPath + "/" + fileBrowser.selectedFile;
            fileBrowser.onSelect(full);
            fileBrowser.open = false;
        }
        if (!hasSelection) ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        fileBrowser.open = false;
    }

    ImGui::End();
}

void SceneUI::promptSavePNG(Scene *scene) {
    browserMode = BrowserMode::SavePNG;
    fileBrowser.openAt(PROJECT_DIR "renders", ".png", [this, scene](const std::string& path) {
        scene->savePNG(path);
        browserMode = BrowserMode::None;
    });
}
