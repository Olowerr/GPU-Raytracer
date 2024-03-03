#include "RenderTexture.h"
#include "Utilities.h"

RenderTexture::RenderTexture()
{
	m_pBuffer = nullptr;
	m_pRTV = nullptr;
	m_pSRV = nullptr;
	m_pUAV = nullptr;
	m_pDepthBuffer = nullptr;
	m_pDSV = nullptr;
}

RenderTexture::RenderTexture(uint32_t width, uint32_t height, Format format, uint32_t flags)
{
	initiate(width, height, format, flags);
}

RenderTexture::RenderTexture(ID3D11Texture2D* pDX11Texture, uint32_t flags)
{
	initiate(pDX11Texture, flags);
}

RenderTexture::~RenderTexture()
{
	shutdown();
}

void RenderTexture::initiate(uint32_t width, uint32_t height, Format format, uint32_t flags)
{
	OKAY_ASSERT(format != Format::INVALID);

	shutdown();

	D3D11_TEXTURE2D_DESC desc{};
	desc.Format = DXGI_FORMAT(format);
	desc.Width = width;
	desc.Height = height;
	desc.BindFlags =
		(CHECK_BIT(flags, BitPos::B_RENDER) ? D3D11_BIND_RENDER_TARGET : 0u) |
		(CHECK_BIT(flags, BitPos::B_SHADER_READ) ? D3D11_BIND_SHADER_RESOURCE : 0u) |
		(CHECK_BIT(flags, BitPos::B_SHADER_WRITE) ? D3D11_BIND_UNORDERED_ACCESS : 0u);
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.CPUAccessFlags = 0u;
	desc.ArraySize = 1u;
	desc.MipLevels = 1u;
	desc.SampleDesc.Count = 1u;
	desc.SampleDesc.Quality = 0u;
	desc.MiscFlags = 0u;

	bool success = SUCCEEDED(Okay::getDevice()->CreateTexture2D(&desc, nullptr, &m_pBuffer));
	OKAY_ASSERT(success);

	createViewsFromFlags(flags);
}

void RenderTexture::initiate(ID3D11Texture2D* pDX11Texture, uint32_t flags)
{
	OKAY_ASSERT(pDX11Texture);

	shutdown();

	m_pBuffer = pDX11Texture;
	m_pBuffer->AddRef();

	createViewsFromFlags(flags);
}

void RenderTexture::shutdown()
{
	DX11_RELEASE(m_pBuffer);
	DX11_RELEASE(m_pRTV);
	DX11_RELEASE(m_pSRV);
	DX11_RELEASE(m_pUAV);

	DX11_RELEASE(m_pDepthBuffer);
	DX11_RELEASE(m_pDSV);
}

void RenderTexture::createViewsFromFlags(uint32_t flags)
{
	ID3D11Device* pDevice = Okay::getDevice();
	bool success = false;

	if (CHECK_BIT(flags, BitPos::B_RENDER))
	{
		success = SUCCEEDED(pDevice->CreateRenderTargetView(m_pBuffer, nullptr, &m_pRTV));
		OKAY_ASSERT(success);
	}

	if (CHECK_BIT(flags, BitPos::B_SHADER_READ))
	{
		success = SUCCEEDED(pDevice->CreateShaderResourceView(m_pBuffer, nullptr, &m_pSRV));
		OKAY_ASSERT(success);
	}

	if (CHECK_BIT(flags, BitPos::B_SHADER_WRITE))
	{
		success = SUCCEEDED(pDevice->CreateUnorderedAccessView(m_pBuffer, nullptr, &m_pUAV));
		OKAY_ASSERT(success);
	}
	
	if (CHECK_BIT(flags, BitPos::B_DEPTH))
	{
		D3D11_TEXTURE2D_DESC depthDesc{};
		m_pBuffer->GetDesc(&depthDesc);
		depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		success = SUCCEEDED(pDevice->CreateTexture2D(&depthDesc, nullptr, &m_pDepthBuffer));
		OKAY_ASSERT(success);

		success = SUCCEEDED(pDevice->CreateDepthStencilView(m_pDepthBuffer, nullptr, &m_pDSV));
		OKAY_ASSERT(success);
	}
}
