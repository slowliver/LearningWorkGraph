/**********************************************************************
Copyright (c) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/

#include <windows.h>
#include <atlbase.h>
#include <conio.h>

#include <d3d12.h>
#include <d3dx12/d3dx12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <wrl.h>
#if defined(_DEBUG)
#include <d3d12sdklayers.h>
#include <dxgidebug.h>
#endif

#include "Source/Framework.h"
#include "Source/Shader.h"

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 613; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

#define UAV_SIZE 1024

bool EnsureWorkGraphsSupported(CComPtr<ID3D12Device9> pDevice);
ID3D12RootSignature* CreateGlobalRootSignature(CComPtr<ID3D12Device9> pDevice);
ID3D12StateObject* CreateGWGStateObject(CComPtr<ID3D12Device9> pDevice, CComPtr<ID3D12RootSignature> pGlobalRootSignature, const LearningWorkGraph::Shader& shader);
D3D12_SET_PROGRAM_DESC PrepareWorkGraph(CComPtr<ID3D12Device9> pDevice, CComPtr<ID3D12StateObject> pStateObject);
bool DispatchWorkGraphAndReadResults(CComPtr<ID3D12Device9> pDevice, CComPtr<ID3D12RootSignature> pGlobalRootSignature, D3D12_SET_PROGRAM_DESC SetProgramDesc, char* pResult);

static HWND g_hwnd = {};

using Microsoft::WRL::ComPtr;

extern "C" const GUID DXGI_DEBUG_D3D12;
namespace LearningWorkGraph
{
class Application
{
public:
	Application() {}

	void Initialize();
	void Terminate();

	ID3D12Device9* GetD3D12Device9() { return m_d3d12Device.Get(); }

private:
	void WaitForGPU();

private:
	static constexpr uint32_t k_frameCount = 2;
	ComPtr<ID3D12Device9> m_d3d12Device = nullptr;
	ComPtr<ID3D12CommandQueue> m_d3d12CommandQueue = nullptr;
	ComPtr<IDXGISwapChain3> m_dxgiSwapChain = nullptr;
	ComPtr<ID3D12DescriptorHeap> m_renderTargetViewHeap = nullptr;
	ComPtr<ID3D12CommandAllocator> m_commandAllocators[k_frameCount] = {};
	ComPtr<ID3D12Resource> m_renderTargets[k_frameCount] = {};
	ComPtr<ID3D12GraphicsCommandList> m_commandList;

	// Fence Objects.
	uint32_t m_frameIndex = 0;
	HANDLE m_fenceEvent = {};
	ComPtr<ID3D12Fence> m_fence = nullptr;
	uint64_t m_fenceValues[k_frameCount] = {};

	size_t m_renderTargetViewDescriptorSize = 0;
};

void Application::Initialize()
{
#if 0
	// Parse the command line parameters
	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	pSample->ParseCommandLineArgs(argv, argc);
	LocalFree(argv);
#endif



#if 0
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
#endif

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
	LWG_CHECK(SUCCEEDED(dxgiFactory4->CreateSwapChainForHwnd(m_d3d12CommandQueue.Get(), g_hwnd, &swapChainDesc, nullptr, nullptr, &swapChain)));

	// This sample does not support fullscreen transitions.
	LWG_CHECK(SUCCEEDED(dxgiFactory4->MakeWindowAssociation(g_hwnd, DXGI_MWA_NO_ALT_ENTER)));

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
}

static LRESULT WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	auto* application = reinterpret_cast<LearningWorkGraph::Application*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

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

	case WM_PAINT:
		if (application)
		{
			//			application->OnUpdate();
			//			application->OnRender();
		}
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	// Handle any messages the switch statement didn't.
	return DefWindowProc(hWnd, message, wParam, lParam);
}

int main()
{
	uint32_t windowSize[2] = { 1280, 720 };
	auto instance = GetModuleHandleA(NULL);

	// Initialize the window class.
	WNDCLASSEX windowClass = { 0 };
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = instance;
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.lpszClassName = L"DXSampleClass";
	RegisterClassEx(&windowClass);

	RECT windowRect = { 0, 0, windowSize[0], windowSize[1] };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	// Create the window and store a handle to it.
	g_hwnd = CreateWindow(
		windowClass.lpszClassName,
		L"Test",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr,        // We have no parent window.
		nullptr,        // We aren't using menus.
		instance,
		NULL);

	ShowWindow(g_hwnd, SW_SHOW);

	auto application = LearningWorkGraph::Application();
	application.Initialize();

	auto* d3d12Device9 = application.GetD3D12Device9();

	if (!EnsureWorkGraphsSupported(d3d12Device9))
	{
		return -1;
	}

	auto shader = LearningWorkGraph::Shader();
	shader.CompileFromFile("Shader/Shader.shader");

	CComPtr<ID3D12RootSignature> pGlobalRootSignature = CreateGlobalRootSignature(d3d12Device9);

	CComPtr<ID3D12StateObject> pStateObject = CreateGWGStateObject(d3d12Device9, pGlobalRootSignature, shader);
	D3D12_SET_PROGRAM_DESC SetProgramDesc = PrepareWorkGraph(d3d12Device9, pStateObject);

	char result[UAV_SIZE / sizeof(char)];
	if (DispatchWorkGraphAndReadResults(d3d12Device9, pGlobalRootSignature, SetProgramDesc, result))
	{
		printf("SUCCESS: Output was \"%s\"\n", result);
//		_getch();
	}

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
	
	return 0;
}

#define ERROR_QUIT(value, ...) if(!(value)) { printf("ERROR: "); printf(__VA_ARGS__); printf("\nPress any key to terminate...\n"); _getch(); throw 0; }

static const wchar_t* kProgramName = L"Hello World";

bool EnsureWorkGraphsSupported(CComPtr<ID3D12Device9> pDevice)
{
	D3D12_FEATURE_DATA_D3D12_OPTIONS21 Options = {};
	pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS21, &Options, sizeof(Options));
	LWG_CHECK_WITH_MESSAGE(Options.WorkGraphsTier != D3D12_WORK_GRAPHS_TIER_NOT_SUPPORTED,
		"Failed to ensure work graphs were supported. Check driver and graphics card.");

	return (Options.WorkGraphsTier != D3D12_WORK_GRAPHS_TIER_NOT_SUPPORTED);
}

ID3D12RootSignature* CreateGlobalRootSignature(CComPtr<ID3D12Device9> pDevice)
{
	ID3D12RootSignature* pRootSignature = nullptr;

	CD3DX12_ROOT_PARAMETER RootSignatureUAV;
	RootSignatureUAV.InitAsUnorderedAccessView(0, 0);

	CD3DX12_ROOT_SIGNATURE_DESC Desc(1, &RootSignatureUAV, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);
	CComPtr<ID3DBlob> pSerialized;

	HRESULT hr = D3D12SerializeRootSignature(&Desc, D3D_ROOT_SIGNATURE_VERSION_1, &pSerialized, NULL);
	if (SUCCEEDED(hr))
	{
		hr = pDevice->CreateRootSignature(0, pSerialized->GetBufferPointer(), pSerialized->GetBufferSize(), IID_PPV_ARGS(&pRootSignature));
	}

	ERROR_QUIT(SUCCEEDED(hr), "Failed to create global root signature.");
	return pRootSignature;
}

ID3D12StateObject* CreateGWGStateObject(CComPtr<ID3D12Device9> pDevice, CComPtr<ID3D12RootSignature> pGlobalRootSignature, const LearningWorkGraph::Shader& shader)
{
	ID3D12StateObject* d3d12StateObject = nullptr;

	auto desc = CD3DX12_STATE_OBJECT_DESC(D3D12_STATE_OBJECT_TYPE_EXECUTABLE);

	CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT* globalRootSignatureDesc = desc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	globalRootSignatureDesc->SetRootSignature(pGlobalRootSignature);

	// シェーダライブラリを設定.
	CD3DX12_DXIL_LIBRARY_SUBOBJECT* libraryDesc = desc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	CD3DX12_SHADER_BYTECODE libraryCode(shader.GetData(), shader.GetSize());
	libraryDesc->SetDXILLibrary(&libraryCode);

	// ワークグラフのセットアップ.
	CD3DX12_WORK_GRAPH_SUBOBJECT* workGraphDesc = desc.CreateSubobject<CD3DX12_WORK_GRAPH_SUBOBJECT>();
	workGraphDesc->IncludeAllAvailableNodes();		// すべての利用可能なノードを使用する.
	workGraphDesc->SetProgramName(kProgramName);

	HRESULT hr = pDevice->CreateStateObject(desc, IID_PPV_ARGS(&d3d12StateObject));
	LWG_CHECK_WITH_MESSAGE(SUCCEEDED(hr) && d3d12StateObject, "Failed to create Work Graph State Object.");

	return d3d12StateObject;
}

inline ID3D12Resource* AllocateBuffer(CComPtr<ID3D12Device9> pDevice, UINT64 Size, D3D12_RESOURCE_FLAGS ResourceFlags, D3D12_HEAP_TYPE HeapType)
{
	ID3D12Resource* pResource;

	CD3DX12_HEAP_PROPERTIES HeapProperties(HeapType);
	CD3DX12_RESOURCE_DESC ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(Size, ResourceFlags);
	HRESULT hr = pDevice->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE, &ResourceDesc, D3D12_RESOURCE_STATE_COMMON, NULL, IID_PPV_ARGS(&pResource));
	ERROR_QUIT(SUCCEEDED(hr), "Failed to allocate buffer.");

	return pResource;
}

D3D12_SET_PROGRAM_DESC PrepareWorkGraph(CComPtr<ID3D12Device9> pDevice, CComPtr<ID3D12StateObject> pStateObject)
{
	CComPtr<ID3D12Resource> pBackingMemoryResource = nullptr;

	CComPtr<ID3D12StateObjectProperties1> pStateObjectProperties;	
	CComPtr<ID3D12WorkGraphProperties> pWorkGraphProperties;
	pStateObjectProperties = pStateObject;
	pWorkGraphProperties = pStateObject;

	// GPU で使用するメモリを確保.
	UINT index = pWorkGraphProperties->GetWorkGraphIndex(kProgramName);
	D3D12_WORK_GRAPH_MEMORY_REQUIREMENTS MemoryRequirements = {};
	pWorkGraphProperties->GetWorkGraphMemoryRequirements(index, &MemoryRequirements);
	if (MemoryRequirements.MaxSizeInBytes > 0)
	{
		pBackingMemoryResource = AllocateBuffer(pDevice, MemoryRequirements.MaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT);
	}

	UINT test = pWorkGraphProperties->GetNumEntrypoints(index);

	D3D12_SET_PROGRAM_DESC Desc = {};
	Desc.Type = D3D12_PROGRAM_TYPE_WORK_GRAPH;
	Desc.WorkGraph.ProgramIdentifier = pStateObjectProperties->GetProgramIdentifier(kProgramName);
	Desc.WorkGraph.Flags = D3D12_SET_WORK_GRAPH_FLAG_INITIALIZE;
	if (pBackingMemoryResource)
	{
		Desc.WorkGraph.BackingMemory = { pBackingMemoryResource->GetGPUVirtualAddress(), MemoryRequirements.MaxSizeInBytes };
	}

	return Desc;
}

inline bool RunCommandListAndWait(CComPtr<ID3D12Device9> pDevice, CComPtr<ID3D12CommandQueue> pCommandQueue, CComPtr<ID3D12CommandAllocator> pCommandAllocator, CComPtr<ID3D12GraphicsCommandList10> pCommandList, CComPtr<ID3D12Fence> pFence)
{
	if (SUCCEEDED(pCommandList->Close()))
	{
		pCommandQueue->ExecuteCommandLists(1, CommandListCast(&pCommandList.p));
		pCommandQueue->Signal(pFence, 1);

		HANDLE hCommandListFinished = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (hCommandListFinished)
		{
			pFence->SetEventOnCompletion(1, hCommandListFinished);
			DWORD waitResult = WaitForSingleObject(hCommandListFinished, INFINITE);
			CloseHandle(hCommandListFinished);

			if (waitResult == WAIT_OBJECT_0 && SUCCEEDED(pDevice->GetDeviceRemovedReason()))
			{
				pCommandAllocator->Reset();
				pCommandList->Reset(pCommandAllocator, nullptr);
				return true;
			}
		}
	}

	return false;
}

bool DispatchWorkGraphAndReadResults(CComPtr<ID3D12Device9> pDevice, CComPtr<ID3D12RootSignature> pGlobalRootSignature, D3D12_SET_PROGRAM_DESC SetProgramDesc, char* pResult)
{
	CComPtr<ID3D12CommandQueue> pCommandQueue;
	CComPtr<ID3D12CommandAllocator> pCommandAllocator;
	CComPtr<ID3D12GraphicsCommandList10> pCommandList;
	CComPtr<ID3D12Fence> pFence;

	D3D12_COMMAND_QUEUE_DESC CommandQueueDesc = {};
	CommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT;
	CommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	pDevice->CreateCommandQueue(&CommandQueueDesc, IID_PPV_ARGS(&pCommandQueue));

	pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pCommandAllocator));
	pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pCommandAllocator, nullptr, IID_PPV_ARGS(&pCommandList));
	pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence));

	if (pCommandQueue && pCommandAllocator && pCommandList && pFence)
	{
		CComPtr<ID3D12Resource> pUAVBuffer = AllocateBuffer(pDevice, UAV_SIZE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT);
		CComPtr<ID3D12Resource> pReadbackBuffer = AllocateBuffer(pDevice, UAV_SIZE, D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_READBACK);

		// dispatch work graph
		D3D12_DISPATCH_GRAPH_DESC DispatchGraphDesc = {};
		DispatchGraphDesc.Mode = D3D12_DISPATCH_MODE_NODE_CPU_INPUT;
		DispatchGraphDesc.NodeCPUInput = { };
		DispatchGraphDesc.NodeCPUInput.EntrypointIndex = 0;
		DispatchGraphDesc.NodeCPUInput.NumRecords = 1;

		pCommandList->SetComputeRootSignature(pGlobalRootSignature);
		pCommandList->SetComputeRootUnorderedAccessView(0, pUAVBuffer->GetGPUVirtualAddress());
		pCommandList->SetProgram(&SetProgramDesc);
		pCommandList->DispatchGraph(&DispatchGraphDesc);

		// read results
		D3D12_RESOURCE_BARRIER Barrier = {};
		Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		Barrier.Transition.pResource = pUAVBuffer;
		Barrier.Transition.Subresource = 0;
		Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

		pCommandList->ResourceBarrier(1, &Barrier);
		pCommandList->CopyResource(pReadbackBuffer, pUAVBuffer);

		if (RunCommandListAndWait(pDevice, pCommandQueue, pCommandAllocator, pCommandList, pFence))
		{
			char* pOutput;
			D3D12_RANGE range{ 0, UAV_SIZE };
			if (SUCCEEDED(pReadbackBuffer->Map(0, &range, (void**)&pOutput)))
			{
				memcpy(pResult, pOutput, UAV_SIZE);
				pReadbackBuffer->Unmap(0, nullptr);
				return true;
			}
		}
	}
	
	ERROR_QUIT(true, "Failed to dispatch work graph and read results.");
	return false;
}