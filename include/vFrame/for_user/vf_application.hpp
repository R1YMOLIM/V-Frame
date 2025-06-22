#pragma once

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


namespace vf_core {

    class VFRAME_API Application {
    public:
        Application(int width, int height, const char* appName);
        virtual ~Application();

        void run();
        // Віртуальні методи для користувача
        virtual void onStart() {}
        virtual void onUpdate(float deltaTime) {}

    private:
        class Impl;                  // Forward declaration внутрішнього класу
        Impl* pImpl = nullptr;       // "Opaque pointer" — приховує імплементацію

        // Заборонити копіювання
        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;
    };
}
