cmake_minimum_required(VERSION 3.16)
project(RaytracingWindowsTriangles)

set(CMAKE_CXX_STANDARD 17)

# GLFW
find_package(OpenGL REQUIRED)
add_subdirectory(external/glfw)

# GLAD
add_library(glad "external/glad/src/glad.c")
target_include_directories(glad PUBLIC "external/glad/include")
include_directories(glm)

# Executable
add_executable(RaytracingWindowsTriangles src/main.cpp
        src/Scene.cpp
        src/BaseModel.cpp
        src/BaseModel.h)
target_link_libraries(RaytracingWindowsTriangles glfw glad OpenGL::GL)
target_include_directories(RaytracingWindowsTriangles PRIVATE external/glad/include)


