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
	:m_pMainRaytracingCS(nullptr), m_pScene(nullptr), m_renderData(), m_pRenderDataBuffer(nullptr),
	m_pGpuResourceManager(nullptr), m_pResourceManager(nullptr), m_pTargetTexture(nullptr)
{
}

RayTracer::RayTracer(const RenderTexture& target, const GPUResourceManager& gpuResourceManager)
{
	initiate(target, gpuResourceManager);
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

	m_meshData.shutdown();
	m_spheres.shutdown();
	m_directionalLights.shutdown();
	m_pointLights.shutdown();
	m_spotLights.shutdown();
	m_accumulationTexture.shutdown();

	DX11_RELEASE(m_pRenderDataBuffer);
	DX11_RELEASE(m_pMainRaytracingCS);
}

void RayTracer::initiate(const RenderTexture& target, const GPUResourceManager& gpuResourceManager)
{
	shutdown();

	m_pTargetTexture = &target;
	m_pGpuResourceManager = &gpuResourceManager;
	m_pResourceManager = &gpuResourceManager.getResourceManager();

	m_renderData.textureDims = target.getDimensions();

	ID3D11Device* pDevice = Okay::getDevice();
	bool success = false;
	
	// Accumulation Texture
	m_accumulationTexture.initiate(m_renderData.textureDims.x, m_renderData.textureDims.y, TextureFormat::F_32X4, TextureFlags::SHADER_WRITE);


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
	m_directionalLights.initiate(sizeof(GPU_DirectionalLight), SRV_START_SIZE, nullptr);
	m_pointLights.initiate(sizeof(GPU_PointLight), SRV_START_SIZE, nullptr);
	m_spotLights.initiate(sizeof(GPU_SpotLight), SRV_START_SIZE, nullptr);
}

void RayTracer::render()
{
	m_pGpuResourceManager->bindResources();

	calculateProjectionData();
	updateBuffers();

	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();

	// Clear
	static const float CLEAR_COLOUR[4]{ 0.2f, 0.4f, 0.6f, 1.f };
	pDevCon->ClearUnorderedAccessViewFloat(*m_pTargetTexture->getUAV(), CLEAR_COLOUR);

	ID3D11ShaderResourceView* srvs[5u]{};
	srvs[0] = m_spheres.getSRV();
	srvs[1] = m_meshData.getSRV();
	srvs[2] = m_directionalLights.getSRV();
	srvs[3] = m_pointLights.getSRV();
	srvs[4] = m_spotLights.getSRV();
	pDevCon->CSSetShaderResources(RT_SPHERE_DATA_SLOT, 5u, srvs);


	// Bind standard resources
	pDevCon->CSSetShader(m_pMainRaytracingCS, nullptr, 0u);
	pDevCon->CSSetUnorderedAccessViews(RT_RESULT_BUFFER_SLOT, 1u, m_pTargetTexture->getUAV(), nullptr);
	pDevCon->CSSetUnorderedAccessViews(RT_ACCUMULATION_BUFFER_SLOT, 1u, m_accumulationTexture.getUAV(), nullptr);
	pDevCon->CSSetConstantBuffers(RT_RENDER_DATA_SLOT, 1u, &m_pRenderDataBuffer);

	// Dispatch and unbind
	pDevCon->Dispatch(m_renderData.textureDims.x / 16u, m_renderData.textureDims.y / 9u, 1u);

	static ID3D11UnorderedAccessView* nullUAV = nullptr;
	pDevCon->CSSetUnorderedAccessViews(0u, 1u, &nullUAV, nullptr);
}

void RayTracer::reloadShaders()
{
	Okay::reloadShader(SHADER_PATH "RayTracerCS.hlsl", &m_pMainRaytracingCS);
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
	glm::vec3 camForward = camTra.getForwardVec();
	glm::vec3 camRight = camTra.getRightVec();
	glm::vec3 camUp = camTra.getUpVec();
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

	auto dirLightView = reg.view<DirectionalLight, Transform>();
	m_directionalLights.update((uint32_t)dirLightView.size_hint(), [&](char* pMappedBufferData)
		{
			GPU_DirectionalLight* pGPUData = nullptr;
			for (entt::entity entity : dirLightView)
			{
				pGPUData = (GPU_DirectionalLight*)pMappedBufferData;

				auto [dirLight, transform] = dirLightView[entity];
				pGPUData->light = dirLight;
				pGPUData->light.effectiveAngle = glm::cos(glm::radians(dirLight.effectiveAngle));
				pGPUData->direction = transform.getForwardVec();
			}
		});

	auto pointLightView = reg.view<PointLight, Transform>();
	m_pointLights.update((uint32_t)pointLightView.size_hint(), [&](char* pMappedBufferData)
		{
			GPU_PointLight* pGPUData = nullptr;
			for (entt::entity entity : pointLightView)
			{
				pGPUData = (GPU_PointLight*)pMappedBufferData;

				auto [pointLight, transform] = pointLightView[entity];
				pGPUData->light = pointLight;
				pGPUData->position = transform.position;
			}
		});

	auto spotLightView = reg.view<SpotLight, Transform>();
	m_spotLights.update((uint32_t)spotLightView.size_hint(), [&](char* pMappedBufferData)
	{
		GPU_SpotLight* pGPUData = nullptr;
		for (entt::entity entity : spotLightView)
		{
			pGPUData = (GPU_SpotLight*)pMappedBufferData;

			auto [spotLight, transform] = spotLightView[entity];
			pGPUData->light = spotLight;
			pGPUData->light.maxAngle = glm::cos(glm::radians(spotLight.maxAngle));
			pGPUData->position = transform.position;
			pGPUData->direction = transform.getForwardVec();
		}
	});


	
	// Render Data
	m_renderData.numSpheres = (uint32_t)sphereView.size_hint();
	m_renderData.numMeshes = (uint32_t)meshView.size_hint();
	m_renderData.numDirLights = (uint32_t)dirLightView.size_hint();
	m_renderData.numPointLights = (uint32_t)pointLightView.size_hint();
	m_renderData.numSpotLights = (uint32_t)spotLightView.size_hint();
	m_renderData.numAccumulationFrames += m_renderData.accumulationEnabled;

	Okay::updateBuffer(m_pRenderDataBuffer, &m_renderData, sizeof(RenderData));
}

void RayTracer::onResize()
{
	glm::uvec2 newDims = m_pTargetTexture->getDimensions();

	m_renderData.textureDims = newDims;
	m_accumulationTexture.resize(newDims.x, newDims.y);
	resetAccumulation();
}