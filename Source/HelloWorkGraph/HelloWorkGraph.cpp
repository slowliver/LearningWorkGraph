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

// C++ STL
#include <array>
#include <random>
#include <bit>
#include <functional>

#include <windows.h>
#include <d3d12.h>
#include <d3dx12/d3dx12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <wrl.h>
#if defined(_DEBUG)
#include <d3d12sdklayers.h>
#include <dxgidebug.h>
#endif

#include <Framework/Application.h>
#include <Framework/Framework.h>
#include <Framework/Shader.h>

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 613; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

using Microsoft::WRL::ComPtr;

class HelloWorkGraphApplication : public LearningWorkGraph::Application
{
private:
	struct ApplicationConstantBuffer final
	{
		uint32_t m_numSortElements;
		uint32_t m_dummy[63];
	};
	static_assert(sizeof(ApplicationConstantBuffer) == 256);
	struct PassConstantBuffer final
	{
		uint32_t m_inc;
		uint32_t m_dir;
	};
	static_assert(sizeof(PassConstantBuffer) % 4 == 0);

public:
	virtual void OnInitialize() override;
	virtual void OnUpdate() override;
	virtual void OnRender() override;

private:
	ID3D12Resource* CreateBuffer(uint64_t Size, D3D12_RESOURCE_FLAGS ResourceFlags, D3D12_HEAP_TYPE HeapType);

	bool EnsureWorkGraphsSupported();
	D3D12_SET_PROGRAM_DESC PrepareWorkGraph();

	void PreExecute();
	void PostExecute();

	void CreateBasePipeline();

	void CreateComputePipeline();
	void ExecuteComputeShader();

	void CreateWorkGraphPipeline();
	void ExecuteWorkGraph();

private:
	enum class PipelineMode
	{
		Compute,
		WorkGraph,
		Count
	} m_pipelineMode = PipelineMode::Compute;
	struct ConstantBufferRegisterID
	{
		enum
		{
			Application = 0,
			Pass,
			Count
		};
	};
	struct RootParameterSlotID
	{
		enum
		{
			ApplicationConstantBufferView = 0,
			PassConstants,
			ShaderResourceView,
			UnorderedAccessView,
			Count
		};
	};
	ComPtr<ID3D12CommandAllocator> m_commandAllocator = nullptr;
	ComPtr<ID3D12GraphicsCommandList10> m_commandList = nullptr;
	ComPtr<ID3D12Fence> m_fence = nullptr;
	uint64_t m_fenceValue = 0;

	uint32_t m_numSortElementsUnsafe = 1 << 16;
	uint32_t m_numSortElements = 0;
	ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
	ComPtr<ID3D12Resource> m_gpuTimeCPUReadbackBuffer = nullptr;
	ComPtr<ID3D12Resource> m_applicationConstantBuffer = nullptr;
	ComPtr<ID3D12Resource> m_initialBuffer = nullptr;
	ComPtr<ID3D12Resource> m_sortBuffer = nullptr;
	ComPtr<ID3D12Resource> m_sortCPUReadbackBuffer = nullptr;

	// Reset each frame.
	uint32_t m_queryIndex = 0;

	struct ComputePipeline
	{
		ComPtr<ID3D12PipelineState> m_pipelineState = nullptr;
	} m_computePipeline = {};

	struct WorkGraphPipeline
	{
		ComPtr<ID3D12StateObject> m_stateObject = nullptr;
		ComPtr<ID3D12StateObjectProperties1> m_stateObjectProperties = nullptr;
		ComPtr<ID3D12WorkGraphProperties> m_workGraphProperties = nullptr;
		D3D12_WORK_GRAPH_MEMORY_REQUIREMENTS m_memoryRequirements = {};
		ComPtr<ID3D12Resource> m_backingMemoryBuffer = nullptr;
	} m_workGraphPipeline = {};

private:
	static constexpr const wchar_t* k_programName = L"Hello World";

};

void HelloWorkGraphApplication::OnInitialize()
{
	if (!EnsureWorkGraphsSupported())
	{
		return;
	}

	m_numSortElements = std::bit_ceil(m_numSortElementsUnsafe);

	CreateBasePipeline();
	CreateComputePipeline();
	CreateWorkGraphPipeline();
}

