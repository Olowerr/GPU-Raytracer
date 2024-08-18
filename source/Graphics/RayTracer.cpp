#include "RayTracer.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"
#include "shaders/ShaderResourceRegisters.h"
#include "ResourceManager.h"
#include "BvhBuilder.h"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/quaternion.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

RayTracer::RayTracer()
	:m_pMainRaytracingCS(nullptr), m_pScene(nullptr), m_renderData(), m_pRenderDataBuffer(nullptr),
	m_pResourceManager(nullptr), m_pTargetTexture(nullptr), m_pEnvironmentMapSRV(nullptr),
	m_pTextures(nullptr), m_maxBvhLeafTriangles(0u), m_maxBvhDepth(0u)
{
}

RayTracer::RayTracer(const RenderTexture& target, const ResourceManager& resourceManager, std::string_view environmentMapPath)
{
	initiate(target, resourceManager);
}

RayTracer::~RayTracer()
{
	shutdown();
}

void RayTracer::shutdown()
{
	m_pScene = nullptr;
	m_pResourceManager = nullptr;

	m_meshData.shutdown();
	m_spheres.shutdown();
	m_directionalLights.shutdown();
	m_pointLights.shutdown();
	m_spotLights.shutdown();
	m_accumulationTexture.shutdown();

	DX11_RELEASE(m_pRenderDataBuffer);
	DX11_RELEASE(m_pMainRaytracingCS);

	m_pResourceManager = nullptr;

	m_trianglePositions.shutdown();
	m_triangleInfo.shutdown();
	m_bvhTree.shutdown();

	DX11_RELEASE(m_pTextures);

	DX11_RELEASE(m_pEnvironmentMapSRV);
}

void RayTracer::initiate(const RenderTexture& target, const ResourceManager& resourceManager, std::string_view environmentMapPath)
{
	shutdown();

	m_pTargetTexture = &target;
	m_pResourceManager = &resourceManager;

	m_renderData.textureDims = target.getDimensions();

	m_maxBvhLeafTriangles = 5u;
	m_maxBvhDepth = 30u;

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

	loadTextureData();
	loadEnvironmentMap(environmentMapPath);
	loadMeshAndBvhData();
	bindResources();

	{ // Basic Sampler
		D3D11_SAMPLER_DESC simpDesc{};
		simpDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		simpDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		simpDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		simpDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		simpDesc.MinLOD = -FLT_MAX;
		simpDesc.MaxLOD = FLT_MAX;
		simpDesc.MipLODBias = 0.f;
		simpDesc.MaxAnisotropy = 1u;
		simpDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		ID3D11SamplerState* pSimp = nullptr;
		bool success = SUCCEEDED(pDevice->CreateSamplerState(&simpDesc, &pSimp));
		OKAY_ASSERT(success);
		Okay::getDeviceContext()->VSSetSamplers(0u, 1u, &pSimp);
		Okay::getDeviceContext()->PSSetSamplers(0u, 1u, &pSimp);
		Okay::getDeviceContext()->CSSetSamplers(0u, 1u, &pSimp);
		DX11_RELEASE(pSimp);
	}
}

