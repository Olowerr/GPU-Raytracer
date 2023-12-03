#include "RayTracer.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"
#include "shaders/ShaderResourceRegisters.h"
#include "GPUResourceManager.h"
#include "ResourceManager.h"
#include "BvhBuilder.h"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/quaternion.hpp"

RayTracer::RayTracer()
	:m_pTargetUAV(nullptr), m_pMainRaytracingCS(nullptr), m_pScene(nullptr), m_renderData(),
	m_pAccumulationUAV(nullptr), m_pRenderDataBuffer(nullptr), m_pGpuResourceManager(nullptr),
	m_pResourceManager(nullptr)
{
}

RayTracer::RayTracer(ID3D11Texture2D* pTarget, const GPUResourceManager& gpuResourceManager)
{
	initiate(pTarget, gpuResourceManager);
}

RayTracer::~RayTracer()
{
	shutdown();
}

void RayTracer::shutdown()
{
	m_pScene = nullptr;
	m_pGpuResourceManager = nullptr;
	m_pResourceManager = nullptr;

	DX11_RELEASE(m_pTargetUAV);
	DX11_RELEASE(m_pAccumulationUAV);
	DX11_RELEASE(m_pRenderDataBuffer);
	DX11_RELEASE(m_pMainRaytracingCS);
	
	m_meshData.shutdown();
	m_spheres.shutdown();
}

void RayTracer::initiate(ID3D11Texture2D* pTarget, const GPUResourceManager& gpuResourceManager)
{
	OKAY_ASSERT(pTarget);

	shutdown();

	m_pGpuResourceManager = &gpuResourceManager;
	m_pResourceManager = &gpuResourceManager.getResourceManager();

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
	m_spheres.initiate(sizeof(glm::vec3) + sizeof(Sphere), SRV_START_SIZE, nullptr);
	m_meshData.initiate(sizeof(GPU_MeshComponent), SRV_START_SIZE, nullptr);
}

void RayTracer::render()
{
	calculateProjectionData();
	updateBuffers();

	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();

	// Clear
	static const float CLEAR_COLOUR[4]{ 0.2f, 0.4f, 0.6f, 1.f };
	pDevCon->ClearUnorderedAccessViewFloat(m_pTargetUAV, CLEAR_COLOUR);

	// TODO: Bind only at start

	ID3D11ShaderResourceView* srvs[2u]{};
	srvs[0] = m_spheres.getSRV();
	srvs[1] = m_meshData.getSRV();
	pDevCon->CSSetShaderResources(RT_SPHERE_DATA_SLOT, 2u, srvs);


	// Bind standard resources
	pDevCon->CSSetShader(m_pMainRaytracingCS, nullptr, 0u);
	pDevCon->CSSetUnorderedAccessViews(RT_RESULT_BUFFER_SLOT, 1u, &m_pTargetUAV, nullptr);
	pDevCon->CSSetUnorderedAccessViews(RT_ACCUMULATION_BUFFER_SLOT, 1u, &m_pAccumulationUAV, nullptr);
	pDevCon->CSSetConstantBuffers(RT_RENDER_DATA_SLOT, 1u, &m_pRenderDataBuffer);

	// Dispatch and unbind
	pDevCon->Dispatch(m_renderData.textureDims.x / 16u, m_renderData.textureDims.y / 9u, 1u);

	static ID3D11UnorderedAccessView* nullUAV = nullptr;
	pDevCon->CSSetUnorderedAccessViews(0u, 1u, &nullUAV, nullptr);
}

void RayTracer::reloadShaders()
{
	ID3D11ComputeShader* pNewShader = nullptr;
	if (!Okay::createShader(SHADER_PATH "RayTracerCS.hlsl", &pNewShader))
		return;

	DX11_RELEASE(m_pMainRaytracingCS);
	m_pMainRaytracingCS = pNewShader;

	resetAccumulation();
}

