#pragma once
#include "DirectX/DX11.h"
#include "Utilities.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "GPUStorage.h"
#include "DirectX/RenderTexture.h"

#include "glm/glm.hpp"

class Scene;
class ResourceManager;

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
	uint32_t triStart = Okay::INVALID_UINT;
	uint32_t triEnd = Okay::INVALID_UINT;
	uint32_t firstChildIdx = Okay::INVALID_UINT;
};

struct EntityAABB
{
	EntityAABB(entt::entity entity, Okay::AABB aabb)
		:entity(entity), aabb(aabb) { }

	entt::entity entity;
	Okay::AABB aabb;
};

struct OctTreeNode
{
	Okay::AABB boundingBox;
	std::vector<EntityAABB> entities;
	uint32_t children[8u]
	{
		Okay::INVALID_UINT,
		Okay::INVALID_UINT,
		Okay::INVALID_UINT,
		Okay::INVALID_UINT,
		Okay::INVALID_UINT,
		Okay::INVALID_UINT,
		Okay::INVALID_UINT,
		Okay::INVALID_UINT,
	};
};

class RayTracer
{
public:
	enum DebugDisplayMode : uint32_t
	{
		None = 0,
		BBCheckCount = 1,
		TriCheckCount= 2,
	};

public:
	RayTracer();
	RayTracer(const RenderTexture& target, const ResourceManager& resourceManager, std::string_view environmentMapPath = "");
	~RayTracer();

	void shutdown();
	void initiate(const RenderTexture& target, const ResourceManager& resourceManager, std::string_view environmentMapPath = "");

	void loadMeshAndBvhData(uint32_t maxDepth, uint32_t maxLeafTriangles);
	void createOctTree(const Scene& scene, uint32_t maxDepth, uint32_t maxLeafObjects);

	inline const std::vector<MeshDesc>& getMeshDescriptors() const;
	inline const std::vector<GPUNode>& getBvhTreeNodes() const;

	inline const GPUStorage& getTrianglesPos() const;
	inline const GPUStorage& getTrianglesInfo() const;

	uint32_t getGlobalNodeIdx(const MeshComponent& meshComp, uint32_t localNodeIdx) const;

	inline void setScene(const Scene& pScene);
	void render();

	inline void toggleAccumulation(bool enable);
	inline void resetAccumulation();
	inline uint32_t getNumAccumulationFrames() const;

	void reloadShaders();

	inline float& getDOFStrength();
	inline float& getDOFDistance();

	void onResize();

	inline void setDebugMode(DebugDisplayMode mode);
	inline uint32_t& getDebugMaxCount();

private: // Scene & Resources
	const Scene* m_pScene;
	const ResourceManager* m_pResourceManager;

	void calculateProjectionData();
	void loadTextureData();
	void loadEnvironmentMap(std::string_view path);
	void loadOctTree(const std::vector<OctTreeNode>& nodes);
	void refitOctTreeNode(OctTreeNode& node);

private: // Main DX11
	struct RenderData // Aligned 16
	{
		uint32_t accumulationEnabled = 1u;
		uint32_t numAccumulationFrames = 0u;

		uint32_t numSpheres = 0u;
		uint32_t numMeshes = 0u;

		uint32_t numDirLights = 0u;
		uint32_t numPointLights = 0u;
		uint32_t numSpotLights = 0u;
		DebugDisplayMode debugMode = DebugDisplayMode::None;

		glm::uvec2 textureDims{};
		glm::vec2 viewPlaneDims{};

		glm::mat4 cameraInverseProjectionMatrix = glm::mat4(1.f);
		glm::mat4 cameraInverseViewMatrix = glm::mat4(1.f);
		glm::vec3 cameraPosition{};
		float cameraNearZ = 0.f;

		glm::vec3 cameraUpDir = glm::vec3(0.f);
		float dofStrength = 0.f;
		glm::vec3 cameraRightDir = glm::vec3(0.f);
		float dofDistance = 0.f;

		uint32_t debugMaxCount = 500;
		glm::vec3 pad0;
	};

	void updateBuffers();

	const RenderTexture* m_pTargetTexture;
	RenderTexture m_accumulationTexture;

	RenderData m_renderData;
	ID3D11Buffer* m_pRenderDataBuffer;

	ID3D11ComputeShader* m_pMainRaytracingCS;

private: // DX11 Resources
	GPUStorage m_trianglePositions;
	GPUStorage m_triangleInfo;

	GPUStorage m_bvhTree;
	std::vector<GPUNode> m_bvhTreeNodes;

	// The order of m_textureAtlasData & m_meshDescs matches the respective std::vector in ResourceManager.
	ID3D11ShaderResourceView* m_pTextures;

	ID3D11ShaderResourceView* m_pEnvironmentMapSRV;

	std::vector<MeshDesc> m_meshDescs;
	std::vector<GPU_MeshComponent> m_gpuMeshes;

	GPUStorage m_octTree;

	void bindResources() const;

private: // Scene Entities GPU Data
	GPUStorage m_meshData;
	GPUStorage m_spheres;
	GPUStorage m_directionalLights;
	GPUStorage m_pointLights;
	GPUStorage m_spotLights;
};

inline void RayTracer::setScene(const Scene& scene) { m_pScene = &scene; }

inline void RayTracer::toggleAccumulation(bool enable)
{
	m_renderData.accumulationEnabled = (uint32_t)enable;
	resetAccumulation();
}

inline void RayTracer::resetAccumulation()
{
	m_renderData.numAccumulationFrames = 0u;

	static const float CLEAR_COLOUR[4] = { 0.f, 0.f, 0.f, 0.f };
	Okay::getDeviceContext()->ClearUnorderedAccessViewFloat(*m_accumulationTexture.getUAV(), CLEAR_COLOUR);
}

inline uint32_t RayTracer::getNumAccumulationFrames() const { return m_renderData.numAccumulationFrames; }

inline float& RayTracer::getDOFStrength() { return m_renderData.dofStrength; }
inline float& RayTracer::getDOFDistance() { return m_renderData.dofDistance; }

inline void RayTracer::setDebugMode(RayTracer::DebugDisplayMode mode) { m_renderData.debugMode = mode; }
inline uint32_t& RayTracer::getDebugMaxCount() { return m_renderData.debugMaxCount; }

inline const std::vector<MeshDesc>& RayTracer::getMeshDescriptors() const { return m_meshDescs; }
inline const std::vector<GPUNode>& RayTracer::getBvhTreeNodes() const { return m_bvhTreeNodes; }

inline const GPUStorage& RayTracer::getTrianglesPos() const { return m_trianglePositions; }
inline const GPUStorage& RayTracer::getTrianglesInfo() const { return m_triangleInfo; }

inline uint32_t RayTracer::getGlobalNodeIdx(const MeshComponent& meshComp, uint32_t localNodeIdx) const
{
	const MeshDesc& desc = m_meshDescs[meshComp.meshID];
	return desc.bvhTreeStartIdx + localNodeIdx;
}
