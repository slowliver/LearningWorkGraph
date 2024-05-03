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
#include <math.h>

#include <Framework/Application.h>
#include <Framework/Framework.h>
#include <Framework/Shader.h>

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 613; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

#define UAV_SIZE 1024

using Microsoft::WRL::ComPtr;

class HelloWorkGraphApplication : public LearningWorkGraph::Application
{
private:
	struct Vertex
	{
		float m_position[2] = {};
	};
	struct SceneParameters
	{
		float m_viewProjectionMatrix[4][4];
		float m_padding[48]; // Padding so the constant buffer is 256-byte aligned.
	};
	static_assert(sizeof(SceneParameters) == 256);

public:
	virtual void OnInitialize() override;
	virtual void OnUpdate() override;
	virtual void OnRender() override;

private:
	bool EnsureWorkGraphsSupported();
	ID3D12RootSignature* CreateGlobalRootSignature();
	ID3D12StateObject* CreateGWGStateObject(ComPtr<ID3D12RootSignature> pGlobalRootSignature, const LearningWorkGraph::Shader& shader);
	ID3D12Resource* AllocateBuffer(UINT64 Size, D3D12_RESOURCE_FLAGS ResourceFlags, D3D12_HEAP_TYPE HeapType);
	D3D12_SET_PROGRAM_DESC PrepareWorkGraph(ComPtr<ID3D12StateObject> pStateObject);
	bool RunCommandListAndWait(ComPtr<ID3D12CommandQueue> pCommandQueue, ComPtr<ID3D12CommandAllocator> pCommandAllocator, ComPtr<ID3D12GraphicsCommandList10> pCommandList, ComPtr<ID3D12Fence> pFence);
	bool DispatchWorkGraphAndReadResults(ComPtr<ID3D12RootSignature> pGlobalRootSignature, D3D12_SET_PROGRAM_DESC SetProgramDesc, char* pResult);

	void CreatePipeline();
	
	void ExecuteComputeShader();

private:
	ComPtr<ID3D12CommandQueue> m_commandQueue = nullptr;
	ComPtr<ID3D12CommandAllocator> m_commandAllocator = nullptr;
	ComPtr<ID3D12GraphicsCommandList10> m_commandList = nullptr;
	ComPtr<ID3D12Fence> m_fence = nullptr;

private:
	static constexpr uint32_t k_circle = 64;
	static constexpr const wchar_t* k_programName = L"Hello World";
	ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
	ComPtr<ID3D12PipelineState> m_pipelineState = nullptr;
	ComPtr<ID3D12Resource> m_vertexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
	ComPtr<ID3D12Resource> m_indexBuffer = nullptr;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};

	ComPtr<ID3D12DescriptorHeap> m_dynamicCBVSRVUAVHeap = nullptr;
	size_t m_cbvSRVUAVDescriptorSize = 0;

	// Constant Buffer.
	ComPtr<ID3D12DescriptorHeap> m_staticConstantBufferViewHeap = nullptr;
	SceneParameters m_sceneParameters = {};
	ComPtr<ID3D12Resource> m_sceneParametersConstantBuffer = {};
	std::byte* m_sceneParametersDataBegin = nullptr;

	// Instance Buffer.
	static constexpr uint32_t k_numInstance = 128;
	ComPtr<ID3D12DescriptorHeap> m_staticShaderResourceViewHeap = nullptr;
	ComPtr<ID3D12Resource> m_instanceBuffer = nullptr;
};

void HelloWorkGraphApplication::OnInitialize()
{
	auto* d3d12Device9 = GetD3D12Device9();

	CreatePipeline();

	ExecuteComputeShader();

#if 1
	if (!EnsureWorkGraphsSupported())
	{
		return;
	}

	auto shader = LearningWorkGraph::Shader();
	shader.CompileFromFile("Shader/Shader.shader", "", "lib_6_8");

	ComPtr<ID3D12RootSignature> pGlobalRootSignature = CreateGlobalRootSignature();

	ComPtr<ID3D12StateObject> pStateObject = CreateGWGStateObject(pGlobalRootSignature, shader);
	D3D12_SET_PROGRAM_DESC SetProgramDesc = PrepareWorkGraph(pStateObject);

	char result[UAV_SIZE / sizeof(char)];
	if (DispatchWorkGraphAndReadResults(pGlobalRootSignature, SetProgramDesc, result))
	{
		printf("SUCCESS: Output was \"%s\"\n", result);
	}
#endif
}

bool HelloWorkGraphApplication::EnsureWorkGraphsSupported()
{
	D3D12_FEATURE_DATA_D3D12_OPTIONS21 Options = {};
	GetD3D12Device9()->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS21, &Options, sizeof(Options));
	LWG_CHECK_WITH_MESSAGE(Options.WorkGraphsTier != D3D12_WORK_GRAPHS_TIER_NOT_SUPPORTED, "Failed to ensure work graphs were supported. Check driver and graphics card.");
	return (Options.WorkGraphsTier != D3D12_WORK_GRAPHS_TIER_NOT_SUPPORTED);
}

