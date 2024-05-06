#pragma once

#include <vector>
#include <string>

#include <windows.h>
#include <dxcapi.h>
#include <wrl.h>
#include <d3dx12/d3dx12.h>

//struct ID3DBlob;
namespace LearningWorkGraph
{
struct ShaderDefine
{
	std::string_view m_key;
	std::string_view m_value;
};

class Shader
{
public:
	~Shader();
	bool CompileFromMemory(std::string_view source, std::string_view entryPoint, std::string_view target, const std::vector<ShaderDefine>* defines = nullptr);
	bool CompileFromFile(std::string_view filePath, std::string_view entryPoint, std::string_view target, const std::vector<ShaderDefine>* defines = nullptr);
//	void Map(const char* sourcePath);

	const void* GetData() const { return m_compilerBufferData.get(); }
	size_t GetSize() const { return m_compilerBufferDataSize; }

private:
	void Release();

private:
	std::unique_ptr<std::byte[]> m_compilerBufferData = nullptr;
	size_t m_compilerBufferDataSize = 0;
};
}