// Texture.h

#pragma once

#include <string>
#include <glm/glm.hpp>
#include "Uniform.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#include "../../external/stb_image.h"
#endif

enum class TextureFilter {
    NEAREST,
    LINEAR,
    LINEAR_MIPMAP
};
enum class TextureWrap {
    REPEAT,
    CLAMP_TO_EDGE,
    MIRRORED_REPEAT
};
struct TextureParams {
    TextureFilter minFilter = TextureFilter::LINEAR_MIPMAP;
    TextureFilter magFilter = TextureFilter::LINEAR;
    TextureWrap wrapS = TextureWrap::REPEAT;
    TextureWrap wrapT = TextureWrap::CLAMP_TO_EDGE;
    bool generateMipmaps = true;
};

class Texture {

    GLuint textureID = 0;
    int assignedUnit = -1;
    Uniform<int> samplerUniform;

    std::string originalPath;
    int width = 0;
    int height = 0;
    int channels = 0;
    bool isHDR = false;

    TextureParams params;

    // Internal helpers
    void createPlaceholderTexture();

    void applyParameters() const;

    bool loadImageToGPU(const char* path);

public:
    Texture() = default;

    Texture(GLuint program, const char* uniformName, int unit, const TextureParams& texParams = TextureParams());

    Texture(GLuint program, const char* uniformName, int unit, const char* path, const TextureParams& texParams = TextureParams());

    ~Texture() {
        if (textureID != 0) {
            glDeleteTextures(1, &textureID);
        }
        // Future: cancel any pending async loads here
    }

    Texture(Texture&& other) noexcept;

    Texture& operator=(Texture&& other) noexcept;

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    void bind() const;

    void bind(int customUnit) const;

    bool load(const char* path);

    bool reload();

    void updateFromData(const void* pixels, int w, int h, GLenum format, GLenum type = GL_UNSIGNED_BYTE);

    // Parameter control

    void setFilter(TextureFilter min, TextureFilter mag) {
        params.minFilter = min;
        params.magFilter = mag;
        applyParameters();
    }

    void setWrap(TextureWrap s, TextureWrap t) {
        params.wrapS = s;
        params.wrapT = t;
        applyParameters();
    }

    void setParameters(const TextureParams& newParams) {
        params = newParams;
        applyParameters();
    }

    // Getters

    [[nodiscard]] bool isValid() const { return textureID != 0; }
    [[nodiscard]] int getWidth() const { return width; }
    [[nodiscard]] int getHeight() const { return height; }
    [[nodiscard]] int getChannels() const { return channels; }
    [[nodiscard]] bool getIsHDR() const { return isHDR; }
    [[nodiscard]] int getUnit() const { return assignedUnit; }
    [[nodiscard]] GLuint getID() const { return textureID; }
    [[nodiscard]] const std::string& getPath() const { return originalPath; }

     [[nodiscard]] long long gpuSizeBytes() const {
        int bytesPerChannel = isHDR ? 2 : 1;
        long long size = width * height * channels * bytesPerChannel;
        return params.generateMipmaps ? size * 4 / 3 : size;
    }

};