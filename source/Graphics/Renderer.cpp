#include "Renderer.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"
#include "shaders/ShaderResourceRegisters.h"
#include "ResourceManager.h"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/quaternion.hpp"

#include <execution>

Renderer::Renderer()
	:m_pTargetUAV(nullptr), m_pMainRaytracingCS(nullptr), m_pScene(nullptr), m_renderData(),
	m_pAccumulationUAV(nullptr), m_pRenderDataBuffer(nullptr)
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
	
	shutdownGPUStorage(m_meshData);
	shutdownGPUStorage(m_spheres);
}

void Renderer::initiate(ID3D11Texture2D* pTarget, Scene* pScene)
{
	OKAY_ASSERT(pTarget);
	OKAY_ASSERT(pScene);

	shutdown();

	m_pScene = pScene;

	D3D11_TEXTURE2D_DESC textureDesc{};
	pTarget->GetDesc(&textureDesc);
	m_renderData.textureDims.x = textureDesc.Width;
	m_renderData.textureDims.y = textureDesc.Height;


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

	// Accumulation UAV
	success = SUCCEEDED(pDevice->CreateUnorderedAccessView(pAccumulationBuffer, nullptr, &m_pAccumulationUAV));
	DX11_RELEASE(pAccumulationBuffer);
	OKAY_ASSERT(success);


	// Render Data
	success = Okay::createConstantBuffer(&m_pRenderDataBuffer, &m_renderData, sizeof(RenderData));
	OKAY_ASSERT(success);


	// Raytrace Computer Shader
	success = Okay::createShader(SHADER_PATH "RayTracerCS.hlsl", &m_pMainRaytracingCS);
	OKAY_ASSERT(success);


	// Scene GPU Data
	const uint32_t SRV_START_SIZE = 10u;
	createGPUStorage(m_spheres, sizeof(glm::vec3) + sizeof(Sphere), SRV_START_SIZE);
	createGPUStorage(m_meshData, sizeof(glm::vec3) * 3, 100u);
}

void Renderer::render()
{
	calculateProjectionData();
	updateBuffers();

	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();

	// Clear
	static const float CLEAR_COLOUR[4]{ 0.2f, 0.4f, 0.6f, 1.f };
	pDevCon->ClearUnorderedAccessViewFloat(m_pTargetUAV, CLEAR_COLOUR);

	// Bind standard resources
	pDevCon->CSSetShader(m_pMainRaytracingCS, nullptr, 0u);
	pDevCon->CSSetUnorderedAccessViews(CPU_SLOT_RESULT_BUFFER, 1u, &m_pTargetUAV, nullptr);
	pDevCon->CSSetUnorderedAccessViews(CPU_SLOT_ACCUMULATION_BUFFER, 1u, &m_pAccumulationUAV, nullptr);
	pDevCon->CSSetConstantBuffers(CPU_SLOT_RENDER_DATA, 1u, &m_pRenderDataBuffer);

	// Bind scene data
	pDevCon->CSSetShaderResources(CPU_SLOT_SPHERE_DATA, 1u, &m_spheres.pSRV);
	pDevCon->CSSetShaderResources(CPU_SLOT_TRIANGLE_DATA, 1u, &m_meshData.pSRV);

	// Dispatch and unbind
	pDevCon->Dispatch(m_renderData.textureDims.x / 16u, m_renderData.textureDims.y / 9u, 1u);

	static ID3D11UnorderedAccessView* nullUAV = nullptr;
	pDevCon->CSSetUnorderedAccessViews(0u, 1u, &nullUAV, nullptr);
}

void Renderer::reloadShaders()
{
	ID3D11ComputeShader* pNewShader = nullptr;
	if (!Okay::createShader(SHADER_PATH "RayTracerCS.hlsl", &pNewShader))
		return;

	DX11_RELEASE(m_pMainRaytracingCS);
	m_pMainRaytracingCS = pNewShader;

	resetAccumulation();
}

