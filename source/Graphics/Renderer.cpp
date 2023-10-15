#include "Renderer.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"
#include "shaders/ShaderResourceRegisters.h"
#include "ResourceManager.h"
#include "BvhBuilder.h"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/quaternion.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include <execution>

Renderer::Renderer()
	:m_pTargetUAV(nullptr), m_pMainRaytracingCS(nullptr), m_pScene(nullptr), m_renderData(),
	m_pAccumulationUAV(nullptr), m_pRenderDataBuffer(nullptr), m_pResourceManager(nullptr),
	m_pTextureAtlasSRV(nullptr), m_maxBvhLeafTriangles(100u), m_maxBvhDepth(100u)
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
	DX11_RELEASE(m_pTextureAtlasSRV);
	
	shutdownGPUStorage(m_meshData);
	shutdownGPUStorage(m_spheres);
	shutdownGPUStorage(m_triangleData);
	shutdownGPUStorage(m_textureAtlasDesc);
	shutdownGPUStorage(m_bvhTree);
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
	// m_bvhTree created in Renderer::loadTriangleData().
	// m_textureAtlasData created in Renderer::createTextureAtlas().



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
		success = SUCCEEDED(pDevice->CreateSamplerState(&simpDesc, &pSimp));
		OKAY_ASSERT(success);
		Okay::getDeviceContext()->CSSetSamplers(0u, 1u, &pSimp);
		DX11_RELEASE(pSimp);
	}
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
	pDevCon->CSSetShaderResources(TEXTURE_ATLAS_DESC_CPU_SLOT, 1u, &m_textureAtlasDesc.pSRV);
	pDevCon->CSSetShaderResources(TEXTURE_ATLAS_CPU_SLOT, 1u, &m_pTextureAtlasSRV);
	pDevCon->CSSetShaderResources(BVH_TREE_CPU_SLOT, 1u, &m_bvhTree.pSRV);

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
	struct GPUNode
	{
		Okay::AABB boundingBox;
		uint32_t triStart = Okay::INVALID_UINT, triEnd = Okay::INVALID_UINT;
		uint32_t childIdxs[2] { Okay::INVALID_UINT, Okay::INVALID_UINT };
		uint32_t parentIdx = Okay::INVALID_UINT;
	};

	const std::vector<Mesh>& meshes = m_pResourceManager->getAll<Mesh>();
	const uint32_t numMeshes = (uint32_t)meshes.size();

	m_meshDescs.resize(numMeshes);

	uint32_t numTotalTriangles = 0u;
	for (uint32_t i = 0; i < numMeshes; i++)
		numTotalTriangles += (uint32_t)meshes[i].getTriangles().size();

	createGPUStorage(m_triangleData, sizeof(Okay::Triangle), numTotalTriangles);

	uint32_t triBufferCurStartIdx = 0;
	std::vector<GPUNode> gpuNodes;

	updateGPUStorage(m_triangleData, 0u, [&](char* pMappedBufferData)
	{
		Okay::Triangle* pTriWriteLocation = (Okay::Triangle*)pMappedBufferData;
		BvhBuilder bvhBuilder(m_maxBvhLeafTriangles, m_maxBvhDepth);

		uint32_t numMeshes = (uint32_t)meshes.size();

		for (uint32_t i = 0; i < numMeshes; i++)
		{
			const Mesh& mesh = meshes[i];
			const std::vector<Okay::Triangle>& meshTris = mesh.getTriangles();

			bvhBuilder.buildTree(mesh);
			const std::vector<BvhNode>& nodes = bvhBuilder.getTree();

			uint32_t numNodes = (uint32_t)nodes.size();
			uint32_t gpuPrevSize = (uint32_t)gpuNodes.size();

			gpuNodes.resize(gpuPrevSize + numNodes);

			uint32_t localTriStart = 0u;
			for (uint32_t k = 0; k < numNodes; k++)
			{
				GPUNode& gpuNode = gpuNodes[gpuPrevSize + k];
				const BvhNode& bvhNode = nodes[k];
				
				uint32_t numTriIndicies = (uint32_t)bvhNode.triIndicies.size();

				gpuNode.boundingBox = bvhNode.boundingBox;

				gpuNode.childIdxs[0] = bvhNode.childIdxs[0];
				gpuNode.childIdxs[1] = bvhNode.childIdxs[1];
				gpuNode.parentIdx = bvhNode.parentIdx;

				if (!bvhNode.isLeaf())
					continue;

				gpuNode.triStart = triBufferCurStartIdx + localTriStart;
				gpuNode.triEnd = gpuNode.triStart + numTriIndicies;

				localTriStart += numTriIndicies;

				for (uint32_t j = 0; j < numTriIndicies; j++)
				{
					pTriWriteLocation[j] = meshTris[bvhNode.triIndicies[j]];
				}
				pTriWriteLocation += numTriIndicies;
			}

			m_meshDescs[i].bvhTreeStartIdx = gpuPrevSize;
			m_meshDescs[i].startIdx = triBufferCurStartIdx;
			m_meshDescs[i].endIdx = triBufferCurStartIdx + (uint32_t)meshTris.size();

			triBufferCurStartIdx += (uint32_t)meshTris.size();
		}
	});

	createGPUStorage(m_bvhTree, sizeof(GPUNode), (uint32_t)gpuNodes.size());
	updateGPUStorage(m_bvhTree, 0u, [&](char* pMappedBufferData)
	{
		memcpy(pMappedBufferData, gpuNodes.data(), sizeof(GPUNode) * gpuNodes.size());
	});
}

