#include "Renderer.h"
#include "ECS/Scene.h"
#include "ECS/Components.h"

#include <execution>

thread_local std::mt19937 Renderer::s_RandomEngine;
std::uniform_int_distribution<std::mt19937::result_type> Renderer::s_Distribution;

Renderer::Renderer()
	:m_pTargetUAV(nullptr), m_pMainRaytracingCS(nullptr), m_pScene(nullptr), m_renderData(),
	m_pAccumulationUAV(nullptr), m_pRenderDataBuffer(nullptr), m_pSphereDataBuffer(nullptr),
	m_pSphereDataSRV(nullptr), m_pRandomVectorBuffer(nullptr), m_pRandomVectorSRV(nullptr)
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
	DX11_RELEASE(m_pTargetUAV);
	DX11_RELEASE(m_pAccumulationUAV);
	DX11_RELEASE(m_pRenderDataBuffer);
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

	D3D11_TEXTURE2D_DESC textureDesc{};
	pTarget->GetDesc(&textureDesc);
	m_renderData.m_textureDims.x = textureDesc.Width;
	m_renderData.m_textureDims.y = textureDesc.Height;


	ID3D11Device* pDevice = Okay::getDevice();
	bool success = false;

	// Target Texture UAV
	success = SUCCEEDED(pDevice->CreateUnorderedAccessView(pTarget, nullptr, &m_pTargetUAV));
	OKAY_ASSERT(success);
	
	// Accumulation Buffer
	ID3D11Texture2D* pAccumulationBuffer = nullptr;
	textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	success = SUCCEEDED(pDevice->CreateTexture2D(&textureDesc, nullptr, &pAccumulationBuffer));
	OKAY_ASSERT(success);

	// Accumilation UAV
	success = SUCCEEDED(pDevice->CreateUnorderedAccessView(pAccumulationBuffer, nullptr, &m_pAccumulationUAV));
	DX11_RELEASE(pAccumulationBuffer);
	OKAY_ASSERT(success);


	// Render Data
	success = Okay::createConstantBuffer(&m_pRenderDataBuffer, &m_renderData, sizeof(RenderData));
	OKAY_ASSERT(success);


	// Raytrace Computer Shader
	success = Okay::createShader("resources/RayTracerCS.hlsl", &m_pMainRaytracingCS);
	OKAY_ASSERT(success);


	// Sphere data
	m_sphereBufferCapacity = 10u;
	success = Okay::createStructuredBuffer(&m_pSphereDataBuffer, &m_pSphereDataSRV, nullptr, sizeof(SphereComponent), m_sphereBufferCapacity);
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
	updateBuffers();

	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();

	// Bind and Dispatch
	static const float CLEAR_COLOUR[4]{ 0.2f, 0.4f, 0.6f, 1.f };
	pDevCon->ClearUnorderedAccessViewFloat(m_pTargetUAV, CLEAR_COLOUR);

	pDevCon->CSSetShader(m_pMainRaytracingCS, nullptr, 0u);
	pDevCon->CSSetUnorderedAccessViews(0u, 1u, &m_pTargetUAV, nullptr);
	pDevCon->CSSetUnorderedAccessViews(1u, 1u, &m_pAccumulationUAV, nullptr);
	pDevCon->CSSetShaderResources(0u, 1u, &m_pSphereDataSRV);
	pDevCon->CSSetShaderResources(1u, 1u, &m_pRandomVectorSRV);
	pDevCon->CSSetConstantBuffers(0u, 1u, &m_pRenderDataBuffer);

	pDevCon->Dispatch(m_renderData.m_textureDims.x / 16u, m_renderData.m_textureDims.y / 9u, 1u);

	static ID3D11UnorderedAccessView* nullUAV = nullptr;
	pDevCon->CSSetUnorderedAccessViews(0u, 1u, &nullUAV, nullptr);
}

void Renderer::updateBuffers()
{
	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();

	// Sphere Data
	auto sphereView = m_pScene->getRegistry().view<SphereComponent>();
	if (m_sphereBufferCapacity < sphereView.size())
	{
		m_sphereBufferCapacity = sphereView.size() + 10u;
		DX11_RELEASE(m_pSphereDataBuffer);
		DX11_RELEASE(m_pSphereDataSRV);
		bool success = Okay::createStructuredBuffer(&m_pSphereDataBuffer, &m_pSphereDataSRV, nullptr, sizeof(SphereComponent), m_sphereBufferCapacity);
		OKAY_ASSERT(success);
	}

	D3D11_MAPPED_SUBRESOURCE sub{};
	if (SUCCEEDED(pDevCon->Map(m_pSphereDataBuffer, 0u, D3D11_MAP_WRITE_DISCARD, 0u, &sub)))
	{
		char* coursor = (char*)sub.pData;
		for (entt::entity entity : sphereView)
		{
			memcpy(coursor, &sphereView.get<SphereComponent>(entity), sizeof(SphereComponent));
			coursor += sizeof(SphereComponent);
		}
		pDevCon->Unmap(m_pSphereDataBuffer, 0u);
	}

	
	// Render Data
	m_renderData.m_numSpheres = (uint32_t)sphereView.size();
	if (m_renderData.m_accumulationEnabled == 1)
		m_renderData.m_numAccumulationFrames++;
	Okay::updateBuffer(m_pRenderDataBuffer, &m_renderData, sizeof(RenderData));


	// Random Vectors
	if (SUCCEEDED(pDevCon->Map(m_pRandomVectorBuffer, 0u, D3D11_MAP_WRITE_DISCARD, 0u, &sub)))
	{
		static const float InverseUint32Max = 1.f / (float)std::numeric_limits<uint32_t>::max();
		std::for_each(/*std::execution::par,*/ m_bufferIndices.begin(), m_bufferIndices.end(), [&](uint32_t i)
			{
				float randX = ((float)s_Distribution(s_RandomEngine) * InverseUint32Max) * 2.f - 1.f;
				float randY = ((float)s_Distribution(s_RandomEngine) * InverseUint32Max) * 2.f - 1.f;
				float randZ = ((float)s_Distribution(s_RandomEngine) * InverseUint32Max) * 2.f - 1.f;

				//((glm::vec3*)sub.pData)[i] = glm::vec3(randX, randY, randZ);
				((glm::vec3*)sub.pData)[i] = glm::normalize(glm::vec3(randX, randY, randZ));
			});

		pDevCon->Unmap(m_pRandomVectorBuffer, 0u);
	}
}