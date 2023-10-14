#include "DX11.h"
#include "Utilities.h"

#include <d3dcompiler.h>

namespace Okay
{
	struct DX11
	{
		ID3D11Device* pDevice = nullptr;
		ID3D11DeviceContext* pDeviceContext = nullptr;
	};

	static DX11 dx11;

	void initiateDX11()
	{
		D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
		uint32_t flags = 0;
#ifndef DIST	
		flags = D3D11_CREATE_DEVICE_DEBUG;
#endif

		HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
			&featureLevel, 1u, D3D11_SDK_VERSION, &dx11.pDevice, nullptr, &dx11.pDeviceContext);

		OKAY_ASSERT(SUCCEEDED(hr));
	}

	void shutdownDX11()
	{
		DX11_RELEASE(dx11.pDevice);
		DX11_RELEASE(dx11.pDeviceContext);
	}

	ID3D11Device* getDevice()
	{
		return dx11.pDevice;
	}

	ID3D11DeviceContext* getDeviceContext()
	{
		return dx11.pDeviceContext;
	}

	bool createSwapChain(IDXGISwapChain** ppSwapChain, HWND hWnd)
	{
		OKAY_ASSERT(ppSwapChain);
		OKAY_ASSERT(hWnd);

		DXGI_SWAP_CHAIN_DESC desc{};
		desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		desc.BufferCount = 1u;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS;

		desc.BufferDesc.Width = 0u; // 0u defaults to the window dimensions
		desc.BufferDesc.Height = 0u;
		desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		desc.BufferDesc.RefreshRate.Numerator = 0u;
		desc.BufferDesc.RefreshRate.Denominator = 1u;

		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;

		desc.OutputWindow = hWnd;
		desc.Windowed = true;
		desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		IDXGIDevice* idxDevice = nullptr;
		IDXGIAdapter* adapter = nullptr;
		IDXGIFactory* factory = nullptr;

		dx11.pDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&idxDevice);
		idxDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&adapter);
		adapter->GetParent(__uuidof(IDXGIFactory), (void**)&factory);

		factory->CreateSwapChain(dx11.pDevice, &desc, ppSwapChain);
		DX11_RELEASE(idxDevice);
		DX11_RELEASE(adapter);
		DX11_RELEASE(factory);

		return *ppSwapChain;
	}

	bool createStructuredBuffer(ID3D11Buffer** ppBuffer, ID3D11ShaderResourceView** ppSRV, const void* pData, uint32_t eleByteSize, uint32_t numElements, bool immutable)
	{
		D3D11_BUFFER_DESC bufferDesc{};
		bufferDesc.ByteWidth = eleByteSize * numElements;
		bufferDesc.CPUAccessFlags = immutable ? 0 : D3D11_CPU_ACCESS_WRITE;
		bufferDesc.Usage = immutable ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DYNAMIC;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = eleByteSize;
		D3D11_SUBRESOURCE_DATA inData{};
		inData.pSysMem = pData;
		inData.SysMemPitch = 0;
		inData.SysMemSlicePitch = 0;

		HRESULT hr = dx11.pDevice->CreateBuffer(&bufferDesc, pData ? &inData : nullptr, ppBuffer);
		if (FAILED(hr))
			return false;

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = numElements;

		return SUCCEEDED(dx11.pDevice->CreateShaderResourceView(*ppBuffer, &srvDesc, ppSRV));

	}

	bool createConstantBuffer(ID3D11Buffer** ppBuffer, const void* pData, size_t byteSize, bool immutable)
	{
		D3D11_BUFFER_DESC desc{};
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.Usage = immutable ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DYNAMIC;
		desc.CPUAccessFlags = immutable ? 0 : D3D11_CPU_ACCESS_WRITE;
		desc.ByteWidth = (uint32_t)byteSize;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;
		D3D11_SUBRESOURCE_DATA inData{};
		inData.pSysMem = pData;
		inData.SysMemPitch = inData.SysMemSlicePitch = 0;
		return SUCCEEDED(dx11.pDevice->CreateBuffer(&desc, pData ? &inData : nullptr, ppBuffer));
	}

	void updateBuffer(ID3D11Buffer* pBuffer, const void* pData, size_t byteWidth)
	{
		OKAY_ASSERT(pBuffer);
		D3D11_MAPPED_SUBRESOURCE sub{};
		if (FAILED(dx11.pDeviceContext->Map(pBuffer, 0u, D3D11_MAP_WRITE_DISCARD, 0u, &sub)))
			return;

		memcpy(sub.pData, pData, byteWidth);
		dx11.pDeviceContext->Unmap(pBuffer, 0u);
	}

	bool createSRVFromTextureData(ID3D11ShaderResourceView** ppSRV, const unsigned char* pTextureData, uint32_t width, uint32_t height)
	{
		OKAY_ASSERT(ppSRV);
		OKAY_ASSERT(pTextureData);
		OKAY_ASSERT(width);
		OKAY_ASSERT(height);

		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = width;
		desc.Height = height;
		
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.MipLevels = 1u;
		
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.CPUAccessFlags = 0u;
		desc.SampleDesc.Count = 1u;
		desc.SampleDesc.Quality = 0u;
		
		desc.ArraySize = 1u;
		desc.MiscFlags = 0u;

		D3D11_SUBRESOURCE_DATA inData{};
		inData.pSysMem = pTextureData;
		inData.SysMemPitch = width * 4u;
		inData.SysMemSlicePitch = 0u;

		ID3D11Texture2D* pTexture = nullptr;
		bool success = false;

		success = SUCCEEDED(dx11.pDevice->CreateTexture2D(&desc, &inData, &pTexture));
		if (!success)
			return false;

		success = SUCCEEDED(dx11.pDevice->CreateShaderResourceView(pTexture, nullptr, ppSRV));
		DX11_RELEASE(pTexture);

		return success;
	}


	class IncludeReader : public ID3DInclude
	{
	public:

		// Inherited via ID3DInclude
		virtual HRESULT __stdcall Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes) override
		{
			if (!Okay::readBinary(std::string(SHADER_PATH) + pFileName, m_includeBuffer))
				return E_FAIL;

			*ppData = m_includeBuffer.c_str();
			*pBytes = (uint32_t)m_includeBuffer.size();

			return S_OK;
		}

		virtual HRESULT __stdcall Close(LPCVOID pData) override
		{
			m_includeBuffer.resize(0u);
			return S_OK;
		}

	private:
		std::string m_includeBuffer;
	};

	template bool createShader(std::string_view path, ID3D11ComputeShader** ppShader, std::string* pOutShaderData);

	template<typename ShaderType>
	bool createShader(std::string_view path, ShaderType** ppShader, std::string* pOutShaderData)
	{
		if (!ppShader)
			return false;

		std::string_view fileEnding = Okay::getFileEnding(path);

		if (fileEnding == ".cso" || fileEnding == ".CSO")
		{
			std::string shaderData;

			// if pOutShaderData is nullptr, it is simply used to point to the actual buffer
			// Allowing faster and (imo) a bit cleaner code 
			if (!pOutShaderData)
				pOutShaderData = &shaderData;

			if (!Okay::readBinary(path, *pOutShaderData))
				return false;

			if constexpr (std::is_same<ShaderType, ID3D11ComputeShader>())
				return SUCCEEDED(dx11.pDevice->CreateComputeShader(pOutShaderData->c_str(), pOutShaderData->length(), nullptr, ppShader));
		}
		else
		{
			// Convert char-string to wchar_t-string
			wchar_t* lpPath = new wchar_t[path.size() + 1ull]{};
			mbstowcs_s(nullptr, lpPath, path.size() + 1ull, path.data(), path.size());

			ID3DBlob* shaderData = nullptr;
			ID3DBlob* compileOutput = nullptr;

			// If neither are defined a compiler error is produced. Forcing the user to ensure the correct one is used
#if defined(DIST)
			uint32_t optimizationLevel = D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION;
#elif defined(_DEBUG)
			uint32_t optimizationLevel = D3DCOMPILE_OPTIMIZATION_LEVEL0 | D3DCOMPILE_DEBUG;
#elif defined(NDEBUG)
			uint32_t optimizationLevel = D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_DEBUG;
#endif

			const char* shaderTypeTarget = nullptr;
			if constexpr (std::is_same<ShaderType, ID3D11ComputeShader>())	shaderTypeTarget = "cs_5_0";


			IncludeReader includer;
			HRESULT hr = D3DCompileFromFile(lpPath, nullptr, &includer, "main", shaderTypeTarget, optimizationLevel, 0u, &shaderData, &compileOutput);
			OKAY_DELETE_ARRAY(lpPath);

			if (compileOutput)
				printf("Shader compilation output: %s\n", (char*)compileOutput->GetBufferPointer());

			if (FAILED(hr))
			{
				if (!compileOutput)
					printf("Shader compilation failed but no errors were produced, file might not have been found.\n");

				return false;
			}

			if (pOutShaderData)
				pOutShaderData->assign((char*)shaderData->GetBufferPointer(), shaderData->GetBufferSize());

			if constexpr (std::is_same<ShaderType, ID3D11ComputeShader>())
				return SUCCEEDED(dx11.pDevice->CreateComputeShader(shaderData->GetBufferPointer(), shaderData->GetBufferSize(), nullptr, ppShader));
		}

		return false;
	}
}