ID3D12RootSignature* HelloWorkGraphApplication::CreateGlobalRootSignature()
{
	ID3D12RootSignature* pRootSignature = nullptr;

	CD3DX12_ROOT_PARAMETER RootSignatureUAV;
	RootSignatureUAV.InitAsUnorderedAccessView(0, 0);

	CD3DX12_ROOT_SIGNATURE_DESC Desc(1, &RootSignatureUAV, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);
	CComPtr<ID3DBlob> pSerialized;

	HRESULT hr = D3D12SerializeRootSignature(&Desc, D3D_ROOT_SIGNATURE_VERSION_1, &pSerialized, NULL);
	if (SUCCEEDED(hr))
	{
		hr = GetD3D12Device9()->CreateRootSignature(0, pSerialized->GetBufferPointer(), pSerialized->GetBufferSize(), IID_PPV_ARGS(&pRootSignature));
	}

	LWG_CHECK_WITH_MESSAGE(SUCCEEDED(hr), "Failed to create global root signature.");
	return pRootSignature;
}

ID3D12StateObject* HelloWorkGraphApplication::CreateGWGStateObject(ComPtr<ID3D12RootSignature> pGlobalRootSignature, const LearningWorkGraph::Shader& shader)
{
	ID3D12StateObject* d3d12StateObject = nullptr;

	auto desc = CD3DX12_STATE_OBJECT_DESC(D3D12_STATE_OBJECT_TYPE_EXECUTABLE);

	CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT* globalRootSignatureDesc = desc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	globalRootSignatureDesc->SetRootSignature(pGlobalRootSignature.Get());

	// シェーダライブラリを設定.
	CD3DX12_DXIL_LIBRARY_SUBOBJECT* libraryDesc = desc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	CD3DX12_SHADER_BYTECODE libraryCode(shader.GetData(), shader.GetSize());
	libraryDesc->SetDXILLibrary(&libraryCode);

	// ワークグラフのセットアップ.
	CD3DX12_WORK_GRAPH_SUBOBJECT* workGraphDesc = desc.CreateSubobject<CD3DX12_WORK_GRAPH_SUBOBJECT>();
	workGraphDesc->IncludeAllAvailableNodes();		// すべての利用可能なノードを使用する.
	workGraphDesc->SetProgramName(k_programName);

	HRESULT hr = GetD3D12Device9()->CreateStateObject(desc, IID_PPV_ARGS(&d3d12StateObject));
	LWG_CHECK_WITH_MESSAGE(SUCCEEDED(hr) && d3d12StateObject, "Failed to create Work Graph State Object.");

	return d3d12StateObject;
}

inline ID3D12Resource* HelloWorkGraphApplication::AllocateBuffer(UINT64 Size, D3D12_RESOURCE_FLAGS ResourceFlags, D3D12_HEAP_TYPE HeapType)
{
	ID3D12Resource* pResource;

	CD3DX12_HEAP_PROPERTIES HeapProperties(HeapType);
	CD3DX12_RESOURCE_DESC ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(Size, ResourceFlags);
	HRESULT hr = GetD3D12Device9()->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE, &ResourceDesc, D3D12_RESOURCE_STATE_COMMON, NULL, IID_PPV_ARGS(&pResource));
	LWG_CHECK_WITH_MESSAGE(SUCCEEDED(hr), "Failed to allocate buffer.");

	return pResource;
}

D3D12_SET_PROGRAM_DESC HelloWorkGraphApplication::PrepareWorkGraph(ComPtr<ID3D12StateObject> pStateObject)
{
	ComPtr<ID3D12Resource> pBackingMemoryResource = nullptr;

	ComPtr<ID3D12StateObjectProperties1> pStateObjectProperties;
	ComPtr<ID3D12WorkGraphProperties> pWorkGraphProperties;
	pStateObject->QueryInterface(IID_PPV_ARGS(&pStateObjectProperties));
	pStateObject->QueryInterface(IID_PPV_ARGS(&pWorkGraphProperties));

	// GPU で使用するメモリを確保.
	UINT index = pWorkGraphProperties->GetWorkGraphIndex(k_programName);
	D3D12_WORK_GRAPH_MEMORY_REQUIREMENTS MemoryRequirements = {};
	pWorkGraphProperties->GetWorkGraphMemoryRequirements(index, &MemoryRequirements);
	if (MemoryRequirements.MaxSizeInBytes > 0)
	{
		pBackingMemoryResource = AllocateBuffer(MemoryRequirements.MaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT);
	}

	UINT test = pWorkGraphProperties->GetNumEntrypoints(index);

	D3D12_SET_PROGRAM_DESC Desc = {};
	Desc.Type = D3D12_PROGRAM_TYPE_WORK_GRAPH;
	Desc.WorkGraph.ProgramIdentifier = pStateObjectProperties->GetProgramIdentifier(k_programName);
	Desc.WorkGraph.Flags = D3D12_SET_WORK_GRAPH_FLAG_INITIALIZE;
	if (pBackingMemoryResource)
	{
		Desc.WorkGraph.BackingMemory = { pBackingMemoryResource->GetGPUVirtualAddress(), MemoryRequirements.MaxSizeInBytes };
	}

	return Desc;
}