void RayTracer::loadMeshAndBvhData()
{
	const std::vector<Mesh>& meshes = m_pResourceManager->getAll<Mesh>();
	const uint32_t numMeshes = (uint32_t)meshes.size();

	if (!numMeshes)
		return;

	auto tryOffsetIdx = [](uint32_t idx, uint32_t offset)
		{
			return idx == Okay::INVALID_UINT ? idx : idx + offset;
		};

	m_meshDescs.resize(numMeshes);

	uint32_t numTotalTriangles = 0u;
	for (uint32_t i = 0; i < numMeshes; i++)
	{
		numTotalTriangles += (uint32_t)meshes[i].getTrianglesPos().size();
	}

	uint32_t triBufferCurStartIdx = 0;
	std::vector<Okay::Triangle> gpuTrianglePositions;
	std::vector<Okay::TriangleInfo> gpuTriangleInfo;
	gpuTrianglePositions.reserve(numTotalTriangles);
	gpuTriangleInfo.reserve(numTotalTriangles);

	m_bvhTreeNodes.clear();
	m_bvhTreeNodes.shrink_to_fit();
	BvhBuilder bvhBuilder(m_maxBvhLeafTriangles, m_maxBvhDepth);

	for (uint32_t i = 0; i < numMeshes; i++)
	{
		const Mesh& mesh = meshes[i];
		const std::vector<Okay::Triangle>& meshTriPos = mesh.getTrianglesPos();
		const std::vector<Okay::TriangleInfo>& meshTriInfo = mesh.getTrianglesInfo();

		bvhBuilder.buildTree(mesh);
		const std::vector<BvhNode>& nodes = bvhBuilder.getTree();

		const uint32_t numNodes = (uint32_t)nodes.size();
		const uint32_t gpuNodesPrevSize = (uint32_t)m_bvhTreeNodes.size();

		m_bvhTreeNodes.resize(gpuNodesPrevSize + numNodes);

		uint32_t localTriStart = 0u;
		for (uint32_t k = 0; k < numNodes; k++)
		{
			GPUNode& gpuNode = m_bvhTreeNodes[gpuNodesPrevSize + k];
			const BvhNode& bvhNode = nodes[k];

			const uint32_t numTriIndicies = (uint32_t)bvhNode.triIndicies.size();

			gpuNode.boundingBox = bvhNode.boundingBox;
			gpuNode.firstChildIdx = tryOffsetIdx(bvhNode.firstChildIdx, gpuNodesPrevSize);

			if (!bvhNode.isLeaf())
				continue;

			gpuNode.triStart = triBufferCurStartIdx + localTriStart;
			gpuNode.triEnd = gpuNode.triStart + numTriIndicies;

			localTriStart += numTriIndicies;

			for (uint32_t j = 0; j < numTriIndicies; j++)
			{
				const Okay::Triangle& triPos = meshTriPos[bvhNode.triIndicies[j]];
				const Okay::TriangleInfo& triInfo = meshTriInfo[bvhNode.triIndicies[j]];

				gpuTrianglePositions.emplace_back(triPos);
				gpuTriangleInfo.emplace_back(triInfo);
			}
		}

		m_meshDescs[i].numBvhNodes = numNodes;
		m_meshDescs[i].bvhTreeStartIdx = gpuNodesPrevSize;
		m_meshDescs[i].startIdx = triBufferCurStartIdx;
		m_meshDescs[i].endIdx = triBufferCurStartIdx + (uint32_t)meshTriPos.size();

		triBufferCurStartIdx += (uint32_t)meshTriPos.size();
	}

	m_trianglePositions.initiate(sizeof(Okay::Triangle), numTotalTriangles, gpuTrianglePositions.data());
	m_triangleInfo.initiate(sizeof(Okay::TriangleInfo), numTotalTriangles, gpuTriangleInfo.data());
	m_bvhTree.initiate(sizeof(GPUNode), (uint32_t)m_bvhTreeNodes.size(), m_bvhTreeNodes.data());
}

