#include "Renderer.h"
#include "ECS/Scene.h"
#include "ECS/Components.h"

Renderer::Renderer()
	:m_pTargetTexture(nullptr), m_pTargetUAV(nullptr), m_width(0u), m_height(0u),
	m_pMainRaytracingCS(nullptr), m_pScene(nullptr)
{
}

Renderer::Renderer(ID3D11Texture2D* pTarget, Scene* pScene)
{
	initiate(pTarget, pScene);
}

Renderer::~Renderer()
{
	shutdown();
}

void Renderer::shutdown()
{
	m_pScene = nullptr;
	m_width = m_height = 0u;
	DX11_RELEASE(m_pTargetTexture);
	DX11_RELEASE(m_pTargetUAV);
	DX11_RELEASE(m_pMainRaytracingCS);
	DX11_RELEASE(m_pSphereDataBuffer);
	DX11_RELEASE(m_pSphereDataSRV);
}

void Renderer::initiate(ID3D11Texture2D* pTarget, Scene* pScene)
{
	OKAY_ASSERT(pTarget);
	OKAY_ASSERT(pScene);

	shutdown();

	m_pScene = pScene;

	m_pTargetTexture = pTarget;
	m_pTargetTexture->AddRef();

	D3D11_TEXTURE2D_DESC bufferDesc{};
	m_pTargetTexture->GetDesc(&bufferDesc);
	m_width = bufferDesc.Width;
	m_height = bufferDesc.Height;

	bool success = false;

	success = SUCCEEDED(Okay::getDevice()->CreateUnorderedAccessView(m_pTargetTexture, nullptr, &m_pTargetUAV));
	OKAY_ASSERT(success);

	success = Okay::createShader("resources/RayTracerCS.hlsl", &m_pMainRaytracingCS);
	OKAY_ASSERT(success);

	success = Okay::createStructuredBuffer(&m_pSphereDataBuffer, &m_pSphereDataSRV, nullptr, sizeof(SphereComponent), 2u);
	OKAY_ASSERT(success);
}

void Renderer::render()
{
	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();
	

	// Copy Sphere data to GPU resource
	D3D11_MAPPED_SUBRESOURCE sub{};
	if (FAILED(pDevCon->Map(m_pSphereDataBuffer, 0u, D3D11_MAP_WRITE_DISCARD, 0u, &sub)))
		return;

	auto sphereView = m_pScene->getRegistry().view<SphereComponent>();
	char* coursor = (char*)sub.pData;
	for (entt::entity entity : sphereView)
	{
		memcpy(coursor, &sphereView.get<SphereComponent>(entity), sizeof(SphereComponent));
		coursor += sizeof(SphereComponent);
	}
	
	pDevCon->Unmap(m_pSphereDataBuffer, 0u);


	// Bind and Dispatch
	static const float CLEAR_COLOUR[4]{ 0.2f, 0.4f, 0.6f, 1.f };
	pDevCon->ClearUnorderedAccessViewFloat(m_pTargetUAV, CLEAR_COLOUR);

	pDevCon->CSSetShader(m_pMainRaytracingCS, nullptr, 0u);
	pDevCon->CSSetUnorderedAccessViews(0u, 1u, &m_pTargetUAV, nullptr);
	pDevCon->CSSetShaderResources(0u, 1u, &m_pSphereDataSRV);

	pDevCon->Dispatch(m_width / 16u, m_height / 9u, 1u);

	static ID3D11UnorderedAccessView* nullUAV = nullptr;
	pDevCon->CSSetUnorderedAccessViews(0u, 1u, &nullUAV, nullptr);
}