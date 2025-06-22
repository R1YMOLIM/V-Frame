#ifdef _MSC_VER
    #define NOMINMAX // Block windows.h min and max macros (if you use MSVC++ compiler and Windows)
#endif

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true; // Set to true to enable validation layers
#endif

#include <vFrame/vf_vulkan.hpp>

namespace vf_vulkan {
    GLFWwindow* window = nullptr;

    class
    VulkanContext::Impl
    {
    public:
        Impl()
            : instance{}
            , physicalDevice{}
            , debugMessenger{}
            , device{}
            , graphicsQueue{}
            , presentQueue{}
            , surface{}
            , swapChain{}
            , deviceProperties{}
            , deviceFeatures{}
            , fragShaderModule{}
            , vertShaderModule{}
            , swapChainImageFormat{}
            , swapChainExtent{}
			, commandPool{ VK_NULL_HANDLE }
        {}

        ~Impl() {
            if (device.has_value()) {
                vkDeviceWaitIdle(*device);

                cleanupSwapChain();

                for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                    if (imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
                        vkDestroySemaphore(*device, imageAvailableSemaphores[i], nullptr);
                        imageAvailableSemaphores[i] = VK_NULL_HANDLE;
                    }

                    if (inFlightFences[i] != VK_NULL_HANDLE) {
                        vkDestroyFence(*device, inFlightFences[i], nullptr);
                        inFlightFences[i] = VK_NULL_HANDLE;
                    }
                }

                for (size_t i = 0; i < renderFinishedSemaphores.size(); i++) {
                    if (renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
                        vkDestroySemaphore(*device, renderFinishedSemaphores[i], nullptr);
                        renderFinishedSemaphores[i] = VK_NULL_HANDLE;
                    }
                }

                if (vertShaderModule != VK_NULL_HANDLE) {
                    vkDestroyShaderModule(*device, vertShaderModule, nullptr);
                    vertShaderModule = VK_NULL_HANDLE;
                }

                if (fragShaderModule != VK_NULL_HANDLE) {
                    vkDestroyShaderModule(*device, fragShaderModule, nullptr);
                    fragShaderModule = VK_NULL_HANDLE;
                }

                if (commandPool != VK_NULL_HANDLE) {
                    vkDestroyCommandPool(*device, commandPool, nullptr);
                    commandPool = VK_NULL_HANDLE;
                }

                vkDestroyDevice(*device, nullptr);
                device.reset();
            }

            if (surface.has_value()) {
                vkDestroySurfaceKHR(*instance, *surface, nullptr);
                surface.reset();
            }

            if (debugMessenger.has_value()) {
                destroyDebugUtilsMessengerEXT(*instance, *debugMessenger, nullptr);
                debugMessenger.reset();
            }

            if (instance.has_value()) {
                vkDestroyInstance(*instance, nullptr);
                instance.reset();
            }
        }

        void setAppName(const char* name) {
            config_v.appName = name;
        }

        void
        setWindow(GLFWwindow* window_ptr)
        {
            window = window_ptr;
        }

        void
        init()
        {
            createInstance();
            setupDebugMessenger();
            createSurface(window);
            pickPhysicalDevice();
            createLogicalDevice();
            createSwapChain();        // Create swap chain after logical device creation
            createImageViews();
            createRenderPass();
            createGraphicsPipeline(); // Create graphics pipeline after image views
            createFrameBuffers();     // Save result rendering tobto GPU draw and present on screen and connect to concrent render pass
            createCommandPull();
            createCommandBuffer();
            createSyncObjects();
        }

        VkInstance getInstance() const {
            if (!instance.has_value()) {
                throw std::runtime_error("Vulkan instance not initialized!");
            }
            return *instance;
        }

        // Draw on Screen!
        void
            drawFrame()
        {
            // 1. Очікуємо на паркан поточного кадру
        // Це гарантує, що GPU завершив роботу над цим 'currentFrame' з попереднього циклу.
            vkWaitForFences(*device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

            // 2. Отримуємо індекс наступного доступного зображення зі swapchain.
            // imageAvailableSemaphores[currentFrame] буде сигналізовано, коли зображення стане доступним.
            uint32_t imageIndex;
            VkResult result = vkAcquireNextImageKHR(*device, *swapChain, UINT64_MAX,
                imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

            // Обробка помилок
            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                recreateSwapChain();
                return;
            }
            else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
                throw std::runtime_error("failed to acquire swap chain image!");
            }

            // 3. Перевіряємо, чи попередній кадр, що використовував *це конкретне зображення* (imageIndex), вже завершився.
            // Якщо imagesInFlight[imageIndex] не VK_NULL_HANDLE, це означає, що якесь попереднє відправлення
            // все ще використовує це зображення.
            if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
                vkWaitForFences(*device, 1, &imagesInFlight[imageIndex], VK_TRUE, std::numeric_limits<uint64_t>::max());
            }
            // 4. Тепер, коли ми впевнені, що inFlightFences[currentFrame] вільний (з пункту 1),
            // і що imagesInFlight[imageIndex] теж вільний (з пункту 3),
            // ми можемо скинути (unsignal) inFlightFences[currentFrame] та
            // призначити поточний inFlightFences[currentFrame] до imagesInFlight[imageIndex].
            // Only reset the fence if we are submitting work
            vkResetFences(*device, 1, &inFlightFences[currentFrame]);
            imagesInFlight[imageIndex] = inFlightFences[currentFrame];

            // 5. Запис команд у командний буфер поточного кадру
            // vkResetCommandBuffer(commandBuffers[currentFrame], 0); // Не обов'язково, якщо recordCommandBuffer завжди перезаписує
            recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

