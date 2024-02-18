#include "DebugRenderer.h"
#include "Scene/Scene.h"
#include "ResourceManager.h"
#include "shaders/ShaderResourceRegisters.h"
#include "Importer.h"

#include <stack>
#include <DirectXCollision.h>

constexpr glm::vec3 BVH_NODE_COLOUR = glm::vec3(0.9f, 0.7f, 0.5f);

DebugRenderer::DebugRenderer()
	:m_pScene(nullptr), m_pGpuResourceManager(nullptr), m_pResourceManager(nullptr),
	m_pVS(nullptr), m_pPS(nullptr), m_pDSV(nullptr), m_pRTV(nullptr), m_viewport(), m_pRenderDataBuffer(nullptr),
	m_pShereTriBuffer(nullptr), m_sphereNumVerticies(0u), m_pBvhNodeBuffer(nullptr), m_bvhNodeNumVerticies(0u),
	m_renderBvhTree(false), m_pBoundingBoxVS(nullptr), m_pBoundingBoxPS(nullptr), m_pDoubleSideRS(nullptr)
{
}

DebugRenderer::DebugRenderer(ID3D11Texture2D* pTarget, const GPUResourceManager& pGpuResourceManager)
{
	initiate(pTarget, pGpuResourceManager);
}

DebugRenderer::~DebugRenderer()
{
	shutdown();
}

void DebugRenderer::shutdown()
{
	m_pScene = nullptr;
	m_pGpuResourceManager = nullptr;
	m_pResourceManager = nullptr;

	DX11_RELEASE(m_pRenderDataBuffer);
	DX11_RELEASE(m_pVS);
	DX11_RELEASE(m_pPS);
	DX11_RELEASE(m_pDSV);
	DX11_RELEASE(m_pRTV);
	DX11_RELEASE(m_pShereTriBuffer);
	DX11_RELEASE(m_pBvhNodeBuffer);
	DX11_RELEASE(m_pBoundingBoxVS);
	DX11_RELEASE(m_pBoundingBoxPS);
	DX11_RELEASE(m_pDoubleSideRS);
}

