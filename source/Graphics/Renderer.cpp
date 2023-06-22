#include "Renderer.h"
#include "ECS/Scene.h"
#include "ECS/Components.h"

#include <execution>

thread_local std::mt19937 Renderer::s_RandomEngine;
std::uniform_int_distribution<std::mt19937::result_type> Renderer::s_Distribution;

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
	DX11_RELEASE(m_pRandomVectorBuffer);
	DX11_RELEASE(m_pRandomVectorSRV);
}

void Renderer::initiate(ID3D11Texture2D* pTarget, Scene* pScene)
{
	OKAY_ASSERT(pTarget);
	OKAY_ASSERT(pScene);

	shutdown();

	m_pScene = pScene;

	m_pTargetTexture = pTarget;
	m_pTargetTexture->AddRef();

	D3D11_TEXTURE2D_DESC textureDesc{};
	m_pTargetTexture->GetDesc(&textureDesc);
	m_width = textureDesc.Width;
	m_height = textureDesc.Height;

	ID3D11Device* pDevice = Okay::getDevice();
	bool success = false;

	// Target Texture UAV
	success = SUCCEEDED(pDevice->CreateUnorderedAccessView(m_pTargetTexture, nullptr, &m_pTargetUAV));
	OKAY_ASSERT(success);

	// Raytrace Computer Shader
	success = Okay::createShader("resources/RayTracerCS.hlsl", &m_pMainRaytracingCS);
	OKAY_ASSERT(success);

	// Sphere data
	success = Okay::createStructuredBuffer(&m_pSphereDataBuffer, &m_pSphereDataSRV, nullptr, sizeof(SphereComponent), 2u);
	OKAY_ASSERT(success);

	// Random Vectors Buffer
	success = Okay::createStructuredBuffer(&m_pRandomVectorBuffer, &m_pRandomVectorSRV, nullptr, sizeof(glm::vec3), NUM_RANDOM_VECTORS);
	OKAY_ASSERT(success);

	m_bufferIndices.resize(NUM_RANDOM_VECTORS);
	for (uint32_t i = 0; i < NUM_RANDOM_VECTORS; i++)
		m_bufferIndices[i] = i;
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

	randomizeRandomVectors();

	// Bind and Dispatch
	static const float CLEAR_COLOUR[4]{ 0.2f, 0.4f, 0.6f, 1.f };
	pDevCon->ClearUnorderedAccessViewFloat(m_pTargetUAV, CLEAR_COLOUR);

	pDevCon->CSSetShader(m_pMainRaytracingCS, nullptr, 0u);
	pDevCon->CSSetUnorderedAccessViews(0u, 1u, &m_pTargetUAV, nullptr);
	pDevCon->CSSetShaderResources(0u, 1u, &m_pSphereDataSRV);
	pDevCon->CSSetShaderResources(1u, 1u, &m_pRandomVectorSRV);

	pDevCon->Dispatch(m_width / 16u, m_height / 9u, 1u);

	static ID3D11UnorderedAccessView* nullUAV = nullptr;
	pDevCon->CSSetUnorderedAccessViews(0u, 1u, &nullUAV, nullptr);
}

void Renderer::randomizeRandomVectors()
{
	D3D11_MAPPED_SUBRESOURCE sub{};
	if (FAILED(Okay::getDeviceContext()->Map(m_pRandomVectorBuffer, 0u, D3D11_MAP_WRITE_DISCARD, 0u, &sub)))
		return;

	static const float InverseUint32Max = 1.f / (float)std::numeric_limits<uint32_t>::max();
	std::for_each(std::execution::par, m_bufferIndices.begin(), m_bufferIndices.end(), [&](uint32_t i)
	{
		// Check if the rand thing is slow, should prolly used thread local random
		float randX = ((float)s_Distribution(s_RandomEngine) * InverseUint32Max) * 2.f - 1.f;
		float randY = ((float)s_Distribution(s_RandomEngine) * InverseUint32Max) * 2.f - 1.f;
		float randZ = ((float)s_Distribution(s_RandomEngine) * InverseUint32Max) * 2.f - 1.f;
		
		((glm::vec4*)sub.pData)[i] = glm::vec4(randX, randY, randZ, 0);
		//((glm::vec4*)sub.pData)[i] = glm::normalize(glm::vec4(randX, randY, randZ, 0));
	});

	Okay::getDeviceContext()->Unmap(m_pRandomVectorBuffer, 0u);
}
