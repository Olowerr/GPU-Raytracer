#pragma once
#include "DirectX/DX11.h"
#include "Utilities.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "GPUStorage.h"
#include "RayTracer.h"
#include "DirectX/RenderTexture.h"

#include "glm/glm.hpp"

class Scene;
class ResourceManager;


class DebugRenderer
{
public:
	enum DrawMode : unsigned char
	{
		None = 0,
		DrawSingle,
		DrawWithChildren,
		DrawWithDecendants,
	};

public:
	DebugRenderer();
	DebugRenderer(const RenderTexture& target, const RayTracer& rayTracer);
	~DebugRenderer();

	void shutdown();
	void initiate(const RenderTexture& target, const RayTracer& rayTracer);

	inline void setScene(const Scene& pScene);
	void reloadShaders();

	void render(bool includeObjects);

	void renderBvhNodeBBs(Entity entity, uint32_t localNodeIdx);
	void renderBvhNodeGeometry(Entity entity, uint32_t localNodeIdx);
	void setBvhNodeDrawMode(DrawMode mode);

	void renderOctTreeNodeBBs(uint32_t nodeIDx);
	void setOctTreeNodeDrawMode(DrawMode mode);


	void onResize();

private: // Scene & Resources
	const Scene* m_pScene = nullptr;
	const RayTracer* m_pRayTracer = nullptr;
	const ResourceManager* m_pResourceManager = nullptr;

	DrawMode m_bvhDrawMode = DrawMode::None;
	DrawMode m_octTreeDrawMode = DrawMode::None;

private: // Pipeline
	struct RenderData // Aligned 16
	{
		glm::mat4 cameraViewProjectMatrix = glm::mat4(1.f);
		glm::mat4 objectWorldMatrix = glm::mat4(1.f);
		uint32_t vertStartIdx = 0u;
		uint32_t nodeIdx = 0u;
		glm::vec2 pad0 = glm::vec2(0.f);
		MaterialColour3 albedo;
		glm::vec3 cameraDir = glm::vec3(0.f);
		float pad1 = 0.f;
		glm::vec3 cameraPos = glm::vec3(0.f);
		uint32_t mode = 0;
	};

	void updateCameraData();
	void bindGeometryPipeline(bool clearTarget = true);

	// General
	const RenderTexture* m_pTargetTexture = nullptr;
	RenderData m_renderData;
	ID3D11Buffer* m_pRenderDataBuffer = nullptr;
	void drawNodeBoundingBox(uint32_t nodeIdx, float colourStrength);
	void drawNodeGeometry(uint32_t nodeIdx, float colourStrength);

	void drawOctTreeNodeBoundingBox(uint32_t nodeIdx, float colourStrength);

	template<typename NodeFunction, typename GPUNodeType, typename... Args>
	void executeDrawMode(DrawMode drawMode, const std::vector<GPUNodeType>& nodeList, uint32_t nodeIdx, uint32_t numChildren, NodeFunction pDrawFunc, Args... args);

	// Mesh pipelime
	ID3D11VertexShader* m_pVS = nullptr;
	D3D11_VIEWPORT m_viewport = D3D11_VIEWPORT{};
	ID3D11PixelShader* m_pPS = nullptr;

	GPUStorage m_sphereTriData;

	// Bvh pipeline
	bool m_renderBvhTree = false;
	ID3D11ShaderResourceView* m_pBvhNodeBuffer = nullptr;
	uint32_t m_bvhNodeNumVerticies = 0u;
	ID3D11RasterizerState* m_pDoubleSideRS = nullptr;

	ID3D11VertexShader* m_pBoundingBoxVS = nullptr;
	ID3D11PixelShader* m_pBoundingBoxPS = nullptr;
	
	// Skybox
	ID3D11VertexShader* m_pSkyboxVS = nullptr;
	ID3D11PixelShader* m_pSkyboxPS = nullptr;
	GPUStorage m_cubeTriData;
	ID3D11RasterizerState* m_noCullRS = nullptr;
	ID3D11DepthStencilState* m_pLessEqualDSS = nullptr;
};

inline void DebugRenderer::setScene(const Scene& pScene)			{ m_pScene = &pScene; }
inline void DebugRenderer::setBvhNodeDrawMode(DrawMode mode)		{ m_bvhDrawMode = mode; }
inline void DebugRenderer::setOctTreeNodeDrawMode(DrawMode mode)	{ m_octTreeDrawMode = mode; }
