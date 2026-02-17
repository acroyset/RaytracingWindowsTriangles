//
// Created by acroy on 11/9/2025.
//
#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS
#elif defined(__APPLE__) || defined(__MACH__)
    #define PLATFORM_MAC
#elif defined(__linux__)
    #define PLATFORM_LINUX
#else
    #define PLATFORM_UNKNOWN
#endif


#ifndef SHADERWINDOW_H
#define SHADERWINDOW_H

#include <fstream>
#include <imgui.h>
#include <iostream>
#include <sstream>
#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Texture.h"
#include "Uniform.h"
#include "Timer.h"

using namespace glm;

enum class PresentMode { Screen, Texture };

class ShaderWindow {
    GLFWwindow* window = nullptr;

    GLuint vao = 0;

    int fbWidth = 0, fbHeight = 0;

    GLuint shaderProgram = 0;

    bool apple = false;

    GLuint fbo[2] = {0, 0};
    GLuint textures[2] = {0, 0};
    int currentBuffer = 0;
    bool useFeedback = false;

    PresentMode presentMode = PresentMode::Screen;

    Timer totalTime;
    Timer deltaTime;

    float dt = 0.0f;

    int nextTextureUnit = 1;  // Start at 1, 0 is feedback buffer
    int maxTextureUnits = 0;

    public:

    explicit ShaderWindow();

    ~ShaderWindow() {
        glDeleteVertexArrays(1, &vao);
        glDeleteProgram(shaderProgram);
        glDeleteFramebuffers(2, fbo);
        glDeleteTextures(2, textures);
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void render();

    void resize(int w, int h);

    void start();

    template<typename T>
    Uniform<T> createUniform(const std::string& name) const {
        return Uniform<T>(shaderProgram, name);
    }

    Texture createTexture(const std::string& name) {
        if (nextTextureUnit >= maxTextureUnits) std::cerr << "Texture units exceeded" << std::endl;

        return {shaderProgram, name.c_str(), nextTextureUnit++};
    }
    Texture createTexture(const std::string& name, const std::string& path) {
        if (nextTextureUnit >= maxTextureUnits) std::cerr << "Texture units exceeded" << std::endl;

        return {shaderProgram, name.c_str(), nextTextureUnit++, path.c_str()};
    }

    void setupFramebuffers();

    void clearFeedbackBuffers();

    // for ImGui viewport
    bool resizeRenderTarget(int w, int h);     // only used in Texture mode

    [[nodiscard]] ImTextureID outputTexture() const;         // only valid in Texture mode


    [[nodiscard]] bool keyPressed(const int key) const {
        if (glfwGetKey(window, key) == GLFW_PRESS) {
            return true;
        }
        return false;
    }

    [[nodiscard]] vec2 getMousePos() const {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        return {xpos, ypos};
    }
    void setMousePos(const vec2 pos) const {glfwSetCursorPos(window, pos.x, pos.y);}

    [[nodiscard]] float getDeltaTime() const {return dt;}
    [[nodiscard]] float getTimeSinceStart() const {return float(totalTime.elapsedSeconds());}

    [[nodiscard]] GLFWwindow* getWindow() const {return window;}

    void setFeedbackMode(const bool enabled) {useFeedback = enabled;}
    [[nodiscard]] bool isFeedbackEnabled() const {return useFeedback;}

    [[nodiscard]] bool open() const {return !glfwWindowShouldClose(window);}

    [[nodiscard]] uvec2 size() const {return {fbWidth, fbHeight};}

    void setPresentMode(PresentMode m) { presentMode = m; }
    [[nodiscard]] PresentMode getPresentMode() const { return presentMode; }

};


#endif //SHADERWINDOW_H
