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
	enum TreeNodeDrawMode : unsigned char
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
	void setBvhNodeDrawMode(TreeNodeDrawMode mode);

	void renderOctTreeNodeBBs(uint32_t nodeIDx);
	void setOctTreeNodeDrawMode(TreeNodeDrawMode mode);


	void onResize();

private: // Scene & Resources
	const Scene* m_pScene;
	const RayTracer* m_pRayTracer;
	const ResourceManager* m_pResourceManager;

	TreeNodeDrawMode m_bvhDrawMode;
	TreeNodeDrawMode m_octTreeDrawMode;

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
	const RenderTexture* m_pTargetTexture;
	RenderData m_renderData;
	ID3D11Buffer* m_pRenderDataBuffer;
	void drawNodeBoundingBox(uint32_t nodeIdx, uint32_t baseNodeIdx, const MeshDesc& meshDesc);
	void drawNodeGeometry(uint32_t nodeIdx, const MeshComponent& meshComp);

	void drawOctTreeNodeBoundingBox(uint32_t nodeIdx);

	template<typename NodeFunction, typename GPUNodeType, typename... Args>
	void executeDrawMode(uint32_t nodeIdx, const std::vector<GPUNodeType>& list, NodeFunction pFunc, TreeNodeDrawMode drawMode, Args... args);

	// Mesh pipelime
	ID3D11VertexShader* m_pVS;
	D3D11_VIEWPORT m_viewport;
	ID3D11PixelShader* m_pPS;

	GPUStorage m_sphereTriData;

	// Bvh pipeline
	bool m_renderBvhTree;
	ID3D11ShaderResourceView* m_pBvhNodeBuffer;
	uint32_t m_bvhNodeNumVerticies;
	ID3D11RasterizerState* m_pDoubleSideRS;

	ID3D11VertexShader* m_pBoundingBoxVS;
	ID3D11PixelShader* m_pBoundingBoxPS;
	
	// Skybox
	ID3D11VertexShader* m_pSkyboxVS;
	ID3D11PixelShader* m_pSkyboxPS;
	GPUStorage m_cubeTriData;
	ID3D11RasterizerState* m_noCullRS;
	ID3D11DepthStencilState* m_pLessEqualDSS;
};

inline void DebugRenderer::setScene(const Scene& pScene)			{ m_pScene = &pScene; }
inline void DebugRenderer::setBvhNodeDrawMode(TreeNodeDrawMode mode) { m_bvhDrawMode = mode; }
inline void DebugRenderer::setOctTreeNodeDrawMode(TreeNodeDrawMode mode) { m_octTreeDrawMode = mode; }