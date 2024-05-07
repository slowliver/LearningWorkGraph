#pragma once

#include <stdint.h>
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

namespace LearningWorkGraph
{
class Framework;

struct ApplicationDesc
{
	const Framework* m_framework;
	uint32_t m_argc;
	const char** m_argv;
};

class Application
{
public:
	Application();
	~Application();

	void Initialize(const ApplicationDesc& applicationDesc);
	void Terminate();

	ID3D12Device9* GetD3D12Device9() { return m_d3d12Device.Get(); }

	virtual void OnInitialize(const ApplicationDesc& applicationDesc) = 0;
	virtual void OnUpdate() = 0;
	virtual void OnRender() = 0;

	static Application* GetMainApplication() { return s_instance; }

	HWND GetHWND();

protected:
	static constexpr uint32_t k_frameCount = 2;
	static Application* s_instance;
	Microsoft::WRL::ComPtr<ID3D12Device9> m_d3d12Device = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue = nullptr;
	Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_queryHeap = nullptr;
	Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain = nullptr;
	
};
}