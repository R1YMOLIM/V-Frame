
#ifdef _WIN32
#  define VFRAME_API __declspec(dllexport)
#else
#  define VFRAME_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

	VFRAME_API void vframe_init();

#ifdef __cplusplus
}
#endif