bool HelloWorkGraphApplication::EnsureWorkGraphsSupported()
{
	D3D12_FEATURE_DATA_D3D12_OPTIONS21 Options = {};
	GetD3D12Device9()->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS21, &Options, sizeof(Options));
	LWG_CHECK_WITH_MESSAGE(Options.WorkGraphsTier != D3D12_WORK_GRAPHS_TIER_NOT_SUPPORTED, "Failed to ensure work graphs were supported. Check driver and graphics card.");
	return (Options.WorkGraphsTier != D3D12_WORK_GRAPHS_TIER_NOT_SUPPORTED);
}

ID3D12Resource* HelloWorkGraphApplication::CreateBuffer(uint64_t size, D3D12_RESOURCE_FLAGS resourceFlags, D3D12_HEAP_TYPE heapType)
{
	ID3D12Resource* resource = nullptr;
	auto heapProperties = CD3DX12_HEAP_PROPERTIES(heapType);
	auto desc = CD3DX12_RESOURCE_DESC::Buffer(size, resourceFlags);
	LWG_CHECK_HRESULT(m_d3d12Device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, NULL, IID_PPV_ARGS(&resource)));
	return resource;
}

D3D12_SET_PROGRAM_DESC HelloWorkGraphApplication::PrepareWorkGraph()
{
	D3D12_SET_PROGRAM_DESC setProgramDesc = {};
	setProgramDesc.Type = D3D12_PROGRAM_TYPE_WORK_GRAPH;
	setProgramDesc.WorkGraph.ProgramIdentifier = m_workGraphPipeline.m_stateObjectProperties->GetProgramIdentifier(k_programName);
	setProgramDesc.WorkGraph.Flags = D3D12_SET_WORK_GRAPH_FLAG_INITIALIZE;
	if (m_workGraphPipeline.m_backingMemoryBuffer)
	{
		setProgramDesc.WorkGraph.BackingMemory = { m_workGraphPipeline.m_backingMemoryBuffer->GetGPUVirtualAddress(), m_workGraphPipeline.m_memoryRequirements.MaxSizeInBytes };
	}
	return setProgramDesc;
}

