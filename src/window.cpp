
#include "../include/vFrame/window.hpp"

#include <iostream>
#include <stdexcept>   // Для std::runtime_error
#include <GLFW/glfw3.h>

using std::cerr;
using std::endl;
using std::runtime_error;
using std::string;

GLFWwindow* window = nullptr;

void vf_window::Window::init(int w_w, int w_h, const char* name)
{
    try
    {
        if (!glfwInit())
        {
            throw runtime_error("Failed to initialize GLFW");
        }

        if (!glfwVulkanSupported())
        {
            throw runtime_error("Vulkan not supported");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(w_w, w_h, name, NULL, NULL);
        if (!window)
        {
            glfwTerminate();
            throw runtime_error("Failed to create GLFW window");
        }

        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
        }

        glfwTerminate();

    }
    catch (const runtime_error& e)
    {
        cerr << e.what() << endl;
        exit(EXIT_FAILURE);
    }
}
