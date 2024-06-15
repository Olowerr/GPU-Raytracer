#pragma once
#include "Utilities.h"
#include "GPUStorage.h"

#include "glm/glm.hpp"

#include <vector>

class Entity;
class ResourceManager;
struct MeshComponent;

// Defines the start & end triangle index for a mesh in the vertex buffer, as well as the index of the root node in m_bvhTree
struct MeshDesc
{
	uint32_t startIdx;
	uint32_t endIdx;
	uint32_t bvhTreeStartIdx;
	uint32_t numBvhNodes;
};

struct GPUNode
{
	Okay::AABB boundingBox;
	uint32_t triStart = Okay::INVALID_UINT, triEnd = Okay::INVALID_UINT;
	uint32_t childIdxs[2]{ Okay::INVALID_UINT, Okay::INVALID_UINT };
	uint32_t parentIdx = Okay::INVALID_UINT;
};

class GPUResourceManager
{
public:
	GPUResourceManager();
	GPUResourceManager(const ResourceManager& resourceManager);
	~GPUResourceManager();

	void shutdown();
	void initiate(const ResourceManager& resourceManager);

	void loadResources(std::string_view environmentMapPath = "");

	void loadMeshAndBvhData();
	inline uint32_t& getMaxBvhLeafTriangles();
	inline uint32_t& getMaxBvhDepth();

	inline const ResourceManager& getResourceManager() const;
	inline const std::vector<MeshDesc>& getMeshDescriptors() const;

	inline const std::vector<GPUNode>& getBvhTreeNodes() const;

	void bindResources() const;

	inline const GPUStorage& getTrianglesPos() const;
	inline const GPUStorage& getTrianglesInfo() const;

	uint32_t getGlobalNodeIdx(const MeshComponent& meshComp, uint32_t localNodeIdx) const;

private:
	const ResourceManager* m_pResourceManager;

	GPUStorage m_trianglePositions;
	GPUStorage m_triangleInfo;

	GPUStorage m_bvhTree;
	uint32_t m_maxBvhLeafTriangles;
	uint32_t m_maxBvhDepth;
	std::vector<GPUNode> m_bvhTreeNodes;

	// The order of m_textureAtlasData & m_meshDescs matches the respective std::vector in ResourceManager.
	ID3D11ShaderResourceView* m_pTextureAtlasSRV;
	GPUStorage m_textureAtlasDesc;

	ID3D11ShaderResourceView* m_pEnvironmentMapSRV;

	std::vector<MeshDesc> m_meshDescs;

private:
	void loadTextureData();
	void loadEnvironmentMap(std::string_view path);
};

inline uint32_t& GPUResourceManager::getMaxBvhLeafTriangles()	{ return m_maxBvhLeafTriangles; }
inline uint32_t& GPUResourceManager::getMaxBvhDepth()			{ return m_maxBvhDepth; }

inline const ResourceManager& GPUResourceManager::getResourceManager() const		{ return *m_pResourceManager; }
inline const std::vector<MeshDesc>& GPUResourceManager::getMeshDescriptors() const	{ return m_meshDescs; }
inline const std::vector<GPUNode>& GPUResourceManager::getBvhTreeNodes() const		{ return m_bvhTreeNodes; }

inline const GPUStorage& GPUResourceManager::getTrianglesPos() const { return m_trianglePositions; }
inline const GPUStorage& GPUResourceManager::getTrianglesInfo() const { return m_triangleInfo; }