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

static const char* MaterialTypeLabel(MaterialType t) {
    switch (t) {
        case Opaque:      return "Opaque";
        case Specular:    return "Specular";
        case Transparent: return "Transparent";
        case Emissive:    return "Emissive";
        default:          return "Unknown";
    }
}

static bool DrawMaterialInspector(Material& m, int matIndex, int textureID){
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

    // --- Header ---
    ImGui::Text("Material #%d", matIndex);
    ImGui::SameLine();
    ImGui::TextDisabled("(%s)", MaterialTypeLabel(type));
    ImGui::Separator();

    // --- Type selector drives everything ---
    const char* items[] = {"Opaque", "Specular", "Transparent", "Emissive"};
    int t = int(type);
    if (ImGui::Combo("Type", &t, items, IM_ARRAYSIZE(items))) {
        type = MaterialType(t);
        changed = true;

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

    // --- Common group: Base color (and texture info) ---
    if (textureID != -1) {
        ImGui::TextDisabled("Texture bound (ID %d). Diffuse color UI hidden.", textureID);
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
    } else if (type == Opaque) {
        ImGui::SeparatorText("Surface");

        changed |= ImGui::SliderFloat("Roughness", &diffuseRoughness, 0.0f, 1.0f);
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

                vec3 diffuseColor = vec3(scene.floorDiffuseColor);
                if (ColorEdit3("Floor Diffuse Color", diffuseColor)) {
                    scene.floorDiffuseColor = vec4(diffuseColor, scene.floorDiffuseColor.w);
                    ui_resetAccum = true;
                }

                bool specularFloor = scene.floorSpecularColor.w != -1.0f;
                if (ImGui::Checkbox("Specular Floor", &specularFloor)) {
                    if (specularFloor) scene.floorSpecularColor.w = 0.0f;
                    else scene.floorSpecularColor.w = -1.0f;
                    ui_resetAccum = true;
                }

                if (specularFloor) {
                    vec3 specularColor = vec3(scene.floorSpecularColor);
                    if (ColorEdit3("Floor Specular Color", specularColor)) {
                        scene.floorSpecularColor = vec4(specularColor, scene.floorSpecularColor.w);
                        ui_resetAccum = true;
                    }
                }

                ui_resetAccum |= ImGui::SliderFloat("Floor Smoothness", &scene.floorDiffuseColor.w, 0.0f, 1.0f);

                if (specularFloor) ui_resetAccum |= ImGui::SliderFloat("Floor Specular Probability", &scene.floorSpecularColor.w, 0.0f, 1.0f);

                ImGui::Unindent();
            }
        }

        ImGui::Separator();
        ImGui::Text("Models");

        ImGui::Indent();

        const bool hasModels = !scene.models.empty();
        if (!hasModels) {
            ImGui::TextDisabled("(no models)");
        }
        else {

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

        // total height of the bottom block: 3 buttons + 2 gaps
        float blockHeight = 3.0f * buttonHeight + 2.0f * spacing;

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

        if (ImGui::Button("Load JSON", ImVec2(-FLT_MIN, buttonHeight))) {
            Scene* s = &scene;
            fileBrowser.openAt(PROJECT_DIR "scenes", ".json", [this, s](const std::string& path) {
                s->startLoadJob(std::filesystem::relative(path, PROJECT_DIR).string());
                browserMode = BrowserMode::None;
            });
        }

        if (ImGui::Button("Save JSON", ImVec2(-FLT_MIN, buttonHeight))) {
            Scene* s = &scene;
            fileBrowser.openAt(PROJECT_DIR "scenes", ".json", [this, s](const std::string& path) {
                s->saveJSON(std::filesystem::relative(path, PROJECT_DIR).string());
                browserMode = BrowserMode::None;
            });
        }

        if (scene.busy) {
            DrawBusyOverlay(scene.busyLabel.c_str());
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

        if (ImGui::CollapsingHeader("Camera")) {
            ui_resetAccum |= ImGui::SliderFloat("FOV", &scene.fovDeg, 20, 140);
            bool dof = scene.aperture > 0;
            if (ImGui::Checkbox("Depth of Field", &dof)) {
                if (dof) scene.aperture = 0.001;
                else scene.aperture = 0.0;
                ui_resetAccum = true;
            }

            if (dof) {
                ImGui::Indent();
                ui_resetAccum |= ImGui::SliderFloat("Aperture", &scene.aperture, 0.001f, 0.5f);
                ui_resetAccum |= ImGui::SliderFloat("Focus Distance", &scene.focusDistance, 0.0f, 1000.0f);
                ui_resetAccum |= ImGui::Checkbox("Focus Distance Plane", &scene.focusDistancePlane);
                ImGui::Unindent();
            }

            float s = scene.sensitivity*100;
            if (ImGui::SliderFloat("Sensitivity", &s, 0.1, 10)) scene.sensitivity = s/100;

            ImGui::SliderFloat("Speed", &scene.speed, 0.01, 1000);
        }

        ImGui::Separator();

        ImGui::Text("Debug");

        ui_resetAccum |= ImGui::Checkbox("##Debug View" , &scene.debugView);
        if (scene.debugView) {

            const char* names[] = { "Normals", "Heatmap", "Depth" };

            // Use a custom getter function
            int debugMode = scene.debugMode;
            if (ImGui::SliderInt("Debug Mode", &debugMode, 0, 2, names[debugMode])) {
                scene.debugMode = static_cast<DebugMode>(debugMode);
                ui_resetAccum = true;
            }

            switch (debugMode) {
                case Normals:
                    break;
                case Heatmap:
                    ui_resetAccum |= ImGui::SliderInt("Triangle Threshold", &scene.triTh, 1, 50);
                    ui_resetAccum |= ImGui::SliderInt("AABB Threshold", &scene.aabbTh, 1, 250);
                    break;
                case Depth:
                    ui_resetAccum |= ImGui::SliderFloat("Depth Scale", &scene.depthScale, 1, 5000);
                default: ;
            }

        }

        ImGui::End();
    }

    // fps stats
    {
        ImGui::Begin("FPS Stats");

        ImGui::Text("Samples: %d", scene.sampleCount);
        ImGui::Text("Frame: %d", scene.frameCount);

        ImGui::Separator();

        char overlay[128];
        sprintf(overlay, "FPS: %.2f (%.2f ms)  CPU: %.2f ms  GPU: %.2f ms", 1/scene.totalTime, scene.totalTime*1000.0f, scene.cpuTime*1000.0f, scene.gpuTime*1000.0f);

        float maxVal = (*std::max_element(scene.dtData.begin(), scene.dtData.end())) * 1.2f;
        ImGui::PlotHistogram(
            "##Frame Time (ms)",
            scene.dtData.data(),
            int(scene.dtData.size()),
            0,
            overlay,
            0.0f,
            maxVal,
            ImVec2(0, 100)
            );
        if (ImGui::Button("Reset")) {
            scene.dtData.clear();
        }
        int maxNum = int(10.0f/scene.totalTime);
        if (int(scene.dtData.size()) > maxNum) {
            scene.dtData.erase(scene.dtData.begin());
        }

        ImGui::End();
    }

    // scene stats
    {
        ImGui::Begin("Scene Stats");

        ImGui::Text("Width: %d", scene.window.size().x);
        ImGui::Text("Height: %d", scene.window.size().y);

        ImGui::Separator();

        ImGui::Text("Position: %.2f, %.2f, %.2f", scene.cameraPos.x, scene.cameraPos.y, scene.cameraPos.z);
        ImGui::Text("Forward: %.2f, %.2f, %.2f", scene.camForward.x, scene.camForward.y, scene.camForward.z);

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
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Sent", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Size",  ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            auto Row = [&](const char* label, int count, int sent, const std::string& size)
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(label);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%d", count);

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%d", sent);

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%s", size.c_str());
            };

            auto RowNoSent = [&](const char* label, int count, const std::string& size)
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(label);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%d", count);

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%d", count);

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%s", size.c_str());
            };

            Row("Triangles", package.triangles, package.trianglesSent, bytesToReadable(package.triangleBytes));
            Row("Vertices", package.vertices, package.verticesSent, bytesToReadable(package.verticesBytes));
            Row("Tex Coords", package.texCoords, package.texCoordsSent, bytesToReadable(package.texCoordsBytes));
            Row("Normals", package.normals, package.normalsSent, bytesToReadable(package.normalsBytes));
            Row("BVH Nodes", package.BVHNodes, package.BVHNodesSent, bytesToReadable(package.BVHNodesBytes));
            RowNoSent("Models", int(scene.models.size()), "");
            RowNoSent("Materials", package.materials, bytesToReadable(package.materialsBytes));
            RowNoSent("Textures", package.textures, bytesToReadable(package.texturesBytes));
            RowNoSent("Transforms", package.transforms, bytesToReadable(package.transformsBytes));

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(ImVec4(1,1,0.4f,1), "Total");

            ImGui::TableSetColumnIndex(3);
            ImGui::TextColored(ImVec4(1,1,0.4f,1),
                "%s", bytesToReadable(package.totalSize).c_str());

            ImGui::EndTable();
        }

        ImGui::End();
    }

    // inspector
    if (selectedModel >= 0) {
        ImGui::Begin("Inspector");

        ImGui::Indent();
        bool changedT = false;
        bool changedM = false;

        Model& model = scene.models[selectedModel];

        // Local aliases
        Transformation& transform = model.transformation;

        ImGui::Text("Transformations");

        // Rotation UI in degrees (convert to/from radians for nicer UX)
        vec3 rotDeg = degrees(transform.rotation);
        changedT |= DragFloat3("Position", transform.position, 3.0f);

        static bool uniformScale = false;

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

        ImGui::Indent();
        Material& m = model.materials[selectedColor];

        changedM |= DrawMaterialInspector(m, selectedColor, model.textureID);

        ImGui::Unindent();

        int textureID = model.textureID;
        if (textureID != -1) {
            ImGui::Separator();
            ImGui::Text("Texture");
            ImGui::Text(scene.textureLabels[textureID].c_str());
            ImGui::Text("ID: %i", textureID);

            float s = scene.textureScales[textureID];
            bool wrapTexture = s > 0;;
            if (ImGui::Checkbox("Wrap Texture", &wrapTexture)) {
                if (wrapTexture) {
                    scene.textureScales[textureID] = 0.001;
                } else {
                    scene.textureScales[textureID] = 0;
                }
                ui_resetAccum = true;
            }

            if (wrapTexture) {
                if (ImGui::DragFloat("Texture Scale", &s, 0.005f, 0.0001f, 2.0f, "%.4f")) {
                    scene.textureScales[textureID] = s;
                    ui_resetAccum = true;
                }
            }
        }
        ImGui::Unindent();

        if (changedT) {
            transform.rotation = radians(rotDeg);

            transform.setMatrix();

            scene.ssboModelTransformations.update(selectedModel, transform.matrix);
            scene.ssboModelInvTransformations.update(selectedModel, transform.inverseMatrix);

            ui_resetAccum = true;
        }
        // All materials for this model share the same contiguous range:

        if (changedM) {
            int start = scene.modelOffsets[selectedModel][5];
            int end = start + int(model.materials.size());
            scene.ssboMaterials.update(int(start), int(end), model.materials.data());
            ui_resetAccum = true;
        }

        ImGui::End();
    }

    drawFileBrowser();


    if (ui_resetAccum) scene.resetAccumulation();
}

void SceneUI::renderViewport(Scene& scene){
    scene.window.setPresentMode(PresentMode::Texture);

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
    if (scene.window.resizeRenderTarget((int)avail.x, (int)avail.y)) scene.resetAccumulation();

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

    if (browserMode == BrowserMode::SaveJSON) {
        static char saveName[128] = "scene";

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
            // strip .json for display
            std::string stripped = fileBrowser.selectedFile;
            if (stripped.find(".json") != std::string::npos)
                stripped = stripped.substr(0, stripped.size() - 5);
            strncpy(saveName, stripped.c_str(), IM_ARRAYSIZE(saveName));
            fileBrowser.selectedFile = ""; // consume it
        }
    } else {
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