
#ifndef VFRAME_H
#define VFRAME_H

#ifdef _WIN32
#  define VFRAME_API __declspec(dllexport)
#else
#  define VFRAME_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <GLFW/glfw3.h>

    typedef struct vf {
        GLFWwindow* window;
        int width;
        int height;

        float clearColor[4];  // RGBA
    } vFrameWindow;

    VFRAME_API int vFrameWindow_Init(vFrameWindow* vWindow, int width, int height, const char* title);
    VFRAME_API void vFrameWindow_Destroy(vFrameWindow* vWindow);
    VFRAME_API void vFrameWindow_SetClearColor(vFrameWindow* vWindow, float r, float g, float b, float a);


#ifdef __cplusplus
}
#endif

#endif // VFRAME_H