void DebugRenderer::initiate(ID3D11Texture2D* pTarget, const GPUResourceManager& pGpuResourceManager)
{
	OKAY_ASSERT(pTarget);

	shutdown();
	m_pGpuResourceManager = &pGpuResourceManager;

	bool success = false;
	ID3D11Device* pDevice = Okay::getDevice();

	// General Pipeline
	success = Okay::createShader(SHADER_PATH "DebugVS.hlsl", &m_pVS);
	OKAY_ASSERT(success);
	
	success = Okay::createShader(SHADER_PATH "DebugPS.hlsl", &m_pPS);
	OKAY_ASSERT(success);

	success = SUCCEEDED(pDevice->CreateRenderTargetView(pTarget, nullptr, &m_pRTV));
	OKAY_ASSERT(success);

	ID3D11Texture2D* depthBuffer = nullptr;
	D3D11_TEXTURE2D_DESC depthDesc{};
	pTarget->GetDesc(&depthDesc);
	depthDesc.Format = DXGI_FORMAT_D16_UNORM;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	success = SUCCEEDED(pDevice->CreateTexture2D(&depthDesc, nullptr, &depthBuffer));
	OKAY_ASSERT(success);

	success = SUCCEEDED(pDevice->CreateDepthStencilView(depthBuffer, nullptr, &m_pDSV));
	DX11_RELEASE(depthBuffer);
	OKAY_ASSERT(success);

	success = Okay::createConstantBuffer(&m_pRenderDataBuffer, nullptr, sizeof(RenderData));
	OKAY_ASSERT(success);

	glm::vec4 clearColor = glm::vec4(0.f, 0.f, 0.f, 0.f);
	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();
	pDevCon->ClearRenderTargetView(m_pRTV, &clearColor.x);
	pDevCon->ClearDepthStencilView(m_pDSV, D3D11_CLEAR_DEPTH, 1.f, 0);

	m_viewport.Height = (float)depthDesc.Height;
	m_viewport.Width = (float)depthDesc.Width;
	m_viewport.MinDepth = 0.f;
	m_viewport.MaxDepth = 1.f;
	m_viewport.TopLeftX = 0.f;
	m_viewport.TopLeftY = 0.f;

	// Sphere Mesh
	MeshData sphereData;
	success = Importer::loadMesh("resources/meshes/sphere.fbx", sphereData);
	OKAY_ASSERT(success);

	Mesh sphereMesh(sphereData, "");
	const std::vector<Okay::Triangle>& spehreTris = sphereMesh.getTriangles();
	m_sphereNumVerticies = (uint32_t)sphereData.positions.size();

	ID3D11Buffer* pSphereBuffer = nullptr;
	success = Okay::createStructuredBuffer(&pSphereBuffer, &m_pShereTriBuffer, spehreTris.data(), sizeof(Okay::Triangle), (uint32_t)spehreTris.size());
	DX11_RELEASE(pSphereBuffer);
	OKAY_ASSERT(success);

	// BVH Tree Rendering
	success = Okay::createShader(SHADER_PATH "DebugBBVS.hlsl", &m_pBoundingBoxVS);
	OKAY_ASSERT(success);

	success = Okay::createShader(SHADER_PATH "DebugBBPS.hlsl", &m_pBoundingBoxPS);
	OKAY_ASSERT(success);

	D3D11_RASTERIZER_DESC dsRsDesc{};
	dsRsDesc.FillMode = D3D11_FILL_SOLID;
	dsRsDesc.CullMode =	D3D11_CULL_NONE;
	dsRsDesc.FrontCounterClockwise = FALSE;
	dsRsDesc.DepthBias = 0;
	dsRsDesc.SlopeScaledDepthBias =	0.0f;
	dsRsDesc.DepthBiasClamp = 0.0f;
	dsRsDesc.DepthClipEnable = TRUE;
	dsRsDesc.ScissorEnable = FALSE;
	dsRsDesc.MultisampleEnable = FALSE;
	dsRsDesc.AntialiasedLineEnable = FALSE;
	success = SUCCEEDED(pDevice->CreateRasterizerState(&dsRsDesc, &m_pDoubleSideRS));
	OKAY_ASSERT(success);

	DirectX::BoundingBox box;
	box.Center = DirectX::XMFLOAT3(0.f, 0.f, 0.f);
	box.Extents = DirectX::XMFLOAT3(1.f, 1.f, 1.f);
	
	// Corners format: 0-3 one side, 4-7 other side, both clockwise order when looking from +Z direction
	DirectX::XMFLOAT3 corners[8]{};
	box.GetCorners(corners); 

	DirectX::XMFLOAT3 lines[24] = // 12 lines, two points per line
	{
		// First side
		corners[0], corners[1],
		corners[1], corners[2],
		corners[2], corners[3],
		corners[3], corners[0],

		// Other side
		corners[4], corners[5],
		corners[5], corners[6],
		corners[6], corners[7],
		corners[7], corners[4],

		// Connecting
		corners[0], corners[4],
		corners[1], corners[5],
		corners[2], corners[6],
		corners[3], corners[7],
	};

	ID3D11Buffer* pBvhNodeVertBuffer = nullptr;
	success = Okay::createStructuredBuffer(&pBvhNodeVertBuffer, &m_pBvhNodeBuffer, lines, (uint32_t)sizeof(DirectX::XMFLOAT3), 24u);
	DX11_RELEASE(pBvhNodeVertBuffer);
	OKAY_ASSERT(success);

	m_bvhNodeNumVerticies = 24u;
}

template<typename ShaderType>
void reloadShader(std::string_view path, ShaderType** ppShader) // TODO: Move to DX11.h
{
	ShaderType* pNewShader = nullptr;
	if (!Okay::createShader(path, &pNewShader))
		return;

	DX11_RELEASE(*ppShader);
	*ppShader = pNewShader;
}

void DebugRenderer::reloadShaders()
{
	reloadShader(SHADER_PATH "DebugVS.hlsl", &m_pVS);
	reloadShader(SHADER_PATH "DebugPS.hlsl", &m_pPS);
	reloadShader(SHADER_PATH "DebugBBVS.hlsl", &m_pBoundingBoxVS);
	reloadShader(SHADER_PATH "DebugBBPS.hlsl", &m_pBoundingBoxPS);
}

void DebugRenderer::render(bool includeObjects)
{
	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();

	static const glm::vec4 clearColor = glm::vec4(0.f, 0.f, 0.f, 0.f);
	pDevCon->ClearRenderTargetView(m_pRTV, &clearColor.x);

	// TODO: Draw Skybox


	if (!includeObjects)
		return;

	m_pGpuResourceManager->bindResources();
	bindGeometryPipeline();
	updateCameraData();

	const entt::registry& reg = m_pScene->getRegistry();
	const std::vector<MeshDesc>& meshDescs = m_pGpuResourceManager->getMeshDescriptors();

	auto meshView = reg.view<MeshComponent, Transform>();
	for (entt::entity entity : meshView) // Draw Meshes
	{
		auto [meshComp, transform] = meshView[entity];

		const MeshDesc& desc = meshDescs[meshComp.meshID];

		m_renderData.objectWorldMatrix = glm::transpose(transform.calculateMatrix());
		m_renderData.vertStartIdx = desc.startIdx * 3u;
		m_renderData.albedo = meshComp.material.albedo;

		Okay::updateBuffer(m_pRenderDataBuffer, &m_renderData, sizeof(RenderData));

		pDevCon->Draw((desc.endIdx - desc.startIdx) * 3u, 0u);
	}
	
	pDevCon->VSSetShaderResources(RM_TRIANGLE_DATA_SLOT, 1u, &m_pShereTriBuffer);

	auto sphereView = reg.view<Sphere, Transform>();
	for (entt::entity entity : sphereView) // Draw Spheres
	{
		auto [sphere, transform] = sphereView[entity];

		Transform traCopy = transform;
		traCopy.scale = glm::vec3(sphere.radius);

		m_renderData.objectWorldMatrix = glm::transpose(traCopy.calculateMatrix());
		m_renderData.vertStartIdx = 0u;
		m_renderData.albedo = sphere.material.albedo;

		Okay::updateBuffer(m_pRenderDataBuffer, &m_renderData, sizeof(RenderData));

		pDevCon->Draw(m_sphereNumVerticies, 0u);
	}

	ID3D11ShaderResourceView* pOrigTriangleBuffer = m_pGpuResourceManager->getTriangleData().getSRV();
	pDevCon->VSSetShaderResources(RM_TRIANGLE_DATA_SLOT, 1u, &pOrigTriangleBuffer);
}