            // 6. Налаштовуємо інформацію про відправлення команд
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

            // Чекаємо на imageAvailableSemaphores[currentFrame] перед рендерингом
            VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;

            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffers[currentFrame]; // Використовуємо буфер поточного кадру

            // Сигналізуємо renderFinishedSemaphores[currentFrame] після завершення рендерингу
            VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[imageIndex] };
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;

            // Відправляємо команди на графічну чергу.
            // inFlightFences[currentFrame] буде сигналізовано, коли ВСІ операції завершаться.
            if (vkQueueSubmit(*graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
                throw std::runtime_error("failed to submit draw command buffer!");
            }

            // 7. Налаштовуємо інформацію про представлення кадру
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

            // Чекаємо на renderFinishedSemaphores[currentFrame] перед представленням
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores; // Посилаємось на той же масив signalSemaphores

            VkSwapchainKHR swapChains[] = { *swapChain };
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapChains;
            presentInfo.pImageIndices = &imageIndex;

            presentInfo.pResults = nullptr;

            // Представляємо кадр
            result = vkQueuePresentKHR(*presentQueue, &presentInfo);

            // Check if we resize window
            if (result == VK_ERROR_OUT_OF_DATE_KHR ||
                result == VK_SUBOPTIMAL_KHR || framebufferResized) {
                framebufferResized = false;
                recreateSwapChain();
            }
            else if (result != VK_SUCCESS) {
                throw std::runtime_error("failed to present swap chain image!");
            }

            // 8. Переходимо до наступного кадру в циклі
            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        }
    private:
        struct VulkanConfig {
            const char* appName = "Vulkan App";
            uint32_t appVersion = VK_MAKE_VERSION(1, 0, 0);
            const char* engineName = "vFrame Engine";
            uint32_t engineVersion = VK_MAKE_VERSION(1, 0, 0);
            uint32_t apiVersion = VK_API_VERSION_1_4;
        } config_v;

        std::optional<VkInstance> instance;
        std::optional<VkPhysicalDevice> physicalDevice;
        std::optional<VkDebugUtilsMessengerEXT> debugMessenger;
        std::optional<VkDevice> device;
        std::optional<VkQueue> graphicsQueue;
        std::optional<VkQueue> presentQueue;
        std::optional<VkSurfaceKHR> surface;
        std::optional<VkSwapchainKHR> swapChain;
        std::optional<VkRenderPass> renderPass;
        // Pipeline Layout — це "контракт" для uniform буферів, текстур і push constants, що дає шейдерам доступ до потрібних ресурсів.
        std::optional<VkPipelineLayout> pipelineLayout;
        std::optional<VkPipeline> graphicsPipeline;
       
        // Don't optional because he always have value
        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures deviceFeatures;
        // Image views for the swap chain images
        VkShaderModule vertShaderModule;
        VkShaderModule fragShaderModule;
        VkFormat swapChainImageFormat; // Format of the swap chain images
        VkExtent2D swapChainExtent; // Resolution of the swap chain images
        VkCommandPool commandPool;

        std::vector<VkImage> swapChainImages; // Images in the swap chain
        std::vector<VkImageView> swapChainImageViews; // Image views for the swap chain images
        std::vector<VkFramebuffer> swapChainFramebuffers;
        std::vector<VkCommandBuffer> commandBuffers;
        // Нові/змінені змінні для "кадрів на льоту"
        std::vector<VkSemaphore> imageAvailableSemaphores;
        std::vector<VkSemaphore> renderFinishedSemaphores;
        std::vector<VkFence> inFlightFences;
        // Додатково: для відстеження, який Image View використовується яким кадром
        std::vector<VkFence> imagesInFlight; // Це буде вектор, що містить VkFence для кожного image in swapchain

        size_t currentFrame = 0; // Для відстеження поточного кадруa
        bool framebufferResized = false;

        // ### PRE CONFIGURATION ###
        // Function from GLSL to SPIR-V bytecode
		const int MAX_FRAMES_IN_FLIGHT = 2; // Maximum number of frames in flight

        VkShaderModule createShaderModule(const std::vector<char>& code) {
            VkShaderModuleCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = code.size();

            // In 32 bits because it's SPIR-V bytecode
            createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
            VkShaderModule shaderModule;

            if (vkCreateShaderModule(*device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
                throw std::runtime_error("failed to create shader module!");
            }

            return shaderModule; // Return the created shader module
        }

        static std::vector<char> readFile(const std::string& filename) {
            std::ifstream file(filename, std::ios::ate | std::ios::binary);

            if (!file.is_open()) {
                throw std::runtime_error("failed to open file: " + filename);
            }

            // Allocate a buffer 
            size_t fileSize = (size_t)file.tellg();
            std::vector<char> buffer(fileSize);

            // Read a bytes file
            file.seekg(0);
            file.read(buffer.data(), fileSize);

            file.close();

            return buffer; // Return the buffer with file data
        }

        std::vector<const char*> getRequiredExtensions() {
            uint32_t glfwExtensionCount = 0;
            const char** glfwExtensions =
                glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

            std::vector<const char*> extensions(glfwExtensions,
                glfwExtensions + glfwExtensionCount);

            if (enableValidationLayers) {
                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }

            extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

            return extensions;
        }

        const std::vector<const char*> validationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };

        const std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME, // For swapchain support
        };

        struct QueueFamilyIndices
        {
            std::optional<uint32_t> graphicsFamily;
            std::optional<uint32_t> presentFamily;

            std::optional<uint32_t> computeFamily;
            std::optional<uint32_t> transferFamily;

            bool
            isComplete() const
            {
                return graphicsFamily.has_value() &&
                    presentFamily.has_value();
            }
        };

        struct SwapChainSupportDetails {
            VkSurfaceCapabilitiesKHR capabilities{}; // Basic surface capabilities min/max number of images in swap chain, min/-max width and height of images)
            std::vector<VkSurfaceFormatKHR> formats; // Formats supported by the surface (color space, format of images)
            std::vector<VkPresentModeKHR> presentModes; // Presentation modes supported by the surface (FIFO, MAILBOX, IMMEDIATE, etc.)
        };

        // Color depth
        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const
            std::vector<VkSurfaceFormatKHR>& availableFormats)
        {
            for (const auto& availableFormat : availableFormats) {
                if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                    availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    return availableFormat; // Return the preferred format
                }
            }

            return availableFormats[0]; // Return the first available format if preferred is not found
        }

        // Presentation mode
        VkPresentModeKHR chooseSwapPresentMode(const
            std::vector<VkPresentModeKHR>& aviablePresentMode)
        {
            for (const auto& availablePresentMode : aviablePresentMode) {
                if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                    return availablePresentMode; // Return the preferred mode
                }
            }

            return VK_PRESENT_MODE_FIFO_KHR; // Return FIFO as a fallback if preferred is not found
        }

        // Swap chain extent (resolution of the swap chain images)
        VkExtent2D chooseSwapExtent(const
            VkSurfaceCapabilitiesKHR& capabilities)
        {
            if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
                return capabilities.currentExtent; // Use the current extent if it is defined
            }
            else {
                // If not found check the real window size
                int width, height;
                glfwGetFramebufferSize(window, &width, &height);

                VkExtent2D actualExtent = {
                    static_cast<uint32_t>(width),
                    static_cast<uint32_t>(height)
                };

                actualExtent.width = std::clamp(actualExtent.width,
                    capabilities.minImageExtent.width,
                    capabilities.maxImageExtent.width);
                actualExtent.height = std::clamp(actualExtent.height,
                    capabilities.minImageExtent.height,
                    capabilities.maxImageExtent.height);

                return actualExtent; // Clamp the extent to the min/max values
            }
        }

        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device)
        {
            SwapChainSupportDetails details;
            uint32_t formatCount;
            uint32_t presentModeCount;

            // Get surface capabilities
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, *surface,
                &details.capabilities);

            vkGetPhysicalDeviceSurfaceFormatsKHR(device, *surface, &formatCount,
                nullptr);

            // Get surface formats
            if (formatCount != 0)
            {
                details.formats.resize(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(device, *surface, &formatCount, details.formats.data());
            }

            // Get present modes
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, *surface,
                &presentModeCount, nullptr);

            if (presentModeCount != 0)
            {
                details.presentModes.resize(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(device, *surface,
                    &presentModeCount, details.presentModes.data());
            }

            return details;
        }

        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
            QueueFamilyIndices indices;

            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

            for (uint32_t i = 0; i < queueFamilyCount; i++) {
                // Check if this family supports graphics
                if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    indices.graphicsFamily = i;
                }

                // Check if this family supports presenting to our surface
                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, *surface, &presentSupport);
                if (presentSupport) {
                    indices.presentFamily = i;
                }

                if (indices.isComplete()) {
                    break;
                }
            }

            return indices;
        }

        bool
        checkDeviceExtensionsSupport(VkPhysicalDevice device)
        {
            //return true; // For simplicity, assume all devices support the required extensions
            uint32_t extensionCount;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

            std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

            for (const auto& extension : availableExtensions) {
                requiredExtensions.erase(extension.extensionName);
            }

            return requiredExtensions.empty(); // If all required extensions are found, return true
        }

        // Debug messanger
        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* /*pUserData*/) {

            if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
            }
            return VK_FALSE;
        }

        VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
            const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
            const VkAllocationCallbacks* pAllocator,
            VkDebugUtilsMessengerEXT* pDebugMessenger) {

            auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
                vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

            if (func != nullptr) {
                return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
            }
            else {
                return VK_ERROR_EXTENSION_NOT_PRESENT;
            }
        }

        void
        destroyDebugUtilsMessengerEXT(VkInstance instance,
                VkDebugUtilsMessengerEXT debugMessenger,
                const VkAllocationCallbacks* pAllocator) {
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,
                "vkDestroyDebugUtilsMessengerEXT");
            if (func != nullptr) {
                func(instance, debugMessenger, pAllocator);
            }
        }

        void
        populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
            createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            createInfo.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            createInfo.messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            createInfo.pfnUserCallback = debugCallback;
            createInfo.pUserData = nullptr;
        }

        void
        recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
        {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT для буферів, які перезаписуються щокадру
            // VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, якщо буфер може бути повторно поданий поки він ще в черзі
            // VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT для вторинних буферів, що продовжують рендер-пас
            beginInfo.flags = 0; // Optional
            beginInfo.pInheritanceInfo = nullptr; // Optional

            if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            // Render Pass визначає, як буде використовуватися Framebuffer
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = *renderPass; // Ваш створений Render Pass
            renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex]; // Фреймбуфер для поточного зображення
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = swapChainExtent; // Розмір області рендерингу

            VkClearValue clearColor = { {{0.1f, 0.1f, 0.1f, 1.0f}} };
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues = &clearColor;
            // Починаємо рендер-пас. Команди, що йдуть далі, будуть частиною цього пасу.
            // VK_SUBPASS_CONTENTS_INLINE: всі команди для цього рендер-пасу будуть записані безпосередньо в цей буфер.
            // VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: команди для цього рендер-пасу будуть у вторинних буферах.
            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(swapChainExtent.width);
            viewport.height = static_cast<float>(swapChainExtent.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = { 0, 0 };
            scissor.extent = swapChainExtent;
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // Команда малювання
            /*
            * • vertexCount: Even though we don’t have a vertex buffer, we technically
              still have 3 vertices to draw.
             • instanceCount: Used for instanced rendering, use 1 if you’re not doing
             that.
             • firstVertex: Used as an offset into the vertex buffer, defines the lowest
             value of gl_VertexIndex.
             • firstInstance: Used as an offset for instanced rendering, defines the
             lowest value of gl_InstanceIndex.
            */
            vkCmdDraw(commandBuffer, 4, 1, 0, 0);

            vkCmdEndRenderPass(commandBuffer); // Enable Render Pass

            // Finish recording buffer
            if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to record command buffer!");
            }
        }

        void
        recreateSwapChain()
        {
            int width = 0, height = 0;
            glfwGetFramebufferSize(window, &width, &height);
            while (width == 0 || height == 0) {
                glfwGetFramebufferSize(window, &width, &height);
                glfwWaitEvents();
            }

            vkDeviceWaitIdle(*device);

            cleanupSwapChain();
            createSwapChain();
            createImageViews();
            // ВАЖЛИВО: перестворювати Render Pass, Graphics Pipeline, та Command Buffers!
            createRenderPass();
            createGraphicsPipeline();
            createFrameBuffers();
            createCommandBuffer();
        }

        void
        cleanupSwapChain() {
            if (device.has_value()) {
                for (auto framebuffer : swapChainFramebuffers) {
                    vkDestroyFramebuffer(*device, framebuffer, nullptr);
                }
                swapChainFramebuffers.clear();

                for (auto imageView : swapChainImageViews) {
                    vkDestroyImageView(*device, imageView, nullptr);
                }
                swapChainImageViews.clear();

                if (graphicsPipeline.has_value()) {
                    vkDestroyPipeline(*device, *graphicsPipeline, nullptr);
                    graphicsPipeline.reset();
                }

                if (pipelineLayout.has_value()) {
                    vkDestroyPipelineLayout(*device, *pipelineLayout, nullptr);
                    pipelineLayout.reset();
                }

                if (renderPass.has_value()) {
                    vkDestroyRenderPass(*device, *renderPass, nullptr);
                    renderPass.reset();
                }

                if (swapChain.has_value()) {
                    vkDestroySwapchainKHR(*device, *swapChain, nullptr);
                    swapChain.reset();
                }
            }
        }

        // ### BOOL FUNCTIONS ###
        bool
        checkValidationLayerSupport()
        {
            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

            std::vector<VkLayerProperties> availableLayers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount,
                availableLayers.data());

            for (const char* layerName : validationLayers)
            {
                bool layerFound = false;
                for (const auto& layerProperties : availableLayers)
                {
                    if (strcmp(layerName, layerProperties.layerName) == 0)
                    {
                        layerFound = true;
                        break;
                    }
                }
                if (!layerFound)
                {
                    return false;
                }
            }

            return true;
        }

        bool
        isDeviceSuitable(VkPhysicalDevice device)
        {
            // Here you would check if the device supports the required features, queues, etc.
            // For simplicity, we will assume all devices are suitable in this example.
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
            vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

            //return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
            //	deviceFeatures.geometryShader; // Example feature check
            // For any GPU
            return true;
        }

        // ### INT FUNCTIONS ###
        int
        rateDeviceSuitability(VkPhysicalDevice device)
        {
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
            vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

            int score = 0;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);

            // Discrete GPUs have a significant performance advantage
            if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                score += 1000;
            }

            score += deviceProperties.limits.maxImageDimension2D; // Example scoring based on max image dimension

            if (!deviceFeatures.geometryShader)
            {
                return 0;
            }

            return score;
        }

        // ### INIT VULKAN FUNCTIONS ###
        void
        createInstance() {
            // 1. Заповнюємо інформацію про додаток
            VkApplicationInfo appInfo{};
            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName = config_v.appName;
            appInfo.applicationVersion = config_v.appVersion;
            appInfo.pEngineName = config_v.engineName;
            appInfo.engineVersion = config_v.engineVersion;
            appInfo.apiVersion = config_v.apiVersion;

            // 2. Перевірка validation layers
            if (enableValidationLayers && !checkValidationLayerSupport()) {
                throw std::runtime_error("validation layers requested, but not available!");
            }

            // 3. Отримуємо розширення
            auto extensions = getRequiredExtensions();

            // 4. Заповнюємо структуру createInfo
            VkInstanceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            createInfo.pApplicationInfo = &appInfo;
            createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
            createInfo.ppEnabledExtensionNames = extensions.data();

            createInfo.enabledLayerCount = enableValidationLayers ?
                static_cast<uint32_t>(validationLayers.size()) : 0;
            createInfo.ppEnabledLayerNames = enableValidationLayers ?
                validationLayers.data() : nullptr;

            createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

            VkInstance tempInstance; 
            VkResult result = vkCreateInstance(&createInfo, nullptr, &tempInstance); 

            // 5. Створюємо інстанс
            if (result != VK_SUCCESS) {
                throw std::runtime_error("failed to create Vulkan instance!");
            }
            instance = tempInstance; 

            if (enableValidationLayers && !checkValidationLayerSupport()) {
                throw std::runtime_error("Validation layers requested, but not available!");
            }

            uint32_t extensionCount = 0;
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

            std::cout << "Available Vulkan extensions:\n";
            for (const auto& ext : availableExtensions) {
                std::cout << "\t" << ext.extensionName << "\n";
            }
        }

        void
        setupDebugMessenger()
        {
            if (!enableValidationLayers) return;
            VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
            populateDebugMessengerCreateInfo(debugCreateInfo);

            VkDebugUtilsMessengerEXT tempDebugMessenger;
            if (CreateDebugUtilsMessengerEXT(*instance, &debugCreateInfo, nullptr, &tempDebugMessenger) != VK_SUCCESS) {
                throw std::runtime_error("failed to set up debug messenger!");
            }
            debugMessenger = tempDebugMessenger;
        }

        void
        createSurface(GLFWwindow* window) {
            VkSurfaceKHR tempSurface;
            if (glfwCreateWindowSurface(*instance, window, nullptr, &tempSurface) != VK_SUCCESS) {
                throw std::runtime_error("failed to create window surface!");
            }
            surface = tempSurface;
        }

        void
        pickPhysicalDevice()
        {
            uint32_t deviceCount = 0;
            vkEnumeratePhysicalDevices(*instance, &deviceCount, nullptr);

            if (deviceCount == 0)
            {
                throw std::runtime_error("failed to find GPUs with Vulkan support!");
            }

            std::vector<VkPhysicalDevice> devices(deviceCount);
            vkEnumeratePhysicalDevices(*instance, &deviceCount, devices.data());

            for (const auto& device : devices)
            {
                if (isDeviceSuitable(device))
                {
                    physicalDevice = device;
                    break;
                }
            }

            if (!physicalDevice.has_value())
            {
                throw std::runtime_error("failed to find a suitable GPU!");
            }

            /*	To evaluate the suitability of a device we can start by querying for some details.
                Basic device properties like the name, type and supported Vulkan version can
                be queried using vkGetPhysicalDeviceProperties.*/

            vkGetPhysicalDeviceProperties(*physicalDevice, &deviceProperties);

            // For special fetures for VR and more
            vkGetPhysicalDeviceFeatures(*physicalDevice, &deviceFeatures);

            /*	Use an ordered map to automatically sort candidates by
            increasing score*/
            std::multimap<int, VkPhysicalDevice> candidates;

            for (const auto& device : devices)
            {
                int score = rateDeviceSuitability(device);
                candidates.insert({ score, device });
            }
            // Check if the best candidate is suitable at all

            if (candidates.rbegin()->first > 0)
            {
                physicalDevice = candidates.rbegin()->second;
                vkGetPhysicalDeviceProperties(*physicalDevice, &deviceProperties);
                std::cout << "Selected physical device: " << deviceProperties.deviceName << std::endl;
            }
            else
            {
                throw std::runtime_error("failed to find a suitable GPU!");
            }
        }

        void
        createLogicalDevice()
        {
            QueueFamilyIndices indices = findQueueFamilies(*physicalDevice);

            std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
            std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

            float queuePriority = 1.0f;
            for (uint32_t queueFamily : uniqueQueueFamilies) {
                VkDeviceQueueCreateInfo queueCreateInfo{};
                queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

                queueCreateInfo.queueFamilyIndex = queueFamily;
                queueCreateInfo.queueCount = 1; // One queue per family
                queueCreateInfo.pQueuePriorities = &queuePriority; // Set the priority for the queue
                queueCreateInfos.push_back(queueCreateInfo);
            }

            VkPhysicalDeviceFeatures deviceFeatures{}; // Enable any required features like geometry shaders, etc.

            VkDeviceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
            createInfo.pQueueCreateInfos = queueCreateInfos.data();

            createInfo.pEnabledFeatures = &deviceFeatures;  // Enable required features
            createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
            createInfo.ppEnabledExtensionNames = deviceExtensions.data();

            if (enableValidationLayers) {
                createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
                createInfo.ppEnabledLayerNames = validationLayers.data();
            }
            else {
                createInfo.enabledLayerCount = 0;
            }

            VkDevice tempDevice;
            if (vkCreateDevice(*physicalDevice, &createInfo, nullptr, &tempDevice) != VK_SUCCESS) {
                throw std::runtime_error("failed to create logical device!");
            }
            device = tempDevice;

            // Get the graphics queue from the device and get function manipulator for queue operations
            VkQueue tempGraphicsQueue;
            vkGetDeviceQueue(*device, indices.graphicsFamily.value(), 0, &tempGraphicsQueue);
            graphicsQueue = tempGraphicsQueue;

            VkQueue tempPresentQueue;
            vkGetDeviceQueue(*device, indices.presentFamily.value(), 0, &tempPresentQueue);
            presentQueue = tempPresentQueue;
        }

        void
        createSwapChain()
        {
            // Query swap chain support details for the physical device
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(*physicalDevice);
            VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
            VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
            VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

            // Images rendering before presenting on screen
            uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 2;
            if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
                imageCount = swapChainSupport.capabilities.maxImageCount;
            }

            VkSwapchainCreateInfoKHR createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            createInfo.surface = *surface; // The surface to present images on

            createInfo.minImageCount = imageCount; // Minimum number of images in the swap chain
            createInfo.imageFormat = surfaceFormat.format; // Color format of the images
            createInfo.imageColorSpace = surfaceFormat.colorSpace; // Color space of the images
            createInfo.imageExtent = extent; // Resolution of the images
            createInfo.imageArrayLayers = 1; // Number of layers in the images (1 for 2D images)
            createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Usage of the images (color attachment for rendering)

            /*Якщо малювання та показ кадру відбувається в різних чергах, зображення має бути доступне обом. (present, render)

                VK_SHARING_MODE_CONCURRENT — тоді обидві черги мають доступ.

                Якщо все в одній черзі — використовуємо VK_SHARING_MODE_EXCLUSIVE для кращої продуктивності.*/
            QueueFamilyIndices indices = findQueueFamilies(*physicalDevice);
            uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

            if (indices.graphicsFamily != indices.presentFamily) {
                createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // Images can be shared between different queues
                createInfo.queueFamilyIndexCount = 2; // Number of queue families sharing the images
                createInfo.pQueueFamilyIndices = queueFamilyIndices; // Indices of the queue families
            }
            else {
                createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // Images are not shared between different queues
                createInfo.queueFamilyIndexCount = 0; // No need to specify queue family indices
                createInfo.pQueueFamilyIndices = nullptr;
            }

            createInfo.preTransform =
                swapChainSupport.capabilities.currentTransform; // Transform images
            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // opacity surface (usually opaque - full no opacity)
            createInfo.presentMode = presentMode; // Presentation mode for the swap chain
            createInfo.clipped = VK_TRUE; // NO rendering outside the surface area
            createInfo.oldSwapchain = VK_NULL_HANDLE; // Use this to recreate the swap chain if needed

            VkSwapchainKHR tempSwapChain;
            if (vkCreateSwapchainKHR(*device, &createInfo, nullptr, &tempSwapChain)
                != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create swap chain!");
            }
            swapChain = tempSwapChain;

            // Get the images in the swap chain
            vkGetSwapchainImagesKHR(*device, *swapChain, &imageCount, nullptr);
            swapChainImages.resize(imageCount);
            vkGetSwapchainImagesKHR(*device, *swapChain, &imageCount,
                swapChainImages.data());

            // Store the format and extent of the swap chain images
            swapChainImageFormat = surfaceFormat.format;
            swapChainExtent = extent;
        }

        void
        createImageViews()
        {
            // Resize the image views vector to match the number of swap chain images
            swapChainImageViews.resize(swapChainImages.size());

            for (size_t i = 0; i < swapChainImages.size(); i++) {
                VkImageViewCreateInfo createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                createInfo.image = swapChainImages[i];         // Вказуємо на зображення swap chain
                createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;   // Тип view — 2D
                createInfo.format = swapChainImageFormat;      // Формат зображення (треба мати цю змінну)
                createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
                createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                createInfo.subresourceRange.baseMipLevel = 0;
                createInfo.subresourceRange.levelCount = 1;
                createInfo.subresourceRange.baseArrayLayer = 0;
                createInfo.subresourceRange.layerCount = 1;

                if (vkCreateImageView(*device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create image views!");
                }
            }
        }

        void
        createRenderPass()
        {
            VkAttachmentDescription colorAttachment{};
            colorAttachment.format = swapChainImageFormat; // Такий самий формат зображення наприклад VK_FORMAT_B8G8R8A8_SRGB
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // Без MSAA
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; //перед початком субпасу очищаємо буфер (до кольору, заданого у vkCmdBeginRenderPass).
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // після завершення кадру результат треба зберегти (щоб передати на екран).
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; //GPU може не турбуватись про попередній вміст (ми все одно зробимо CLEAR).
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // ісля рендеру зображення одразу готове до vkQueuePresentKHR.

            // Subpasses and attachment referance
            VkAttachmentReference colorAttachmentRef{};
            // 	VkAttachmentDescription alone so value is 0
            colorAttachmentRef.attachment = 0;
            /*	Vulkan will automatically transition the attachment to this layout
                when the subpass is started.We intend to use the attachment to function as
                    a color buffer and the VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL layout
                    will give us the best performance, as its name implies.*/
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkSubpassDependency dependency{};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // Зовнішні операції
            dependency.dstSubpass = 0; // Наш перший (і єдиний) суппас

            // Які стадії конвеєра мають завершитися перед початком суппаса
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask = 0; // Ми ще не записуємо в цю пам'ять

            // Які стадії конвеєра можуть початися після виконання залежності
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            // GPU може записувати в кольоровий атачмент після цієї точки
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            VkSubpassDescription subpass{};
            // Support graphics subpasses
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1; // Цей Subpass буде писав у наш кольоровий attachment
            subpass.pColorAttachments = &colorAttachmentRef;

            // Render Pass
            VkRenderPassCreateInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.attachmentCount = 1; // Масив із одного VkAttachmentDescription.
            renderPassInfo.pAttachments = &colorAttachment;
            renderPassInfo.subpassCount = 1; // масив із одного VkSubpassDescription.
            renderPassInfo.pSubpasses = &subpass;

			VkRenderPass tempRenderPass;
            if (vkCreateRenderPass(*device, &renderPassInfo, nullptr,
                &tempRenderPass) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create render pass!");
            }

			renderPass = tempRenderPass; 
        }

        void
        createGraphicsPipeline()
        {
            // This function would create the graphics pipeline, shaders, etc.
            // For simplicity, we will not implement it in this example.
            auto vertShaderCode =
                readFile("shaders/vert.spv");
            auto fragShaderCode =
                readFile("shaders/frag.spv");

            vertShaderModule = createShaderModule(vertShaderCode);
            fragShaderModule = createShaderModule(fragShaderCode);

            VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
            vertShaderStageInfo.sType =
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT; // Vertex shader stage
            vertShaderStageInfo.module = vertShaderModule; // Shader module for vertex shader
            vertShaderStageInfo.pName = "main"; // Entry point in the shader

            VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
            fragShaderStageInfo.sType =
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT; // Fragment shader stage
            fragShaderStageInfo.module = fragShaderModule; // Shader module for fragment shader
            fragShaderStageInfo.pName = "main"; // Entry point in the shader

            VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

            VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
            vertexInputInfo.sType =
                VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            // Тут ти кажеш Vulkan, що немає жодного прив’язування буферів вершин, тобто ти не передаєш жодних геометричних даних(це нормально, якщо наприклад ти жорстко задаєш вершини прямо в шейдері).
            vertexInputInfo.vertexBindingDescriptionCount = 0;
            vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
            // Те ж саме: ти не описуєш атрибути вершини, такі як позиція, колір, UV тощо.
            vertexInputInfo.vertexAttributeDescriptionCount = 0;
            vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

            // Input Assembly
            VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
            inputAssembly.sType =
                VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            inputAssembly.primitiveRestartEnable = VK_FALSE; // Restart strichky (for strip)

            // Viewport and scrirros
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float)swapChainExtent.width;
            viewport.height = (float)swapChainExtent.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor{};
            scissor.offset = { 0, 0 };
            scissor.extent = swapChainExtent;

            // And then you only need to specify their count at pipeline creation time:
            VkPipelineViewportStateCreateInfo viewportState{};
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.pViewports = &viewport;
            viewportState.scissorCount = 1;
            viewportState.pScissors = &scissor;

            std::vector<VkDynamicState> dynamicStates = {
                VK_DYNAMIC_STATE_VIEWPORT, // Dynamic viewport state
                VK_DYNAMIC_STATE_SCISSOR // Dynamic scissor state
            };

            VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
            dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
            dynamicStateInfo.pDynamicStates = dynamicStates.data(); // Pointer to the dynamic states

            // Rasterizer
            VkPipelineRasterizationStateCreateInfo rasterizer{};
            rasterizer.sType =
                VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterizer.depthClampEnable = VK_FALSE; // Якщо ці картинки вийшли за межі ближньої та далекої площин то затискається до них а не відкидається це корисно для карти тінів
            rasterizer.rasterizerDiscardEnable = VK_FALSE; // Якщо для rasterizerDiscardEnable встановлено значення VK_TRUE, то геометрія ніколи не проходить через етап растеризації. Це фактично вимикає будь-який вивід до буфера кадру.
            rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
            // Товшина ліній
            rasterizer.lineWidth = 1.0f;
            // Чи ми будем рендирити сзадню частину
            rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
            // Яку саме будем вважати заадньою частиною
            rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
            // Це невеликий додатковий зсув значень глибини (depth) для полігонів під час растеризації.
            rasterizer.depthBiasEnable = VK_FALSE;
            rasterizer.depthBiasConstantFactor = 0.0f; // Optional константний зсув глибини.
            rasterizer.depthBiasClamp = 0.0f; // Optional  зсув залежно від кута нахилу полігону (для більш точного усунення артефактів на нахилених поверхнях).
            rasterizer.depthBiasSlopeFactor = 0.0f; // Optional	максимальне обмеження зсуву.

            // Multisampling
            VkPipelineMultisampleStateCreateInfo multisampling{};
            multisampling.sType =
                VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisampling.sampleShadingEnable = VK_FALSE; // в нашому випадку він не потрібний для трикутника
            multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            multisampling.minSampleShading = 1.0f; // Optional
            multisampling.pSampleMask = nullptr; // Optional
            multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
            multisampling.alphaToOneEnable = VK_FALSE; // Optional

            // Color blending
            /*
            After a fragment shader has returned a color, it needs to be combined with the
            color that is already in the framebuffer. This transformation is known as color
            blending and there are two ways to do it:
                • Mix the old and new value to produce a final color
                • Combine the old and new value using a bitwise operation */
            VkPipelineColorBlendAttachmentState colorBlendAttachment{};
            colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachment.blendEnable = VK_FALSE;
            colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;  // Множник для кольору джерела (ми множим на 1 тому без змін)
            colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Множим на 0 (ігнорувати старий колір)
            colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;             // Додати 2 кольори з урахуванням множників
            // Це все аналогічно тільки для альфа каналу
            colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

            // Псевдо код
            /*	if (blendEnable) {
                finalColor.rgb = (srcColorBlendFactor * newColor.rgb)
                    < colorBlendOp > (dstColorBlendFactor * oldColor.rgb);
                finalColor.a = (srcAlphaBlendFactor * newColor.a) < alphaBlendOp >
                    (dstAlphaBlendFactor * oldColor.a);

            }
            else {
                finalColor = newColor;

            }
                finalColor = finalColor & colorWriteMask;*/

            VkPipelineColorBlendStateCreateInfo colorBlending{};
            colorBlending.sType =
                VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlending.logicOpEnable = VK_FALSE;              // Чи включені логічні операції замість змішування кольору
            colorBlending.logicOp = VK_LOGIC_OP_COPY;            // Копіювання кольору без змін
            colorBlending.attachmentCount = 1;                   // Кількість вкладених структур VkPipelineColorBlendAttachmentState — тобто скільки color attachment в рендері буде конфігуровано. У твоєму випадку — 1 (зазвичай для одного кольорового буфера).
            colorBlending.pAttachments = &colorBlendAttachment;  // Вказівник на елемент структур де описані налаштування змішування
            // Константи змішування кольорів (RGBA)
            colorBlending.blendConstants[0] = 0.0f;
            colorBlending.blendConstants[1] = 0.0f;
            colorBlending.blendConstants[2] = 0.0f;
            colorBlending.blendConstants[3] = 0.0f;

            // Pipeline layout
            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

            // Тут нема жодних descriptor set layouts (setLayoutCount = 0 і pSetLayouts = nullptr),
            // тобто шейдери не отримують uniform буфери чи текстури через descriptor sets.
            pipelineLayoutInfo.setLayoutCount = 0;
            pipelineLayoutInfo.pSetLayouts = nullptr;

            // Нема push constants, тому pushConstantRangeCount = 0 і pPushConstantRanges = nullptr.
            pipelineLayoutInfo.pushConstantRangeCount = 0;
            pipelineLayoutInfo.pPushConstantRanges = nullptr;

            // Створення pipeline layout із такими параметрами.
            VkPipelineLayout tempPipelineLayout;
            if (vkCreatePipelineLayout(*device, &pipelineLayoutInfo, nullptr, &tempPipelineLayout) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create pipeline layout!");
            }
            pipelineLayout = tempPipelineLayout;

            VkGraphicsPipelineCreateInfo pipelineInfo{};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.stageCount = 2;
            pipelineInfo.pStages = shaderStages;
            pipelineInfo.pVertexInputState = &vertexInputInfo;
            pipelineInfo.pInputAssemblyState = &inputAssembly;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &rasterizer;
            pipelineInfo.pMultisampleState = &multisampling;
            pipelineInfo.pDepthStencilState = nullptr; // Optional
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.pDynamicState = &dynamicStateInfo;
            //Then we reference all of the structures describing the fixed-function stage.
            pipelineInfo.layout = *pipelineLayout;
            // After that comes the pipeline layout, which is a Vulkan handle rather than a struct pointer
            pipelineInfo.renderPass = *renderPass;
            pipelineInfo.subpass = 0;
            // Optionals
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
            pipelineInfo.basePipelineIndex = -1;

            VkPipeline tempGraphicsPipeline;
            if (vkCreateGraphicsPipelines(*device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                &tempGraphicsPipeline) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create graphics pipeline!");
            }

            graphicsPipeline = tempGraphicsPipeline;
        }

        // Framebuffer	Об’єкт, що містить зображення для малювання (attachments)
        void
        createFrameBuffers()
        {
            // Let's resize image swap chain! To hold all of the framebuffers
            swapChainFramebuffers.resize(swapChainImageViews.size());
            for (size_t i = 0; i < swapChainImageViews.size(); i++)
            {
                VkImageView attachments[] = {
                    swapChainImageViews[i],
                };

                // It this trongly attachments and layers must be same in Render Pass
                VkFramebufferCreateInfo framebufferInfo{};
                framebufferInfo.sType =
                    VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                framebufferInfo.renderPass = *renderPass;
                framebufferInfo.attachmentCount = 1;
                framebufferInfo.pAttachments = attachments;
                framebufferInfo.width = swapChainExtent.width;
                framebufferInfo.height = swapChainExtent.height;
                framebufferInfo.layers = 1;

                if (vkCreateFramebuffer(*device, &framebufferInfo, nullptr,
                    &swapChainFramebuffers[i]) != VK_SUCCESS)
                {
                    throw std::runtime_error("failed to create framebuffer!");
                }
            }
        }

        // Command Pool	Менеджер пам’яті для командних буферів
        void
        createCommandPull()
        {
            QueueFamilyIndices queueFamilyIndices =
                findQueueFamilies(*physicalDevice);

            VkCommandPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            poolInfo.queueFamilyIndex =
                queueFamilyIndices.graphicsFamily.value();

            if (vkCreateCommandPool(*device, &poolInfo, nullptr,
                &commandPool) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create command pool!");
            }
        }

        // Command Buffer Набір команд для GPU (малювання, копіювання, тощо)
        void
        createCommandBuffer()
        {
            commandBuffers.resize(MAX_FRAMES_IN_FLIGHT); // Розмір відповідає кількості кадрів у польоті

            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = commandPool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = (uint32_t)commandBuffers.size(); // Виділяємо всі буфери одразу
            if (vkAllocateCommandBuffers(*device, &allocInfo,
                commandBuffers.data()) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to allocate command buffers!");
            }
        }

        void
        createSyncObjects()
        {
            // Семафори та паркани для "кадрів у польоті"
            imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
            inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

            // Семафори, які сигналізують завершення рендерингу для КОЖНОГО ЗОБРАЖЕННЯ SWAPCHAIN
            renderFinishedSemaphores.resize(swapChainImages.size());

            imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

            VkSemaphoreCreateInfo semaphoreInfo{};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            VkFenceCreateInfo fenceInfo{};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

            // Цикл для imageAvailableSemaphores та inFlightFences (MAX_FRAMES_IN_FLIGHT)
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                if (vkCreateSemaphore(*device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                    vkCreateFence(*device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create synchronization objects for a frame!");
                }
            }

            // ОКРЕМИЙ ЦИКЛ для renderFinishedSemaphores (розмір swapChainImages.size())
            for (size_t i = 0; i < swapChainImages.size(); i++) {
                if (vkCreateSemaphore(*device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create render finished semaphore for swapchain image!");
                }
            }
        }
    };

	// ### CONFIGURATION FOR USER ###
    VulkanContext::VulkanContext()
        : pImpl(std::make_unique<Impl>()) {
    }

    VulkanContext::~VulkanContext() = default;

    void
    VulkanContext::vfGetWindow(GLFWwindow* window_ptr)
    {
        pImpl->setWindow(window_ptr);
    }

    void
    VulkanContext::init() {
        pImpl->init();
    }

    void
    VulkanContext::setAppName(const char* name) {
        pImpl->setAppName(name);
    }

    VkInstance VulkanContext::getInstance() const {
        return pImpl->getInstance();
    }

    void
    VulkanContext::drawFrame() {
        pImpl->drawFrame();
	}

    void
    VulkanContextDeleter::operator()(VulkanContext* ctx) const {
        if (ctx) {
            delete ctx;
        }
    }

    std::unique_ptr<VulkanContext, VulkanContextDeleter> createVulkanContext() {
        return std::unique_ptr<VulkanContext, VulkanContextDeleter>(new VulkanContext());
    }
}

