#pragma once
#include "DirectX/DX11.h"
#include "Utilities.h"

#include "glm/glm.hpp"

#include <vector>
#include <random>

class Scene;

struct RenderData
{
	uint32_t m_accumulationEnabled = 0;
	uint32_t m_numAccumulationFrames = 0u;
	uint32_t m_numSpheres = 0u;
	float padding0 = 0.f;
	glm::uvec2 m_textureDims{};
	float padding1[2]{};
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
	void render();

	inline void toggleAccumulation(bool enable);
	inline void resetAccumulation();

private: // Scene
	Scene* m_pScene;

private: // DX11
	ID3D11UnorderedAccessView* m_pTargetUAV;

	ID3D11UnorderedAccessView* m_pAccumulationUAV;

	RenderData m_renderData;
	ID3D11Buffer* m_pRenderDataBuffer;

	ID3D11ComputeShader* m_pMainRaytracingCS;

	ID3D11Buffer* m_pSphereDataBuffer;
	ID3D11ShaderResourceView* m_pSphereDataSRV;

	static const uint32_t NUM_RANDOM_VECTORS = 100u;
	std::vector<uint32_t> m_bufferIndices;
	ID3D11Buffer* m_pRandomVectorBuffer;
	ID3D11ShaderResourceView* m_pRandomVectorSRV;

	void updateBuffers();

	static thread_local std::mt19937 s_RandomEngine;
	static std::uniform_int_distribution<std::mt19937::result_type> s_Distribution;
};

inline void Renderer::setScene(Scene* pScene)
{
	OKAY_ASSERT(pScene);
	m_pScene = pScene;
}

inline void Renderer::toggleAccumulation(bool enable)
{
	m_renderData.m_accumulationEnabled = (int)enable;
	resetAccumulation();
}

inline void Renderer::resetAccumulation()
{
	m_renderData.m_numAccumulationFrames = 0u;

	static const float CLEAR_COLOUR[4] = { 0.f, 0.f, 0.f, 0.f };
	Okay::getDeviceContext()->ClearUnorderedAccessViewFloat(m_pAccumulationUAV, CLEAR_COLOUR);
}