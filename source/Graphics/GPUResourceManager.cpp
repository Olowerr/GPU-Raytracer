#include "GPUResourceManager.h"
#include "Utilities.h"
#include "BvhBuilder.h"
#include "ResourceManager.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"
#include "shaders/ShaderResourceRegisters.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

GPUResourceManager::GPUResourceManager()
	:m_pResourceManager(nullptr), m_pTextureAtlasSRV(nullptr), m_pEnvironmentMapSRV(nullptr),
	m_maxBvhLeafTriangles(0u), m_maxBvhDepth(0u)
{
}

GPUResourceManager::GPUResourceManager(const ResourceManager& resourceManager)
{
	initiate(resourceManager);
}

GPUResourceManager::~GPUResourceManager()
{
	shutdown();
}

void GPUResourceManager::shutdown()
{
	m_pResourceManager = nullptr;

	m_triangleData.shutdown();
	m_bvhTree.shutdown();

	DX11_RELEASE(m_pTextureAtlasSRV);
	m_textureAtlasDesc.shutdown();

	DX11_RELEASE(m_pEnvironmentMapSRV);
}

void GPUResourceManager::initiate(const ResourceManager& resourceManager)
{
	shutdown();

	m_pResourceManager = &resourceManager;

	m_maxBvhLeafTriangles = 100u;
	m_maxBvhDepth = 100u;

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
		bool success = SUCCEEDED(Okay::getDevice()->CreateSamplerState(&simpDesc, &pSimp));
		OKAY_ASSERT(success);
		Okay::getDeviceContext()->CSSetSamplers(0u, 1u, &pSimp);
		DX11_RELEASE(pSimp);
	}
}

void GPUResourceManager::bindResources()
{
	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();

	ID3D11ShaderResourceView* srvs[5u]{};
	srvs[RM_TRIANGLE_DATA_SLOT]			= m_triangleData.getSRV();
	srvs[RM_TEXTURE_ATLAS_DESC_SLOT]	= m_textureAtlasDesc.getSRV();
	srvs[RM_TEXTURE_ATLAS_SLOT]			= m_pTextureAtlasSRV;
	srvs[RM_BVH_TREE_SLOT]				= m_bvhTree.getSRV();
	srvs[RM_ENVIRONMENT_MAP_SLOT]		= m_pEnvironmentMapSRV;

	pDevCon->VSSetShaderResources(RM_TRIANGLE_DATA_SLOT, 5u, srvs);
	pDevCon->PSSetShaderResources(RM_TRIANGLE_DATA_SLOT, 5u, srvs);
	pDevCon->CSSetShaderResources(RM_TRIANGLE_DATA_SLOT, 5u, srvs);
}

void GPUResourceManager::loadResources(std::string_view environmentMapPath)
{
	loadMeshAndBvhData();
	loadTextureData();
	loadEnvironmentMap(environmentMapPath);
	bindResources();
}

inline uint32_t tryOffsetIdx(uint32_t idx, uint32_t offset)
{
	return idx == Okay::INVALID_UINT ? idx : idx + offset;
}

