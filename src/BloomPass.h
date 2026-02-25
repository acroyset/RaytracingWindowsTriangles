//
// Created by acroy on 2/20/2026.
//

#ifndef BLOOMPASS_H
#define BLOOMPASS_H

#include "Rendering/ShaderPass.h"
#include <string>
#include <glad/glad.h>

class BloomPass : public ShaderPass {
public:
    float threshold   = 1.0f;
    float knee        = 0.2f;
    float strength    = 0.02f;
    int   numMips     = 12;
    float persistence = 0.97f;

    BloomPass() = default;
    ~BloomPass() override;

    void init(const std::string& glslVersion);

    // ── ShaderPass interface ──────────────────────────────────────
    // inputTex: the HDR scene (from raytracer)
    // outputTexture: HDR + bloom composited, ready for PP to tonemap
    void   execute(GLuint inputTex, bool toScreen) override;
    [[nodiscard]] GLuint outputTexture() const override;
    void   resize(int w, int h) override;
    bool   reload() override;

private:

    int allocatedMips = 0;

    struct Mip {
        GLuint fbo  = 0;
        GLuint tex  = 0;
        GLuint fboH = 0;   // intermediate for separable H pass
        GLuint texH = 0;
        int w = 0, h = 0;
    };

    static constexpr int MAX_MIPS = 16;

    Mip  threshMip{};
    Mip  downMips[MAX_MIPS]{};
    Mip  upMips  [MAX_MIPS]{};
    Mip  compositeMip{};        // full-res: hdr + bloom*strength
    int  fullW = 0, fullH = 0;
    GLuint vao = 0;

    std::string glslVersion;

    // Programs
    GLuint threshProg    = 0;
    GLuint downHProg = 0;
    GLuint downVProg = 0;
    GLuint upProg        = 0;
    GLuint compositeProg = 0;

    // Threshold
    GLint uThreshSrc   = -1, uThreshTexel = -1;
    GLint uThresholdVal= -1, uKneeVal     = -1;

    // Downsample
    GLint  uDownHSrc = -1, uDownHTexel = -1;
    GLint  uDownVSrc = -1, uDownVTexel = -1;

    // Upsample
    GLint uUpSmaller = -1, uUpCurrent = -1, uUpTexel = -1, uUpMixWeight = -1;

    // Composite
    GLint uCompHDR      = -1;
    GLint uCompBloom    = -1;
    GLint uCompStrength = -1;

    void allocMip(Mip& m, int w, int h);
    static void freeMip(Mip& m);
    void allocChain();
    void freeChain();
    void drawFullscreen() const;
    [[nodiscard]] GLuint buildProgram(const std::string& fragPath) const;
    void   buildAllPrograms();
    void   cacheUniforms();
};



#endif //BLOOMPASS_H
