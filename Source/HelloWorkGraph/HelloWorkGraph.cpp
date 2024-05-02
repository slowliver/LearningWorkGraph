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
	virtual void OnUpdate() override {}
	virtual void OnRender() override;

private:
	bool EnsureWorkGraphsSupported();
	ID3D12RootSignature* CreateGlobalRootSignature();
	ID3D12StateObject* CreateGWGStateObject(ComPtr<ID3D12RootSignature> pGlobalRootSignature, const LearningWorkGraph::Shader& shader);
	ID3D12Resource* AllocateBuffer(UINT64 Size, D3D12_RESOURCE_FLAGS ResourceFlags, D3D12_HEAP_TYPE HeapType);
	D3D12_SET_PROGRAM_DESC PrepareWorkGraph(ComPtr<ID3D12StateObject> pStateObject);
	bool RunCommandListAndWait(ComPtr<ID3D12CommandQueue> pCommandQueue, ComPtr<ID3D12CommandAllocator> pCommandAllocator, ComPtr<ID3D12GraphicsCommandList10> pCommandList, ComPtr<ID3D12Fence> pFence);
	bool DispatchWorkGraphAndReadResults(ComPtr<ID3D12RootSignature> pGlobalRootSignature, D3D12_SET_PROGRAM_DESC SetProgramDesc, char* pResult);

private:
	static constexpr const wchar_t* k_programName = L"Hello World";
	ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
	ComPtr<ID3D12PipelineState> m_pipelineState = nullptr;

};

void HelloWorkGraphApplication::OnInitialize()
{
	auto* d3d12Device9 = GetD3D12Device9();

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

	{
		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature = nullptr;
		ComPtr<ID3DBlob> error = nullptr;
		LWG_CHECK(SUCCEEDED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)));
		LWG_CHECK(SUCCEEDED(m_d3d12Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature))));
	}


	// Define the vertex input layout.
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	auto vertexShader = LearningWorkGraph::Shader();
	vertexShader.CompileFromFile("Shader/Shader.shader", "VSMain", "vs_6_5");

	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDesc.pRootSignature = m_rootSignature.Get();
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.GetData(), vertexShader.GetSize());
#if 0
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
#endif
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;
	LWG_CHECK(SUCCEEDED(m_d3d12Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState))));
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
	ComPtr<ID3D12CommandQueue> pCommandQueue;
	ComPtr<ID3D12CommandAllocator> pCommandAllocator;
	ComPtr<ID3D12GraphicsCommandList10> pCommandList;
	ComPtr<ID3D12Fence> pFence;

	auto* device = GetD3D12Device9();

	D3D12_COMMAND_QUEUE_DESC CommandQueueDesc = {};
	CommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT;
	CommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	device->CreateCommandQueue(&CommandQueueDesc, IID_PPV_ARGS(&pCommandQueue));

	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pCommandAllocator));
	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&pCommandList));
	device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence));

	if (pCommandQueue && pCommandAllocator && pCommandList && pFence)
	{
		ComPtr<ID3D12Resource> pUAVBuffer = AllocateBuffer(UAV_SIZE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT);
		ComPtr<ID3D12Resource> pReadbackBuffer = AllocateBuffer(UAV_SIZE, D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_READBACK);

		// dispatch work graph
		D3D12_DISPATCH_GRAPH_DESC DispatchGraphDesc = {};
		DispatchGraphDesc.Mode = D3D12_DISPATCH_MODE_NODE_CPU_INPUT;
		DispatchGraphDesc.NodeCPUInput = { };
		DispatchGraphDesc.NodeCPUInput.EntrypointIndex = 0;
		DispatchGraphDesc.NodeCPUInput.NumRecords = 1;

		pCommandList->SetComputeRootSignature(pGlobalRootSignature.Get());
		pCommandList->SetComputeRootUnorderedAccessView(0, pUAVBuffer->GetGPUVirtualAddress());
		pCommandList->SetProgram(&SetProgramDesc);
		pCommandList->DispatchGraph(&DispatchGraphDesc);

		// read results
		D3D12_RESOURCE_BARRIER Barrier = {};
		Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		Barrier.Transition.pResource = pUAVBuffer.Get();
		Barrier.Transition.Subresource = 0;
		Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

		pCommandList->ResourceBarrier(1, &Barrier);
		pCommandList->CopyResource(pReadbackBuffer.Get(), pUAVBuffer.Get());

		if (RunCommandListAndWait(pCommandQueue, pCommandAllocator, pCommandList, pFence))
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

	LWG_CHECK_WITH_MESSAGE(true, "Failed to dispatch work graph and read results.");
	return false;
}

void HelloWorkGraphApplication::OnRender()
{
	// Record all the commands we need to render the scene into the command list.
	{
		// Command list allocators can only be reset when the associated 
 // command lists have finished execution on the GPU; apps should use 
 // fences to determine GPU execution progress.
		LWG_CHECK(SUCCEEDED(m_commandAllocators[m_frameIndex]->Reset()));

		// However, when ExecuteCommandList() is called on a particular command 
		// list, that command list can then be reset at any time and must be before 
		// re-recording.
		LWG_CHECK(SUCCEEDED(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get())));

		// Set necessary state.
		D3D12_VIEWPORT viewport =
		{
			0.0f,
			0.0f,
			1280.0f,
			720.0f,
			0.0f,
			1.0f
		};
		D3D12_RECT scissorRect =
		{
			0,
			0,
			1280,
			720,
		};
		m_commandList->SetPipelineState(m_pipelineState.Get());
		m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
		m_commandList->RSSetViewports(1, &viewport);
		m_commandList->RSSetScissorRects(1, &scissorRect);

		D3D12_RESOURCE_BARRIER barrier = {};
		
		// Indicate that the back buffer will be used as a render target.
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_commandList->ResourceBarrier(1, &barrier);

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_renderTargetViewDescriptorSize);
		m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

		// Record commands.
		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
#if 0
		m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
		m_commandList->DrawInstanced(3, 1, 0, 0);
#endif

		// Indicate that the back buffer will now be used to present.
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		m_commandList->ResourceBarrier(1, &barrier);

		LWG_CHECK(SUCCEEDED(m_commandList->Close()));
	}

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_d3d12CommandQueue->ExecuteCommandLists(1, ppCommandLists);
}

int main()
{
	auto framework = LearningWorkGraph::Framework();
	framework.Initialize();

	auto application = HelloWorkGraphApplication();
	application.Initialize(&framework);

	framework.Run();

	framework.Terminate();
	
	return 0;
}
