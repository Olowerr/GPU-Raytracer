#pragma once
#include "DirectX/DX11.h"
#include "Utilities.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "GPUStorage.h"
#include "DirectX/RenderTexture.h"

#include "glm/glm.hpp"

class Scene;
class GPUResourceManager;
class ResourceManager;

class RayTracer
{
public:
	RayTracer();
	RayTracer(const RenderTexture& target, const GPUResourceManager& pGpuResourceManager);
	~RayTracer();

	void shutdown();
	void initiate(const RenderTexture& target, const GPUResourceManager& pGpuResourceManager);

	inline void setScene(const Scene& pScene);
	void render();

	inline void toggleAccumulation(bool enable);
	inline void resetAccumulation();
	inline uint32_t getNumAccumulationFrames() const;

	void reloadShaders();

	inline float& getDOFStrength();
	inline float& getDOFDistance();

	void onResize();

private: // Scene & Resources
	const Scene* m_pScene;
	const GPUResourceManager* m_pGpuResourceManager;
	const ResourceManager* m_pResourceManager;

	void calculateProjectionData();

private: // DX11
	struct RenderData // Aligned 16
	{
		uint32_t accumulationEnabled = 1u;
		uint32_t numAccumulationFrames = 0u;

		uint32_t numSpheres = 0u;
		uint32_t numMeshes = 0u;

		glm::uvec2 textureDims{};
		glm::vec2 viewPlaneDims{};

		glm::mat4 cameraInverseProjectionMatrix = glm::mat4(1.f);
		glm::mat4 cameraInverseViewMatrix = glm::mat4(1.f);
		glm::vec3 cameraPosition{};
		float cameraNearZ = 0.f;

		glm::vec3 cameraUpDir;
		float dofStrength = 0.f;
		glm::vec3 cameraRightDir;
		float dofDistance = 0.f;
	};

	void updateBuffers();

	const RenderTexture* m_pTargetTexture;
	RenderTexture m_accumulationTexture;

	RenderData m_renderData;
	ID3D11Buffer* m_pRenderDataBuffer;

	ID3D11ComputeShader* m_pMainRaytracingCS;

private: // Scene GPU Data
	GPUStorage m_meshData;
	GPUStorage m_spheres;
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
