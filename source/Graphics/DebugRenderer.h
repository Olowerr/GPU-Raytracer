#pragma once
#include "DirectX/DX11.h"
#include "Utilities.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "GPUStorage.h"
#include "GPUResourceManager.h"

#include "glm/glm.hpp"

class Scene;
class ResourceManager;


class DebugRenderer
{
public:
	enum BvhNodeDrawMode : unsigned char
	{
		None = 0,
		DrawSingle,
		DrawWithChildren,
		DrawWithDecendants,
	};

public:
	DebugRenderer();
	DebugRenderer(ID3D11Texture2D* pTarget, const GPUResourceManager& pGpuResourceManager);
	~DebugRenderer();

	void shutdown();
	void initiate(ID3D11Texture2D* pTarget, const GPUResourceManager& pGpuResourceManager);

	inline void setScene(const Scene& pScene);
	void reloadShaders();

	void render(bool includeObjects);
	void renderNodeBBs(Entity entity, uint32_t localNodeIdx);
	void renderNodeGeometry(Entity entity, uint32_t localNodeIdx);

	void setBvhNodeDrawMode(BvhNodeDrawMode mode);

private: // Scene & Resources
	const Scene* m_pScene;
	const GPUResourceManager* m_pGpuResourceManager;
	const ResourceManager* m_pResourceManager;

	BvhNodeDrawMode m_bvhDrawMode;

private: // Pipeline
	struct RenderData // Aligned 16
	{
		// TEMP
		glm::mat4 cameraViewProjectMatrix = glm::mat4(1.f);
		glm::mat4 objectWorldMatrix = glm::mat4(1.f);
		uint32_t vertStartIdx = 0u;
		uint32_t bvhNodeIdx = 0u;
		glm::vec2 pad0 = glm::vec2(0.f);
		MaterialColour3 albedo;
		glm::vec3 cameraDir = glm::vec3(0.f);
		float pad1 = 0.f;
	};

	void updateCameraData();
	void bindPipeline(bool clearTarget = true);

	// General
	RenderData m_renderData;
	ID3D11Buffer* m_pRenderDataBuffer;
	void drawNodeBoundingBox(uint32_t nodeIdx, uint32_t baseNodeIdx, const MeshDesc& meshDesc);
	void drawNodeGeometry(uint32_t nodeIdx, const MeshComponent& meshComp);

	template<typename NodeFunction, typename... Args>
	void executeDrawMode(uint32_t nodeIdx, NodeFunction pFunc, Args... args);

	// Mesh pipelime
	ID3D11VertexShader* m_pVS;
	D3D11_VIEWPORT m_viewport;
	ID3D11PixelShader* m_pPS;

	ID3D11DepthStencilView* m_pDSV;
	ID3D11RenderTargetView* m_pRTV;

	ID3D11ShaderResourceView* m_pShereTriBuffer;
	uint32_t m_sphereNumVerticies;

	// Bvh pipeline
	bool m_renderBvhTree;
	ID3D11ShaderResourceView* m_pBvhNodeBuffer;
	uint32_t m_bvhNodeNumVerticies;
	ID3D11RasterizerState* m_pDoubleSideRS;

	ID3D11VertexShader* m_pLineVS;
	ID3D11PixelShader* m_pLinePS;
};

inline void DebugRenderer::setScene(const Scene& pScene)			{ m_pScene = &pScene; }
inline void DebugRenderer::setBvhNodeDrawMode(BvhNodeDrawMode mode) { m_bvhDrawMode = mode; }