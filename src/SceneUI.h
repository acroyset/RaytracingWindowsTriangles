//
// Created by acroy on 2/17/2026.
//

#ifndef SCENEUI_H
#define SCENEUI_H

#include <imgui.h>
#include <glm/glm.hpp>
#include <iomanip>
#include <ios>
#include <string>
#include "FileBrowser.h"

using namespace glm;

class Scene;


inline std::string bytesToReadable(long long bytes, int sigFigs = 3) {
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

class SceneUI {

    int selectedModel = -1;
    int selectedColor = 0;

    vec2 viewportImgMinScreen{0}, viewportImgMaxScreen{0};

    FileBrowser fileBrowser;
    enum class BrowserMode { None, AddModel, AddTexture, LoadJSON, SaveJSON };
    BrowserMode browserMode = BrowserMode::None;
    std::string pendingModelPath;
    bool showTexturePrompt = false;

    public:

    SceneUI() = default;

    bool typing = false;
    bool viewportFullscreen = false;
    bool skipMouseFrame = false;

    void render(Scene& scene);

    void renderViewport(Scene& scene);

    void drawFileBrowser();

    [[nodiscard]] vec2 getCenter() const {
        return 0.5f * (viewportImgMinScreen + viewportImgMaxScreen);
    }

};



#endif //SCENEUI_H
