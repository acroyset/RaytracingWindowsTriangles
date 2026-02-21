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
#include "ShaderPass.h"

class Shader : public ShaderPass {
    GLuint program   = 0;
    GLuint vao       = 0;
    GLuint fbo[2]    = {0, 0};
    GLuint tex[2]    = {0, 0};
    int    write     = 0;
    int    width     = 0;
    int    height    = 0;
    bool   hasFeedback = false;

    std::string vertPath, fragPath, glslVersion;
    std::vector<std::string> includePaths;

    int nextUnit = 2;
    std::vector<int> freeUnits;

    GLint locInput    = -1;
    GLint locPrevious = -1;

    void buildProgram(const char* vertPath, const char* fragPath,
                      const std::string& version,
                      const std::vector<std::string>& includes);
    void allocateFBOs(int w, int h);

    int getNextUnit() {
        if (!freeUnits.empty()) { int u = freeUnits.back(); freeUnits.pop_back(); return u; }
        return nextUnit++;
    }

public:
    Shader(const char* vertPath, const char* fragPath,
           const std::string& version,
           const std::vector<std::string>& includes = {});

    ~Shader() override;

    Shader(const Shader&)            = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&&) noexcept;

    // ── ShaderPass interface ──────────────────────────────────────
    void   execute(GLuint inputTex, bool toScreen) override;
    [[nodiscard]] GLuint outputTexture() const override;
    void   resize(int w, int h) override;
    void   clearFeedback()      override;
    bool   reload()             override;

    // ── Shader-specific extras ────────────────────────────────────
    void enableFeedback();

    template<typename T>
    [[nodiscard]] Uniform<T> createUniform(const std::string& name) const {
        return Uniform<T>(program, name);
    }

    [[nodiscard]] Texture createTexture(const std::string& uniformName);
    [[nodiscard]] Texture createTexture(const std::string& uniformName, const std::string& path);
    void releaseTexture(const Texture& t) { freeUnits.push_back(t.getUnit()); }

    [[nodiscard]] GLuint getProgram() const { return program; }
    void bind() const { glUseProgram(program); }
};

#endif //SHADER_H
