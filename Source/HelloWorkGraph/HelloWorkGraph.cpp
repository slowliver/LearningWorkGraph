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
#include <random>

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
public:
	virtual void OnInitialize() override;
	virtual void OnUpdate() override;
	virtual void OnRender() override;

private:
	ID3D12Resource* CreateBuffer(uint64_t Size, D3D12_RESOURCE_FLAGS ResourceFlags, D3D12_HEAP_TYPE HeapType);

	bool EnsureWorkGraphsSupported();
	ID3D12RootSignature* CreateGlobalRootSignature();
	ID3D12StateObject* CreateGWGStateObject(ComPtr<ID3D12RootSignature> pGlobalRootSignature, const LearningWorkGraph::Shader& shader);
	D3D12_SET_PROGRAM_DESC PrepareWorkGraph(ComPtr<ID3D12StateObject> pStateObject);
	bool RunCommandListAndWait(ComPtr<ID3D12CommandQueue> pCommandQueue, ComPtr<ID3D12CommandAllocator> pCommandAllocator, ComPtr<ID3D12GraphicsCommandList10> pCommandList, ComPtr<ID3D12Fence> pFence);
	bool DispatchWorkGraphAndReadResults(ComPtr<ID3D12RootSignature> pGlobalRootSignature, D3D12_SET_PROGRAM_DESC SetProgramDesc, char* pResult);

	void CreateBasePipeline();
	void CreateComputePipeline();

	void ExecuteComputeShader();

private:
	enum CBVSRVUAVRootParameterSlotID
	{
		ConstantBufferView = 0,
		ShaderResourceView = 1,
		UnorderedAccessView = 2,
		Count
	};
	ComPtr<ID3D12CommandQueue> m_commandQueue = nullptr;
	ComPtr<ID3D12CommandAllocator> m_commandAllocator = nullptr;
	ComPtr<ID3D12GraphicsCommandList10> m_commandList = nullptr;
	ComPtr<ID3D12Fence> m_fence = nullptr;
	uint64_t m_fenceValue = 0;

	uint32_t m_numSortElements = 1 << 10;
	ComPtr<ID3D12Resource> m_inputBuffer = nullptr;
	ComPtr<ID3D12Resource> m_sortedBuffer = nullptr;
	ComPtr<ID3D12Resource> m_sortedBufferCPUReadback = nullptr;

	struct ComputePipeline
	{
		ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
		ComPtr<ID3D12PipelineState> m_pipelineState = nullptr;
	} m_computePipeline;

private:
	static constexpr const wchar_t* k_programName = L"Hello World";

};

void Kernel(const std::unique_ptr<int[]>& a, int p, int q)
{
	int d = 1 << (p - q);

	for (int i = 0; i < (1 << 5); i++)
	{
		bool up = ((i >> p) & 2) == 0;

		if ((i & d) == 0 && (a[i] > a[i | d]) == up)
		{
			int t = a[i];
			a[i] = a[i | d];
			a[i | d] = t;
		}
	}
}

void BitonicSort(int logn, const std::unique_ptr<int[]>& a)
{
	for (int i = 0; i < logn; i++)
	{
		for (int j = 0; j <= i; j++)
		{
			Kernel(a, i, j);
		}
	}
}

