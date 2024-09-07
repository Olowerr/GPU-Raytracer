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

#include <stack>
#include <chrono>
#include <DirectXCollision.h>

RayTracer::RayTracer()
	:m_pMainRaytracingCS(nullptr), m_pScene(nullptr), m_renderData(), m_pRenderDataBuffer(nullptr),
	m_pResourceManager(nullptr), m_pTargetTexture(nullptr), m_pEnvironmentMapSRV(nullptr), m_pTextures(nullptr)
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
	m_octTree.initiate(sizeof(GPU_OctTreeNode), SRV_START_SIZE, nullptr);

	loadTextureData();
	loadEnvironmentMap(environmentMapPath);
	loadMeshAndBvhData(30, 5);

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

void RayTracer::loadMeshAndBvhData(uint32_t maxDepth, uint32_t maxLeafTriangles)
{
	std::chrono::time_point<std::chrono::system_clock> bvhTreeTimerStart;

	printf("\nBvh Tree build start\n");
	printf("maxDepth: %u\nmaxLeafTriangles: %u\n", maxDepth, maxLeafTriangles);

	bvhTreeTimerStart = std::chrono::system_clock::now();

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
	BvhBuilder bvhBuilder(maxLeafTriangles, maxDepth);

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

	std::chrono::duration<float> duration = std::chrono::system_clock::now() - bvhTreeTimerStart;

	printf("numNodes: %u\nnumMeshes: %u\n", (uint32_t)m_bvhTreeNodes.size(), numMeshes);
	printf("Bvh Tree build time: %.3fms\n", duration.count() * 1000.f);
}

