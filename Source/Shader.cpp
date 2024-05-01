// Framework.cpp : スタティック ライブラリ用の関数を定義します。
//

#include "Shader.h"
#include "Framework.h"

#include <memory>

class DXCompiler
{
public:
	DXCompiler();
	~DXCompiler();

	IDxcUtils* GetUtils() { return m_utils; }
	IDxcCompiler* GetCompiler() { return m_compiler; }


private:
	HMODULE m_dll = {};
	CComPtr<IDxcUtils> m_utils = nullptr;
	CComPtr<IDxcCompiler> m_compiler = nullptr;
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
	m_compiler.Release();
	m_utils.Release();
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
}

bool Shader::Compile(std::string_view source)
{
	auto utils = g_dxcompiler->GetUtils();
	CComPtr<IDxcBlobEncoding> sourceBlob;
	if (SUCCEEDED(utils->CreateBlob(source.data(), source.size(), 0, &sourceBlob)))
	{
		auto compiler = g_dxcompiler->GetCompiler();
		CComPtr<IDxcOperationResult> result;
		if (SUCCEEDED(compiler->Compile(sourceBlob, nullptr, nullptr, L"lib_6_8", nullptr, 0, nullptr, 0, nullptr, &result)))
		{
			HRESULT hr = {};
			result->GetStatus(&hr);
			if (SUCCEEDED(hr))
			{
				ID3DBlob* data = nullptr;
				result->GetResult((IDxcBlob**)&data);
				m_compilerBufferData = std::unique_ptr<std::byte[]>(new std::byte[data->GetBufferSize()]);
				m_compilerBufferDataSize = data->GetBufferSize();
				std::memcpy(m_compilerBufferData.get(), data->GetBufferPointer(), data->GetBufferSize());
				return true;
			}
		}
	}
	return false;
}
}