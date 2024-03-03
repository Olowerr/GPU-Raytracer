#pragma once
#include "DX11.h"

#include "glm/glm.hpp"

class RenderTexture
{
public: 

	// Maybe rework these enums? feels kinda 'clunky',
	// and writing 'RenderTexture::Flags::SHADER_READ' is long lol
	// Maybe 'Description' struct?
	enum BitPos : uint32_t
	{
		B_RENDER = 0,
		B_SHADER_READ = 1,
		B_SHADER_WRITE = 2,
		B_DEPTH = 3,
	};

	enum Format : uint32_t
	{
		INVALID = DXGI_FORMAT_UNKNOWN,
		F_8X1 = DXGI_FORMAT_R8_UNORM,
		F_8X4 = DXGI_FORMAT_R8G8B8A8_UNORM,
		F_32X4 = DXGI_FORMAT_R32G32B32A32_FLOAT,
	};

	enum Flags : uint32_t
	{
		RENDER = 1 << BitPos::B_RENDER,
		SHADER_READ = 1 << BitPos::B_SHADER_READ,
		SHADER_WRITE = 1 << BitPos::B_SHADER_WRITE,
		DEPTH = 1 << BitPos::B_DEPTH,
	};

	RenderTexture();
	RenderTexture(uint32_t width, uint32_t height, Format format, uint32_t flags);
	RenderTexture(ID3D11Texture2D* pDX11Texture, uint32_t flags);
	~RenderTexture();

	void initiate(uint32_t width, uint32_t height, Format format, uint32_t flags);
	void initiate(ID3D11Texture2D* pDX11Texture, uint32_t flags);
	void shutdown();

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