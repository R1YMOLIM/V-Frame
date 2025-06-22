
#include "vFrame/window.hpp"
#include <stdexcept>

namespace vf_window {
    Window::Window(int w, int h, const char* title) {
        if (!glfwInit())
            throw std::runtime_error("Failed to initialize GLFW");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(w, h, title, nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }
    }

    Window::~Window() {
        if (window)
            glfwDestroyWindow(window);
        glfwTerminate();
    }

    GLFWwindow* Window::getHandle() const {
        return window;
    }

    bool Window::shouldClose() const {
        return glfwWindowShouldClose(window);
    }

    void Window::pollEvents() const {
        glfwPollEvents();
    }
}