void HelloWorkGraphApplication::CreateBasePipeline()
{
	// Create GPU time buffer.
	{
		m_gpuTimeCPUReadbackBuffer = CreateBuffer
		(
			sizeof(uint64_t) * 2,
			D3D12_RESOURCE_FLAG_NONE,
			D3D12_HEAP_TYPE_READBACK
		);
	}

	// Create application constant buffer.
	{
		m_applicationConstantBuffer = CreateBuffer
		(
			sizeof(ApplicationConstantBuffer),
			D3D12_RESOURCE_FLAG_NONE,
			D3D12_HEAP_TYPE_UPLOAD
		);
		ApplicationConstantBuffer* applicationConstantBuffer = nullptr;
		auto range = CD3DX12_RANGE(0, sizeof(ApplicationConstantBuffer));
		LWG_CHECK_HRESULT(m_applicationConstantBuffer->Map(0, &range, (void**)&applicationConstantBuffer));
		applicationConstantBuffer->m_numSortElements = m_numSortElements;
		memset(applicationConstantBuffer->m_dummy, 0, sizeof(applicationConstantBuffer->m_dummy));
		m_applicationConstantBuffer->Unmap(0, NULL);
	}

	// Create inital buffer.
	{
		auto randomEngine = std::mt19937();
		auto random = std::uniform_int_distribution<uint32_t>(0, m_numSortElements - 1);
		m_initialBuffer = CreateBuffer
		(
			sizeof(uint32_t) * m_numSortElements,
			D3D12_RESOURCE_FLAG_NONE,
			D3D12_HEAP_TYPE_UPLOAD
		);
		uint32_t* buffer = nullptr;
		auto range = CD3DX12_RANGE(0, sizeof(uint32_t) * m_numSortElements);
		LWG_CHECK_HRESULT(m_initialBuffer->Map(0, &range, (void**)&buffer));
		for (uint32_t i = 0; i < m_numSortElements; ++i)
		{
#if 1
			buffer[i] = (i < m_numSortElementsUnsafe) ? random(randomEngine) : UINT32_MAX;
#else
			buffer[i] = 0;
#endif
		}
		m_initialBuffer->Unmap(0, NULL);
		m_initialBuffer->SetName(L"initialInputBuffer");
	}

	// Create sort buffer.
	{
		m_sortBuffer = CreateBuffer
		(
			sizeof(uint32_t) * m_numSortElements,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			D3D12_HEAP_TYPE_DEFAULT
		);
		m_sortBuffer->SetName(L"sortedBuffer");
		m_sortCPUReadbackBuffer = CreateBuffer
		(
			sizeof(uint32_t) * m_numSortElements,
			D3D12_RESOURCE_FLAG_NONE,
			D3D12_HEAP_TYPE_READBACK
		);
		m_sortCPUReadbackBuffer->SetName(L"sortedCPUReadbackBuffer");
	}

	// Create root signature.
	{
		CD3DX12_ROOT_PARAMETER rootParameter[RootParameterSlotID::Count] = {};
		rootParameter[RootParameterSlotID::ApplicationConstantBufferView].InitAsConstantBufferView(ConstantBufferRegisterID::Application, 0);
		rootParameter[RootParameterSlotID::PassConstants].InitAsConstants(sizeof(PassConstantBuffer) / sizeof(uint32_t), ConstantBufferRegisterID::Pass, 0);
		rootParameter[RootParameterSlotID::ShaderResourceView].InitAsShaderResourceView(0, 0);
		rootParameter[RootParameterSlotID::UnorderedAccessView].InitAsUnorderedAccessView(0, 0);
		auto rootSignatureDesc = CD3DX12_ROOT_SIGNATURE_DESC(RootParameterSlotID::Count, rootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);
		ComPtr<ID3DBlob> serialized = nullptr;
		LWG_CHECK_HRESULT(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, NULL));
		LWG_CHECK_HRESULT(m_d3d12Device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
	}

	LWG_CHECK_HRESULT(m_d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
	LWG_CHECK_HRESULT(m_d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
	LWG_CHECK_HRESULT(m_d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
}

void HelloWorkGraphApplication::PreExecute()
{
	m_queryIndex = 0;
	m_commandList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, m_queryIndex++);

	// Copy initial buffer to sorted buffer.
	{
		{
			std::array<D3D12_RESOURCE_BARRIER, 2> barriers = {};
			barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(m_initialBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE, 0);
			barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_sortBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST, 0);
			m_commandList->ResourceBarrier(barriers.size(), barriers.data());
		}
		m_commandList->CopyBufferRegion(m_sortBuffer.Get(), 0, m_initialBuffer.Get(), 0, sizeof(uint32_t) * m_numSortElements);
		{
			std::array<D3D12_RESOURCE_BARRIER, 2> barriers = {};
			barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(m_initialBuffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON, 0);
			barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_sortBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0);
			m_commandList->ResourceBarrier(barriers.size(), barriers.data());
		}
	}
}

void HelloWorkGraphApplication::PostExecute()
{
	m_commandList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, m_queryIndex++);
	m_commandList->ResolveQueryData(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, m_gpuTimeCPUReadbackBuffer.Get(), 0);

	// read results
	{
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_sortBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE, 0);
		m_commandList->ResourceBarrier(1, &barrier);
		m_commandList->CopyResource(m_sortCPUReadbackBuffer.Get(), m_sortBuffer.Get());
	}

	// Close and execute the command list.
	LWG_CHECK_HRESULT(m_commandList->Close());
	ID3D12CommandList* commandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(1, commandLists);

	if (m_swapChain)
	{
		m_swapChain->Present(1, 0);
	}

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
	LWG_CHECK_HRESULT(m_sortCPUReadbackBuffer->Map(0, &range, (void**)&outputTemp));
	memcpy(output.get(), outputTemp, sizeof(uint32_t) * m_numSortElements);
	m_sortCPUReadbackBuffer->Unmap(0, NULL);

#if 0
	for (uint32_t i = 0; i < m_numSortElementsUnsafe; ++i)
	{
		printf("%u : %u\n", i, output[i]);
	}
