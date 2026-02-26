//
// Created by acroy on 2/19/2026.
//

#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>

void Shader::buildProgram(const char* vertPath, const char* fragPath, const std::string& version, const std::vector<std::string>& includes) {
    std::string vsrc = injectVersion(readFile(vertPath), version);
    std::string fsrc = injectVersion(mergeIncludes(fragPath, includes), version);

    GLuint vs = compileShader(GL_VERTEX_SHADER,   vsrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsrc);

    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok; glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len; glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(program, len, nullptr, log.data());
        std::cerr << "Link error:\n" << log << "\n";
        glDeleteProgram(program); program = 0;
    }

    // Cache the pipeline-owned uniform locations
    locInput    = glGetUniformLocation(program, "u_input");
    locPrevious = glGetUniformLocation(program, "u_previousFrame");
}

void Shader::allocateFBOs(int w, int h) {
    width = w; height = h;

    // Delete old textures/FBOs if resizing
    if (tex[0]) glDeleteTextures(2, tex);
    if (fbo[0]) glDeleteFramebuffers(2, fbo);

    glGenTextures(2, tex);
    glGenFramebuffers(2, fbo);

    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, tex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, tex[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "Shader FBO " << i << " incomplete\n";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    for (int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);

        // Build draw buffers list
        std::vector<GLenum> drawBuffers = { GL_COLOR_ATTACHMENT0 };

        for (int a = 0; a < numAOVs; a++) {
            glGenTextures(1, &aovTex[i][a]);
            glBindTexture(GL_TEXTURE_2D, aovTex[i][a]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1 + a, GL_TEXTURE_2D, aovTex[i][a], 0);
            drawBuffers.push_back(GL_COLOR_ATTACHMENT1 + a);
        }

        glDrawBuffers((GLsizei)drawBuffers.size(), drawBuffers.data());
    }
}


Shader::Shader(const char* vertPath, const char* fragPath, const std::string& version, const std::vector<std::string>& includes) {
    this->vertPath = vertPath;
    this->fragPath = fragPath;
    this->glslVersion = version;
    this->includePaths = includes;

    glGenVertexArrays(1, &vao);
    buildProgram(vertPath, fragPath, version, includes);
}

Shader::~Shader() {
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);
    if (tex[0]) glDeleteTextures(2, tex);
    if (fbo[0]) glDeleteFramebuffers(2, fbo);
}

Shader::Shader(Shader&& o) noexcept
    : program(o.program), vao(o.vao),
      fbo{o.fbo[0], o.fbo[1]}, tex{o.tex[0], o.tex[1]},
      write(o.write), width(o.width), height(o.height),
      hasFeedback(o.hasFeedback), nextUnit(o.nextUnit),
      locInput(o.locInput), locPrevious(o.locPrevious)
{
    o.program = o.vao = 0;
    o.fbo[0] = o.fbo[1] = o.tex[0] = o.tex[1] = 0;
}

void Shader::enableFeedback() {
    hasFeedback = true;
}

void Shader::resize(int w, int h) {
    if (w == width && h == height) return;
    allocateFBOs(w, h);
}

void Shader::clearFeedback() {
    if (!fbo[0]) return;
    for (unsigned int i : fbo) {
        glBindFramebuffer(GL_FRAMEBUFFER, i);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool Shader::reload() {
    GLuint newProgram = 0;

    std::string vsrc = injectVersion(readFile(vertPath.c_str()), glslVersion);
    std::string fsrc = injectVersion(mergeIncludes(fragPath.c_str(), includePaths), glslVersion);
    if (vsrc.empty() || fsrc.empty()) return false;

    GLuint vs = compileShader(GL_VERTEX_SHADER,   vsrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsrc);

    newProgram = glCreateProgram();
    glAttachShader(newProgram, vs);
    glAttachShader(newProgram, fs);
    glLinkProgram(newProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok; glGetProgramiv(newProgram, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len; glGetProgramiv(newProgram, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(newProgram, len, nullptr, log.data());
        std::cerr << "Reload failed:\n" << log << "\n";
        glDeleteProgram(newProgram);
        return false;
    }

    // Swap only on success
    glDeleteProgram(program);
    program = newProgram;

    // Re-cache pipeline uniforms
    locInput    = glGetUniformLocation(program, "u_input");
    locPrevious = glGetUniformLocation(program, "u_previousFrame");


    return true;
}

Texture Shader::createTexture(const std::string& uniformName) {
    return {program, uniformName.c_str(), getNextUnit()};
}

Texture Shader::createTexture(const std::string& uniformName, const std::string& path) {
    return {program, uniformName.c_str(), getNextUnit(), path.c_str()};
}

void Shader::execute(GLuint inputTex, bool toScreen) {
    if (!fbo[0]) return;

    glUseProgram(program);
    glBindVertexArray(vao);

    // ── Bind u_input (previous shader's output) ──────────────────
    if (locInput != -1 && inputTex != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, inputTex);
        glUniform1i(locInput, 0);
    }

    // ── Bind u_previousFrame (own last frame) ────────────────────
    if (hasFeedback && locPrevious != -1) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, tex[1 - write]);
        glUniform1i(locPrevious, 1);
    }

    // ── Draw ─────────────────────────────────────────────────────
    // Always write to internal FBO first (needed for feedback),
    // then blit to screen if this is the last pass.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[write]);
    glViewport(0, 0, width, height);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    if (toScreen) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo[write]);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Advance ping-pong only when feedback is on;
    // non-feedback shaders always expose their result from the same slot.
    if (hasFeedback) write = 1 - write;
}

[[nodiscard]] GLuint Shader::outputTexture() const {
    return hasFeedback ? tex[1 - write] : tex[write];
}

void Shader::enableAOVs(int count) {
    numAOVs = count;
}

GLuint Shader::getAOVTexture(int idx) const {
    // return the last-written slot (same logic as outputTexture)
    int slot = hasFeedback ? (1 - write) : write;
    return aovTex[slot][idx];
}