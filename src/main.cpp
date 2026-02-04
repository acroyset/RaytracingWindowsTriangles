#include <ctime>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Scene.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"


#define STB_IMAGE_IMPLEMENTATION
#include <filesystem>

#include "ShaderWindow.h"
#include "stb_image.h"


static void setDefault2DParams() {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);        // horiz repeat
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // clamp vertically
}
GLuint LoadEnvLatLongTextureAuto(const char* path) {
    stbi_set_flip_vertically_on_load(false); // equirect usually not flipped

    int w=0, h=0, n=0;  // n = channels in file
    GLuint tex=0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    // Alignment fix (prevents rainbow banding on RGB 3-byte rows)
    GLint prevAlign = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevAlign);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    if (stbi_is_hdr(path)) {
        float* data = stbi_loadf(path, &w, &h, &n, 0); // keep original n (3 or 4)
        if (!data) {
            fprintf(stderr, "HDR load failed: %s (%s)\n", path, stbi_failure_reason());
            glBindTexture(GL_TEXTURE_2D, 0);
            glDeleteTextures(1, &tex);
            glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlign);
            return 0;
        }
        GLenum srcFmt = (n == 4) ? GL_RGBA : GL_RGB;
        GLint  dstFmt = (n == 4) ? GL_RGBA16F : GL_RGB16F; // linear HDR
        glTexImage2D(GL_TEXTURE_2D, 0, dstFmt, w, h, 0, srcFmt, GL_FLOAT, data);
        stbi_image_free(data);
    } else {
        unsigned char* data = stbi_load(path, &w, &h, &n, 0); // keep original n (3 or 4)
        if (!data) {
            fprintf(stderr, "LDR load failed: %s (%s)\n", path, stbi_failure_reason());
            glBindTexture(GL_TEXTURE_2D, 0);
            glDeleteTextures(1, &tex);
            glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlign);
            return 0;
        }
        GLenum srcFmt = (n == 4) ? GL_RGBA : GL_RGB;
        // sRGB internal formats → sampling returns LINEAR color automatically
        GLint  dstFmt = (n == 4) ? GL_SRGB8_ALPHA8 : GL_SRGB8;
        glTexImage2D(GL_TEXTURE_2D, 0, dstFmt, w, h, 0, srcFmt, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    }



    setDefault2DParams();
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

class Timer {
    std::clock_t start;
    std::clock_t pause{};
    bool paused = false;

public:
    explicit Timer(const bool paused = false) {
        start = std::clock();
        this->paused = paused;
        if (paused) {
            pause = std::clock();
        }
    }

    float reset() {
        const std::clock_t end = std::clock();
        const float t = (float)(end - start) / CLOCKS_PER_SEC;
        start = end;
        return t;
    }
    [[nodiscard]] float elapsed() const {
        const std::clock_t offset = paused ? std::clock() - pause : 0;
        const std::clock_t end = std::clock();
        const float t = (float)(end - start - offset) / CLOCKS_PER_SEC;
        return t;
    }
    void start_stop() {
        if (paused) {
            paused = false;
            const std::clock_t elapsed = std::clock() - pause;
            start += elapsed;
        } else {
            paused = true;
            pause = std::clock();
        }
    }
};

int main() {
    ShaderWindow window{};
    window.setFeedbackMode(true);

    unsigned int width = window.size().x;
    unsigned int height = window.size().y;
    Scene scene(int(width), int(height), 1,3, 16);

    Timer t;

    scene.addModel("models/bull.obj", vec3(0, 50, 0), vec3(100), vec3(0.9), 0.3);
    scene.addModel("models/cube/cube.obj", vec3(0, -20, 0), vec3(300, 20, 300), vec3(0.9), 0.3);
    //scene.addModel("models/dragon800K.obj", vec3(0, 70.498, 0), vec3(100), vec3(0.01), 0.99, vec3(0.15), 0.4);
    //scene.addModel("models/sphere.obj", vec3(200, 50, 0), vec3(50), vec3(1, 0.7, 0.3), 0, vec3(0), 0, 0, 1, 2);

    //scene.addModel("models/quad.txt", vec3(0,4999, 0), vec3(1000), vec3(0.95), 0, vec3(0), 0, 0, 1, 20);
    //scene.addModel("models/cubeInternal.txt", vec3(0,2000, 0), vec3(3000), vec3(0.95), 0);
    //scene.addModel(dragon, vec3(0,762.30, 0), vec3(2500), vec3(0.01), 0.99, vec3(0.95), 0.15);


    float duration = t.reset();

    // Bind before drawing
    GLuint skyTex = LoadEnvLatLongTextureAuto("sky.png");

    //scene.displayBVH();

    scene.set_ssbo();

    const auto uEnvLatLong = window.createUniform<int>("uEnvLatLong");
    const auto uEnvYaw = window.createUniform<float>("uEnvYaw");

    // display bvh stats
    if (true) {
        int leafNodes = 0, depth = 0, triPerLeaf = 0;
        int minTriPerLeaf = 100000000, maxTriPerLeaf = 0;
        int minDepth = 100000000, maxDepth = 0;
        scene.get_BVH_stats(0, leafNodes, depth, minDepth, maxDepth, triPerLeaf, minTriPerLeaf, maxTriPerLeaf, 1);
        std::cout << std::endl;
        std::cout << "Time (ms): " << duration*1000.0f << std::endl;
        std::cout << "Triangles: " << scene.getNumTris() << std::endl;
        std::cout << "Node Count: " << scene.getNumBVHNodes() << std::endl;
        std::cout << "Leaf Count: " << leafNodes << std::endl;
        std::cout << "Leaf Depth: " << std::endl;
        std::cout << "  -  Min: " << minDepth << std::endl;
        std::cout << "  -  Max: " << maxDepth << std::endl;
        std::cout << "  -  Mean: " << float(depth)/float(leafNodes) << std::endl;
        std::cout << "Leaf Tris: " << std::endl;
        std::cout << "  -  Min: " << minTriPerLeaf << std::endl;
        std::cout << "  -  Max: " << maxTriPerLeaf << std::endl;
        std::cout << "  -  Mean: " << float(triPerLeaf)/float(leafNodes) << std::endl;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    bool apple = false;

#ifdef PLATFORM_MAC
    apple = true;
#endif

    const char* glsl_version = apple ? "#version 330" : "#version 430";
    ImGui_ImplGlfw_InitForOpenGL(window.getWindow(), true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // render loop
    while (window.open()) {
        window.start();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        scene.updateFrame(window.getShaderProgram(), window.getWindow(), window.getDeltaTime());


        glActiveTexture(GL_TEXTURE0 + 5); // choose a slot
        glBindTexture(GL_TEXTURE_2D, skyTex);
        uEnvLatLong.set(5);
        uEnvYaw.set(0.0f);

        window.render();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


        glfwSwapBuffers(window.getWindow());
        glfwPollEvents();
    }



    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    return 0;
}


