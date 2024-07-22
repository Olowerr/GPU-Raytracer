#include "DX11.h"
#include "Utilities.h"
#include "Graphics/Texture.h"
#include "RenderTexture.h"

#include <d3dcompiler.h>

namespace Okay
{
	struct DX11
	{
		ID3D11Device* pDevice = nullptr;
		ID3D11DeviceContext* pDeviceContext = nullptr;

		ID3D11VertexShader* pScaleVS = nullptr;
		ID3D11PixelShader* pScalePS = nullptr;
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

		// Initiate scale shaders & sampelr
		bool success = false;

		success = createShader(SHADER_PATH "Scale/ScaleVS.hlsl", &dx11.pScaleVS);
		OKAY_ASSERT(success);
		
		success = createShader(SHADER_PATH "Scale/ScalePS.hlsl", &dx11.pScalePS);
		OKAY_ASSERT(success);
	}

	void shutdownDX11()
	{
		DX11_RELEASE(dx11.pDevice);
		DX11_RELEASE(dx11.pDeviceContext);

		DX11_RELEASE(dx11.pScaleVS);
		DX11_RELEASE(dx11.pScalePS);
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
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS | DXGI_USAGE_SHADER_INPUT;

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

	bool createSRVFromTextureData(ID3D11ShaderResourceView** ppSRV, const Texture& texture)
	{
		OKAY_ASSERT(ppSRV);

		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = texture.getWidth();
		desc.Height = texture.getHeight();
		
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
		inData.pSysMem = texture.getTextureData();
		inData.SysMemPitch = desc.Width * 4u;
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

	void getCPUTextureData(ID3D11Texture2D* pSourceTexture, void** ppOutData)
	{
		OKAY_ASSERT(pSourceTexture);
		OKAY_ASSERT(ppOutData);

		D3D11_TEXTURE2D_DESC desc{};
		pSourceTexture->GetDesc(&desc);
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.BindFlags = 0u;

		ID3D11Texture2D* stagingBuffer = nullptr;
		bool success = SUCCEEDED(dx11.pDevice->CreateTexture2D(&desc, nullptr, &stagingBuffer));
		OKAY_ASSERT(success);

		dx11.pDeviceContext->CopyResource(stagingBuffer, pSourceTexture);

		D3D11_MAPPED_SUBRESOURCE sub{};
		dx11.pDeviceContext->Map(stagingBuffer, 0u, D3D11_MAP_READ, 0u, &sub);

		uint32_t byteSize = sub.RowPitch * desc.Height;
		(*ppOutData) = new unsigned char[byteSize] {};

		// Need to copy row by row to account for potential padding between rows
		// https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_mapped_subresource#remarks
		for (uint32_t y = 0; y < desc.Height; y++)
		{
			uint32_t targetOffset = y * desc.Width * 4;
			uint32_t sourceOffset = y * sub.RowPitch;
			memcpy((unsigned char*)(*ppOutData) + targetOffset, (unsigned char*)sub.pData + sourceOffset, sub.RowPitch);
		}

		dx11.pDeviceContext->Unmap(stagingBuffer, 0u);
		DX11_RELEASE(stagingBuffer);
	}

	void scaleTexture(Texture& texture, uint32_t newWidth, uint32_t newHeight) // Move to ResourceManager?
	{
		if (texture.getWidth() == newWidth && texture.getHeight() == newHeight)
			return;

		ID3D11ShaderResourceView* pSRV = nullptr;
		bool success = createSRVFromTextureData(&pSRV, texture);
		OKAY_ASSERT(success);

		dx11.pDeviceContext->ClearState();

		D3D11_VIEWPORT viewport{};
		viewport.TopLeftX = 0.f;
		viewport.TopLeftY = 0.f;
		viewport.Width = (float)newWidth;
		viewport.Height = (float)newHeight;
		viewport.MinDepth = 0.f;
		viewport.MaxDepth = 1.f;

		RenderTexture target(newWidth, newHeight, TextureFormat::F_8X4, TextureFlags::RENDER);

		dx11.pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		dx11.pDeviceContext->VSSetShader(dx11.pScaleVS, nullptr, 0u);
		dx11.pDeviceContext->RSSetViewports(1u, &viewport);
		dx11.pDeviceContext->PSSetShader(dx11.pScalePS, nullptr, 0u);
		dx11.pDeviceContext->PSSetShaderResources(0u, 1u, &pSRV);
		dx11.pDeviceContext->OMSetRenderTargets(1u, target.getRTV(), nullptr);

		dx11.pDeviceContext->Draw(6u, 0u);

		DX11_RELEASE(pSRV);

		unsigned char* pNewTextureData = nullptr;
		getCPUTextureData(*target.getBuffer(), (void**)&pNewTextureData);

		texture = std::move(Texture(pNewTextureData, newWidth, newHeight, texture.getName()));
	}

	void createTextureArray(ID3D11ShaderResourceView** ppSRV, const std::vector<Texture>& textures, uint32_t width, uint32_t height)
	{
		OKAY_ASSERT(ppSRV);
		OKAY_ASSERT(textures.size());
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

		desc.ArraySize = (uint32_t)textures.size();
		desc.MiscFlags = 0u;

		std::vector<D3D11_SUBRESOURCE_DATA> textureDatas(desc.ArraySize);

		for (uint32_t i = 0; i < desc.ArraySize; i++)
		{
			D3D11_SUBRESOURCE_DATA& resourceData = textureDatas[i];

			resourceData.pSysMem = textures[i].getTextureData();
			resourceData.SysMemPitch = desc.Width * 4u;
			resourceData.SysMemSlicePitch = 0u;
		}

		ID3D11Texture2D* pTextureArray = nullptr;
		bool success = false;

		success = SUCCEEDED(dx11.pDevice->CreateTexture2D(&desc, textureDatas.data(), &pTextureArray));
		OKAY_ASSERT(success);

		success = SUCCEEDED(dx11.pDevice->CreateShaderResourceView(pTextureArray, nullptr, ppSRV));
		DX11_RELEASE(pTextureArray);
		OKAY_ASSERT(success);
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
	template bool createShader(std::string_view path, ID3D11VertexShader** ppShader, std::string* pOutShaderData);
	template bool createShader(std::string_view path, ID3D11PixelShader** ppShader, std::string* pOutShaderData);

	template void reloadShader(std::string_view path, ID3D11ComputeShader** ppShader);
	template void reloadShader(std::string_view path, ID3D11VertexShader** ppShader);
	template void reloadShader(std::string_view path, ID3D11PixelShader** ppShader);

	template<typename ShaderType>
	void reloadShader(std::string_view path, ShaderType** ppShader)
	{
		ShaderType* pNewShader = nullptr;
		if (!Okay::createShader(path, &pNewShader))
			return;

		DX11_RELEASE(*ppShader);
		*ppShader = pNewShader;
	}

	template<typename ShaderType>
	bool createShader(std::string_view path, ShaderType** ppShader, std::string* pOutShaderData)
	{
		if (!ppShader)
			return false;

		std::string_view fileEnding = Okay::getFileEnding(path);

		std::string shaderData;
		if (!pOutShaderData)
			pOutShaderData = &shaderData;

		if (fileEnding == ".cso" || fileEnding == ".CSO")
		{

			if (!Okay::readBinary(path, *pOutShaderData))
				return false;

			if constexpr (std::is_same<ShaderType, ID3D11ComputeShader>())
				return SUCCEEDED(dx11.pDevice->CreateComputeShader(pOutShaderData->c_str(), pOutShaderData->length(), nullptr, ppShader));
			if constexpr (std::is_same<ShaderType, ID3D11VertexShader>())
				return SUCCEEDED(dx11.pDevice->CreateVertexShader(pOutShaderData->c_str(), pOutShaderData->length(), nullptr, ppShader));
			if constexpr (std::is_same<ShaderType, ID3D11PixelShader>())
				return SUCCEEDED(dx11.pDevice->CreatePixelShader(pOutShaderData->c_str(), pOutShaderData->length(), nullptr, ppShader));

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
			if constexpr (std::is_same<ShaderType, ID3D11VertexShader>())	shaderTypeTarget = "vs_5_0";
			if constexpr (std::is_same<ShaderType, ID3D11PixelShader>())	shaderTypeTarget = "ps_5_0";


			IncludeReader includer;
			HRESULT hr = D3DCompileFromFile(lpPath, nullptr, &includer, "main", shaderTypeTarget, optimizationLevel, 0u, &shaderData, &compileOutput);
			OKAY_DELETE_ARRAY(lpPath);

			if (compileOutput)
				printf("'%s' - Shader compilation output:\n%s\n", path.data(), (char*)compileOutput->GetBufferPointer());

			if (FAILED(hr))
			{
				if (!compileOutput)
					printf("'%s' - Shader compilation failed but no errors were produced, file might not have been found\n", path.data());

				return false;
			}

			pOutShaderData->assign((char*)shaderData->GetBufferPointer(), shaderData->GetBufferSize());

			if constexpr (std::is_same<ShaderType, ID3D11ComputeShader>())
				return SUCCEEDED(dx11.pDevice->CreateComputeShader(pOutShaderData->c_str(), pOutShaderData->length(), nullptr, ppShader));
			if constexpr (std::is_same<ShaderType, ID3D11VertexShader>())
				return SUCCEEDED(dx11.pDevice->CreateVertexShader(pOutShaderData->c_str(), pOutShaderData->length(), nullptr, ppShader));
			if constexpr (std::is_same<ShaderType, ID3D11PixelShader>())
				return SUCCEEDED(dx11.pDevice->CreatePixelShader(pOutShaderData->c_str(), pOutShaderData->length(), nullptr, ppShader));

		}

		return false;
	}
}