#endif

	{
		uint64_t gpuTimeFrequency = 0;
		m_commandQueue->GetTimestampFrequency(&gpuTimeFrequency);
		uint64_t* queryResultPointer = nullptr;
		auto range = CD3DX12_RANGE(0, sizeof(uint64_t) * 2);
		LWG_CHECK_HRESULT(m_gpuTimeCPUReadbackBuffer->Map(0, &range, (void**)&queryResultPointer));
		const auto gpuTime = (queryResultPointer[1] - queryResultPointer[0]) * 1000.0f / gpuTimeFrequency;
		m_gpuTimeCPUReadbackBuffer->Unmap(0, NULL);
		char gpuTimeText[256] = {};
		const char* pipelineModeText = m_pipelineMode == PipelineMode::Compute ? "Compute" : "Work Graph";
		sprintf(gpuTimeText, "Pipeline Mode: %s, GPU Time: %fms\n", pipelineModeText, gpuTime);
		printf(gpuTimeText);
		SetConsoleTitleA(gpuTimeText);
	}
}

void HelloWorkGraphApplication::CreateComputePipeline()
{
	auto computeShader = LearningWorkGraph::Shader();
	LWG_CHECK(computeShader.CompileFromFile("Shader/Shader.shader", "CSMain", "cs_6_5"));

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc = {};
	computePipelineStateDesc.pRootSignature = m_rootSignature.Get();
	computePipelineStateDesc.CS = CD3DX12_SHADER_BYTECODE(computeShader.GetData(), computeShader.GetSize());
	LWG_CHECK_HRESULT(m_d3d12Device->CreateComputePipelineState(&computePipelineStateDesc, IID_PPV_ARGS(&m_computePipeline.m_pipelineState)));
}

void HelloWorkGraphApplication::ExecuteComputeShader()
{
	m_commandList->SetComputeRootSignature(m_rootSignature.Get());
	m_commandList->SetPipelineState(m_computePipeline.m_pipelineState.Get());
	m_commandList->SetComputeRootConstantBufferView(RootParameterSlotID::ApplicationConstantBufferView, m_applicationConstantBuffer->GetGPUVirtualAddress());
	m_commandList->SetComputeRootUnorderedAccessView(RootParameterSlotID::UnorderedAccessView, m_sortBuffer->GetGPUVirtualAddress());

	const uint32_t log2n = static_cast<uint32_t>(std::log2f(m_numSortElements));
	uint32_t inc = 0;
	// Main-block.
	for (uint32_t i = 0; i < log2n; ++i)
	{
		inc = 1 << i;
		// Sub-block.
		for (uint32_t j = 0; j < i + 1; ++j)
		{
			const bool isFirstStep = (i == 0 && j == 0);
			if (!isFirstStep)
			{
				auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(m_sortBuffer.Get());
				m_commandList->ResourceBarrier(1, &barrier);
			}
			PassConstantBuffer passConstantBuffer = { inc, 2 << i };
			m_commandList->SetComputeRoot32BitConstants(RootParameterSlotID::PassConstants, sizeof(PassConstantBuffer) / sizeof(uint32_t), &passConstantBuffer, 0);
			m_commandList->Dispatch(max(1, m_numSortElements / 2 / 1024), 1, 1);
			inc /= 2;
		}
	}
}

