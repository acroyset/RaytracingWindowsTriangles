//
// Created by acroy on 2/11/2026.
//

#include "Texture.h"

void Texture::createPlaceholderTexture() {
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

void Texture::applyParameters() const {
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

bool Texture::loadImageToGPU(const char* path) {
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

Texture::Texture(GLuint program, const char* uniformName, int unit, const TextureParams& texParams)
        : assignedUnit(unit), samplerUniform(program, uniformName), params(texParams) {
    glGenTextures(1, &textureID);
    createPlaceholderTexture();
}

Texture::Texture(GLuint program, const char* uniformName, int unit, const char* path, const TextureParams& texParams)
    : assignedUnit(unit), samplerUniform(program, uniformName), params(texParams) {
    glGenTextures(1, &textureID);
    if (!loadImageToGPU(path)) {
        createPlaceholderTexture();
    } else {
        originalPath = path;
    }
}

Texture::Texture(Texture&& other) noexcept
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

Texture& Texture::operator=(Texture&& other) noexcept {
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

void Texture::bind() const {

    if (textureID == 0 || assignedUnit < 0) return;

    glActiveTexture(GL_TEXTURE0 + assignedUnit);
    glBindTexture(GL_TEXTURE_2D, textureID);
    samplerUniform.set(assignedUnit);
}

void Texture::bind(int customUnit) const {
    if (textureID == 0) return;

    glActiveTexture(GL_TEXTURE0 + customUnit);
    glBindTexture(GL_TEXTURE_2D, textureID);
    // Note: doesn't update uniform - you'd need to manually set it
}

bool Texture::load(const char* path) {
    if (loadImageToGPU(path)) {
        originalPath = path;
        return true;
    }
    return false;
}

bool Texture::reload() {
    if (originalPath.empty()) {
        fprintf(stderr, "Cannot reload: no original path stored\n");
        return false;
    }
    return loadImageToGPU(originalPath.c_str());
}

void Texture::updateFromData(const void* pixels, int w, int h, GLenum format, GLenum type) {
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