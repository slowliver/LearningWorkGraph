﻿#include <Framework/Framework.h>
#include <Framework/Application.h>

#include <Windows.h>

#if 0
static LRESULT WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	auto* application = LearningWorkGraph::Application::GetMainApplication();

	switch (message)
	{
	case WM_CREATE:
	{
		// Save the DXSample* passed in to CreateWindow.
		LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
	}
	return 0;

	case WM_KEYDOWN:
		if (application)
		{
			//			pSample->OnKeyDown(static_cast<UINT8>(wParam));
		}
		return 0;

	case WM_KEYUP:
		if (application)
		{
			//			pSample->OnKeyUp(static_cast<UINT8>(wParam));
		}
		return 0;

#if 0
	case WM_PAINT:
		if (application)
		{
	//		application->OnUpdate();
	//		application->OnRender();
	//		application->Present();
		}
		return 0;
#endif

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	// Handle any messages the switch statement didn't.
	return DefWindowProc(hWnd, message, wParam, lParam);
}
#endif

namespace LearningWorkGraph
{
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
		windowClass.lpfnWndProc = ::DefWindowProc;// WindowProc;
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
			"Test",
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
	// Main sample loop.
	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		// Process any messages in the queue.
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

void Framework::ShowDialog(std::string_view title, std::string_view message)
{
	MessageBoxA(NULL, message.data(), title.data(), MB_OK);
}
}