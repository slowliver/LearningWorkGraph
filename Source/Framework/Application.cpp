#include <Framework/Application.h>
#include <Framework/Framework.h>

#include <d3dx12/d3dx12.h>

using Microsoft::WRL::ComPtr;
namespace LearningWorkGraph
{
Application* Application::s_instance = nullptr;

Application::Application()
{
	LWG_CHECK(!s_instance);
	s_instance = this;
}

Application::~Application()
{
	LWG_CHECK(s_instance == this);
	s_instance = nullptr;
}

void Application::Initialize(const Framework* framework)
{
	LWG_CHECK(framework);

	UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
	// Enable the debug layer (requires the Graphics Tools "optional feature").
	// NOTE: Enabling the debug layer after device creation will invalidate the active device.
	{
		ComPtr<ID3D12Debug> debugController = nullptr;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();

			// Enable additional debug layers.
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	ComPtr<IDXGIFactory4> dxgiFactory4 = nullptr;
	if (SUCCEEDED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory4))))
	{
		ComPtr<IDXGIAdapter1> hardwareAdapter;
		// function GetHardwareAdapter() copy-pasted from the publicly distributed sample provided at: https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-d3d12createdevice
		for (UINT adapterIndex = 0; ; ++adapterIndex)
		{
			IDXGIAdapter1* adapter = nullptr;
			if (DXGI_ERROR_NOT_FOUND == dxgiFactory4->EnumAdapters1(adapterIndex, &adapter))
			{
				// No more adapters to enumerate.
				break;
			}

			// Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
			if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
			{
				hardwareAdapter = adapter;
				break;
			}
			adapter->Release();
		}
		D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_d3d12Device));
	}

	LWG_CHECK_WITH_MESSAGE(m_d3d12Device, "Failed to initialize compiler.");

	OnInitialize();
}

void Application::Terminate()
{
	HRESULT hr = {};
#if defined(_DEBUG) && 0
	ComPtr<IDXGIDebug> dxgiDebug = nullptr;

	typedef HRESULT(WINAPI* DXGIGetDebugInterfaceCallback)(const IID&, void**);
	if (auto dll = GetModuleHandleA("dxgidebug.dll"))
	{
		if (auto* dxgiGetDebugInterface = (DXGIGetDebugInterfaceCallback)GetProcAddress(dll, "DXGIGetDebugInterface"))
		{
			hr = dxgiGetDebugInterface(IID_PPV_ARGS(&dxgiDebug));
			if (SUCCEEDED(hr))
			{
				dxgiDebug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_DETAIL);
			}
		}
	}
#endif
}
}