void Renderer::loadTextureData()
{
	static const uint32_t CHANNELS = STBI_rgb_alpha;
	static const uint32_t SPACING = 0u;

	const std::vector<Texture>& textures = m_pResourceManager->getAll<Texture>();
	const size_t numTextures = textures.size();
	if (!numTextures)
		return;

	std::vector<uint32_t> xPositions;
	xPositions.resize(numTextures, 0u);

	/*
	* TODO: Outline the textures with the edge colours for n-pixels.
	* This could remove bleeding between textures or empty areas.
	* Need to adjust texture atlas width and make sure they UVs still start at the orignal texture positions.
	* Only need to outline sides of textures with nothing next to it, e.g. no outline needed for top side.
	* Can maybe rework SPACING into OUTLINE_THICKNESS or something, don't think SPACING will be necessary after this change.
	*/ 

	uint32_t totWidth = 0u, maxHeight = 0u;
	for (size_t i = 0; i < numTextures; i++)
	{
		totWidth += textures[i].getWidth() + (i > 0u ? SPACING : 0u);
		if (textures[i].getHeight() > maxHeight)
			maxHeight = textures[i].getHeight();

		if (i > 0)
			xPositions[i] = xPositions[i - 1] + textures[i - 1].getWidth() + SPACING;
	}

	unsigned char* pResultData = new unsigned char[totWidth * maxHeight * CHANNELS]{};
	const uint32_t rowPitch = totWidth * CHANNELS;

	unsigned char* coursor = nullptr;
	for (size_t i = 0; i < numTextures; i++)
	{
		for (uint32_t y = 0; y < textures[i].getHeight(); y++)
		{
			coursor = pResultData + xPositions[i] * CHANNELS + y * rowPitch;
			memcpy(coursor, textures[i].getTextureData() + y * textures[i].getWidth() * CHANNELS, textures[i].getWidth() * CHANNELS);
		}
		stbi_image_free(textures[i].getTextureData());
	}

	// For debugging
	//stbi_write_png("TextureAtlas.png", totWidth, maxHeight, 4, pResultData, rowPitch);
	
	bool success = Okay::createSRVFromTextureData(&m_pTextureAtlasSRV, pResultData, totWidth, maxHeight);
	delete[] pResultData;
	OKAY_ASSERT(success);
	
	/*
	* Need for each texture:
	* UV offset
	* Ratio between texture size and atlas size
	* 
	* In Shader:
	* Find ratio & offset based on TextureIdx
	* Multiply UV Coordinate by ratio and apply offset
	*/

	glm::vec2 inverseAtlasDims = 1.f / glm::vec2((float)totWidth, (float)maxHeight);
	createGPUStorage(m_textureAtlasDesc, sizeof(std::pair<glm::vec2, glm::vec2>), (uint32_t)numTextures);

	updateGPUStorage(m_textureAtlasDesc, 0u, [&](char* pMappedBufferData)
	{
		using Vec2Pair = std::pair<glm::vec2, glm::vec2>;
		Vec2Pair* pointer = (Vec2Pair*)pMappedBufferData;

		for (size_t i = 0; i < numTextures; i++)
		{
			glm::vec2 textureDims((float)textures[i].getWidth(), (float)textures[i].getHeight());
			glm::vec2 texturePos((float)xPositions[i], 0.f); // All textures on line until fancier atlas generation

			// Defines ratio (first) and offset (second) of each individual texture in the textureAtlas
			pointer->first = textureDims * inverseAtlasDims;
			pointer->second = texturePos * inverseAtlasDims;

			pointer += 1u;
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


	// Inverse Projection Matrix // Only used in Cherno way
	m_renderData.cameraInverseProjectionMatrix = glm::transpose(glm::inverse(
		glm::perspectiveFovLH(glm::radians(camData.fov), windowDimsVec.x, windowDimsVec.y, camData.nearZ, camData.farZ)));


	// Camera view vectors
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

