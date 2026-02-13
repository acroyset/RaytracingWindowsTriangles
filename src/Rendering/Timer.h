//
// Created by acroy on 2/12/2026.
//

#ifndef TIMER_H
#define TIMER_H
#include <string>
#include <GLFW/glfw3.h>

class Timer {
    double start;

    public:

    std::string name;

    Timer() {
        start = glfwGetTime();
    }

    explicit Timer(const std::string &name) {
        this->name = name;
        start = glfwGetTime();
    }

    [[nodiscard]] double elapsedSeconds() const {
        return glfwGetTime() - start;
    }

    double reset() {
        double elapsed = elapsedSeconds();
        start = glfwGetTime();
        return elapsed;
    }
};

#endif //TIMER_H
