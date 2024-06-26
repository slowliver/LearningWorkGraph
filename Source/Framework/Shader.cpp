﻿#include <Framework/Shader.h>
#include <Framework/Framework.h>

#include <memory>
#include <vector>
#include <cstdio>
#include <type_traits>

using Microsoft::WRL::ComPtr;

class DXCompiler
{
public:
	DXCompiler();
	~DXCompiler();

	IDxcUtils* GetUtils() { return m_utils.Get(); }
	IDxcCompiler* GetCompiler() { return m_compiler.Get(); }

private:
	HMODULE m_dll = {};
	ComPtr<IDxcUtils> m_utils = nullptr;
	ComPtr<IDxcCompiler> m_compiler = nullptr;
};

DXCompiler::DXCompiler()
{
	m_dll = LoadLibraryA("dxcompiler.dll");
	if (!m_dll)
	{
		return;
	}
	DxcCreateInstanceProc dxcCreateInstance = (DxcCreateInstanceProc)GetProcAddress(m_dll, "DxcCreateInstance");
	auto resultUtils = SUCCEEDED(dxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_utils)));
	auto resultCompiler = SUCCEEDED(dxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_compiler)));
	if (!resultUtils || !resultCompiler)
	{
		return;
	}
#if 0
	if (SUCCEEDED(pUtils->CreateBlob(kSourceCode, sizeof(kSourceCode), 0, &pSource)))
	{
		if (SUCCEEDED(pCompiler->Compile(pSource, nullptr, nullptr, L"lib_6_8", nullptr, 0, nullptr, 0, nullptr, &pOperationResult)))
		{
			HRESULT hr;
			pOperationResult->GetStatus(&hr);
			if (SUCCEEDED(hr))
			{
				pOperationResult->GetResult((IDxcBlob**)&pGwgLibrary);
			}
		}
	}
#endif
}

DXCompiler::~DXCompiler()
{
	m_compiler.Reset();
	m_utils.Reset();
	if (m_dll)
	{
		FreeLibrary(m_dll);
		m_dll = NULL;
	}
}

static std::unique_ptr<DXCompiler> g_dxcompiler = std::unique_ptr<DXCompiler>(new DXCompiler());

namespace LearningWorkGraph
{
Shader::~Shader()
{
	Release();
}

bool Shader::CompileFromMemory(std::string_view source, std::string_view entryPoint, std::string_view target, const std::vector<ShaderDefine>* defines)
{
	Release();
	auto utils = g_dxcompiler->GetUtils();
	ComPtr<IDxcBlobEncoding> sourceBlob;
	if (SUCCEEDED(utils->CreateBlob(source.data(), source.size(), 0, &sourceBlob)))
	{
		auto compiler = g_dxcompiler->GetCompiler();
		ComPtr<IDxcOperationResult> result;

		auto wEntryPointSize = MultiByteToWideChar(CP_ACP, 0, entryPoint.data(), -1, NULL, 0);
		auto wEntryPoint = std::unique_ptr<wchar_t[]>(new wchar_t[wEntryPointSize]);
		MultiByteToWideChar(CP_ACP, 0, entryPoint.data(), -1, wEntryPoint.get(), wEntryPointSize);

		auto wTargetSize = MultiByteToWideChar(CP_ACP, 0, target.data(), -1, NULL, 0);
		auto wTarget = std::unique_ptr<wchar_t[]>(new wchar_t[wTargetSize]);
		MultiByteToWideChar(CP_ACP, 0, target.data(), -1, wTarget.get(), wTargetSize);

		const wchar_t* arguments[] =
		{
			L"",
#if defined(_DEBUG)
			DXC_ARG_DEBUG,
#endif
		};

		auto wDefines = std::vector<DxcDefine>();
		auto wDefinesHolder = std::vector<std::pair<std::wstring, std::wstring>>();
		if (defines)
		{
			for (const auto& define : *defines)
			{
				auto& wDefine = wDefines.emplace_back();
				auto& wDefineHolder = wDefinesHolder.emplace_back();
				wDefineHolder.first.resize(MultiByteToWideChar(CP_ACP, 0, define.m_key.data(), -1, NULL, 0));
				MultiByteToWideChar(CP_ACP, 0, define.m_key.data(), -1, wDefineHolder.first.data(), wDefineHolder.first.size());
				wDefineHolder.second.resize(MultiByteToWideChar(CP_ACP, 0, define.m_value.data(), -1, NULL, 0));
				MultiByteToWideChar(CP_ACP, 0, define.m_value.data(), -1, wDefineHolder.second.data(), wDefineHolder.second.size());
				wDefine.Name = wDefineHolder.first.data();
				wDefine.Value = wDefineHolder.second.data();
			}
		}

 		if (SUCCEEDED(compiler->Compile(sourceBlob.Get(), nullptr, wEntryPoint.get(), wTarget.get(), arguments, std::extent_v<decltype(arguments)>, wDefines.data(), wDefines.size(), nullptr, &result)))
		{
			HRESULT hr = {};
			result->GetStatus(&hr);
			if (SUCCEEDED(hr))
			{
				ID3DBlob* data = nullptr;
				result->GetResult((IDxcBlob**)&data);
				m_compilerBufferData = std::unique_ptr<std::byte[]>(new std::byte[data->GetBufferSize()]);
				m_compilerBufferDataSize = data->GetBufferSize();
				std::memcpy(m_compilerBufferData.get(), data->GetBufferPointer(), m_compilerBufferDataSize);
				return true;
			}
			else
			{
				ComPtr<IDxcBlobEncoding> errorBuffer = nullptr;
				result->GetErrorBuffer(&errorBuffer);
				if (errorBuffer)
				{
					printf((const char*)errorBuffer->GetBufferPointer());
				}
			}
		}
	}
	return false;
}

bool Shader::CompileFromFile(std::string_view filePath, std::string_view entryPoint, std::string_view target, const std::vector<ShaderDefine>* defines)
{
	char fullFilePath[MAX_PATH];
	GetFullPathNameA(filePath.data(), MAX_PATH, fullFilePath, NULL);
	FILE* file = std::fopen(fullFilePath, "r");
	if (!file)
	{
		return false;
	}
	auto memory = std::vector<char>();
	memory.reserve(4096);
	int c = 0;
	while ((c = std::fgetc(file)) != EOF)
	{
		memory.push_back((char)c);
	}
	std::fclose(file);
	return CompileFromMemory(std::string(memory.begin(), memory.end()), entryPoint, target, defines);
}

void Shader::Release()
{
	m_compilerBufferData.reset();
	m_compilerBufferDataSize = 0;
}
}