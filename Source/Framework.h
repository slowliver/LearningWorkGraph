#pragma once

#include <Windows.h>
#include <string_view>

namespace LearningWorkGraph
{
class Framework
{
public:
	static void ShowDialog(std::string_view title, std::string_view message);
};


void Framework::ShowDialog(std::string_view title, std::string_view message)
{
	MessageBoxA(NULL, message.data(), title.data(), MB_OK);
}

}