void DebugRenderer::renderNodeBBs(Entity entity, uint32_t localNodeIdx)
{
	if (!entity)
		return;

	const MeshComponent* pMeshComp = entity.tryGetComponent<MeshComponent>();
	if (!pMeshComp)
		return;
	
	updateCameraData();

	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();

	pDevCon->IASetInputLayout(nullptr);
	pDevCon->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	pDevCon->VSSetShader(m_pBoundingBoxVS, nullptr, 0u);
	pDevCon->VSSetConstantBuffers(RZ_RENDER_DATA_SLOT, 1u, &m_pRenderDataBuffer);
	pDevCon->VSSetShaderResources(RM_TRIANGLE_DATA_SLOT, 1u, &m_pBvhNodeBuffer);

	pDevCon->RSSetViewports(1u, &m_viewport);

	pDevCon->PSSetShader(m_pBoundingBoxPS, nullptr, 0u);
	pDevCon->PSSetConstantBuffers(RZ_RENDER_DATA_SLOT, 1u, &m_pRenderDataBuffer);
	
	pDevCon->OMSetRenderTargets(1u, &m_pRTV, nullptr);

	const Transform& transformComp = entity.getComponent<Transform>();
	m_renderData.objectWorldMatrix = glm::transpose(transformComp.calculateMatrix());

	const MeshDesc& meshDesc = m_pGpuResourceManager->getMeshDescriptors()[pMeshComp->meshID];
	uint32_t globalNodeIdx = m_pGpuResourceManager->getGlobalNodeIdx(*pMeshComp, localNodeIdx);
	executeDrawMode(globalNodeIdx, &DebugRenderer::drawNodeBoundingBox, globalNodeIdx, meshDesc);

	ID3D11ShaderResourceView* pOrigTriangleSRV = m_pGpuResourceManager->getTriangleData().getSRV();
	pDevCon->VSSetShaderResources(RM_TRIANGLE_DATA_SLOT, 1u, &pOrigTriangleSRV);
}

void DebugRenderer::renderNodeGeometry(Entity entity, uint32_t localNodeIdx)
{
	if (!entity)
		return;

	const MeshComponent* pMeshComp = entity.tryGetComponent<MeshComponent>();
	if (!pMeshComp)
		return;

	bindGeometryPipeline(false);
	updateCameraData();

	ID3D11ShaderResourceView* pTriangleBufferSRV = m_pGpuResourceManager->getTriangleData().getSRV();

	const Transform& transformComp = entity.getComponent<Transform>();
	m_renderData.objectWorldMatrix = glm::transpose(transformComp.calculateMatrix());

	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();
	pDevCon->VSSetShaderResources(RM_TRIANGLE_DATA_SLOT, 1u, &pTriangleBufferSRV);
	pDevCon->RSSetState(m_pDoubleSideRS);

	uint32_t globalNodeIdx = m_pGpuResourceManager->getGlobalNodeIdx(*pMeshComp, localNodeIdx);
	executeDrawMode(globalNodeIdx, &DebugRenderer::drawNodeGeometry, *pMeshComp);

	pDevCon->RSSetState(nullptr);
}

void DebugRenderer::updateCameraData()
{
	const Entity camEntity = m_pScene->getFirstCamera();
	const Transform& camTra = camEntity.getComponent<Transform>();
	const Camera& camData = camEntity.getComponent<Camera>();

	const glm::mat3 rotationMatrix = glm::toMat3(glm::quat(glm::radians(camTra.rotation)));
	const glm::vec3& camForward = rotationMatrix[2];

	m_renderData.cameraDir = camForward;
	m_renderData.cameraViewProjectMatrix = glm::transpose(
		glm::perspectiveFovLH(glm::radians(camData.fov), m_viewport.Width, m_viewport.Height, camData.nearZ, camData.farZ) *
		glm::lookAtLH(camTra.position, camTra.position + camForward, glm::vec3(0.f, 1.f, 0.f)));
}

