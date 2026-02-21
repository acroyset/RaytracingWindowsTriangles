//
// Created by acroy on 2/10/2026.
//

#include "ShaderWindow.h"

static void errorCallback(int code, const char* desc) {
    std::cerr << "GLFW error " << code << ": " << desc << "\n";
}

static void escapeKeyCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(w, GLFW_TRUE);
}

static void framebufferSizeCallback(GLFWwindow* /*w*/, int width, int height) {
    glViewport(0, 0, width, height);
}

ShaderWindow::ShaderWindow(const std::string& name) {
    #ifdef PLATFORM_MAC
    apple = true;
    #endif

    glslVersion = apple ? "330 core" : "430 core";

    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
    }
    glfwSetErrorCallback(errorCallback);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, apple ? 3 : 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #endif

    glfwWindowHint(GLFW_REFRESH_RATE, 180);
    GLFWmonitor* mon = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(mon);

    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    window = glfwCreateWindow(mode->width, mode->height, name.c_str(), nullptr, nullptr);
    glfwSetWindowPos(window, 0, 0);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    // GLAD (GLAD1-style loader)
    if (!gladLoadGL()) {
        std::cerr << "Failed to initialize GLAD2\n";
    }


    //glfwSetKeyCallback(window, escapeKeyCallback); // close on escape key

    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    glDisable(GL_DEPTH_TEST);
    glClearColor(0.02f, 0.02f, 0.03f, 1.0f);
}

ShaderWindow::~ShaderWindow() {
    glfwDestroyWindow(window);
    glfwTerminate();
}

void ShaderWindow::addPass(ShaderPass* pass) {
    pass->resize(fbWidth, fbHeight);
    passes.push_back(pass);
}

void ShaderWindow::removePass(ShaderPass* pass) {
    passes.erase(std::remove(passes.begin(), passes.end(), pass), passes.end());
}

void ShaderWindow::render() const {
    std::vector<ShaderPass*> active;
    for (auto* s : passes)
        if (s && s->enabled) active.push_back(s);

    GLuint prevOutput = 0;
    for (size_t i = 0; i < active.size(); i++) {
        bool isLast = (i == active.size() - 1);
        active[i]->execute(prevOutput, isLast);
        prevOutput = active[i]->outputTexture();
    }
}

bool ShaderWindow::resizeAll(int w, int h) {
    if (fbWidth == w && fbHeight == h) return false;
    fbWidth = w; fbHeight = h;
    glViewport(0, 0, w, h);
    for (auto* s : passes) s->resize(w, h);
    return true;
}

void ShaderWindow::clearAllFeedback() const {
    for (auto* s : passes) s->clearFeedback();
}

void ShaderWindow::reload() const {
    for (ShaderPass* s : passes) {
        s->reload();
    }
}

void ShaderWindow::start() {
    dt = float(deltaTime.reset());
    glClear(GL_COLOR_BUFFER_BIT);
}

[[nodiscard]] bool ShaderWindow::keyPressed(const int key) const {
    if (glfwGetKey(window, key) == GLFW_PRESS) {
        return true;
    }
    return false;
}

[[nodiscard]] vec2 ShaderWindow::getMousePos() const {
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    return {xpos, ypos};
}

void ShaderWindow::setMousePos(const vec2 pos) const {
    glfwSetCursorPos(window, pos.x, pos.y);
}

[[nodiscard]] GLuint ShaderWindow::outputTexture() const {
    for (int i = int(passes.size()) - 1; i >= 0; i--)
        if (passes[i] && passes[i]->enabled)
            return passes[i]->outputTexture();
    return 0;
}