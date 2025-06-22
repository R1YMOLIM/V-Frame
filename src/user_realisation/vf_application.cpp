#include "vFrame/for_user/vf_application.hpp"
#include "vFrame/window.hpp"
#include "vFrame/vf_vulkan.hpp"

#include <chrono>
#include <memory>

namespace vf_core {

    // Внутрішній клас реалізації
    class Application::Impl {
    public:
        Impl(int width, int height, const char* appName)
            : window(width, height, appName), appName(appName)
        {
            context = vf_vulkan::createVulkanContext();
            context->setAppName(appName);
            context->vfGetWindow(window.getHandle());
            context->init();
        }

        ~Impl() {
            // Унікальні вказівники автоматично звільняють ресурси
        }

        void run(Application* parent) {
            using clock = std::chrono::high_resolution_clock;
            auto lastTime = clock::now();

            parent->onStart();

            while (!window.shouldClose()) {
                window.pollEvents();

                auto now = clock::now();
                float deltaTime = std::chrono::duration<float>(now - lastTime).count();
                lastTime = now;

                parent->onUpdate(deltaTime);
                context->drawFrame();
            }
        }

    private:
        vf_window::Window window;
        std::unique_ptr<vf_vulkan::VulkanContext, vf_vulkan::VulkanContextDeleter> context;
        const char* appName;
    };

    // Конструктор
    Application::Application(int width, int height, const char* appName)
        : pImpl(new Impl(width, height, appName))
    {
    }

    // Деструктор
    Application::~Application() {
        delete pImpl;
    }

    // Запуск головного циклу
    void Application::run() {
        pImpl->run(this);
    }

}
