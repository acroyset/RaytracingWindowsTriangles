//
// Created by acroy on 2/20/2026.
//

#ifndef SHADERPASS_H
#define SHADERPASS_H

#include <glad/glad.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

static std::string readFile(const char* path) {
    const std::ifstream f(path, std::ios::binary);
    if (!f) { std::cerr << "Cannot open: " << path << "\n"; return {}; }
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}
static std::string injectVersion(const std::string& src, const std::string& ver) {
    std::string body = src;
    size_t i = 0;
    while (i < body.size() && std::isspace((unsigned char)body[i])) ++i;
    if (i < body.size() && body.compare(i, 8, "#version") == 0) {
        size_t end = body.find('\n', i);
        body.replace(i, end - i, "#version " + ver);
    } else {
        body = "#version " + ver + "\n" + body;
    }
    return body;
}
static std::string mergeIncludes(const char* mainPath, const std::vector<std::string>& includes) {
    std::string out;
    for (const auto& p : includes) out += readFile(p.c_str()) + "\n\n";
    out += readFile(mainPath);
    return out;
}
static GLuint compileShader(GLenum type, const std::string& src) {
    GLuint sh = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(sh, 1, &c, nullptr);
    glCompileShader(sh);
    GLint ok; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len; glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetShaderInfoLog(sh, len, nullptr, log.data());
        std::cerr << "Shader compile error:\n" << log << "\n";
    }
    return sh;
}

class ShaderPass {
public:
    bool enabled = true;

    virtual ~ShaderPass() = default;

    // inputTex: output of the previous pass (0 if first in chain)
    // toScreen: blit result to the default framebuffer
    virtual void   execute(GLuint inputTex, bool toScreen) = 0;
    [[nodiscard]] virtual GLuint outputTexture() const = 0;
    virtual void   resize(int w, int h) = 0;

    // Optional overrides — default no-ops so simple passes don't need them
    virtual void clearFeedback() {}
    virtual bool reload()        { return false; }
};

#endif //SHADERPASS_H
