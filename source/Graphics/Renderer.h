#pragma once
#include "DirectX/DX11.h"
#include "Utilities.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"

#include "glm/glm.hpp"

class Scene;
class ResourceManager;

struct RenderData
{
	uint32_t accumulationEnabled = 0;
	uint32_t numAccumulationFrames = 0u;

	uint32_t numSpheres = 0u;
	uint32_t numMeshes = 0u;
	
	glm::uvec2 textureDims{};
	glm::vec2 viewPlaneDims{};

	glm::mat4 cameraInverseProjectionMatrix = glm::mat4(1.f);
	glm::mat4 cameraInverseViewMatrix = glm::mat4(1.f);
	glm::vec3 cameraPosition{};
	float cameraNearZ = 0.f;
};

class Renderer
{
public:
	Renderer();
	Renderer(ID3D11Texture2D* pTarget, Scene* pScene, ResourceManager* pResourceManager);
	~Renderer();

	void shutdown();
	void initiate(ID3D11Texture2D* pTarget, Scene* pScene, ResourceManager* pResourceManager);

	inline void setScene(Scene* pScene);
	inline void setCamera(Entity camera);
	void render();

	inline void toggleAccumulation(bool enable);
	inline void resetAccumulation();
	inline uint32_t getNumAccumulationFrames() const;

	void reloadShaders();
	void loadAssetData();

private: // Scene & Resources
	Scene* m_pScene;
	ResourceManager* m_pResourceManager;

	Entity m_camera;
	void calculateProjectionData();

private: // DX11
	void updateBuffers();

	ID3D11UnorderedAccessView* m_pTargetUAV;
	ID3D11UnorderedAccessView* m_pAccumulationUAV;

	RenderData m_renderData;
	ID3D11Buffer* m_pRenderDataBuffer;

	ID3D11ComputeShader* m_pMainRaytracingCS;

private: // Scene GPU Data
	struct GPUStorage
	{
		ID3D11Buffer* pBuffer = nullptr;
		ID3D11ShaderResourceView* pSRV = nullptr;
		uint32_t capacity = 0u;
		uint32_t gpuElementByteSize = 0u;
	};

	void createGPUStorage(GPUStorage& storage, uint32_t elementSize, uint32_t capacity);
	void shutdownGPUStorage(GPUStorage& storage);

	template<typename Func>
	void updateGPUStorage(GPUStorage& storage, uint32_t resizeCapacity, Func function);


	GPUStorage m_triangleData;
	GPUStorage m_meshData;
	GPUStorage m_spheres;

	ID3D11ShaderResourceView* m_pTextureAtlasSRV;
	GPUStorage m_textureAtlasDesc;

	// The order of m_meshTriangleDesc & m_textureAtlasData matches the respective std::vector in ResourceManager.
	
	// Defines the startIdx (.first) and endIdx (.second) of each mesh for the GPU triangle buffer.
	std::vector<std::pair<uint32_t, uint32_t>> m_meshTriangleDesc;
	
	void loadTriangleData();
	void createTextureAtlas();
};

inline void Renderer::setScene(Scene* pScene)
{
	OKAY_ASSERT(pScene);
	m_pScene = pScene;
}

inline void Renderer::setCamera(Entity camera)
{
	OKAY_ASSERT(camera.isValid());
	OKAY_ASSERT(camera.hasComponents<Camera>());
	m_camera = camera;
}

inline void Renderer::toggleAccumulation(bool enable)
{
	m_renderData.accumulationEnabled = (int)enable;
	resetAccumulation();
}

inline void Renderer::resetAccumulation()
{
	m_renderData.numAccumulationFrames = 0u;

	static const float CLEAR_COLOUR[4] = { 0.f, 0.f, 0.f, 0.f };
	Okay::getDeviceContext()->ClearUnorderedAccessViewFloat(m_pAccumulationUAV, CLEAR_COLOUR);
}

inline uint32_t Renderer::getNumAccumulationFrames() const
{
	return m_renderData.numAccumulationFrames;
}

template<typename Func>
void Renderer::updateGPUStorage(GPUStorage& storage, uint32_t resizeCapacity, Func function)
{
	if (storage.capacity < resizeCapacity)
		createGPUStorage(storage, storage.gpuElementByteSize, resizeCapacity + 10u);
	
	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();
	D3D11_MAPPED_SUBRESOURCE sub{};
	if (FAILED(pDevCon->Map(storage.pBuffer, 0u, D3D11_MAP_WRITE_DISCARD, 0u, &sub)))
		return;

	function((char*)sub.pData);

	pDevCon->Unmap(storage.pBuffer, 0u);
}