//
// Created by acroy on 2/20/2026.
//

#include "BloomPass.h"

#include <cmath>

GLuint BloomPass::buildProgram(const std::string& fragPath) const {
    std::string ver  = "#version " + glslVersion + "\n";
    std::string vert = ver + readFile("src/shaders/fullscreen.vert");
    std::string frag = ver + readFile(fragPath.c_str());

    GLuint vs = compileShader(GL_VERTEX_SHADER,   vert);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, frag);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);

    GLint ok; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len; glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        std::cerr << "BloomPass link error:\n" << log << "\n";
        glDeleteProgram(prog); return 0;
    }
    return prog;
}

void BloomPass::cacheUniforms() {
    uThreshSrc    = glGetUniformLocation(threshProg, "u_src");
    uThreshTexel  = glGetUniformLocation(threshProg, "u_texelSize");
    uThresholdVal = glGetUniformLocation(threshProg, "u_threshold");
    uKneeVal      = glGetUniformLocation(threshProg, "u_knee");

    uDownHSrc     = glGetUniformLocation(downHProg, "u_src");
    uDownHTexel   = glGetUniformLocation(downHProg, "u_texelSize");

    uDownVSrc     = glGetUniformLocation(downVProg, "u_src");
    uDownVTexel   = glGetUniformLocation(downVProg, "u_texelSize");

    uUpSmaller    = glGetUniformLocation(upProg, "u_smaller");
    uUpCurrent    = glGetUniformLocation(upProg, "u_current");
    uUpTexel      = glGetUniformLocation(upProg, "u_texelSize");
    uUpMixWeight   = glGetUniformLocation(upProg, "u_weight");

    uCompHDR      = glGetUniformLocation(compositeProg, "u_hdr");
    uCompBloom    = glGetUniformLocation(compositeProg, "u_bloom");
    uCompStrength = glGetUniformLocation(compositeProg, "u_strength");
}

void BloomPass::buildAllPrograms() {
    if (threshProg)    glDeleteProgram(threshProg);
    if (downHProg)     glDeleteProgram(downHProg);
    if (downVProg)     glDeleteProgram(downVProg);
    if (upProg)        glDeleteProgram(upProg);
    if (compositeProg) glDeleteProgram(compositeProg);

    threshProg    = buildProgram("src/shaders/bloom_threshold.frag");
    downHProg     = buildProgram("src/shaders/bloom_downsample_h.frag");
    downVProg     = buildProgram("src/shaders/bloom_downsample_v.frag");
    upProg        = buildProgram("src/shaders/bloom_upsample.frag");
    compositeProg = buildProgram("src/shaders/bloom_composite.frag");

    cacheUniforms();
}

void BloomPass::init(const std::string& version) {
    glslVersion = version;
    glGenVertexArrays(1, &vao);
    buildAllPrograms();
}

bool BloomPass::reload() {
    buildAllPrograms();
    return threshProg && downHProg && downVProg && upProg && compositeProg;
}

void BloomPass::allocMip(Mip& m, int w, int h) {
    m.w = w; m.h = h;

    auto makeTexFBO = [](GLuint& tex, GLuint& fbo, int w, int h) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    };

    makeTexFBO(m.tex,  m.fbo,  w, h);
    makeTexFBO(m.texH, m.fboH, w, h);  // same size, just intermediate
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    allocatedMips = numMips;
}

void BloomPass::freeMip(Mip& m) {
    if (m.tex)  glDeleteTextures(1, &m.tex);
    if (m.fbo)  glDeleteFramebuffers(1, &m.fbo);
    if (m.texH) glDeleteTextures(1, &m.texH);
    if (m.fboH) glDeleteFramebuffers(1, &m.fboH);
    m = {};
}

void BloomPass::allocChain() {
    allocMip(threshMip,    fullW, fullH);
    allocMip(compositeMip, fullW, fullH);

    int w = fullW, h = fullH;
    for (int i = 0; i < numMips; i++) {
        w = std::max(1, w / 2);
        h = std::max(1, h / 2);
        allocMip(downMips[i], w, h);
        allocMip(upMips[i],   w, h);
    }
}

void BloomPass::freeChain() {
    freeMip(threshMip);
    freeMip(compositeMip);
    for (int i = 0; i < MAX_MIPS; i++) {
        freeMip(downMips[i]);
        freeMip(upMips[i]);
    }
}

