//
// Created by acroy on 2/19/2026.
//

#ifndef SHADER_H
#define SHADER_H

#include <string>
#include <vector>
#include <glad/glad.h>
#include "Uniform.h"
#include "Texture.h"

class Shader {
    GLuint program   = 0;
    GLuint vao       = 0;
    GLuint fbo[2]    = {0, 0};
    GLuint tex[2]    = {0, 0};
    int    write     = 0;       // ping-pong write index
    int    width     = 0;
    int    height    = 0;
    bool   hasFeedback = false;

    std::string vertPath;
    std::string fragPath;
    std::string glslVersion;
    std::vector<std::string> includePaths;

    // Reserved texture units:
    //   0 → u_input        (previous shader's output)
    //   1 → u_previousFrame (own last frame, feedback only)
    //   2+ → user textures
    int nextUnit = 2;
    std::vector<int> freeUnits; // reclaimed slots

    // Cached uniform locations for the pipeline's own bindings
    GLint locInput    = -1;
    GLint locPrevious = -1;

    void buildProgram(const char* vertPath, const char* fragPath,
                      const std::string& version,
                      const std::vector<std::string>& includes);
    void allocateFBOs(int w, int h);

    int getNextUnit() {
        if (!freeUnits.empty()) {
            int u = freeUnits.back();
            freeUnits.pop_back();
            return u;
        }
        return nextUnit++;
    }

public:
    bool enabled = true;

    Shader(const char* vertPath, const char* fragPath,
           const std::string& version,
           const std::vector<std::string>& includes = {});

    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&&) noexcept;

    // In the shader: uniform sampler2D u_previousFrame;
    void enableFeedback();

    // Resize internal render targets (called by ShaderWindow)
    void resize(int w, int h);

    // Clear both ping-pong buffers to zero (call after camera jump etc.)
    void clearFeedback() const;

    bool reload();


    template<typename T>
    [[nodiscard]] Uniform<T> createUniform(const std::string& name) const {
        return Uniform<T>(program, name);
    }

    [[nodiscard]] Texture createTexture(const std::string& uniformName);
    [[nodiscard]] Texture createTexture(const std::string& uniformName, const std::string& path);
    void releaseTexture(const Texture& tex) {
        freeUnits.push_back(tex.getUnit());
    }

    // inputTex = 0 means "no input" (first shader in chain)
    void execute(GLuint inputTex, bool toScreen);

    [[nodiscard]] GLuint outputTexture() const {
       return hasFeedback ? tex[1 - write] : tex[write];
    }
    [[nodiscard]] GLuint getProgram()    const { return program; }
    void bind() const { glUseProgram(program); }
};

#endif //SHADER_H
