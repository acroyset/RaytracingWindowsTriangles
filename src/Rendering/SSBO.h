//
// Created by acroy on 11/12/2025.
//

#ifndef SSBO_H
#define SSBO_H

#define GLAD_GL_IMPLEMENTATION
#include <iostream>
#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>

#if defined(__APPLE__) || defined(__MACH__)
    #define PLATFORM_MAC
    #define GL_SHADER_STORAGE_BUFFER 0
#endif

using namespace glm;

template<typename T>

class SSBO {
    GLuint ssbo;

public:

    SSBO() {
        #ifdef PLATFORM_MAC
        std::cerr << "SSBO not available on Mac (OpenGL version 4.3 required not available on mac)" << std::endl;
        #endif
        ssbo = 0;
    }

    explicit SSBO(GLuint ssbo) {
        #ifdef PLATFORM_MAC
        std::cerr << "SSBO not available on Mac (OpenGL version 4.3 required not available on mac)" << std::endl;
        #endif
        this->ssbo = ssbo;
    }

    void set(std::vector<T> data, const int id, const bool dynamicDraw = false) {
        glGenBuffers(1, &ssbo);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
        glBufferData(GL_SHADER_STORAGE_BUFFER, int(data.size() * sizeof(T)), data.data(), dynamicDraw ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, id, ssbo);
    }

    void update(int startIdx, int endIdx, const T* data) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
        const GLsizeiptr offset = (GLsizeiptr)startIdx * sizeof(T);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, sizeof(T) * (endIdx-startIdx), data);
    }

    void update(int startIdx, T data) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
        const GLsizeiptr offset = (GLsizeiptr)startIdx * sizeof(T);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, sizeof(T), &data);
    }
};

#endif //SSBO_H
