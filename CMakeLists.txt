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

# --- Executable (CREATE THE TARGET FIRST) ---
add_executable(RaytracingWindowsTriangles
        src/main.cpp
        src/Scene.cpp
        src/BaseModel.cpp
        src/Rendering/ShaderWindow.cpp
        src/Rendering/Texture.cpp
        src/SceneUI.cpp
        src/Rendering/Shader.cpp
        src/BloomPass.cpp
        src/UniformBlocks.h
)
target_include_directories(RaytracingWindowsTriangles PRIVATE
        external/glad/include
        external/glfw/include
        external/glm
)
target_link_libraries(RaytracingWindowsTriangles PRIVATE glfw glad OpenGL::GL)

# --- ImGui (FetchContent) ---
include(FetchContent)
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_glfw_opengl3
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_glfw_opengl3 PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        external/glad/include
        external/glfw/include
)
target_compile_definitions(RaytracingWindowsTriangles PRIVATE
        PROJECT_DIR="${CMAKE_SOURCE_DIR}/")
target_compile_definitions(imgui_glfw_opengl3 PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)
target_link_libraries(imgui_glfw_opengl3 PUBLIC glfw glad)

# Link ImGui to your app (now that the app target exists)
target_link_libraries(RaytracingWindowsTriangles PRIVATE imgui_glfw_opengl3)

# OIDN
set(OIDN_DIR "${CMAKE_SOURCE_DIR}/external/oidn")

target_include_directories(RaytracingWindowsTriangles PRIVATE
        "${OIDN_DIR}/include"
)

target_link_libraries(RaytracingWindowsTriangles PRIVATE
        "${OIDN_DIR}/lib/OpenImageDenoise.lib"
)

# Copy DLLs to output directory
add_custom_command(TARGET RaytracingWindowsTriangles POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${OIDN_DIR}/bin"
        $<TARGET_FILE_DIR:RaytracingWindowsTriangles>
)
