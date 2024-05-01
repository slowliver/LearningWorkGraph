#pragma once

//#include <memory>
#include <string>

#include <windows.h>
#include <dxcapi.h>
#include <atlbase.h>
#include <d3dx12/d3dx12.h>

//struct ID3DBlob;
namespace LearningWorkGraph
{
class Shader
{
public:
	~Shader();
	bool Compile(std::string_view source);
//	void Map(const char* sourcePath);

	const void* GetData() const { return m_compilerBufferData.get(); }
	size_t GetSize() const { return m_compilerBufferDataSize; }

private:
	std::unique_ptr<std::byte[]> m_compilerBufferData = nullptr;
	size_t m_compilerBufferDataSize = 0;
};
}