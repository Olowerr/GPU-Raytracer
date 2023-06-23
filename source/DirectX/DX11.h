#pragma once
#include <d3d11.h>
#include <string>

namespace Okay
{
	void initiateDX11();
	void shutdownDX11();

	ID3D11Device* getDevice();
	ID3D11DeviceContext* getDeviceContext();

	bool createSwapChain(IDXGISwapChain** ppSwapChain, HWND hWnd);

	template<typename ShaderType>
	bool createShader(std::string_view path, ShaderType** ppShader, std::string* pOutShaderData = nullptr);

	bool createStructuredBuffer(ID3D11Buffer** ppBuffer, ID3D11ShaderResourceView** ppSRV, const void* pData, uint32_t eleByteSize, uint32_t numElements, bool immutable = false);

	bool createConstantBuffer(ID3D11Buffer** ppBuffer, const void* pData, size_t byteSize, bool immutable = false);
	void updateBuffer(ID3D11Buffer* pBuffer, const void* pData, size_t byteWidth);
}