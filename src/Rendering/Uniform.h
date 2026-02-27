// Uniform.h
#ifndef UNIFORM_H
#define UNIFORM_H

#include <iostream>
#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

struct UniformBase {
    UniformBase() = default;

    GLint location = -1;
    explicit UniformBase(GLuint program, const std::string& name) {
        location = glGetUniformLocation(program, name.c_str());

         if (location == -1) std::cerr << "Uniform not found: " << name << "\n";
    }
    [[nodiscard]] bool valid() const { return location != -1; }
};

template<typename T>
struct Uniform : UniformBase {
    using UniformBase::UniformBase;

    void set(const T& value) const {
        static_assert(always_false<T>,
            "Uniform<T>::set called with unsupported type T. "
            "Add a specialization or extend the if constexpr.");
    }

    void setArray(const T* data, GLsizei count) const {
        static_assert(always_false<T>,
            "Uniform<T>::setArray called with unsupported type T array.");
    }

private:
    template<class> static constexpr bool always_false = false;
};


template<typename T>
struct UniformFields;

template<typename T>
struct UniformBlock : UniformBase {

    UniformBlock() = default;

    UniformBlock(GLuint program, const std::string& name)
        : UniformBase()
        , fields(program, name)
    {}

    void set(const T& value) const {
        fields.set(value);
    }

private:
    UniformFields<T> fields;
};


// ---- Implementations for supported Ts ----

template<>
inline void Uniform<bool>::set(const bool& v) const {
    if (!valid()) return;
    glUniform1i(location, v);
}

// float
template<>
inline void Uniform<float>::set(const float& v) const {
    if (!valid()) return;
    glUniform1f(location, v);
}

// int
template<>
inline void Uniform<int>::set(const int& v) const {
    if (!valid()) return;
    glUniform1i(location, v);
}

// unsigned int
template<>
inline void Uniform<unsigned int>::set(const unsigned int& v) const {
    if (!valid()) return;
    glUniform1ui(location, v);
}

// glm::vec2
template<>
inline void Uniform<glm::vec2>::set(const glm::vec2& v) const {
    if (!valid()) return;
    glUniform2fv(location, 1, glm::value_ptr(v));
}

// glm::vec3
template<>
inline void Uniform<glm::vec3>::set(const glm::vec3& v) const {
    if (!valid()) return;
    glUniform3fv(location, 1, glm::value_ptr(v));
}

// glm::vec4
template<>
inline void Uniform<glm::vec4>::set(const glm::vec4& v) const {
    if (!valid()) return;
    glUniform4fv(location, 1, glm::value_ptr(v));
}

// glm::ivec2
template<>
inline void Uniform<glm::ivec2>::set(const glm::ivec2& v) const {
    if (!valid()) return;
    glUniform2iv(location, 1, glm::value_ptr(v));
}

// glm::ivec3
template<>
inline void Uniform<glm::ivec3>::set(const glm::ivec3& v) const {
    if (!valid()) return;
    glUniform3iv(location, 1, glm::value_ptr(v));
}

// glm::ivec4
template<>
inline void Uniform<glm::ivec4>::set(const glm::ivec4& v) const {
    if (!valid()) return;
    glUniform4iv(location, 1, glm::value_ptr(v));
}

// glm::uvec2
template<>
inline void Uniform<glm::uvec2>::set(const glm::uvec2& v) const {
    if (!valid()) return;
    glUniform2uiv(location, 1, glm::value_ptr(v));
}

// glm::uvec3
template<>
inline void Uniform<glm::uvec3>::set(const glm::uvec3& v) const {
    if (!valid()) return;
    glUniform3uiv(location, 1, glm::value_ptr(v));
}

// glm::uvec4
template<>
inline void Uniform<glm::uvec4>::set(const glm::uvec4& v) const {
    if (!valid()) return;
    glUniform4uiv(location, 1, glm::value_ptr(v));
}

// glm::mat3
template<>
inline void Uniform<glm::mat3>::set(const glm::mat3& m) const {
    if (!valid()) return;
    glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(m));
}

// glm::mat4
template<>
inline void Uniform<glm::mat4>::set(const glm::mat4& m) const {
    if (!valid()) return;
    glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(m));
}

// array float
template<>
inline void Uniform<float>::setArray(const float* data, GLsizei n) const {
    if (!valid()) return;
    glUniform1fv(location, n, data);
}

// array int
template<>
inline void Uniform<int>::setArray(const int* data, GLsizei n) const {
    if (!valid()) return;
    glUniform1iv(location, n, data);
}

//array uint
template<>
inline void Uniform<unsigned int>::setArray(const unsigned int* data, GLsizei n) const {
    if (!valid()) return;
    glUniform1uiv(location, n, data);
}

//array vec3
template<>
inline void Uniform<glm::vec3>::setArray(const glm::vec3* data, GLsizei n) const {
    if (!valid()) return;
    glUniform3fv(location, n, glm::value_ptr(data[0]));
}

//array mat4
template<>
inline void Uniform<glm::mat4>::setArray(const glm::mat4* data, GLsizei n) const {
    if (!valid()) return;
    glUniformMatrix4fv(location, n, GL_FALSE, glm::value_ptr(data[0]));
}

#endif // UNIFORM_H