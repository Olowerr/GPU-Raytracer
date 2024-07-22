#pragma once
#include <d3d11.h>
#include <string>
#include <vector>

class Texture;

namespace Okay
{
	void initiateDX11();
	void shutdownDX11();

	ID3D11Device* getDevice();
	ID3D11DeviceContext* getDeviceContext();

	bool createSwapChain(IDXGISwapChain** ppSwapChain, HWND hWnd);

	template<typename ShaderType>
	bool createShader(std::string_view path, ShaderType** ppShader, std::string* pOutShaderData = nullptr);
	
	template<typename ShaderType>
	void reloadShader(std::string_view path, ShaderType** ppShader);

	bool createStructuredBuffer(ID3D11Buffer** ppBuffer, ID3D11ShaderResourceView** ppSRV, const void* pData, uint32_t eleByteSize, uint32_t numElements, bool immutable = false);

	bool createConstantBuffer(ID3D11Buffer** ppBuffer, const void* pData, size_t byteSize, bool immutable = false);
	void updateBuffer(ID3D11Buffer* pBuffer, const void* pData, size_t byteWidth);

	// Assumes 4 channels consisting of 1 byte per channel
	bool createSRVFromTextureData(ID3D11ShaderResourceView** ppSRV, const Texture& texture);

	void getCPUTextureData(ID3D11Texture2D* pSourceTexture, void** ppOutData);

	void scaleTexture(Texture& texture, uint32_t newWidth, uint32_t newHeight);

	void createTextureArray(ID3D11ShaderResourceView** ppSRV, const std::vector<Texture>& textures, uint32_t width, uint32_t height);
}