void RayTracer::calculateProjectionData()
{
	const Entity camera = m_pScene->getFirstCamera();
	OKAY_ASSERT(camera.isValid());

	const glm::vec2 windowDimsVec((float)m_renderData.textureDims.x, (float)m_renderData.textureDims.y);
	const float aspectRatio = windowDimsVec.x / windowDimsVec.y;

	const Camera& camData = camera.getComponent<Camera>();
	const Transform& camTra = camera.getComponent<Transform>();

	// Calculate viewPlane parameters using trigonometry: tan(v) = a/b -> tan(v) * b = a
	const float sideB = camData.nearZ;
	const float vAngleRadians = glm::radians(camData.fov * 0.5f); // FOV/2 to make right triangle (90deg angle, idk name)
	const float sideA = glm::tan(vAngleRadians) * sideB;


	// Convert to viewPlane
	m_renderData.viewPlaneDims.x = sideA * 2.f;
	m_renderData.viewPlaneDims.y = m_renderData.viewPlaneDims.x / aspectRatio;


	// Camera Data
	m_renderData.cameraPosition = camTra.position;
	m_renderData.cameraNearZ = camData.nearZ;

	// Inverse Projection Matrix // Only used in Cherno way
	m_renderData.cameraInverseProjectionMatrix = glm::transpose(glm::inverse(
		glm::perspectiveFovLH(glm::radians(camData.fov), windowDimsVec.x, windowDimsVec.y, camData.nearZ, camData.farZ)));

	// View vectors
	const glm::mat3 rotationMatrix = glm::toMat3(glm::quat(glm::radians(camTra.rotation)));
	const glm::vec3& camForward = rotationMatrix[2];
	const glm::vec3& camRight = rotationMatrix[0];
	const glm::vec3& camUp = rotationMatrix[1];
	m_renderData.cameraRightDir = camRight;
	m_renderData.cameraUpDir = camUp;

	// Inverse View Matrix
	m_renderData.cameraInverseViewMatrix = glm::transpose(glm::inverse(
		glm::lookAtLH(camTra.position, camTra.position + camForward, glm::vec3(0.f, 1.f, 0.f))));
}

void RayTracer::updateBuffers()
{
	const entt::registry& reg = m_pScene->getRegistry();
	
	auto sphereView = reg.view<Sphere, Transform>();
	m_spheres.update((uint32_t)sphereView.size_hint(), [&](char* pMappedBufferData)
	{
		for (entt::entity entity : sphereView)
		{
			auto [sphere, transform] = sphereView[entity];
	
			memcpy(pMappedBufferData, &transform.position, sizeof(glm::vec3));
			pMappedBufferData += sizeof(glm::vec3);
	
			memcpy(pMappedBufferData, &sphere, sizeof(Sphere));
			pMappedBufferData += sizeof(Sphere);
		}
	});
	
	auto meshView = reg.view<MeshComponent, Transform>();
	m_meshData.update((uint32_t)meshView.size_hint(), [&](char* pMappedBufferData)
	{
		const std::vector<MeshDesc>& meshDescs = m_pGpuResourceManager->getMeshDescriptors();
		glm::mat4 transformMatrix{};
		GPU_MeshComponent* gpuData;
		for (entt::entity entity : meshView)
		{
			gpuData = (GPU_MeshComponent*)pMappedBufferData;
	
			auto [meshComp, transform] = meshView[entity];
			transformMatrix = glm::transpose(transform.calculateMatrix());
	
			gpuData->triStart = meshDescs[meshComp.meshID].startIdx;
			gpuData->triEnd = meshDescs[meshComp.meshID].endIdx;
			
			gpuData->boundingBox = m_pResourceManager->getAsset<Mesh>(meshComp.meshID).getBoundingBox();
	
			gpuData->transformMatrix = transformMatrix;
			gpuData->inverseTransformMatrix = glm::inverse(transformMatrix);
	
			gpuData->material = meshComp.material;
			gpuData->bvhNodeStartIdx = meshDescs[meshComp.meshID].bvhTreeStartIdx;
			
			pMappedBufferData += sizeof(GPU_MeshComponent);
		}
	});

	
	// Render Data
	m_renderData.numSpheres = (uint32_t)sphereView.size_hint();
	m_renderData.numMeshes = (uint32_t)meshView.size_hint();
	m_renderData.numAccumulationFrames += m_renderData.accumulationEnabled;

	Okay::updateBuffer(m_pRenderDataBuffer, &m_renderData, sizeof(RenderData));
}