void Renderer::loadTriangleData(const ResourceManager& resourceManager)
{
	const std::vector<Mesh>& meshes = resourceManager.getAll<Mesh>();

	uint32_t totTriangleCount = 0u;
	for (const Mesh& mesh : meshes)
		totTriangleCount += (uint32_t)mesh.getMeshData().positions.size() / 3u;
	
	updateGPUStorage(m_meshData, totTriangleCount, [&](char* pCoursor)
	{
		for (size_t i = 0; i < meshes.size(); i++)
		{
			const std::vector<glm::vec3>& positions = meshes[i].getMeshData().positions;
			const size_t byteWidth = sizeof(glm::vec3) * positions.size();

			memcpy(pCoursor, positions.data(), byteWidth);
			pCoursor += byteWidth;
		}
	});
}

void Renderer::calculateProjectionData()
{
	const glm::vec2 windowDimsVec((float)m_renderData.textureDims.x, (float)m_renderData.textureDims.y);
	const float aspectRatio = windowDimsVec.x / windowDimsVec.y;

	OKAY_ASSERT(m_camera.isValid());
	OKAY_ASSERT(m_camera.hasComponents<Camera>());
	const Camera& camera = m_camera.getComponent<Camera>();

	// Trigonometry, tan(v) = a/b -> tan(v) * b = a
	const float sideB = camera.nearZ; // NearZ
	const float vAngleRadians = glm::radians(camera.fov * 0.5f); // FOV/2 to make right triangle (90deg angle, idk name)
	const float sideA = glm::tan(vAngleRadians) * sideB;


	// Convert to viewPlane
	m_renderData.viewPlaneDims.x = sideA * 2.f;
	m_renderData.viewPlaneDims.y = m_renderData.viewPlaneDims.x / aspectRatio;


	// Camera Data
	m_renderData.cameraPosition = camera.position;
	m_renderData.cameraNearZ = camera.nearZ;


	// Calculate inverseProjectionMatrix // Only used in Cherno way
	m_renderData.cameraInverseProjectionMatrix = glm::transpose(glm::inverse(
		glm::perspectiveFovLH(glm::radians(camera.fov), windowDimsVec.x, windowDimsVec.y, camera.nearZ, camera.farZ)));


	// Calculate inverseViewMatrix, rotationMatrix used to get forward vector of camera
	const glm::mat3 rotationMatrix = glm::toMat3(glm::quat(glm::radians(camera.rotation)));
	m_renderData.cameraInverseViewMatrix = glm::transpose(glm::inverse(
		glm::lookAtLH(camera.position, camera.position + rotationMatrix[2], glm::vec3(0.f, 1.f, 0.f))));
}

void Renderer::updateBuffers()
{
	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();
	entt::registry& reg = m_pScene->getRegistry();

	auto sphereView = reg.view<Sphere, Transform>();
	updateGPUStorage(m_spheres, (uint32_t)sphereView.size_hint(), [&](char* pMappedBufferData)
	{
		for (entt::entity entity : sphereView)
		{
			auto [sphere, transform] = sphereView[entity];

			memcpy(pMappedBufferData, &transform.position, sizeof(glm::vec3));
			pMappedBufferData += sizeof(glm::vec3);

			memcpy(pMappedBufferData, &sphereView.get<Sphere>(entity), sizeof(Sphere));
			pMappedBufferData += sizeof(Sphere);
		}
	});
	
	// Render Data
	m_renderData.numSpheres = (uint32_t)sphereView.size_hint();
	if (m_renderData.accumulationEnabled == 1)
		m_renderData.numAccumulationFrames++;
	Okay::updateBuffer(m_pRenderDataBuffer, &m_renderData, sizeof(RenderData));
}

void Renderer::createGPUStorage(GPUStorage& storage, uint32_t elementSize, uint32_t capacity)
{
	shutdownGPUStorage(storage);

	storage.capacity = capacity;
	storage.gpuElementByteSize = elementSize;
	bool success = Okay::createStructuredBuffer(&storage.pBuffer, &storage.pSRV, nullptr, elementSize, capacity);
	OKAY_ASSERT(success);
}

void Renderer::shutdownGPUStorage(GPUStorage& storage)
{
	DX11_RELEASE(storage.pBuffer);
	DX11_RELEASE(storage.pSRV);
	storage.capacity = 0u;
	storage.gpuElementByteSize = 0u;
}

