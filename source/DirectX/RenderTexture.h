#pragma once
#include "DX11.h"

#include "glm/glm.hpp"


enum TextureFlagsBitPos : uint32_t
{
	B_RENDER = 1,
	B_SHADER_READ = 2,
	B_SHADER_WRITE = 3,
	B_DEPTH = 4,
};

enum TextureFormat : uint32_t
{
	INVALID = 0u,
	F_8X1 = DXGI_FORMAT_R8_UNORM,
	F_8X4 = DXGI_FORMAT_R8G8B8A8_UNORM,
	F_32X4 = DXGI_FORMAT_R32G32B32A32_FLOAT,
};

enum TextureFlags : uint32_t
{
	ALL = 1u,
	RENDER = 1 << TextureFlagsBitPos::B_RENDER,
	SHADER_READ = 1 << TextureFlagsBitPos::B_SHADER_READ,
	SHADER_WRITE = 1 << TextureFlagsBitPos::B_SHADER_WRITE,
	DEPTH = 1 << TextureFlagsBitPos::B_DEPTH,
};

class RenderTexture
{
public: 
	RenderTexture();
	RenderTexture(uint32_t width, uint32_t height, TextureFormat format, uint32_t flags = TextureFlags::ALL);
	RenderTexture(ID3D11Texture2D* pDX11Texture, bool createDepthTexture = true);
	~RenderTexture();

	void initiate(uint32_t width, uint32_t height, TextureFormat format, uint32_t flags = TextureFlags::ALL);
	void initiate(ID3D11Texture2D* pDX11Texture, bool createDepthTexture = true);
	void shutdown();

	inline ID3D11Texture2D* const* getBuffer() const;
	inline ID3D11RenderTargetView* const* getRTV() const;
	inline ID3D11ShaderResourceView* const* getSRV() const;
	inline ID3D11UnorderedAccessView* const* getUAV() const;
	inline ID3D11DepthStencilView* const* getDSV() const;

	inline glm::uvec2 getDimensions() const;

private:
	ID3D11Texture2D* m_pBuffer;
	ID3D11RenderTargetView* m_pRTV;
	ID3D11ShaderResourceView* m_pSRV;
	ID3D11UnorderedAccessView* m_pUAV;

	ID3D11Texture2D* m_pDepthBuffer;
	ID3D11DepthStencilView* m_pDSV;

private:
	void createViewsFromFlags(uint32_t flags);
};

inline ID3D11Texture2D* const* RenderTexture::getBuffer() const
{
	return &m_pBuffer;
}

inline ID3D11RenderTargetView* const* RenderTexture::getRTV() const
{
	return &m_pRTV;
}

inline ID3D11ShaderResourceView* const* RenderTexture::getSRV() const
{
	return &m_pSRV;
}

inline ID3D11UnorderedAccessView* const* RenderTexture::getUAV() const
{
	return &m_pUAV;
}

inline ID3D11DepthStencilView* const* RenderTexture::getDSV() const
{
	return &m_pDSV;
}

inline glm::uvec2 RenderTexture::getDimensions() const
{
	D3D11_TEXTURE2D_DESC desc{};
	m_pBuffer->GetDesc(&desc);

	return glm::uvec2(desc.Width, desc.Height);
}