cmake_minimum_required(VERSION 3.16)
project(RaytracingWindowsTriangles)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(OpenGL REQUIRED)

# Try to find GLFW - this is the most common issue
find_package(glfw3 REQUIRED)
# If the above fails, try this alternative:
# find_package(PkgConfig REQUIRED)
# pkg_check_modules(GLFW REQUIRED glfw3)

# GLAD
add_library(glad "external/glad/src/glad.c")
target_include_directories(glad PUBLIC "external/glad/include")

# GLM include directory
include_directories(glm)

# Executable
add_executable(RaytracingWindowsTriangles
        main.cpp
        Scene.cpp
        BaseModel.cpp
        BaseModel.h
        Scene.h  # Add this if you have Scene.h
)

# Link libraries - order matters!
target_link_libraries(RaytracingWindowsTriangles
        glad
        glfw
        OpenGL::GL
        ${CMAKE_DL_LIBS}  # For dynamic loading on Linux
)

# Include directories
target_include_directories(RaytracingWindowsTriangles PRIVATE
        external/glad/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Platform-specific linking
if(UNIX AND NOT APPLE)
    target_link_libraries(RaytracingWindowsTriangles dl)
endif()