void HelloWorkGraphApplication::OnInitialize()
{
	auto* d3d12Device9 = GetD3D12Device9();

#if 0
	int logn = 5;
	int n = (1 << logn);

	auto randomEngine = std::mt19937();
	auto random = std::uniform_int_distribution<uint32_t>(0, n - 1);

	auto a0 = std::unique_ptr<int[]>(new int[n]);
	for (uint32_t i = 0; i < n; ++i)
	{
		a0[i] = random(randomEngine);
	}

	for (uint32_t i = 0; i < n; ++i)
	{
		printf("%u : %u\n", i, a0[i]);
	}

	BitonicSort(logn, a0);

	printf("-----------------------------------\n");

	for (uint32_t i = 0; i < n; ++i)
	{
		printf("%u : %u\n", i, a0[i]);
	}
#endif
#if 0

	System.Console.WriteLine();

	for (int k = 0; k < a0.Length; k++)
	{
		System.Console.Write(a0[k] + " ");
	}
#endif

	CreateBasePipeline();

	ExecuteComputeShader();

#if 0
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
	ComPtr<ID3DBlob> pSerialized;

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

ID3D12Resource* HelloWorkGraphApplication::CreateBuffer(uint64_t size, D3D12_RESOURCE_FLAGS resourceFlags, D3D12_HEAP_TYPE heapType)
{
	ID3D12Resource* resource = nullptr;
	auto heapProperties = CD3DX12_HEAP_PROPERTIES(heapType);
	auto desc = CD3DX12_RESOURCE_DESC::Buffer(size, resourceFlags);
	LWG_CHECK_HRESULT(m_d3d12Device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, NULL, IID_PPV_ARGS(&resource)));
	return resource;
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
		pBackingMemoryResource = CreateBuffer(MemoryRequirements.MaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT);
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
		pCommandQueue->Signal(pFence.Get(), ++m_fenceValue);

		HANDLE hCommandListFinished = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (hCommandListFinished)
		{
			pFence->SetEventOnCompletion(m_fenceValue, hCommandListFinished);
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
	ComPtr<ID3D12Resource> pUAVBuffer = CreateBuffer(UAV_SIZE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT);
	ComPtr<ID3D12Resource> pReadbackBuffer = CreateBuffer(UAV_SIZE, D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_READBACK);

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

void HelloWorkGraphApplication::CreateBasePipeline()
{
	// Create input resource.
	{
		auto randomEngine = std::mt19937();
		auto random = std::uniform_int_distribution<uint32_t>(0, m_numSortElements - 1);
		m_inputBuffer = CreateBuffer
		(
			sizeof(uint32_t) * m_numSortElements,
			D3D12_RESOURCE_FLAG_NONE,
			D3D12_HEAP_TYPE_UPLOAD
		);
		uint32_t* buffer = nullptr;
		auto range = CD3DX12_RANGE(0, sizeof(uint32_t) * m_numSortElements);
		LWG_CHECK_HRESULT(m_inputBuffer->Map(0, &range, (void**)&buffer));
		for (uint32_t i = 0; i < m_numSortElements; ++i)
		{
			buffer[i] = random(randomEngine);
		}
		m_inputBuffer->Unmap(0, NULL);
	}

	// Create output resource.
	{
		m_sortedBuffer = CreateBuffer
		(
			sizeof(uint32_t) * m_numSortElements,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			D3D12_HEAP_TYPE_DEFAULT
		);
		m_sortedBufferCPUReadback = CreateBuffer
		(
			sizeof(uint32_t) * m_numSortElements,
			D3D12_RESOURCE_FLAG_NONE,
			D3D12_HEAP_TYPE_READBACK
		);
	}

	D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT;
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	LWG_CHECK_HRESULT(m_d3d12Device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_commandQueue)));
	LWG_CHECK_HRESULT(m_d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
	LWG_CHECK_HRESULT(m_d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
	LWG_CHECK_HRESULT(m_d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
}

void HelloWorkGraphApplication::CreateComputePipeline()
{
	auto* device = GetD3D12Device9();

	auto computeShader = LearningWorkGraph::Shader();
	LWG_CHECK(computeShader.CompileFromFile("Shader/Shader.shader", "CSMain", "cs_6_5"));

	CD3DX12_ROOT_PARAMETER rootParameter[CBVSRVUAVRootParameterSlotID::Count] = {};
	rootParameter[CBVSRVUAVRootParameterSlotID::ConstantBufferView].InitAsConstantBufferView(0, 0);
	rootParameter[CBVSRVUAVRootParameterSlotID::ShaderResourceView].InitAsShaderResourceView(0, 0);
	rootParameter[CBVSRVUAVRootParameterSlotID::UnorderedAccessView].InitAsUnorderedAccessView(0, 0);
	auto rootSignatureDesc = CD3DX12_ROOT_SIGNATURE_DESC(CBVSRVUAVRootParameterSlotID::Count, rootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);
	ComPtr<ID3DBlob> serialized = nullptr;
	LWG_CHECK_HRESULT(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, NULL));
	LWG_CHECK_HRESULT(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&m_computePipeline.m_rootSignature)));

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc = {};
	computePipelineStateDesc.pRootSignature = m_computePipeline.m_rootSignature.Get();
	computePipelineStateDesc.CS = CD3DX12_SHADER_BYTECODE(computeShader.GetData(), computeShader.GetSize());
	LWG_CHECK_HRESULT(device->CreateComputePipelineState(&computePipelineStateDesc, IID_PPV_ARGS(&m_computePipeline.m_pipelineState)));
}

void HelloWorkGraphApplication::ExecuteComputeShader()
{
	CreateComputePipeline();
	m_commandList->SetComputeRootSignature(m_computePipeline.m_rootSignature.Get());
	m_commandList->SetPipelineState(m_computePipeline.m_pipelineState.Get());
	m_commandList->SetComputeRootShaderResourceView(CBVSRVUAVRootParameterSlotID::ShaderResourceView, m_inputBuffer->GetGPUVirtualAddress());
	m_commandList->SetComputeRootUnorderedAccessView(CBVSRVUAVRootParameterSlotID::UnorderedAccessView, m_sortedBuffer->GetGPUVirtualAddress());
	m_commandList->Dispatch(1, 1, 1);

	// read results
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_sortedBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE, 0);
	m_commandList->ResourceBarrier(1, &barrier);
	m_commandList->CopyResource(m_sortedBufferCPUReadback.Get(), m_sortedBuffer.Get());

	// Close and execute the command list.
	LWG_CHECK_HRESULT(m_commandList->Close());
	ID3D12CommandList* commandLists[] = { m_commandList.Get()};
	m_commandQueue->ExecuteCommandLists(1, commandLists);
	LWG_CHECK_HRESULT(m_commandQueue->Signal(m_fence.Get(), ++m_fenceValue));

	HANDLE commandListFinished = CreateEventA(NULL, FALSE, FALSE, NULL);
	LWG_CHECK(commandListFinished);

	m_fence->SetEventOnCompletion(m_fenceValue, commandListFinished);
	auto waitResult = WaitForSingleObject(commandListFinished, INFINITE);
	LWG_CHECK(CloseHandle(commandListFinished));

	if (waitResult == WAIT_OBJECT_0 && SUCCEEDED(GetD3D12Device9()->GetDeviceRemovedReason()))
	{
		LWG_CHECK_HRESULT(m_commandAllocator->Reset());
		LWG_CHECK_HRESULT(m_commandList->Reset(m_commandAllocator.Get(), nullptr));
	}

	// Readback to CPU memory.
	uint32_t* outputTemp = nullptr;
	auto output = std::unique_ptr<uint32_t[]>(new uint32_t[m_numSortElements]);
	auto range = CD3DX12_RANGE(0, sizeof(uint32_t) * m_numSortElements);
	LWG_CHECK_HRESULT(m_sortedBufferCPUReadback->Map(0, &range, (void**)&outputTemp));
	memcpy(output.get(), outputTemp, sizeof(uint32_t) * m_numSortElements);
	m_sortedBufferCPUReadback->Unmap(0, nullptr);

	for (uint32_t i = 0; i < m_numSortElements; ++i)
	{
		printf("%u : %u\n", i, output[i]);
	}
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
