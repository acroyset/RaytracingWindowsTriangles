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
#include "Shader.h"

using namespace glm;

class ShaderWindow {
    std::string name;

    GLFWwindow* window  = nullptr;
    int  fbWidth = 0, fbHeight = 0;
    bool apple   = false;
    float dt     = 0.f;

    Timer totalTime, deltaTime;

    std::vector<Shader*> shaders; // non-owning

    std::string glslVersion;

public:
    explicit ShaderWindow(const std::string& name);
    ~ShaderWindow();

    // ── Pipeline ───────────────────────────────────────────────────
    void addShader(Shader* shader);   // appends to chain
    void removeShader(Shader* shader);

    // Runs the full chain, last shader → screen
    void render() const;

    // Call at the start of each frame (polls dt, clears, etc.)
    void start();

    // Rebuilds all shader FBOs to new size
    bool resizeAll(int w, int h);

    // Clears feedback buffers on every shader that has feedback
    void clearAllFeedback() const;

    void reloadShaders() const;

    [[nodiscard]] std::string getGLSLVersion()     const { return glslVersion; }
    [[nodiscard]] bool        open()               const { return !glfwWindowShouldClose(window); }
    [[nodiscard]] uvec2       size()               const { return {fbWidth, fbHeight}; }
    [[nodiscard]] float       getDeltaTime()       const { return dt; }
    [[nodiscard]] float       getTimeSinceStart()  const { return float(totalTime.elapsedSeconds()); }
    [[nodiscard]] GLFWwindow* getWindow()          const { return window; }
    [[nodiscard]] bool        keyPressed(int key)  const;
    [[nodiscard]] vec2        getMousePos()        const;
    void                      setMousePos(vec2 p)  const;
    [[nodiscard]] GLuint      outputTexture() const;
};


#endif //SHADERWINDOW_H
