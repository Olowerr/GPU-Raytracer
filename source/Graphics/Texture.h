#pragma once
#include "DirectX/DX11.h"
#include "Utilities.h"

class Texture
{
public:
	Texture(const unsigned char* pTextureData, uint32_t width, uint32_t height)
		:m_pSRV(nullptr)
	{
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
		inData.SysMemPitch = width * 4u; // four 1 byte channels
		inData.SysMemSlicePitch = 0u;

		bool success = false;
		ID3D11Device* pDevice = Okay::getDevice();
		ID3D11Texture2D* pTexture = nullptr;

		success = SUCCEEDED(pDevice->CreateTexture2D(&desc, &inData, &pTexture));
		OKAY_ASSERT(success);

		success = SUCCEEDED(pDevice->CreateShaderResourceView(pTexture, nullptr, &m_pSRV));
		DX11_RELEASE(pTexture);
		OKAY_ASSERT(success);
	}

	~Texture()
	{
		DX11_RELEASE(m_pSRV);
	}

	inline ID3D11ShaderResourceView* const* getSRV() const;

private:
	ID3D11ShaderResourceView* m_pSRV;
};

inline ID3D11ShaderResourceView* const* Texture::getSRV() const { return &m_pSRV; }