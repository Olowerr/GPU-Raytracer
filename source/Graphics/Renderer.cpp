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
	m_pAccumulationUAV(nullptr), m_pRenderDataBuffer(nullptr), m_pResourceManager(nullptr)
{
}

Renderer::Renderer(ID3D11Texture2D* pTarget, Scene* pScene, ResourceManager* pResourceManager)
{
	initiate(pTarget, pScene, pResourceManager);
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
	shutdownGPUStorage(m_triangleData);
}

void Renderer::initiate(ID3D11Texture2D* pTarget, Scene* pScene, ResourceManager* pResourceManager)
{
	OKAY_ASSERT(pTarget);
	OKAY_ASSERT(pScene);
	OKAY_ASSERT(pResourceManager);

	shutdown();

	m_pScene = pScene;
	m_pResourceManager = pResourceManager;

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
	createGPUStorage(m_meshData, sizeof(GPU_MeshComponent), SRV_START_SIZE);
	// m_triangleData created in Renderer::loadTriangleData().
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
	pDevCon->CSSetUnorderedAccessViews(RESULT_BUFFER_CPU_SLOT, 1u, &m_pTargetUAV, nullptr);
	pDevCon->CSSetUnorderedAccessViews(ACCUMULATION_BUFFER_CPU_SLOT, 1u, &m_pAccumulationUAV, nullptr);
	pDevCon->CSSetConstantBuffers(RENDER_DATA_CPU_SLOT, 1u, &m_pRenderDataBuffer);

	// Bind scene data
	pDevCon->CSSetShaderResources(SPHERE_DATA_CPU_SLOT, 1u, &m_spheres.pSRV);
	pDevCon->CSSetShaderResources(MESH_DATA_CPU_SLOT, 1u, &m_meshData.pSRV);
	pDevCon->CSSetShaderResources(TRIANGLE_DATA_CPU_SLOT, 1u, &m_triangleData.pSRV);

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

void Renderer::loadTriangleData()
{
	const std::vector<Mesh>& meshes = m_pResourceManager->getAll<Mesh>();
	m_meshTriangleDesc.resize(meshes.size());

	uint32_t numTotalTriangles = 0u;
	for (const Mesh& mesh : meshes)
		numTotalTriangles += (uint32_t)mesh.getTriangles().size();

	createGPUStorage(m_triangleData, sizeof(Okay::Triangle), numTotalTriangles);

	updateGPUStorage(m_triangleData, 0u, [&](char* pCoursor)
	{
		uint32_t currentStartIdx = 0u;
		for (size_t i = 0; i < meshes.size(); i++)
		{
			const std::vector<Okay::Triangle>& triangles = meshes[i].getTriangles();
			const size_t byteWidth = sizeof(Okay::Triangle) * triangles.size();

			memcpy(pCoursor, triangles.data(), byteWidth);
			pCoursor += byteWidth;

			m_meshTriangleDesc[i].first = currentStartIdx;
			m_meshTriangleDesc[i].second = currentStartIdx + (uint32_t)triangles.size();

			currentStartIdx = m_meshTriangleDesc[i].second;
		}
	});
}

void Renderer::calculateProjectionData()
{
	const glm::vec2 windowDimsVec((float)m_renderData.textureDims.x, (float)m_renderData.textureDims.y);
	const float aspectRatio = windowDimsVec.x / windowDimsVec.y;

	OKAY_ASSERT(m_camera.isValid());
	OKAY_ASSERT(m_camera.hasComponents<Camera>());
	const Camera& camData = m_camera.getComponent<Camera>();
	const Transform& camTra = m_camera.getComponent<Transform>();

	// Trigonometry, tan(v) = a/b -> tan(v) * b = a
	const float sideB = camData.nearZ; // NearZ
	const float vAngleRadians = glm::radians(camData.fov * 0.5f); // FOV/2 to make right triangle (90deg angle, idk name)
	const float sideA = glm::tan(vAngleRadians) * sideB;


	// Convert to viewPlane
	m_renderData.viewPlaneDims.x = sideA * 2.f;
	m_renderData.viewPlaneDims.y = m_renderData.viewPlaneDims.x / aspectRatio;


	// Camera Data
	m_renderData.cameraPosition = camTra.position;
	m_renderData.cameraNearZ = camData.nearZ;


	// Calculate inverseProjectionMatrix // Only used in Cherno way
	m_renderData.cameraInverseProjectionMatrix = glm::transpose(glm::inverse(
		glm::perspectiveFovLH(glm::radians(camData.fov), windowDimsVec.x, windowDimsVec.y, camData.nearZ, camData.farZ)));


	// Calculate inverseViewMatrix, rotationMatrix used to get forward vector of camera
	const glm::mat3 rotationMatrix = glm::toMat3(glm::quat(glm::radians(camTra.rotation)));
	m_renderData.cameraInverseViewMatrix = glm::transpose(glm::inverse(
		glm::lookAtLH(camTra.position, camTra.position + rotationMatrix[2], glm::vec3(0.f, 1.f, 0.f))));
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

	auto meshView = reg.view<MeshComponent, Transform>();
	updateGPUStorage(m_meshData, (uint32_t)meshView.size_hint(), [&](char* pMappedBufferData)
	{
		glm::mat4 transformMatrix{};
		GPU_MeshComponent* gpuData;
		for (entt::entity entity : meshView)
		{
			gpuData = (GPU_MeshComponent*)pMappedBufferData;

			auto [meshComp, transform] = meshView[entity];
			transformMatrix = transform.calculateMatrix();

			gpuData->triStart = m_meshTriangleDesc[meshComp.meshID].first;
			gpuData->triCount = m_meshTriangleDesc[meshComp.meshID].second;

			gpuData->boundingBox = m_pResourceManager->getAsset<Mesh>(meshComp.meshID).getBoundingBox();
			gpuData->boundingBox.min += glm::vec3(transformMatrix[3]);
			gpuData->boundingBox.max += glm::vec3(transformMatrix[3]);

			gpuData->transformMatrix = glm::transpose(transformMatrix);

			gpuData->material = meshComp.material;
			
			pMappedBufferData += sizeof(GPU_MeshComponent);
		}
	});

	
	// Render Data
	m_renderData.numSpheres = (uint32_t)sphereView.size_hint();
	m_renderData.numMeshes = (uint32_t)meshView.size_hint();
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

