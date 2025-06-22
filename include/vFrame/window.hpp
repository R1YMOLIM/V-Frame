
#pragma once

#ifndef VFRAME_WINDOW_HPP
#define VFRAME_WINDOW_HPP

#if defined _WIN32 || defined __CYGWIN__
#  ifdef VFRAME_BUILD_DLL
#    define VFRAME_API __declspec(dllexport)
#  else
#    define VFRAME_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  ifdef VFRAME_BUILD_DLL
#    define VFRAME_API __attribute__((visibility("default")))
#  else
#    define VFRAME_API
#  endif
#else
#  define VFRAME_API
#endif

#include <GLFW/glfw3.h>
#include <stdexcept>

namespace vf_window {

    class VFRAME_API Window {
    public:
        Window(int w, int h, const char* title); 
        ~Window();

        GLFWwindow* getHandle() const;           
        bool shouldClose() const;                
        void pollEvents() const;                 

    private:
        GLFWwindow* window = nullptr;
    };

} // namespace vf_window

#endif // VFRAME_WINDOW_HPP

