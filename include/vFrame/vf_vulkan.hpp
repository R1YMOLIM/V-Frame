#pragma once
#ifndef VFRAME_VULKAN_HPP
#define VFRAME_VULKAN_HPP

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

#include <iostream>
#include <stdexcept>
#include <vulkan/vulkan.h>
#include <cstdlib>
#include <vector>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <map>
#include <optional>
#include <set>
#include <cstdint> // Necessary for uint32_t
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp
#include <fstream> // For file operations (if needed later)

namespace vf_vulkan {

    class VulkanContext; 

    struct VulkanContextDeleter {
        VFRAME_API void operator()(VulkanContext* ctx) const;
    };

    VFRAME_API std::unique_ptr<VulkanContext, VulkanContextDeleter> createVulkanContext();

    class VFRAME_API VulkanContext {
    public:
        VulkanContext(const VulkanContext&) = delete;
        VulkanContext& operator=(const VulkanContext&) = delete;

        void init();
        void setAppName(const char* name);
        void vfGetWindow(GLFWwindow* window_ptr);
		void drawFrame();

        VkInstance getInstance() const;

    private:
        VulkanContext();  
        ~VulkanContext(); 

        class Impl;
        #pragma warning(push)
        #pragma warning(disable: 4251) // "class needs to have dll-interface"
        std::unique_ptr<Impl> pImpl;
        #pragma warning(pop)

        friend struct VulkanContextDeleter;
        friend VFRAME_API std::unique_ptr<VulkanContext, VulkanContextDeleter> createVulkanContext();
    };

} // namespace vf_vulkan

#endif // VFRAME_VULKAN_HPP
