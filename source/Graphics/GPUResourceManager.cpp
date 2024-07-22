#include "GPUResourceManager.h"
#include "Utilities.h"
#include "BvhBuilder.h"
#include "ResourceManager.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"
#include "shaders/ShaderResourceRegisters.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

GPUResourceManager::GPUResourceManager()
	:m_pResourceManager(nullptr), m_pEnvironmentMapSRV(nullptr), m_pTextures(nullptr),
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

	m_trianglePositions.shutdown();
	m_triangleInfo.shutdown();
	m_bvhTree.shutdown();

	DX11_RELEASE(m_pTextures);

	DX11_RELEASE(m_pEnvironmentMapSRV);
}

void GPUResourceManager::initiate(const ResourceManager& resourceManager)
{
	shutdown();

	m_pResourceManager = &resourceManager;

	m_maxBvhLeafTriangles = 30u;
	m_maxBvhDepth = 30u;
}

void GPUResourceManager::bindResources() const
{
	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();

	pDevCon->ClearState();

	ID3D11ShaderResourceView* srvs[5u]{};
	srvs[RM_TRIANGLE_POS_SLOT] = m_trianglePositions.getSRV();
	srvs[RM_TRIANGLE_INFO_SLOT] = m_triangleInfo.getSRV();
	srvs[RM_TEXTURES_SLOT] = m_pTextures;
	srvs[RM_BVH_TREE_SLOT] = m_bvhTree.getSRV();
	srvs[RM_ENVIRONMENT_MAP_SLOT] = m_pEnvironmentMapSRV;

	pDevCon->VSSetShaderResources(RM_TRIANGLE_POS_SLOT, sizeof(srvs) / sizeof(srvs[0]), srvs);
	pDevCon->PSSetShaderResources(RM_TRIANGLE_POS_SLOT, sizeof(srvs) / sizeof(srvs[0]), srvs);
	pDevCon->CSSetShaderResources(RM_TRIANGLE_POS_SLOT, sizeof(srvs) / sizeof(srvs[0]), srvs);

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
		Okay::getDeviceContext()->VSSetSamplers(0u, 1u, &pSimp);
		Okay::getDeviceContext()->PSSetSamplers(0u, 1u, &pSimp);
		Okay::getDeviceContext()->CSSetSamplers(0u, 1u, &pSimp);
		DX11_RELEASE(pSimp);
	}
}

void GPUResourceManager::loadResources(std::string_view environmentMapPath)
{
	loadTextureData();
	loadEnvironmentMap(environmentMapPath);
	loadMeshAndBvhData();
	bindResources();
}

uint32_t GPUResourceManager::getGlobalNodeIdx(const MeshComponent& pMeshComp, uint32_t localNodeIdx) const
{
	const MeshDesc& desc = m_meshDescs[pMeshComp.meshID];
	return desc.bvhTreeStartIdx + localNodeIdx;
}

inline uint32_t tryOffsetIdx(uint32_t idx, uint32_t offset)
{
	return idx == Okay::INVALID_UINT ? idx : idx + offset;
}

void GPUResourceManager::loadMeshAndBvhData()
{
	const std::vector<Mesh>& meshes = m_pResourceManager->getAll<Mesh>();
	const uint32_t numMeshes = (uint32_t)meshes.size();

	if (!numMeshes)
		return;

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

void GPUResourceManager::loadTextureData()
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
