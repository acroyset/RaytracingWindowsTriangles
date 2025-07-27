#include <ctime>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Scene.h"


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
    Scene scene(width, height, 4,2, 12);

    Timer t;

    //scene.addModel("dragon80K.txt", glm::vec3(-1200, -929.502, 500), glm::vec3(100, 100, 100), glm::vec3(0.8, 0.6, 0.1), 0.3, 0);
    //scene.addModel("dragon80K.txt", glm::vec3(-1000, -859.004, 500), glm::vec3(200, 200, 200), glm::vec3(0.1, 0.8, 0.1), 0.3, 0);
    //scene.addModel("dragon80K.txt", glm::vec3(-630, -647.51, 500), glm::vec3(500, 500, 500), glm::vec3(0.1, 0.1, 0.8), 0.3, 0);
    scene.addModel("dragon800K.txt", glm::vec3(0, -436.016, 500), glm::vec3(800, 800, 800), glm::vec3(0.8, 0.6, 0.1), 0.9, 0);
    //scene.addModel("sponza.txt", glm::vec3(0, 0, 500), glm::vec3(800, 800, 800), glm::vec3(0.9, 0.2, 0.2), 0.6, 0);

    float duration = t.reset();


    std::string prefix;
    //scene.displayBVH(0, prefix);

    scene.set_ssbo();

    createPingPongBuffers(width, height);
    int ping = 0; int pong = 1;

    if (true) {
        int leafNodes = 0, depth = 0, triPerLeaf = 0;
        int minTriPerLeaf = 10000, maxTriPerLeaf = 0;
        int minDepth = 10000, maxDepth = 0;
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

    Timer deltaTimer;
    while (!shouldClose()) {
        const auto dt = float(deltaTimer.reset());
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[ping]);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaderProgram);

        scene.updateFrame(shaderProgram, *window, dt);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, pingpongTex[pong]);
        glUniform1i(glGetUniformLocation(shaderProgram, "uPrevFrame"), 0);

        glDrawArrays(GL_TRIANGLES, 0, 3);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(displayShader); // just draws the texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, pingpongTex[ping]);
        glUniform1i(glGetUniformLocation(displayShader, "screenTex"), 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // Swap ping-pong buffers
        std::swap(ping, pong);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    shutdown();
    return 0;
}


