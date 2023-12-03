#pragma once

#include "GPUStorage.h"

#include "glm/glm.hpp"

#include <vector>

class Entity;
class ResourceManager;

// Defines the start & end vertex index for a mesh in the vertex buffer, as well as the index of the root node in m_bvhTree
struct MeshDesc
{
	uint32_t startIdx;
	uint32_t endIdx;
	uint32_t bvhTreeStartIdx;
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

private:
	const ResourceManager* m_pResourceManager;

	GPUStorage m_triangleData;

	GPUStorage m_bvhTree;
	uint32_t m_maxBvhLeafTriangles;
	uint32_t m_maxBvhDepth;

	// The order of m_textureAtlasData & m_meshDescs matches the respective std::vector in ResourceManager.
	ID3D11ShaderResourceView* m_pTextureAtlasSRV;
	GPUStorage m_textureAtlasDesc;

	ID3D11ShaderResourceView* m_pEnvironmentMapSRV;

	std::vector<MeshDesc> m_meshDescs;

private:
	void loadTextureData();
	void loadEnvironmentMap(std::string_view path);
	void bindResources();
};

inline uint32_t& GPUResourceManager::getMaxBvhLeafTriangles()	{ return m_maxBvhLeafTriangles; }
inline uint32_t& GPUResourceManager::getMaxBvhDepth()			{ return m_maxBvhDepth; }

inline const ResourceManager& GPUResourceManager::getResourceManager() const		{ return *m_pResourceManager; }
inline const std::vector<MeshDesc>& GPUResourceManager::getMeshDescriptors() const	{ return m_meshDescs; }