void HelloWorkGraphApplication::CreateWorkGraphPipeline()
{
	auto shader = LearningWorkGraph::Shader();
	LWG_CHECK(shader.CompileFromFile("Shader/Shader.shader", "", "lib_6_8"));

	auto desc = CD3DX12_STATE_OBJECT_DESC(D3D12_STATE_OBJECT_TYPE_EXECUTABLE);

	CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT* globalRootSignatureDesc = desc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	globalRootSignatureDesc->SetRootSignature(m_rootSignature.Get());

	// シェーダライブラリを設定.
	CD3DX12_DXIL_LIBRARY_SUBOBJECT* libraryDesc = desc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	CD3DX12_SHADER_BYTECODE libraryCode(shader.GetData(), shader.GetSize());
	libraryDesc->SetDXILLibrary(&libraryCode);

	// ワークグラフのセットアップ.
	CD3DX12_WORK_GRAPH_SUBOBJECT* workGraphDesc = desc.CreateSubobject<CD3DX12_WORK_GRAPH_SUBOBJECT>();
	workGraphDesc->IncludeAllAvailableNodes();		// すべての利用可能なノードを使用する.
	workGraphDesc->SetProgramName(k_programName);

	LWG_CHECK_HRESULT(m_d3d12Device->CreateStateObject(desc, IID_PPV_ARGS(&m_workGraphPipeline.m_stateObject)));
	LWG_CHECK_HRESULT(m_workGraphPipeline.m_stateObject.As(&m_workGraphPipeline.m_stateObjectProperties));
	LWG_CHECK_HRESULT(m_workGraphPipeline.m_stateObject.As(&m_workGraphPipeline.m_workGraphProperties));

	// GPU で使用するメモリを確保.
	auto index = m_workGraphPipeline.m_workGraphProperties->GetWorkGraphIndex(k_programName);
	m_workGraphPipeline.m_workGraphProperties->GetWorkGraphMemoryRequirements(index, &m_workGraphPipeline.m_memoryRequirements);
	if (m_workGraphPipeline.m_memoryRequirements.MaxSizeInBytes > 0)
	{
		m_workGraphPipeline.m_backingMemoryBuffer = CreateBuffer(m_workGraphPipeline.m_memoryRequirements.MaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT);
		m_workGraphPipeline.m_backingMemoryBuffer->SetName(L"backingMemoryBuffer");
	}

	//	UINT test = m_workGraphPipeline.m_workGraphProperties->GetNumEntrypoints(index);
}

void HelloWorkGraphApplication::ExecuteWorkGraph()
{
	D3D12_SET_PROGRAM_DESC setProgramDesc = PrepareWorkGraph();

	struct ApplicationRecord
	{
#if 1
		uint32_t m_dispatchGrid;
#endif
		uint32_t m_numSortElements;
	} applicationRecord = { max(1, m_numSortElements / 2 / 1024), m_numSortElements };

	// dispatch work graph
	D3D12_DISPATCH_GRAPH_DESC DispatchGraphDesc = {};
	DispatchGraphDesc.Mode = D3D12_DISPATCH_MODE_NODE_CPU_INPUT;
	DispatchGraphDesc.NodeCPUInput.EntrypointIndex = 0;
	DispatchGraphDesc.NodeCPUInput.NumRecords = 1;
	DispatchGraphDesc.NodeCPUInput.pRecords = &applicationRecord;
	DispatchGraphDesc.NodeCPUInput.RecordStrideInBytes = sizeof(ApplicationRecord);

	m_commandList->SetComputeRootSignature(m_rootSignature.Get());
	m_commandList->SetComputeRootUnorderedAccessView(RootParameterSlotID::UnorderedAccessView, m_sortBuffer->GetGPUVirtualAddress());
	m_commandList->SetProgram(&setProgramDesc);
	m_commandList->DispatchGraph(&DispatchGraphDesc);
}

void HelloWorkGraphApplication::OnUpdate()
{
	if (GetKeyState(VK_F1) & 0x8000)
	{
		m_pipelineMode = PipelineMode::Compute;
	}
	else if (GetKeyState(VK_F2) & 0x8000)
	{
		m_pipelineMode = PipelineMode::WorkGraph;
	}
}

void HelloWorkGraphApplication::OnRender()
{
	PreExecute();

	if (m_pipelineMode == PipelineMode::Compute)
	{
		ExecuteComputeShader();
	}
	else if (m_pipelineMode == PipelineMode::WorkGraph)
	{
		ExecuteWorkGraph();
	}

	PostExecute();
}

int main()
{
	LearningWorkGraph::FrameworkDesc frameworkDesc = {};
	frameworkDesc.m_useWindow = false;

	auto framework = LearningWorkGraph::Framework();
	framework.Initialize(frameworkDesc);

	auto application = HelloWorkGraphApplication();
	application.Initialize(&framework);

	framework.Run();

	framework.Terminate();
	
	return 0;
}
