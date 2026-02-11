// Texture.h

#pragma once

#include <string>
#include <glm/glm.hpp>
#include "Uniform.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"
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
    void createPlaceholderTexture() {
        unsigned char white[4] = {255, 255, 255, 255};
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        width = 1;
        height = 1;
        channels = 4;
        isHDR = false;
    }

    void applyParameters() const {
        glBindTexture(GL_TEXTURE_2D, textureID);

        // Min filter
        GLint minFilter = 0;
        switch (params.minFilter) {
            case TextureFilter::NEAREST: minFilter = GL_NEAREST; break;
            case TextureFilter::LINEAR: minFilter = GL_LINEAR; break;
            case TextureFilter::LINEAR_MIPMAP: minFilter = GL_LINEAR_MIPMAP_LINEAR; break;
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);

        // Mag filter (no mipmap option for mag)
        GLint magFilter = (params.magFilter == TextureFilter::NEAREST) ? GL_NEAREST : GL_LINEAR;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);

        // Wrap S
        GLint wrapS = 0;
        switch (params.wrapS) {
            case TextureWrap::REPEAT: wrapS = GL_REPEAT; break;
            case TextureWrap::CLAMP_TO_EDGE: wrapS = GL_CLAMP_TO_EDGE; break;
            case TextureWrap::MIRRORED_REPEAT: wrapS = GL_MIRRORED_REPEAT; break;
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);

        // Wrap T
        GLint wrapT = 0;
        switch (params.wrapT) {
            case TextureWrap::REPEAT: wrapT = GL_REPEAT; break;
            case TextureWrap::CLAMP_TO_EDGE: wrapT = GL_CLAMP_TO_EDGE; break;
            case TextureWrap::MIRRORED_REPEAT: wrapT = GL_MIRRORED_REPEAT; break;
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);

        if (params.generateMipmaps) {
            glGenerateMipmap(GL_TEXTURE_2D);
        }
    }

    bool loadImageToGPU(const char* path) {
        if (!path || path[0] == '\0') return false;

        stbi_set_flip_vertically_on_load(false);

        // Save previous alignment, set to 1 for safety
        GLint prevAlign;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevAlign);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        bool success = false;

        // Check if HDR
        if (stbi_is_hdr(path)) {
            float* data = stbi_loadf(path, &width, &height, &channels, 0);
            if (data) {
                isHDR = true;
                GLenum srcFormat = (channels == 4) ? GL_RGBA : GL_RGB;
                GLint dstFormat = (channels == 4) ? GL_RGBA16F : GL_RGB16F;

                glBindTexture(GL_TEXTURE_2D, textureID);
                glTexImage2D(GL_TEXTURE_2D, 0, dstFormat, width, height, 0, srcFormat, GL_FLOAT, data);

                stbi_image_free(data);
                success = true;
            }
        } else {
            unsigned char* data = stbi_load(path, &width, &height, &channels, 0);
            if (data) {
                isHDR = false;
                GLenum srcFormat = (channels == 4) ? GL_RGBA : GL_RGB;
                GLint dstFormat = (channels == 4) ? GL_SRGB8_ALPHA8 : GL_SRGB8;

                glBindTexture(GL_TEXTURE_2D, textureID);
                glTexImage2D(GL_TEXTURE_2D, 0, dstFormat, width, height, 0, srcFormat, GL_UNSIGNED_BYTE, data);

                stbi_image_free(data);
                success = true;
            }
        }

        if (success) {
            applyParameters();
        } else {
            fprintf(stderr, "Failed to load texture: %s (%s)\n", path, stbi_failure_reason());
        }

        // Restore alignment
        glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlign);

        return success;
    }