BloomPass::~BloomPass() {
    freeChain();
    if (vao)          glDeleteVertexArrays(1, &vao);
    if (threshProg)   glDeleteProgram(threshProg);
    if (downHProg)    glDeleteProgram(downHProg);
    if (downVProg)    glDeleteProgram(downVProg);
    if (upProg)       glDeleteProgram(upProg);
    if (compositeProg)glDeleteProgram(compositeProg);
}

void BloomPass::resize(int w, int h) {
    if (w == fullW && h == fullH) return;
    fullW = w; fullH = h;
    freeChain();
    allocChain();
}

void BloomPass::drawFullscreen() const {
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void BloomPass::execute(GLuint inputTex, bool toScreen) {
    if (!enabled || !threshProg) return;

    if (numMips != allocatedMips) {
        freeChain();
        allocChain();
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    // ── 1. Threshold ──────────────────────────────────────────────
    glUseProgram(threshProg);
    glBindFramebuffer(GL_FRAMEBUFFER, threshMip.fbo);
    glViewport(0, 0, threshMip.w, threshMip.h);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTex);
    glUniform1i(uThreshSrc, 0);
    glUniform2f(uThreshTexel,  1.0f/float(threshMip.w), 1.0f/float(threshMip.h));
    glUniform1f(uThresholdVal, threshold);
    glUniform1f(uKneeVal,      knee);
    drawFullscreen();

    // ── 2. Downsample chain ───────────────────────────────────────
    GLuint srcTex = threshMip.tex;
    int    srcW   = threshMip.w;
    int    srcH   = threshMip.h;

    for (int i = 0; i < numMips; i++) {
        // Horizontal pass → intermediate texH
        glUseProgram(downHProg);
        glBindFramebuffer(GL_FRAMEBUFFER, downMips[i].fboH);
        glViewport(0, 0, downMips[i].w, downMips[i].h);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, srcTex);
        glUniform1i(uDownHSrc, 0);
        glUniform2f(uDownHTexel, 1.0f / float(srcW), 1.0f / float(srcH));
        drawFullscreen();

        // Vertical pass → final tex
        glUseProgram(downVProg);
        glBindFramebuffer(GL_FRAMEBUFFER, downMips[i].fbo);
        glViewport(0, 0, downMips[i].w, downMips[i].h);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, downMips[i].texH);
        glUniform1i(uDownVSrc, 0);
        glUniform2f(uDownVTexel, 1.0f / float(srcW), 1.0f / float(srcH));
        drawFullscreen();

        srcTex = downMips[i].tex;
        srcW   = downMips[i].w;
        srcH   = downMips[i].h;
    }

    // ── 3. Upsample chain ─────────────────────────────────────────
    glUseProgram(upProg);

    auto currentWeight = float(std::pow(persistence, numMips));
    for (int i = numMips - 1; i >= 0; i--) {
        glBindFramebuffer(GL_FRAMEBUFFER, upMips[i].fbo);
        glViewport(0, 0, upMips[i].w, upMips[i].h);

        GLuint smallerTex = (i == numMips - 1) ? downMips[i].tex : upMips[i + 1].tex;
        int    sW         = (i == numMips - 1) ? downMips[i].w   : upMips[i + 1].w;
        int    sH         = (i == numMips - 1) ? downMips[i].h   : upMips[i + 1].h;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, smallerTex);
        glUniform1i(uUpSmaller, 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, downMips[i].tex);
        glUniform1i(uUpCurrent, 1);

        glUniform2f(uUpTexel, 1.0f/float(sW), 1.0f/float(sH));

        glUniform1f(uUpMixWeight, currentWeight);
        currentWeight /= persistence;
        drawFullscreen();
    }

    // ── 4. Composite: hdr + bloom → full-res output ───────────────
    glUseProgram(compositeProg);
    glBindFramebuffer(GL_FRAMEBUFFER, compositeMip.fbo);
    glViewport(0, 0, compositeMip.w, compositeMip.h);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTex);       // original HDR
    glUniform1i(uCompHDR, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, upMips[0].tex);  // accumulated bloom
    glUniform1i(uCompBloom, 1);

    glUniform1f(uCompStrength, strength);
    drawFullscreen();

    // ── 5. Optionally blit to screen ──────────────────────────────
    if (toScreen) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, compositeMip.fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, fullW, fullH, 0, 0, fullW, fullH,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint BloomPass::outputTexture() const {
    return compositeMip.tex;  // HDR + bloom, ready for PP to tonemap
}
