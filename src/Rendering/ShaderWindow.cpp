//
// Created by acroy on 2/10/2026.
//

#include "ShaderWindow.h"

static std::string loadTextFile(const char* path) {
        std::ifstream f(path, std::ios::in | std::ios::binary);
        if (!f) {
            std::cerr << "Failed to open file: " << path << "\n";
            return {};
        }
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

static std::string withVersionAndDefines(const std::string& src, const std::string& glslVersion, const std::string& extraDefines = "") {
    std::string versionLine = "#version " + glslVersion + "\n";
    std::string body = src;

    // If the shader already has a #version on the first non-empty line, replace it.
    size_t i = 0;
    while (i < body.size() && (body[i] == '\n' || body[i] == '\r' || body[i] == ' ' || body[i] == '\t')) ++i;

    if (i < body.size() && body.compare(i, 8, "#version") == 0) {
        // Replace that line
        size_t lineEnd = body.find('\n', i);
        if (lineEnd == std::string::npos) lineEnd = body.size();
        body.erase(i, lineEnd - i);
        body.insert(i, versionLine);
    } else {
        // Prepend version
        body = versionLine + body;
    }

    if (!extraDefines.empty()) {
        // Insert defines right after the #version line we ensured above
        size_t afterVersion = body.find('\n');
        if (afterVersion == std::string::npos) afterVersion = body.size();
        body.insert(afterVersion + 1, extraDefines + "\n");
    }

    return body;
}

static GLuint compileShader(GLenum type, const std::string& src, const std::string& glslVersion, const std::string& extraDefines = "") {
    std::string finalSrc = withVersionAndDefines(src, glslVersion, extraDefines);

    GLuint sh = glCreateShader(type);
    const char* csrc = finalSrc.c_str();
    glShaderSource(sh, 1, &csrc, nullptr);
    glCompileShader(sh);

    GLint ok = GL_FALSE;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &logLen);
        std::string log((size_t)logLen, '\0');
        glGetShaderInfoLog(sh, logLen, nullptr, log.data());
        std::cerr << "Shader compile error:\n" << log << "\n";
    }
    return sh;
}

static GLuint createProgramFromFiles(const char* vertPath, const char* fragPath, const std::string& version) {
    std::string vsrc = loadTextFile(vertPath);
    std::string fsrc = loadTextFile(fragPath);
    if (vsrc.empty() || fsrc.empty()) return 0;

    GLuint vs = compileShader(GL_VERTEX_SHADER,   vsrc, version);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsrc, version);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLen);
        std::string log((size_t)logLen, '\0');
        glGetProgramInfoLog(prog, logLen, nullptr, log.data());
        std::cerr << "Program link error:\n" << log << "\n";
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

static void errorCallback(int code, const char* desc) {
    std::cerr << "GLFW error " << code << ": " << desc << "\n";
}

static void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(w, GLFW_TRUE);
}

static void framebufferSizeCallback(GLFWwindow* /*w*/, int width, int height) {
    glViewport(0, 0, width, height);
}

ShaderWindow::ShaderWindow() {
    #ifdef PLATFORM_MAC
            apple = true;
    #endif

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

    // Fullscreen on primary monitor
    GLFWmonitor* mon = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(mon);
    window = glfwCreateWindow(mode->width, mode->height, "Fullscreen Shader", mon, nullptr);
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


    glfwSetKeyCallback(window, keyCallback);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    setupFramebuffers();

    // Minimal state
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.02f, 0.02f, 0.03f, 1.0f);

    // Program
    shaderProgram = createProgramFromFiles("shaders/fullscreen.vert", "shaders/fullscreen.frag", apple ? "330 core" : "430 core");

    // Empty VAO required for core profile when using gl_VertexID
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Cursor hidden for screensaver feel
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTextureUnits);
}


void ShaderWindow::render() {
    if (useFeedback) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo[currentBuffer]);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        currentBuffer = 1 - currentBuffer;
    } else {
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

}

void ShaderWindow::start() {
    dt = float(deltaTime.reset());

    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(shaderProgram);

    if (useFeedback) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[1 - currentBuffer]);
        glUniform1i(glGetUniformLocation(shaderProgram, "previousFrame"), 0);
    }
}

void ShaderWindow::setupFramebuffers() {
    glGenFramebuffers(2, fbo);
    glGenTextures(2, textures);

    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, fbWidth, fbHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[i], 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "Framebuffer " << i << " not complete!\n";
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShaderWindow::clearFeedbackBuffers() {
    if (!useFeedback) return;

    for (unsigned int i : fbo) {
        glBindFramebuffer(GL_FRAMEBUFFER, i);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

