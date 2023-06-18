#include "Renderer.h"

Renderer::Renderer()
	:m_pBuffer(nullptr), m_width(0u), m_height(0u)
{
}

Renderer::Renderer(ID3D11Texture2D* pTarget)
{
	initiate(pTarget);
}

Renderer::~Renderer()
{
	shutdown();
}

void Renderer::shutdown()
{
	DX11_RELEASE(m_pBuffer);
	DX11_RELEASE(m_pBufferUAV);
	m_width = m_height = 0u;
	DX11_RELEASE(m_pMainRaytracingCS);
}

void Renderer::initiate(ID3D11Texture2D* pTarget)
{
	OKAY_ASSERT(pTarget);

	shutdown();

	m_pBuffer = pTarget;
	m_pBuffer->AddRef();

	D3D11_TEXTURE2D_DESC bufferDesc{};
	m_pBuffer->GetDesc(&bufferDesc);
	m_width = bufferDesc.Width;
	m_height = bufferDesc.Height;

	Okay::getDevice()->CreateUnorderedAccessView(m_pBuffer, nullptr, &m_pBufferUAV);
	OKAY_ASSERT(m_pBufferUAV);

	Okay::createShader("resources/RayTracerCS.hlsl", &m_pMainRaytracingCS);
	OKAY_ASSERT(m_pMainRaytracingCS);
}

void Renderer::render()
{
	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();

	static const float CLEAR_COLOUR[4]{ 0.2f, 0.4f, 0.6f, 1.f };
	pDevCon->ClearUnorderedAccessViewFloat(m_pBufferUAV, CLEAR_COLOUR);

	pDevCon->CSSetShader(m_pMainRaytracingCS, nullptr, 0u);
	pDevCon->CSSetUnorderedAccessViews(0u, 1u, &m_pBufferUAV, nullptr);

	pDevCon->Dispatch(m_width / 16, m_height / 9, 1u);
}