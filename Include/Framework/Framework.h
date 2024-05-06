#pragma once

#include <Windows.h>
#include <string_view>

#define LWG_CHECK(value) if(!(value)) { LearningWorkGraph::Framework::ShowDialog("Error", #value); std::abort(); }
#define LWG_CHECK_WITH_MESSAGE(value, ...) if(!(value)) { LearningWorkGraph::Framework::ShowDialog("Error", __VA_ARGS__); std::abort(); }
#define LWG_CHECK_HRESULT(value) if(FAILED(value)) { LearningWorkGraph::Framework::ShowDialog("Error", #value); std::abort(); }

namespace LearningWorkGraph
{
struct FrameworkDesc
{
	bool m_useWindow;
};

class Framework
{
public:
	Framework();
	~Framework();

	void Initialize(const FrameworkDesc& desc);
	void Run();
	void Terminate() {}

	HWND GetHWND() const { return m_hwnd; }

	static Framework* GetMainFramework() { return s_instance; }
	static void ShowDialog(std::string_view title, std::string_view message);

private:
	static Framework* s_instance;

private:
	HWND m_hwnd = {};
};
}