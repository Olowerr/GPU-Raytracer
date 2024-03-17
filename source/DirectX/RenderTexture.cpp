#include "RenderTexture.h"

RenderTexture::RenderTexture()
{
	m_pBuffer = nullptr;
	m_pRTV = nullptr;
	m_pSRV = nullptr;
	m_pUAV = nullptr;
	m_pDepthBuffer = nullptr;
	m_pDSV = nullptr;
}

RenderTexture::RenderTexture(uint32_t width, uint32_t height, TextureFormat format, uint32_t flags)
{
	initiate(width, height, format, flags);
}

RenderTexture::RenderTexture(ID3D11Texture2D* pDX11Texture, bool createDepthTexture)
{
	initiate(pDX11Texture);
}

RenderTexture::~RenderTexture()
{
	shutdown();
}

void RenderTexture::initiate(uint32_t width, uint32_t height, TextureFormat format, uint32_t flags)
{
	OKAY_ASSERT(width);
	OKAY_ASSERT(height);
	OKAY_ASSERT(format != TextureFormat::INVALID);
	OKAY_ASSERT(flags);

	shutdown();

	if (flags == TextureFlags::ALL)
		flags = TextureFlags::RENDER | TextureFlags::SHADER_READ | TextureFlags::SHADER_WRITE | TextureFlags::DEPTH;

	D3D11_TEXTURE2D_DESC desc{};
	desc.Format = DXGI_FORMAT(format);
	desc.Width = width;
	desc.Height = height;
	desc.BindFlags =
		(CHECK_BIT(flags, TextureFlagsBitPos::B_RENDER) ? D3D11_BIND_RENDER_TARGET : 0u) |
		(CHECK_BIT(flags, TextureFlagsBitPos::B_SHADER_READ) ? D3D11_BIND_SHADER_RESOURCE : 0u) |
		(CHECK_BIT(flags, TextureFlagsBitPos::B_SHADER_WRITE) ? D3D11_BIND_UNORDERED_ACCESS : 0u);
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

void RenderTexture::initiate(ID3D11Texture2D* pDX11Texture, bool createDepthTexture)
{
	OKAY_ASSERT(pDX11Texture);

	shutdown();

	m_pBuffer = pDX11Texture;
	m_pBuffer->AddRef();

	D3D11_TEXTURE2D_DESC desc{};
	m_pBuffer->GetDesc(&desc);

	uint32_t flags = getTextureFlagsFromDX11(desc.BindFlags, createDepthTexture);
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

void RenderTexture::resize(uint32_t width, uint32_t height)
{
	OKAY_ASSERT(width);
	OKAY_ASSERT(height);

	D3D11_TEXTURE2D_DESC orignalDesc{};
	m_pBuffer->GetDesc(&orignalDesc);

	uint32_t flags = getTextureFlagsFromDX11(orignalDesc.BindFlags, m_pDepthBuffer);
	initiate(width, height, TextureFormat(orignalDesc.Format), flags);
}

void RenderTexture::createViewsFromFlags(uint32_t flags)
{
	ID3D11Device* pDevice = Okay::getDevice();
	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();
	bool success = false;

	if (CHECK_BIT(flags, TextureFlagsBitPos::B_RENDER))
	{
		success = SUCCEEDED(pDevice->CreateRenderTargetView(m_pBuffer, nullptr, &m_pRTV));
		OKAY_ASSERT(success);

		glm::vec4 clearColour = glm::vec4(0.f);
		pDevCon->ClearRenderTargetView(m_pRTV, &clearColour.x);
	}

	if (CHECK_BIT(flags, TextureFlagsBitPos::B_SHADER_READ))
	{
		success = SUCCEEDED(pDevice->CreateShaderResourceView(m_pBuffer, nullptr, &m_pSRV));
		OKAY_ASSERT(success);
	}

	if (CHECK_BIT(flags, TextureFlagsBitPos::B_SHADER_WRITE))
	{
		success = SUCCEEDED(pDevice->CreateUnorderedAccessView(m_pBuffer, nullptr, &m_pUAV));
		OKAY_ASSERT(success);

		glm::vec4 clearColour = glm::vec4(0.f);
		pDevCon->ClearUnorderedAccessViewFloat(m_pUAV, &clearColour.x);
	}
	
	if (CHECK_BIT(flags, TextureFlagsBitPos::B_DEPTH))
	{
		D3D11_TEXTURE2D_DESC depthDesc{};
		m_pBuffer->GetDesc(&depthDesc);
		depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		success = SUCCEEDED(pDevice->CreateTexture2D(&depthDesc, nullptr, &m_pDepthBuffer));
		OKAY_ASSERT(success);

		success = SUCCEEDED(pDevice->CreateDepthStencilView(m_pDepthBuffer, nullptr, &m_pDSV));
		OKAY_ASSERT(success);

		Okay::getDeviceContext()->ClearDepthStencilView(m_pDSV, D3D11_CLEAR_DEPTH, 1.f, 0);
	}
}

uint32_t RenderTexture::getTextureFlagsFromDX11(uint32_t bindFlags, bool depth)
{
	// Convert D3D11 flags to our flags, can create more dynamic way of doing this, but I'm guessing these values won't change :]
	// Maybe different on different systems tho, but feels unlikely :thonk:

	uint32_t flags = 0u;
	flags |= CHECK_BIT(bindFlags, 3) ? TextureFlags::SHADER_READ : 0;	// SRV - Bit position of D3D11_BIND_SHADER_RESOURCE (Value 8)
	flags |= CHECK_BIT(bindFlags, 5) ? TextureFlags::RENDER : 0;		// RTV - Bit position of D3D11_BIND_RENDER_TARGET (Value 32)
	flags |= CHECK_BIT(bindFlags, 7) ? TextureFlags::SHADER_WRITE : 0;	// UAV - Bit position of D3D11_BIND_UNORDERED_ACCESS (Value 128)
	flags |= depth ? TextureFlags::DEPTH : 0;

	return flags;
}