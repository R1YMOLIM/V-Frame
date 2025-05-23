
#ifdef _WIN32
#  define VFRAME_API __declspec(dllexport)
#else
#  define VFRAME_API
#endif

#include <string>

using std::string;

namespace vf_window
{
	class VFRAME_API Window {
	public:
		virtual void init(int w_w, int w_h, const char* name);
	};
}
