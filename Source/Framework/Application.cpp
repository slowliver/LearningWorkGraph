#include <Framework/Application.h>
#include <Framework/Framework.h>

#include <d3dx12/d3dx12.h>

using Microsoft::WRL::ComPtr;
namespace LearningWorkGraph
{
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

	// Describe and create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
#if 0
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT;
#endif
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	LWG_CHECK(SUCCEEDED(m_d3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_d3d12CommandQueue))));

	// Describe and create the swap chain.
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = k_frameCount;
	swapChainDesc.Width = 1280;
	swapChainDesc.Height = 720;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain = nullptr;

	LWG_CHECK(SUCCEEDED(dxgiFactory4->CreateSwapChainForHwnd(m_d3d12CommandQueue.Get(), framework->GetHWND(), &swapChainDesc, nullptr, nullptr, &swapChain)));

	// This sample does not support fullscreen transitions.
	LWG_CHECK(SUCCEEDED(dxgiFactory4->MakeWindowAssociation(framework->GetHWND(), DXGI_MWA_NO_ALT_ENTER)));

	LWG_CHECK(SUCCEEDED(swapChain.As(&m_dxgiSwapChain)));
	m_frameIndex = m_dxgiSwapChain->GetCurrentBackBufferIndex();

	// Create descriptor heaps.
	{
		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC renderTargetViewHeapDesc = {};
		renderTargetViewHeapDesc.NumDescriptors = k_frameCount;
		renderTargetViewHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		renderTargetViewHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		LWG_CHECK(SUCCEEDED(m_d3d12Device->CreateDescriptorHeap(&renderTargetViewHeapDesc, IID_PPV_ARGS(&m_renderTargetViewHeap))));
		m_renderTargetViewDescriptorSize = m_d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	// Create frame resources.
	{
		auto renderTargetViewHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV and a command allocator for each frame.
		for (UINT n = 0; n < k_frameCount; n++)
		{
			LWG_CHECK(SUCCEEDED(m_dxgiSwapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n]))));
			m_d3d12Device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, renderTargetViewHandle);
			renderTargetViewHandle.Offset(1, m_renderTargetViewDescriptorSize);
			LWG_CHECK(SUCCEEDED(m_d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n]))));
		}
	}

	LWG_CHECK(SUCCEEDED(m_d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), NULL, IID_PPV_ARGS(&m_commandList))));
	LWG_CHECK(SUCCEEDED(m_commandList->Close()));

	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	{
		LWG_CHECK(SUCCEEDED(m_d3d12Device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence))));
		m_fenceValues[m_frameIndex]++;

		// Create an event handle to use for frame synchronization.
		m_fenceEvent = CreateEventA(nullptr, FALSE, FALSE, "Fence Event");
		if (m_fenceEvent == nullptr)
		{
			LWG_CHECK(SUCCEEDED(HRESULT_FROM_WIN32(GetLastError())));
		}

		// Wait for the command list to execute; we are reusing the same command 
		// list in our main loop but for now, we just want to wait for setup to 
		// complete before continuing.
		WaitForGPU();
	}

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

// Wait for pending GPU work to complete.
void Application::WaitForGPU()
{
	// Schedule a Signal command in the queue.
	LWG_CHECK(SUCCEEDED(m_d3d12CommandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex])));

	// Wait until the fence has been processed.
	LWG_CHECK(SUCCEEDED(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent)));
	WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

	// Increment the fence value for the current frame.
	++m_fenceValues[m_frameIndex];
}

// Prepare to render the next frame.
void Application::MoveToNextFrame()
{
	// Schedule a Signal command in the queue.
	const auto currentFenceValue = m_fenceValues[m_frameIndex];
	LWG_CHECK(SUCCEEDED(m_d3d12CommandQueue->Signal(m_fence.Get(), currentFenceValue)));

	// Update the frame index.
	m_frameIndex = m_dxgiSwapChain->GetCurrentBackBufferIndex();

	// If the next frame is not ready to be rendered yet, wait until it is ready.
	if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
	{
		LWG_CHECK(SUCCEEDED(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent)));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	// Set the fence value for the next frame.
	m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}
}