void RayTracer::render()
{
	bindResources();

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

void RayTracer::loadTextureData()
{
	const std::vector<Texture>& textures = m_pResourceManager->getAll<Texture>();
	uint32_t numTextures = (uint32_t)textures.size();

	if (!numTextures)
		return;

	uint32_t totArea = 0;
	for (const Texture& texture : textures)
	{
		totArea += texture.getWidth() * texture.getHeight();
	}

	uint32_t newSize = uint32_t(glm::sqrt(totArea / textures.size()));

	std::vector<Texture> scaledTextures;
	scaledTextures.reserve(numTextures);

	for (const Texture& texture : textures)
	{
		scaledTextures.emplace_back(texture.getTextureData(), texture.getWidth(), texture.getHeight(), "");
		Okay::scaleTexture(scaledTextures.back(), newSize, newSize);
	}

	Okay::createTextureArray(&m_pTextures, scaledTextures, newSize, newSize);

	for (const Texture& scaledTexture : scaledTextures)
	{
		unsigned char* pTextureData = scaledTexture.getTextureData();
		OKAY_DELETE_ARRAY(pTextureData);
	}
}

void RayTracer::loadEnvironmentMap(std::string_view path)
{
	int imgWidth, imgHeight, channels = STBI_rgb_alpha;
	uint32_t* pImageData = (uint32_t*)stbi_load(path.data(), &imgWidth, &imgHeight, nullptr, channels);

	if (!pImageData)
		return;

	uint32_t width = imgWidth / 4u;
	uint32_t height = imgHeight / 3u;
	uint32_t byteWidth = width * height * channels;

	D3D11_SUBRESOURCE_DATA data[6]{};
	for (uint32_t i = 0; i < 6u; i++)
	{
		data[i].pSysMem = new char[byteWidth] {};
		data[i].SysMemPitch = width * channels;
		data[i].SysMemSlicePitch = 0u;
	}

	// The coursor points to the location of each side
	uint32_t* coursor = nullptr;

	auto copyImgSection = [&](uint32_t* pTarget)
		{
			for (uint32_t i = 0; i < height; i++)
			{
				memcpy(pTarget, coursor, (size_t)width * channels);
				pTarget += width;
				coursor += imgWidth;
			}
		};

	// Positive X
	coursor = pImageData + imgWidth * height + width * 2u;
	copyImgSection((uint32_t*)data[0].pSysMem);

	// Negative X
	coursor = pImageData + imgWidth * height;
	copyImgSection((uint32_t*)data[1].pSysMem);

	// Positive Y
	coursor = pImageData + width;
	copyImgSection((uint32_t*)data[2].pSysMem);

	// Negative Y
	coursor = pImageData + imgWidth * height * 2u + width;
	copyImgSection((uint32_t*)data[3].pSysMem);

	// Positive Z
	coursor = pImageData + imgWidth * height + width;
	copyImgSection((uint32_t*)data[4].pSysMem);

	// Negative Z
	coursor = pImageData + imgWidth * height + width * 3u;
	copyImgSection((uint32_t*)data[5].pSysMem);

	stbi_image_free(pImageData);

	D3D11_TEXTURE2D_DESC texDesc{};
	texDesc.ArraySize = 6u;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0u;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.Height = height;
	texDesc.Width = width;
	texDesc.MipLevels = 1u;
	texDesc.SampleDesc.Count = 1u;
	texDesc.SampleDesc.Quality = 0u;
	texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

	ID3D11Device* pDevice = Okay::getDevice();
	ID3D11DeviceContext* pDeviceContext = Okay::getDeviceContext();
	bool success = false;

	ID3D11Texture2D* pTextureCube = nullptr;
	success = SUCCEEDED(pDevice->CreateTexture2D(&texDesc, data, &pTextureCube));

	for (uint32_t i = 0; i < 6; i++)
	{
		delete[] data[i].pSysMem;
	}

	OKAY_ASSERT(success);

	success = SUCCEEDED(pDevice->CreateShaderResourceView(pTextureCube, nullptr, &m_pEnvironmentMapSRV));
	DX11_RELEASE(pTextureCube);
	OKAY_ASSERT(success);
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
		glm::mat4 transformMatrix{};
		GPU_MeshComponent* gpuData;
		for (entt::entity entity : meshView)
		{
			gpuData = (GPU_MeshComponent*)pMappedBufferData;
	
			auto [meshComp, transform] = meshView[entity];
			transformMatrix = glm::transpose(transform.calculateMatrix());
	
			gpuData->triStart = m_meshDescs[meshComp.meshID].startIdx;
			gpuData->triEnd = m_meshDescs[meshComp.meshID].endIdx;
			
			gpuData->boundingBox = m_pResourceManager->getAsset<Mesh>(meshComp.meshID).getBoundingBox();
	
			gpuData->transformMatrix = transformMatrix;
			gpuData->inverseTransformMatrix = glm::inverse(transformMatrix);
	
			gpuData->material = meshComp.material;
			gpuData->bvhNodeStartIdx = m_meshDescs[meshComp.meshID].bvhTreeStartIdx;
			
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

void RayTracer::bindResources() const
{
	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();

	//pDevCon->ClearState();

	ID3D11ShaderResourceView* srvs[5u]{};
	srvs[RM_TRIANGLE_POS_SLOT] = m_trianglePositions.getSRV();
	srvs[RM_TRIANGLE_INFO_SLOT] = m_triangleInfo.getSRV();
	srvs[RM_TEXTURES_SLOT] = m_pTextures;
	srvs[RM_BVH_TREE_SLOT] = m_bvhTree.getSRV();
	srvs[RM_ENVIRONMENT_MAP_SLOT] = m_pEnvironmentMapSRV;

	pDevCon->VSSetShaderResources(RM_TRIANGLE_POS_SLOT, sizeof(srvs) / sizeof(srvs[0]), srvs);
	pDevCon->PSSetShaderResources(RM_TRIANGLE_POS_SLOT, sizeof(srvs) / sizeof(srvs[0]), srvs);
	pDevCon->CSSetShaderResources(RM_TRIANGLE_POS_SLOT, sizeof(srvs) / sizeof(srvs[0]), srvs);
}

void RayTracer::onResize()
{
	glm::uvec2 newDims = m_pTargetTexture->getDimensions();

	m_renderData.textureDims = newDims;
	m_accumulationTexture.resize(newDims.x, newDims.y);
	resetAccumulation();
}