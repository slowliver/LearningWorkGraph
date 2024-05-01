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
	static void ShowDialog(std::string_view title, std::string_view message)
	{
		MessageBoxA(NULL, message.data(), title.data(), MB_OK);
	}
};
}