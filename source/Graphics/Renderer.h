#pragma once
#include "DirectX/DX11.h"
#include "Utilities.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"

#include "glm/glm.hpp"

class Scene;

struct RenderData
{
	uint32_t accumulationEnabled = 0;
	uint32_t numAccumulationFrames = 0u;
	uint32_t numSpheres = 0u;
	float padding0;
	
	glm::uvec2 textureDims{};
	glm::vec2 viewPlaneDims{};

	glm::mat4 cameraInverseProjectionMatrix = glm::mat4(1.f);
	glm::mat4 cameraInverseViewMatrix = glm::mat4(1.f);
	glm::vec3 cameraPosition{};
	float cameraNearZ;
};

class Renderer
{
public:
	Renderer();
	Renderer(ID3D11Texture2D* pTarget, Scene* pScene);
	~Renderer();

	void shutdown();
	void initiate(ID3D11Texture2D* pTarget, Scene* pScene);

	inline void setScene(Scene* pScene);
	inline void setCamera(Entity camera);
	void render();

	inline void toggleAccumulation(bool enable);
	inline void resetAccumulation();
	inline uint32_t getNumAccumulationFrames() const;

	void reloadShaders();

private: // Scene
	Scene* m_pScene;
	Entity m_camera;

	void calculateProjectionData();

private: // DX11
	void updateBuffers();

	ID3D11UnorderedAccessView* m_pTargetUAV;

	ID3D11UnorderedAccessView* m_pAccumulationUAV;

	RenderData m_renderData;
	ID3D11Buffer* m_pRenderDataBuffer;

	ID3D11ComputeShader* m_pMainRaytracingCS;

	ID3D11Buffer* m_pSphereDataBuffer;
	ID3D11ShaderResourceView* m_pSphereDataSRV;
	uint32_t m_sphereBufferCapacity;
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
