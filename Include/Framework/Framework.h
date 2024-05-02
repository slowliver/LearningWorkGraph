#pragma once

#include <Windows.h>
#include <string_view>

#define LWG_CHECK(value) if(!(value)) { LearningWorkGraph::Framework::ShowDialog("Error", #value); std::abort(); }
#define LWG_CHECK_WITH_MESSAGE(value, ...) if(!(value)) { LearningWorkGraph::Framework::ShowDialog("Error", __VA_ARGS__); std::abort(); }

namespace LearningWorkGraph
{
class Framework
{
public:
	void Initialize();
	void Run();
	void Terminate() {}

	HWND GetHWND() const { return m_hwnd; }

	static void ShowDialog(std::string_view title, std::string_view message);

private:
	HWND m_hwnd = {};
};
}