void GPUResourceManager::loadMeshAndBvhData()
{
	struct GPUNode // Move this
	{
		Okay::AABB boundingBox;
		uint32_t triStart = Okay::INVALID_UINT, triEnd = Okay::INVALID_UINT;
		uint32_t childIdxs[2]{ Okay::INVALID_UINT, Okay::INVALID_UINT };
		uint32_t parentIdx = Okay::INVALID_UINT;
	};

	const std::vector<Mesh>& meshes = m_pResourceManager->getAll<Mesh>();
	const uint32_t numMeshes = (uint32_t)meshes.size();

	m_meshDescs.resize(numMeshes);

	uint32_t numTotalTriangles = 0u;
	for (uint32_t i = 0; i < numMeshes; i++)
	{
		numTotalTriangles += (uint32_t)meshes[i].getTriangles().size();
	}

	uint32_t triBufferCurStartIdx = 0;
	std::vector<GPUNode> gpuNodes;
	std::vector<Okay::Triangle> gpuTriangles;
	gpuTriangles.reserve(numTotalTriangles);

	BvhBuilder bvhBuilder(m_maxBvhLeafTriangles, m_maxBvhDepth);

	for (uint32_t i = 0; i < numMeshes; i++)
	{
		const Mesh& mesh = meshes[i];
		const std::vector<Okay::Triangle>& meshTris = mesh.getTriangles();

		bvhBuilder.buildTree(mesh);
		const std::vector<BvhNode>& nodes = bvhBuilder.getTree();

		const uint32_t numNodes = (uint32_t)nodes.size();
		const uint32_t gpuNodesPrevSize = (uint32_t)gpuNodes.size();

		gpuNodes.resize(gpuNodesPrevSize + numNodes);

		uint32_t localTriStart = 0u;
		for (uint32_t k = 0; k < numNodes; k++)
		{
			GPUNode& gpuNode = gpuNodes[gpuNodesPrevSize + k];
			const BvhNode& bvhNode = nodes[k];

			const uint32_t numTriIndicies = (uint32_t)bvhNode.triIndicies.size();

			gpuNode.boundingBox = bvhNode.boundingBox;

			gpuNode.childIdxs[0] = tryOffsetIdx(bvhNode.childIdxs[0], gpuNodesPrevSize);
			gpuNode.childIdxs[1] = tryOffsetIdx(bvhNode.childIdxs[1], gpuNodesPrevSize);
			gpuNode.parentIdx = tryOffsetIdx(bvhNode.parentIdx, gpuNodesPrevSize);

			if (!bvhNode.isLeaf())
				continue;

			gpuNode.triStart = triBufferCurStartIdx + localTriStart;
			gpuNode.triEnd = gpuNode.triStart + numTriIndicies;

			localTriStart += numTriIndicies;

			for (uint32_t j = 0; j < numTriIndicies; j++)
			{
				gpuTriangles.emplace_back(meshTris[bvhNode.triIndicies[j]]);
			}
		}

		m_meshDescs[i].bvhTreeStartIdx = gpuNodesPrevSize;
		m_meshDescs[i].startIdx = triBufferCurStartIdx;
		m_meshDescs[i].endIdx = triBufferCurStartIdx + (uint32_t)meshTris.size();

		triBufferCurStartIdx += (uint32_t)meshTris.size();
	}

	m_triangleData.initiate(sizeof(Okay::Triangle), numTotalTriangles, gpuTriangles.data());
	m_bvhTree.initiate(sizeof(GPUNode), (uint32_t)gpuNodes.size(), gpuNodes.data());
}

void GPUResourceManager::loadTextureData()
{
	static const uint32_t CHANNELS = STBI_rgb_alpha;
	static const uint32_t SPACING = 0u;

	const std::vector<Texture>& textures = m_pResourceManager->getAll<Texture>();
	const uint32_t numTextures = (uint32_t)textures.size();
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
	for (uint32_t i = 0; i < numTextures; i++)
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
	for (uint32_t i = 0; i < numTextures; i++)
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

	using Vec2Pair = std::pair<glm::vec2, glm::vec2>;
	m_textureAtlasDesc.initiate(sizeof(Vec2Pair), numTextures, nullptr);

	m_textureAtlasDesc.update(0u, [&](char* pMappedBufferData)
	{
		Vec2Pair* writeLocation = (Vec2Pair*)pMappedBufferData;

		for (size_t i = 0; i < numTextures; i++)
		{
			glm::vec2 textureDims((float)textures[i].getWidth(), (float)textures[i].getHeight());
			glm::vec2 texturePos((float)xPositions[i], 0.f); // All textures on line until fancier atlas generation

			// Defines ratio (first) and offset (second) of each individual texture in the textureAtlas
			writeLocation->first = textureDims * inverseAtlasDims;
			writeLocation->second = texturePos * inverseAtlasDims;

			writeLocation += 1u;
		}
	});
}

void GPUResourceManager::loadEnvironmentMap(std::string_view path)
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