bool HelloWorkGraphApplication::RunCommandListAndWait(ComPtr<ID3D12CommandQueue> pCommandQueue, ComPtr<ID3D12CommandAllocator> pCommandAllocator, ComPtr<ID3D12GraphicsCommandList10> pCommandList, ComPtr<ID3D12Fence> pFence)
{
	if (SUCCEEDED(pCommandList->Close()))
	{
		ID3D12CommandList* commandLists[] = { pCommandList.Get() };
		pCommandQueue->ExecuteCommandLists(1, commandLists);
		pCommandQueue->Signal(pFence.Get(), 1);

		HANDLE hCommandListFinished = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (hCommandListFinished)
		{
			pFence->SetEventOnCompletion(1, hCommandListFinished);
			DWORD waitResult = WaitForSingleObject(hCommandListFinished, INFINITE);
			CloseHandle(hCommandListFinished);

			if (waitResult == WAIT_OBJECT_0 && SUCCEEDED(GetD3D12Device9()->GetDeviceRemovedReason()))
			{
				pCommandAllocator->Reset();
				pCommandList->Reset(pCommandAllocator.Get(), nullptr);
				return true;
			}
		}
	}

	return false;
}

bool HelloWorkGraphApplication::DispatchWorkGraphAndReadResults(ComPtr<ID3D12RootSignature> pGlobalRootSignature, D3D12_SET_PROGRAM_DESC SetProgramDesc, char* pResult)
{
	ComPtr<ID3D12Resource> pUAVBuffer = AllocateBuffer(UAV_SIZE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT);
	ComPtr<ID3D12Resource> pReadbackBuffer = AllocateBuffer(UAV_SIZE, D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_READBACK);

	// dispatch work graph
	D3D12_DISPATCH_GRAPH_DESC DispatchGraphDesc = {};
	DispatchGraphDesc.Mode = D3D12_DISPATCH_MODE_NODE_CPU_INPUT;
	DispatchGraphDesc.NodeCPUInput = { };
	DispatchGraphDesc.NodeCPUInput.EntrypointIndex = 0;
	DispatchGraphDesc.NodeCPUInput.NumRecords = 1;

	m_commandList->SetComputeRootSignature(pGlobalRootSignature.Get());
	m_commandList->SetComputeRootUnorderedAccessView(0, pUAVBuffer->GetGPUVirtualAddress());
	m_commandList->SetProgram(&SetProgramDesc);
	m_commandList->DispatchGraph(&DispatchGraphDesc);

	// read results
	D3D12_RESOURCE_BARRIER Barrier = {};
	Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	Barrier.Transition.pResource = pUAVBuffer.Get();
	Barrier.Transition.Subresource = 0;
	Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

	m_commandList->ResourceBarrier(1, &Barrier);
	m_commandList->CopyResource(pReadbackBuffer.Get(), pUAVBuffer.Get());

	if (RunCommandListAndWait(m_commandQueue, m_commandAllocator, m_commandList, m_fence))
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

	LWG_CHECK_WITH_MESSAGE(true, "Failed to dispatch work graph and read results.");
	return false;
}

void HelloWorkGraphApplication::CreatePipeline()
{
	auto* device = GetD3D12Device9();
	D3D12_COMMAND_QUEUE_DESC CommandQueueDesc = {};
	CommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT;
	CommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	LWG_CHECK_HRESULT(device->CreateCommandQueue(&CommandQueueDesc, IID_PPV_ARGS(&m_commandQueue)));
	LWG_CHECK_HRESULT(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
	LWG_CHECK_HRESULT(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
	LWG_CHECK_HRESULT(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
}

void HelloWorkGraphApplication::ExecuteComputeShader()
{

}

void HelloWorkGraphApplication::OnUpdate()
{}

void HelloWorkGraphApplication::OnRender()
{}

int main()
{
	LearningWorkGraph::FrameworkDesc frameworkDesc = {};

	auto framework = LearningWorkGraph::Framework();
	framework.Initialize(frameworkDesc);

	auto application = HelloWorkGraphApplication();
	application.Initialize(&framework);

	framework.Run();

	framework.Terminate();
	
	return 0;
}