void RayTracer::render()
{
	calculateProjectionData();
	updateBuffers();

	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();

	// Clear
	static const float CLEAR_COLOUR[4]{ 0.2f, 0.4f, 0.6f, 1.f };
	pDevCon->ClearUnorderedAccessViewFloat(*m_pTargetTexture->getUAV(), CLEAR_COLOUR);

	ID3D11ShaderResourceView* srvs[NUM_T_REGISTERS]{};
	srvs[TRIANGLE_POS_SLOT] = m_trianglePositions.getSRV();
	srvs[TRIANGLE_INFO_SLOT] = m_triangleInfo.getSRV();
	srvs[TEXTURES_SLOT] = m_pTextures;
	srvs[BVH_TREE_SLOT] = m_bvhTree.getSRV();
	srvs[ENVIRONMENT_MAP_SLOT] = m_pEnvironmentMapSRV;
	srvs[OCT_TREE_CPU_SLOT] = m_octTree.getSRV();
	srvs[SPHERE_DATA_SLOT] = m_spheres.getSRV();
	srvs[MESH_ENTITY_DATA_SLOT] = m_meshData.getSRV();
	srvs[DIRECTIONAL_LIGHT_DATA_SLOT] = m_directionalLights.getSRV();
	srvs[POINT_LIGHT_DATA_SLOT] = m_pointLights.getSRV();
	srvs[SPOT_LIGHT_DATA_SLOT] = m_spotLights.getSRV();

	pDevCon->VSSetShaderResources(0u, NUM_T_REGISTERS, srvs);
	pDevCon->PSSetShaderResources(0u, NUM_T_REGISTERS, srvs);
	pDevCon->CSSetShaderResources(0u, NUM_T_REGISTERS, srvs);
	pDevCon->CSSetShaderResources(0u, NUM_T_REGISTERS, srvs);


	// Bind standard resources
	pDevCon->CSSetShader(m_pMainRaytracingCS, nullptr, 0u);
	pDevCon->CSSetUnorderedAccessViews(RESULT_BUFFER_SLOT, 1u, m_pTargetTexture->getUAV(), nullptr);
	pDevCon->CSSetUnorderedAccessViews(ACCUMULATION_BUFFER_SLOT, 1u, m_accumulationTexture.getUAV(), nullptr);
	pDevCon->CSSetConstantBuffers(RENDER_DATA_SLOT, 1u, &m_pRenderDataBuffer);

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

void RayTracer::refitOctTreeNode(OctTreeNode& node)
{
	node.boundingBox = Okay::AABB();
	const entt::registry& reg = m_pScene->getRegistry();

	for (const EntityAABB& entityAABB : node.entities)
	{
		const MeshComponent* pMeshComp = reg.try_get<MeshComponent>(entityAABB.entity);
		const Sphere* pSphereComp = reg.try_get<Sphere>(entityAABB.entity);

		if (pMeshComp || pSphereComp)
		{
			node.boundingBox.growTo(entityAABB.aabb.min);
			node.boundingBox.growTo(entityAABB.aabb.max);
		}
	}
}

static std::chrono::time_point<std::chrono::system_clock> octTreeTimerStart;

void RayTracer::createOctTree(const Scene& scene, uint32_t maxDepth, uint32_t maxLeafObjects)
{
	printf("\nOct Tree build start\n");
	printf("maxDepth: %u\n", maxDepth);

	octTreeTimerStart = std::chrono::system_clock::now();

	std::vector<OctTreeNode> nodes;
	OctTreeNode& root = nodes.emplace_back();

	const entt::registry& reg = scene.getRegistry();
	auto meshView = reg.view<MeshComponent, Transform>();
	auto sphereView = reg.view<Sphere, Transform>();

	root.entities.reserve(meshView.size_hint() + sphereView.size_hint());

	for (entt::entity entity : meshView)
	{
		auto [meshComponent, transform] = meshView[entity];
		const Mesh& mesh = m_pResourceManager->getAsset<Mesh>(meshComponent.meshID);

		Okay::AABB meshBB = mesh.getBoundingBox();
		glm::vec3 center = (meshBB.min + meshBB.max) * 0.5f;
		glm::vec3 extents = (meshBB.min - meshBB.max) * 0.5f;

		DirectX::BoundingBox box;
		box.Center = DirectX::XMFLOAT3(center.x, center.y, center.z);
		box.Extents = DirectX::XMFLOAT3(extents.x, extents.y, extents.z);

		glm::vec3 verticies[8u]{};
		box.GetCorners((DirectX::XMFLOAT3*)verticies); // lmao

		glm::mat4x4 matrix = transform.calculateMatrix();

		for (const glm::vec3& vertex : verticies)
		{
			root.boundingBox.growTo(matrix * glm::vec4(vertex.x, vertex.y, vertex.z, 1.f));
		}

		root.entities.emplace_back(entity, meshBB);
	}

	for (entt::entity entity : sphereView)
	{
		auto [sphereComp, transform] = sphereView[entity];

		Okay::AABB sphereBB = Okay::AABB(glm::vec3(-sphereComp.radius), glm::vec3(sphereComp.radius));
		sphereBB.min += transform.position;
		sphereBB.max += transform.position;

		root.boundingBox.growTo(sphereBB.min);
		root.boundingBox.growTo(sphereBB.max);

		root.entities.emplace_back(entity, sphereBB);
	}

	const glm::vec3 CHILD_BBS_OFFSETS[8] =
	{
		glm::vec3(1.f, -1.f, 1.f),
		glm::vec3(-1.f, -1.f, 1.f),
		glm::vec3(1.f, 1.f, 1.f),
		glm::vec3(-1.f, 1.f, 1.f),

		glm::vec3(1.f, -1.f, -1.f),
		glm::vec3(-1.f, -1.f, -1.f),
		glm::vec3(1.f, 1.f, -1.f),
		glm::vec3(-1.f, 1.f, -1.f),
	};

	struct OctTreeNodeStack
	{
		OctTreeNodeStack() = default;
		OctTreeNodeStack(uint32_t parentNodeIndex, uint32_t nodeIndex, uint32_t depth)
			:parentNodeIndex(parentNodeIndex), nodeIndex(nodeIndex), depth(depth)
		{ }

		uint32_t parentNodeIndex = Okay::INVALID_UINT;
		uint32_t nodeIndex = Okay::INVALID_UINT;
		uint32_t depth = Okay::INVALID_UINT;
	};

	std::stack<OctTreeNodeStack> stack;
	std::vector<EntityAABB> childEntities;
	OctTreeNodeStack nodeStackData;

	stack.push(OctTreeNodeStack(Okay::INVALID_UINT, 0u, 0u));

	while (!stack.empty())
	{
		nodeStackData = stack.top();
		stack.pop();

		OctTreeNode* pNode = &nodes[nodeStackData.nodeIndex];

		if (nodeStackData.depth >= maxDepth || pNode->entities.size() <= maxLeafObjects)
			continue;

		Okay::AABB defaultChildBB = pNode->boundingBox;

		glm::vec3 bbCenter = (defaultChildBB.max + defaultChildBB.min) * 0.5f;

		defaultChildBB.max -= bbCenter;
		defaultChildBB.min -= bbCenter;

		defaultChildBB.min *= 0.5f;
		defaultChildBB.max *= 0.5f;

		defaultChildBB.max += bbCenter;
		defaultChildBB.min += bbCenter;

		glm::vec3 defaultChildBBExtents = (defaultChildBB.max - defaultChildBB.min) * 0.5f;

		for (uint32_t i = 0u; i < 8u; i++)
		{
			Okay::AABB childBB = defaultChildBB;
			for (uint32_t k = 0; k < 3u; k++)
			{
				childBB.max[k] += CHILD_BBS_OFFSETS[i][k] * defaultChildBBExtents[k];
				childBB.min[k] += CHILD_BBS_OFFSETS[i][k] * defaultChildBBExtents[k];
			}

			childEntities.clear(); // Fixes warning C26800 "Use of a moved from object: 'object'."

			for (int k = (int)pNode->entities.size() - 1; k >= 0; k--)
			{
				const EntityAABB& entityAABB = pNode->entities[k];

				if (!Okay::AABB::intersects(childBB, entityAABB.aabb))
					continue;

				childEntities.emplace_back(entityAABB.entity, entityAABB.aabb);
				pNode->entities.erase(pNode->entities.begin() + k);
			}

			if (childEntities.empty())
				continue;

			OctTreeNode& childNode = nodes.emplace_back();
			pNode = &nodes[nodeStackData.nodeIndex]; // Re-get the node incase the 'nodes' vector had to reallocate

			pNode->children[i] = uint32_t(nodes.size() - 1);

			childNode.entities = std::move(childEntities);
			refitOctTreeNode(childNode);

			stack.push(OctTreeNodeStack(nodeStackData.nodeIndex, pNode->children[i], nodeStackData.depth + 1u));
		}
	}

	loadOctTree(nodes);
}

void RayTracer::loadOctTree(const std::vector<OctTreeNode>& nodes)
{
	m_octTreeNodes.resize(nodes.size());

	const entt::registry& reg = m_pScene->getRegistry();

	m_gpuMeshes.clear();

	for (uint32_t i = 0; i < (uint32_t)nodes.size(); i++)
	{
		const OctTreeNode& node = nodes[i];
		GPU_OctTreeNode& gpuNode = m_octTreeNodes[i];

		gpuNode.boundingBox = node.boundingBox;

		memcpy(gpuNode.children, node.children, sizeof(node.children));

		uint32_t numMeshes = 0u;
		for (const EntityAABB& entityAABB : node.entities)
		{
			const MeshComponent* pMeshComp = reg.try_get<MeshComponent>(entityAABB.entity);
			if (!pMeshComp)
				continue;

			GPU_MeshComponent& gpuMesh = m_gpuMeshes.emplace_back();

			const Transform& transform = reg.get<Transform>(entityAABB.entity);

			gpuMesh.triStart = m_meshDescs[pMeshComp->meshID].startIdx;
			gpuMesh.triEnd = m_meshDescs[pMeshComp->meshID].endIdx;

			gpuMesh.boundingBox = m_pResourceManager->getAsset<Mesh>(pMeshComp->meshID).getBoundingBox();

			glm::mat4 transformMatrix = glm::transpose(transform.calculateMatrix());
			gpuMesh.transformMatrix = transformMatrix;
			gpuMesh.inverseTransformMatrix = glm::inverse(transformMatrix);

			gpuMesh.material = pMeshComp->material;
			gpuMesh.bvhNodeStartIdx = m_meshDescs[pMeshComp->meshID].bvhTreeStartIdx;

			numMeshes++;
		}

		//for (entt::entity entity : sphereView)
		//{
		//	auto [sphere, transform] = sphereView[entity];
		//
		//	memcpy(pMappedBufferData, &transform.position, sizeof(glm::vec3));
		//	pMappedBufferData += sizeof(glm::vec3);
		//
		//	memcpy(pMappedBufferData, &sphere, sizeof(Sphere));
		//	pMappedBufferData += sizeof(Sphere);
		//}

		gpuNode.meshesStartIdx = (uint32_t)m_gpuMeshes.size() - numMeshes;
		gpuNode.meshesEndIdx = gpuNode.meshesStartIdx + numMeshes;

		//gpuNode.spheresStartIdx = (uint32_t)m_gpuMeshes.size();
		//gpuNode.spheresEndIdx = gpuNode.spheresStartIdx + (uint32_t)node.entities.size();
	}

	m_meshData.updateRaw((uint32_t)m_gpuMeshes.size(), m_gpuMeshes.data());
	m_octTree.updateRaw((uint32_t)m_octTreeNodes.size(), m_octTreeNodes.data());

	m_renderData.numMeshes = 0u;// (uint32_t)meshView.size_hint();
	m_renderData.numSpheres = 0;// (uint32_t)sphereView.size_hint();
	Okay::updateBuffer(m_pRenderDataBuffer, &m_renderData, sizeof(RenderData));

	std::chrono::duration<float> duration = std::chrono::system_clock::now() - octTreeTimerStart;

	printf("numNodes: %u\nnumEntities: %u\n", (uint32_t)nodes.size(), (uint32_t)reg.alive());
	printf("Oct Tree build time: %.3fms\n", duration.count() * 1000.f);
}