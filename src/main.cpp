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

#include "stb_image.h"


GLFWwindow* window = nullptr;
GLuint shaderProgram = 0;
GLuint displayShader = 0;
GLuint vao = 0;

GLuint pingpongFBO[2];
GLuint pingpongTex[2];

void createPingPongBuffers(int width, int height) {
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongTex);

    for (int i = 0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_2D, pingpongTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongTex[i], 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "Ping-pong FBO " << i << " not complete!\n";
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
std::string loadShaderSource(const char* path) {
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}
GLuint compileShader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "Shader compilation failed:\n" << log << std::endl;
    }
    return shader;
}
GLuint createShaderProgram(const char* vertPath, const char* fragPath) {
    GLuint vert = compileShader(GL_VERTEX_SHADER, loadShaderSource(vertPath));
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, loadShaderSource(fragPath));
    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    glDeleteShader(vert);
    glDeleteShader(frag);
    return program;
}
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}
bool setup() {
    if (!glfwInit()) return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    window = glfwCreateWindow(mode->width, mode->height, "Modular OpenGL Shader Window", monitor, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, keyCallback);
    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return false;
    }

    shaderProgram = createShaderProgram("shaders/fullscreen.vert", "shaders/fullscreen.frag");
    displayShader = createShaderProgram("shaders/fullscreen.vert", "shaders/display.frag");

    //glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // --- ImGui init ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // optional docking
    ImGui::StyleColorsDark();

    // Match your GL profile (you requested 4.3 core)
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 430");


    return true;
}
bool shouldClose() {
    return glfwWindowShouldClose(window);
}
void shutdown() {
    glDeleteProgram(shaderProgram);
    glDeleteVertexArrays(1, &vao);
    glfwDestroyWindow(window);
    glfwTerminate();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

}
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
    if (!setup()) return -1;

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    Scene scene(width, height, 1,3, 32);

    Timer t;

    scene.addModel("models/cube/cube.obj", glm::vec3(0, -20, 0), glm::vec3(300, 20, 300), glm::vec3(0.9), 0.3);
    scene.addModel("models/dragon800K.obj", glm::vec3(0, 70.498, 0), glm::vec3(100), glm::vec3(0.01), 0.99, glm::vec3(0.15), 0.4);
    scene.addModel("models/sphere.obj", glm::vec3(200, 50, 0), glm::vec3(50), glm::vec3(1, 0.7, 0.3), 0, glm::vec3(0), 0, 0, 1, 2);

    //scene.addModel("models/quad.txt", glm::vec3(0,4999, 0), glm::vec3(1000), glm::vec3(0.95), 0, glm::vec3(0), 0, 0, 1, 20);
    //scene.addModel("models/cubeInternal.txt", glm::vec3(0,2000, 0), glm::vec3(3000), glm::vec3(0.95), 0);
    //scene.addModel(dragon, glm::vec3(0,762.30, 0), glm::vec3(2500), glm::vec3(0.01), 0.99, glm::vec3(0.95), 0.15);


    float duration = t.reset();

    // Bind before drawing
    GLuint skyTex = LoadEnvLatLongTextureAuto("sky.png");

    //scene.displayBVH();

    scene.set_ssbo();

    createPingPongBuffers(width, height);
    int ping = 0; int pong = 1;

    // display bvh stats
    if (false) {
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

    // render loop
    Timer deltaTimer;
    while (!shouldClose()) {
        // delta time
        const auto dt = float(deltaTimer.reset());

        //bind to ping buffer
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[ping]);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glActiveTexture(GL_TEXTURE0 + 5); // choose a slot
        glBindTexture(GL_TEXTURE_2D, skyTex);
        glUniform1i(glGetUniformLocation(shaderProgram, "uEnvLatLong"), 5);

        // Optional: yaw rotation for the sky (in radians)
        glUniform1f(glGetUniformLocation(shaderProgram, "uEnvYaw"), 0.0f);

        // update camera / uniforms
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        scene.updateFrame(shaderProgram, *window, dt);

        // set uniforms for display shader
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, pingpongTex[pong]);
        glUniform1i(glGetUniformLocation(shaderProgram, "uPrevFrame"), 0);

        glDrawArrays(GL_TRIANGLES, 0, 3);

        // bind to screen
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(displayShader); // just draws the texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, pingpongTex[ping]);
        glUniform1i(glGetUniformLocation(displayShader, "screenTex"), 0);
        glUniform1f(glGetUniformLocation(displayShader, "uExposure"), 1.0f);

        glDrawArrays(GL_TRIANGLES, 0, 3);

        // Swap ping-pong buffers
        std::swap(ping, pong);

        // >>> ImGui render on top <<<
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    shutdown();
    return 0;
}


