#include <Framework/Framework.h>
#include <Framework/Application.h>

#include <Windows.h>

namespace LearningWorkGraph
{
Framework* Framework::s_instance = nullptr;

Framework::Framework()
{
	LWG_CHECK(!s_instance);
	s_instance = this;
}

Framework::~Framework()
{
	LWG_CHECK(s_instance == this);
	s_instance = nullptr;
}

void Framework::Initialize(const FrameworkDesc& desc)
{
	if (desc.m_useWindow)
	{
		uint32_t windowSize[2] = { 1280, 720 };
		auto instance = GetModuleHandleA(NULL);

		// Initialize the window class.
		WNDCLASSEX windowClass = { 0 };
		windowClass.cbSize = sizeof(WNDCLASSEX);
		windowClass.style = CS_HREDRAW | CS_VREDRAW;
		windowClass.lpfnWndProc = DefWindowProcA;
		windowClass.hInstance = instance;
		windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
		windowClass.lpszClassName = "DXSampleClass";
		RegisterClassExA(&windowClass);

		RECT windowRect = { 0, 0, windowSize[0], windowSize[1] };
		AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

		// Create the window and store a handle to it.
		m_hwnd = CreateWindowExA
		(
			0,
			windowClass.lpszClassName,
			"Learning Work Graph",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			windowRect.right - windowRect.left,
			windowRect.bottom - windowRect.top,
			nullptr,        // We have no parent window.
			nullptr,        // We aren't using menus.
			instance,
			NULL
		);

		ShowWindow(m_hwnd, SW_SHOW);
	}
}

void Framework::Run()
{
	if (m_hwnd)
	{
		// Main sample loop.
		MSG msg = {};
		while (msg.message != WM_QUIT)
		{
			if (auto* application = LearningWorkGraph::Application::GetMainApplication())
			{
				application->OnUpdate();
				application->OnRender();
			}
			// Process any messages in the queue.
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}
	else
	{
		while (true)
		{
			if (auto* application = LearningWorkGraph::Application::GetMainApplication())
			{
				application->OnUpdate();
				application->OnRender();
			}
		}
	}
}

void Framework::ShowDialog(std::string_view title, std::string_view message)
{
	MessageBoxA(NULL, message.data(), title.data(), MB_OK);
}
}