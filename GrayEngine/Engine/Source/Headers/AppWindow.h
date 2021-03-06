#pragma once
#include <glfw/glfw3.h>
#include "Engine/Source/Headers/Core.h"
#include "EventListener.h"
#include "Headers/Renderer.h"

namespace GrEngine
{
	typedef void (*InputCallbackFun)();

	struct DllExport AppParameters
	{
		const char* Title;

		uint32_t Width;
		uint32_t Height;
		Renderer* p_Renderer;
		bool free_mode;

		AppParameters(const char* _Title = "Application", uint32_t _Width = 1280, uint32_t _Height = 720, bool free_mode = false)
		{
			Title = _Title;
			Width = _Width;
			Height = _Height;
		}
	};

	class DllExport AppWindow
	{
	public:
		virtual ~AppWindow() {};

		virtual void OnStep() = 0;

		virtual void SetVSync(bool VsyncState) = 0;

		virtual void ProccessInputs() = 0;

		virtual AppParameters* WindowProperties() = 0;

		static AppWindow* Init(const AppParameters& Properties = AppParameters());

		inline GLFWwindow* getWindow() { return window; };

		inline Renderer* getRenderer() { return props.p_Renderer; };

		inline void* getNativeWindow() { return nativeWindow; };

		virtual void MaximizeWindow(bool state) = 0;

		virtual void MinimizeWindow(bool state) = 0;

		virtual bool IsKeyDown(int KEY) = 0;

		virtual void AppShowCursor(bool show) { ShowCursor(show); };

		void AddInputProccess(InputCallbackFun lambda)
		{
			inputs_vector.push_back(lambda);
		}


	protected:
		std::vector<InputCallbackFun> inputs_vector;
		AppParameters props;
		GLFWwindow* window;
		void* nativeWindow;
	};
}