public:
    // Default constructor - creates invalid/empty texture
    Texture() = default;

    // Main constructor - called by ShaderWindow::createTexture()
    Texture(GLuint program, const char* uniformName, int unit, const TextureParams& texParams = TextureParams())
        : assignedUnit(unit), samplerUniform(program, uniformName), params(texParams) {
        glGenTextures(1, &textureID);
        createPlaceholderTexture();
    }

    // Constructor with immediate load
    Texture(GLuint program, const char* uniformName, int unit, const char* path, const TextureParams& texParams = TextureParams())
        : assignedUnit(unit), samplerUniform(program, uniformName), params(texParams) {
        glGenTextures(1, &textureID);
        if (!loadImageToGPU(path)) {
            createPlaceholderTexture();
        } else {
            originalPath = path;
        }
    }

    // Destructor - cleanup
    ~Texture() {
        if (textureID != 0) {
            glDeleteTextures(1, &textureID);
        }
        // Future: cancel any pending async loads here
    }

    // Move semantics (textures shouldn't be copied)
    Texture(Texture&& other) noexcept
        : textureID(other.textureID),
          assignedUnit(other.assignedUnit),
          samplerUniform(other.samplerUniform),
          originalPath(std::move(other.originalPath)),
          width(other.width),
          height(other.height),
          channels(other.channels),
          isHDR(other.isHDR),
          params(other.params) {
        other.textureID = 0; // Prevent double-delete
    }

    Texture& operator=(Texture&& other) noexcept {
        if (this != &other) {
            if (textureID != 0) {
                glDeleteTextures(1, &textureID);
            }
            textureID = other.textureID;
            assignedUnit = other.assignedUnit;
            samplerUniform = other.samplerUniform;
            originalPath = std::move(other.originalPath);
            width = other.width;
            height = other.height;
            channels = other.channels;
            isHDR = other.isHDR;
            params = other.params;
            other.textureID = 0;
        }
        return *this;
    }

    // Delete copy
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // --- Core functionality ---

    // Bind texture and set uniform (call this every frame you want to use it)
    void bind() const {

        if (textureID == 0 || assignedUnit < 0) return;

        glActiveTexture(GL_TEXTURE0 + assignedUnit);
        glBindTexture(GL_TEXTURE_2D, textureID);
        samplerUniform.set(assignedUnit);
    }

    // Bind to a custom unit override
    void bind(int customUnit) const {
        if (textureID == 0) return;

        glActiveTexture(GL_TEXTURE0 + customUnit);
        glBindTexture(GL_TEXTURE_2D, textureID);
        // Note: doesn't update uniform - you'd need to manually set it
    }

    // Load from file path
    bool load(const char* path) {
        if (loadImageToGPU(path)) {
            originalPath = path;
            return true;
        }
        return false;
    }

    // Reload from original path
    bool reload() {
        if (originalPath.empty()) {
            fprintf(stderr, "Cannot reload: no original path stored\n");
            return false;
        }
        return loadImageToGPU(originalPath.c_str());
    }

    // Update from raw pixel data
    void updateFromData(const void* pixels, int w, int h, GLenum format, GLenum type = GL_UNSIGNED_BYTE) {
        width = w;
        height = h;

        // Determine channels from format
        switch(format) {
            case GL_RED: channels = 1; break;
            case GL_RG: channels = 2; break;
            case GL_RGB: channels = 3; break;
            case GL_RGBA: channels = 4; break;
            default: channels = 3;
        }

        isHDR = (type == GL_FLOAT);

        GLint internalFormat;
        if (isHDR) {
            internalFormat = (channels == 4) ? GL_RGBA16F : GL_RGB16F;
        } else {
            internalFormat = (channels == 4) ? GL_RGBA8 : GL_RGB8;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, pixels);
        applyParameters();
    }

    // --- Parameter control ---

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

    // --- Getters ---

    [[nodiscard]] bool isValid() const { return textureID != 0; }
    [[nodiscard]] int getWidth() const { return width; }
    [[nodiscard]] int getHeight() const { return height; }
    [[nodiscard]] int getChannels() const { return channels; }
    [[nodiscard]] bool getIsHDR() const { return isHDR; }
    [[nodiscard]] int getUnit() const { return assignedUnit; }
    [[nodiscard]] GLuint getID() const { return textureID; }
    [[nodiscard]] const std::string& getPath() const { return originalPath; }

};