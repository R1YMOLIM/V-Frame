
#include "vFrame/window.h"
#include <stdio.h>
#include <GLFW/glfw3.h>

static void GLFW_KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    vFrameWindow* vWindow = (vFrameWindow*)glfwGetWindowUserPointer(window);

    if ((key == GLFW_KEY_ESCAPE) && (action == GLFW_PRESS)) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

VFRAME_API void vFrameWindow_SetClearColor(vFrameWindow* vWindow, float r, float g, float b, float a)
{
    if (!vWindow) return;
    vWindow->clearColor[0] = r;
    vWindow->clearColor[1] = g;
    vWindow->clearColor[2] = b;
    vWindow->clearColor[3] = a;
}


VFRAME_API int vFrameWindow_Init(vFrameWindow* vWindow, int width, int height, const char* title)
{
    vWindow->width = width;
    vWindow->height = height;

    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 0;
    }

    if (!glfwVulkanSupported()) {
        fprintf(stderr, "Vulkan not supported\n");
        return 0;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    vWindow->window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!vWindow->window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 0;
    }

    glfwSetWindowUserPointer(vWindow->window, vWindow);
    glfwSetKeyCallback(vWindow->window, GLFW_KeyCallback);

    return 1;
}

VFRAME_API void vFrameWindow_Destroy(vFrameWindow* vWindow)
{
    if (vWindow->window) {
        glfwDestroyWindow(vWindow->window);
        vWindow->window = NULL;
    }
    glfwTerminate();
}

