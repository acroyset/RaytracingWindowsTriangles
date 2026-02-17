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

void SceneUI::ImGuiRender(Scene& scene) {

    bool ui_resetAccum = false;

    ImGui::DockSpaceOverViewport(
        0,
        ImGui::GetMainViewport(),
        ImGuiDockNodeFlags_PassthruCentralNode
    );

    drawViewportDocked(scene);

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


        static char filename[256] = "";
        static bool texture = false;
        static char textureFilename[256] = "";


        if (ImGui::Button("Add Model", ImVec2(-FLT_MIN, buttonHeight))) {
            strcpy(filename, "assets/models/");
            strcpy(textureFilename, "assets/textures/");

            ImGui::OpenPopup("Add Model");
        }

        if (ImGui::Button("Save JSON", ImVec2(-FLT_MIN, buttonHeight))) {
            strcpy(filename, "scenes/");

            ImGui::OpenPopup("Save JSON");
        }

        if (ImGui::Button("Load JSON", ImVec2(-FLT_MIN, buttonHeight))) {
            strcpy(filename, "scenes/");

            ImGui::OpenPopup("Load JSON");
        }

        if (ImGui::BeginPopupModal("Add Model", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            scene.lock = true;
            typing = true;

            ImGui::Text("Enter file name:");
            ImGui::InputText("##filename", filename, IM_ARRAYSIZE(filename));

            if (texture) {
                ImGui::Text("Enter texture path:");
                ImGui::InputText("##path", textureFilename, IM_ARRAYSIZE(textureFilename));
            }
            else texture = ImGui::Button("Texture");

            ImGui::Separator();

            if (ImGui::Button("Add")) {
                scene.startAddJob(filename, texture ? textureFilename : "");
                typing = false;
                texture = false;

                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Save JSON", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            scene.lock = true;
            typing = true;

            ImGui::Text("Enter file name:");
            ImGui::InputText("##filename", filename, IM_ARRAYSIZE(filename));

            ImGui::Separator();

            if (ImGui::Button("Save")) {
                scene.saveJSON(filename);
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
            scene.lock = true;
            typing = true;

            ImGui::Text("Enter file name:");
            ImGui::InputText("##filename", filename, IM_ARRAYSIZE(filename));

            ImGui::Separator();

            if (ImGui::Button("Load")) {
                scene.startLoadJob(filename);
                typing = false;

                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
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

        ImGui::Separator();

        if (ImGui::CollapsingHeader("Scene Statistics"))
        {
            DataPackageSize package = scene.lastSentPackage;

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

                Row("Triangles", int(scene.triangles.size())/3,
                    bytesToReadable(package.triangleDataSize));

                Row("Vertices", int(scene.vertices.size()),
                    bytesToReadable(package.vertexDataSize));

                Row("Texture Coords", int(scene.texCoords.size()),
                    bytesToReadable(package.texCoordDataSize));

                Row("Normals", int(scene.normals.size()),
                    bytesToReadable(package.normalDataSize));

                Row("Materials", scene.getNumMaterials(),
                    bytesToReadable(package.materialDataSize));

                Row("Textures", int(scene.textures.size()),
                    bytesToReadable(package.textureDataSize));

                Row("BVH Nodes", int(scene.BVHnodes.size()),
                    bytesToReadable(package.BVHnodesDataSize));

                Row("Transformations", int(scene.models.size()),
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


    if (ui_resetAccum) {
        scene.frameCount = 0;
        scene.sampleCount = 0;
    }
}

void SceneUI::drawViewportDocked(Scene& scene){
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
    ImGui::Begin("##Viewport", nullptr, flags);

    // Save every frame while not fullscreen so we always have a valid ID
    if (!viewportFullscreen) {
        ImGuiID id = ImGui::GetWindowDockID();
        if (id != 0) savedDockID = id;
    }

    prevFullscreen = viewportFullscreen;

    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 drawSize = avail;
    if (scene.window.resizeRenderTarget((int)avail.x, (int)avail.y)) {
        scene.frameCount = 0;
        scene.sampleCount = 0;
    }

    ImVec2 imgMin = ImGui::GetCursorScreenPos();
    ImVec2 imgMax = ImVec2(imgMin.x + drawSize.x, imgMin.y + drawSize.y);

    viewportImgMinScreen = vec2(imgMin.x, imgMin.y);
    viewportImgMaxScreen = vec2(imgMax.x, imgMax.y);

    ImGui::Image(scene.window.outputTexture(), drawSize, ImVec2(0,1), ImVec2(1,0));


    ImGui::End();
    ImGui::PopStyleVar();
}