#pragma once

#include <stdint.h>
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

namespace LearningWorkGraph
{
class Framework;
class Application
{
public:
	Application();
	~Application();

	void Initialize(const Framework* framework);
	void Terminate();

	ID3D12Device9* GetD3D12Device9() { return m_d3d12Device.Get(); }

	virtual void OnInitialize() = 0;
	virtual void OnUpdate() = 0;
	virtual void OnRender() = 0;

	static Application* GetMainApplication() { return s_instance; }

private:
	void WaitForGPU();
	void MoveToNextFrame();

private:
	static constexpr uint32_t k_frameCount = 2;
	static Application* s_instance;
	Microsoft::WRL::ComPtr<ID3D12Device9> m_d3d12Device = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_d3d12CommandQueue = nullptr;
	Microsoft::WRL::ComPtr<IDXGISwapChain3> m_dxgiSwapChain = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_renderTargetViewHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocators[k_frameCount] = {};
	Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[k_frameCount] = {};
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

	// Fence Objects.
	uint32_t m_frameIndex = 0;
	HANDLE m_fenceEvent = {};
	Microsoft::WRL::ComPtr<ID3D12Fence> m_fence = nullptr;
	uint64_t m_fenceValues[k_frameCount] = {};

	size_t m_renderTargetViewDescriptorSize = 0;
};
}