void DebugRenderer::bindGeometryPipeline(bool clearTarget)
{
	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();
	
	if (clearTarget)
	{
		static const glm::vec4 clearColor = glm::vec4(0.f, 0.f, 0.f, 0.f);
		pDevCon->ClearRenderTargetView(m_pRTV, &clearColor.x);
	}
	pDevCon->ClearDepthStencilView(m_pDSV, D3D11_CLEAR_DEPTH, 1.f, 0);

	pDevCon->IASetInputLayout(nullptr);
	pDevCon->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	pDevCon->VSSetShader(m_pVS, nullptr, 0u);
	pDevCon->RSSetViewports(1u, &m_viewport);
	pDevCon->PSSetShader(m_pPS, nullptr, 0u);
	pDevCon->OMSetRenderTargets(1u, &m_pRTV, m_pDSV);

	pDevCon->VSSetConstantBuffers(RZ_RENDER_DATA_SLOT, 1u, &m_pRenderDataBuffer);
	pDevCon->PSSetConstantBuffers(RZ_RENDER_DATA_SLOT, 1u, &m_pRenderDataBuffer);
}

void DebugRenderer::drawNodeBoundingBox(uint32_t nodeIdx, uint32_t baseNodeIdx, const MeshDesc& meshDesc)
{
	OKAY_ASSERT(nodeIdx < (uint32_t)m_pGpuResourceManager->getBvhTreeNodes().size());

	uint32_t localDepth = nodeIdx - baseNodeIdx;
	float colourStrength = (meshDesc.numBvhNodes - localDepth) / (float)meshDesc.numBvhNodes;
	
	m_renderData.albedo.textureId = Okay::INVALID_UINT;
	m_renderData.albedo.colour = BVH_NODE_COLOUR * colourStrength;
	m_renderData.bvhNodeIdx = nodeIdx;

	Okay::updateBuffer(m_pRenderDataBuffer, &m_renderData, sizeof(RenderData));
	Okay::getDeviceContext()->Draw(m_bvhNodeNumVerticies, 0u);
}

void DebugRenderer::drawNodeGeometry(uint32_t nodeIdx, const MeshComponent& meshComp)
{
	OKAY_ASSERT(nodeIdx < (uint32_t)m_pGpuResourceManager->getBvhTreeNodes().size());

	const GPUNode& node = m_pGpuResourceManager->getBvhTreeNodes()[nodeIdx];

	m_renderData.vertStartIdx = node.triStart * 3u;
	m_renderData.albedo.colour = glm::vec3(0.f, 1.f, 0.f);
	m_renderData.albedo.textureId = Okay::INVALID_UINT;

	Okay::updateBuffer(m_pRenderDataBuffer, &m_renderData, sizeof(RenderData));

	Okay::getDeviceContext()->Draw((node.triEnd - node.triStart) * 3u, 0u);
}

template<typename NodeFunction, typename... Args>
void DebugRenderer::executeDrawMode(uint32_t nodeIdx, NodeFunction pFunc, Args... args)
{
	const std::vector<GPUNode>& gpuNodes = m_pGpuResourceManager->getBvhTreeNodes();

	switch (m_bvhDrawMode)
	{
	case BvhNodeDrawMode::DrawSingle:
	{
		(this->*pFunc)(nodeIdx, std::forward<Args>(args)...);
		break;
	}
	case BvhNodeDrawMode::DrawWithChildren:
	{
		const GPUNode& selectedNode = gpuNodes[nodeIdx];

		(this->*pFunc)(nodeIdx, std::forward<Args>(args)...);
		if (selectedNode.childIdxs[0] != Okay::INVALID_UINT)
		{
			(this->*pFunc)(selectedNode.childIdxs[0], std::forward<Args>(args)...);
			(this->*pFunc)(selectedNode.childIdxs[1], std::forward<Args>(args)...);
		}
		break;
	}
	case BvhNodeDrawMode::DrawWithDecendants:
	{
		std::stack<uint32_t> nodes;
		nodes.push(nodeIdx);

		while (!nodes.empty())
		{
			uint32_t currentIdx = nodes.top();
			const GPUNode& currentNode = gpuNodes[currentIdx];
			nodes.pop();

			if (currentNode.childIdxs[0] != Okay::INVALID_UINT)
			{
				nodes.push(currentNode.childIdxs[0]);
				nodes.push(currentNode.childIdxs[1]);
			}

			(this->*pFunc)(currentIdx, std::forward<Args>(args)...);
		}
		break;
	}

	default:
		break;
	}
}