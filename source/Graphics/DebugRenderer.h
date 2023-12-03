#pragma once
#include "DirectX/DX11.h"
#include "Utilities.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "GPUStorage.h"

#include "glm/glm.hpp"

class Scene;
class GPUResourceManager;
class ResourceManager;

class DebugRenderer
{
public:
	DebugRenderer();
	DebugRenderer(ID3D11Texture2D* pTarget, const GPUResourceManager& pGpuResourceManager);
	~DebugRenderer();

	void shutdown();
	void initiate(ID3D11Texture2D* pTarget, const GPUResourceManager& pGpuResourceManager);

	inline void setScene(const Scene& pScene);
	void render();

	void reloadShaders();

private: // Scene & Resources
	const Scene* m_pScene;
	const GPUResourceManager* m_pGpuResourceManager;
	const ResourceManager* m_pResourceManager;

private: // Pipeline
	struct RenderData // Aligned 16
	{
		// TEMP
		glm::mat4 cameraViewProjectMatrix = glm::mat4(1.f);
		glm::mat4 objectWorldMatrix = glm::mat4(1.f);
		uint32_t vertStartIdx = 0u;
		glm::vec3 pad0 = glm::vec3(0.f);
		MaterialColour3 albedo;
	};

	void bindPipeline();

	RenderData m_renderData;
	ID3D11Buffer* m_pRenderDataBuffer;

	ID3D11VertexShader* m_pVS;
	D3D11_VIEWPORT m_viewport;
	ID3D11PixelShader* m_pPS;

	ID3D11DepthStencilView* m_pDSV;
	ID3D11RenderTargetView* m_pRTV;
};

inline void DebugRenderer::setScene(const Scene& pScene) { m_pScene = &pScene; }