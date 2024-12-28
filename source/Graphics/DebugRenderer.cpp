#include "DebugRenderer.h"
#include "Scene/Scene.h"
#include "ResourceManager.h"
#include "shaders/ShaderResourceRegisters.h"
#include "Importer.h"

#include <stack>
#include <DirectXCollision.h>

constexpr glm::vec3 BVH_NODE_COLOUR = glm::vec3(0.9f, 0.7f, 0.5f);
constexpr glm::vec3 BVH_NODE_GEOMETRY_COLOUR = glm::vec3(0.4f, 0.6f, 0.8f);
constexpr glm::vec3 OCT_NODE_COLOUR = glm::vec3(0.4f, 0.7f, 0.4f);

DebugRenderer::DebugRenderer()
{
}

DebugRenderer::DebugRenderer(const RenderTexture& target, const RayTracer& pGpuResourceManager)
{
	initiate(target, pGpuResourceManager);
}

DebugRenderer::~DebugRenderer()
{
	shutdown();
}

void DebugRenderer::shutdown()
{
	m_pScene = nullptr;
	m_pRayTracer = nullptr;
	m_pResourceManager = nullptr;

	m_sphereTriData.shutdown();
	m_cubeTriData.shutdown();

	DX11_RELEASE(m_pRenderDataBuffer);
	DX11_RELEASE(m_pVS);
	DX11_RELEASE(m_pPS);
	DX11_RELEASE(m_pBvhNodeBuffer);
	DX11_RELEASE(m_pBoundingBoxVS);
	DX11_RELEASE(m_pBoundingBoxPS);
	DX11_RELEASE(m_pDoubleSideRS);
	DX11_RELEASE(m_pSkyboxVS);
	DX11_RELEASE(m_pSkyboxPS);
	DX11_RELEASE(m_noCullRS);
	DX11_RELEASE(m_pLessEqualDSS);
}

void DebugRenderer::initiate(const RenderTexture& target, const RayTracer& rayTracer)
{
	shutdown();
	m_pTargetTexture = &target;
	m_pRayTracer = &rayTracer;

	bool success = false;
	ID3D11Device* pDevice = Okay::getDevice();

	// General Pipeline
	success = Okay::createShader(SHADER_PATH "DebugVS.hlsl", &m_pVS);
	OKAY_ASSERT(success);
	
	success = Okay::createShader(SHADER_PATH "DebugPS.hlsl", &m_pPS);
	OKAY_ASSERT(success);

	success = Okay::createConstantBuffer(&m_pRenderDataBuffer, nullptr, sizeof(RenderData));
	OKAY_ASSERT(success);

	m_viewport.Height = (float)target.getDimensions().y;
	m_viewport.Width = (float)target.getDimensions().x;
	m_viewport.MinDepth = 0.f;
	m_viewport.MaxDepth = 1.f;
	m_viewport.TopLeftX = 0.f;
	m_viewport.TopLeftY = 0.f;

	// Sphere Mesh
	MeshData sphereData;
	success = Importer::loadMesh("resources/meshes/sphere.fbx", sphereData);
	OKAY_ASSERT(success);

	Mesh sphereMesh(sphereData, "");
	const std::vector<Okay::Triangle>& sphereTris = sphereMesh.getTrianglesPos();
	m_sphereTriData.initiate((uint32_t)sizeof(Okay::Triangle), (uint32_t)sphereTris.size(), sphereTris.data());

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

	// Skybox
	success = Okay::createShader(SHADER_PATH "DebugSkyboxVS.hlsl", &m_pSkyboxVS);
	OKAY_ASSERT(success);

	success = Okay::createShader(SHADER_PATH "DebugSkyboxPS.hlsl", &m_pSkyboxPS);
	OKAY_ASSERT(success);

	MeshData cubeData;
	success = Importer::loadMesh("resources/meshes/cube.fbx", cubeData);
	OKAY_ASSERT(success);

	Mesh cubeMesh(cubeData, "");
	const std::vector<Okay::Triangle>& cubeTris = cubeMesh.getTrianglesPos();
	m_cubeTriData.initiate((uint32_t)sizeof(Okay::Triangle), (uint32_t)cubeTris.size(), cubeTris.data());

	D3D11_RASTERIZER_DESC noCullRSDesc{};
	noCullRSDesc.FillMode = D3D11_FILL_SOLID;
	noCullRSDesc.CullMode = D3D11_CULL_NONE;
	noCullRSDesc.FrontCounterClockwise = FALSE;
	noCullRSDesc.DepthBias = 0;
	noCullRSDesc.SlopeScaledDepthBias = 0.0f;
	noCullRSDesc.DepthBiasClamp = 0.0f;
	noCullRSDesc.DepthClipEnable = TRUE;
	noCullRSDesc.ScissorEnable = FALSE;
	noCullRSDesc.MultisampleEnable = FALSE;
	noCullRSDesc.AntialiasedLineEnable = FALSE;
	success = SUCCEEDED(pDevice->CreateRasterizerState(&noCullRSDesc, &m_noCullRS));
	OKAY_ASSERT(success);

	D3D11_DEPTH_STENCIL_DESC dsLessEqaulDesc{};
	dsLessEqaulDesc.DepthEnable = true;
	dsLessEqaulDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dsLessEqaulDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	dsLessEqaulDesc.StencilEnable = false;
	dsLessEqaulDesc.StencilReadMask = 0;
	dsLessEqaulDesc.StencilWriteMask = 0;

	success = SUCCEEDED(pDevice->CreateDepthStencilState(&dsLessEqaulDesc, &m_pLessEqualDSS));
	OKAY_ASSERT(success);
}

void DebugRenderer::reloadShaders()
{
	Okay::reloadShader(SHADER_PATH "DebugVS.hlsl", &m_pVS);
	Okay::reloadShader(SHADER_PATH "DebugPS.hlsl", &m_pPS);
	Okay::reloadShader(SHADER_PATH "DebugBBVS.hlsl", &m_pBoundingBoxVS);
	Okay::reloadShader(SHADER_PATH "DebugBBPS.hlsl", &m_pBoundingBoxPS);
}

void DebugRenderer::render(bool includeObjects)
{
	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();

	//m_pRayTracer->bindResources();
	bindGeometryPipeline();
	updateCameraData();

	// Skybox
	ID3D11ShaderResourceView* pCubeTriBuffer = m_cubeTriData.getSRV();
	uint32_t cubeNumVerticies = m_cubeTriData.getCapacity() * 3u;

	Okay::updateBuffer(m_pRenderDataBuffer, &m_renderData, sizeof(RenderData));
	pDevCon->VSSetShader(m_pSkyboxVS, nullptr, 0u);
	pDevCon->VSSetShaderResources(TRIANGLE_POS_SLOT, 1u, &pCubeTriBuffer);
	pDevCon->RSSetState(m_noCullRS);
	pDevCon->PSSetShader(m_pSkyboxPS, nullptr, 0u);
	pDevCon->OMSetDepthStencilState(m_pLessEqualDSS, 0u);

	pDevCon->Draw(cubeNumVerticies, 0u);
	
	pDevCon->RSSetState(nullptr);
	pDevCon->OMSetDepthStencilState(nullptr, 0u);

	if (!includeObjects)
		return;

	ID3D11ShaderResourceView* pSphereTriBuffer = m_sphereTriData.getSRV();
	uint32_t sphereNumVerticies = m_sphereTriData.getCapacity() * 3u;

	pDevCon->VSSetShader(m_pVS, nullptr, 0u);
	pDevCon->VSSetShaderResources(TRIANGLE_POS_SLOT, 1u, &pSphereTriBuffer);
	pDevCon->PSSetShader(m_pPS, nullptr, 0u);

	const entt::registry& reg = m_pScene->getRegistry();
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

		pDevCon->Draw(sphereNumVerticies, 0u);
	}

	ID3D11ShaderResourceView* pOrigTriangleBuffer = m_pRayTracer->getTrianglesPos().getSRV();
	pDevCon->VSSetShaderResources(TRIANGLE_POS_SLOT, 1u, &pOrigTriangleBuffer);

	const std::vector<MeshDesc>& meshDescs = m_pRayTracer->getMeshDescriptors();
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
}

void DebugRenderer::renderBvhNodeBBs(Entity entity, uint32_t localNodeIdx)
{
	if (m_bvhDrawMode == DrawMode::None)
		return;

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
	pDevCon->VSSetConstantBuffers(DBG_RENDER_DATA_SLOT, 1u, &m_pRenderDataBuffer);
	pDevCon->VSSetShaderResources(TRIANGLE_POS_SLOT, 1u, &m_pBvhNodeBuffer);

	pDevCon->RSSetViewports(1u, &m_viewport);

	pDevCon->PSSetShader(m_pBoundingBoxPS, nullptr, 0u);
	pDevCon->PSSetConstantBuffers(DBG_RENDER_DATA_SLOT, 1u, &m_pRenderDataBuffer);
	
	pDevCon->OMSetRenderTargets(1u, m_pTargetTexture->getRTV(), nullptr);

	const Transform& transformComp = entity.getComponent<Transform>();
	m_renderData.objectWorldMatrix = glm::transpose(transformComp.calculateMatrix());

	const MeshDesc& meshDesc = m_pRayTracer->getMeshDescriptors()[pMeshComp->meshID];
	uint32_t globalNodeIdx = m_pRayTracer->getGlobalNodeIdx(*pMeshComp, localNodeIdx);
	executeDrawMode(m_bvhDrawMode, m_pRayTracer->getBvhTreeNodes(), globalNodeIdx, 2, &DebugRenderer::drawNodeBoundingBox);

	ID3D11ShaderResourceView* pOrigTriangleSRV = m_pRayTracer->getTrianglesPos().getSRV();
	pDevCon->VSSetShaderResources(TRIANGLE_POS_SLOT, 1u, &pOrigTriangleSRV);
}

void DebugRenderer::renderBvhNodeGeometry(Entity entity, uint32_t localNodeIdx)
{
	if (m_bvhDrawMode == DrawMode::None)
		return;

	if (!entity)
		return;

	const MeshComponent* pMeshComp = entity.tryGetComponent<MeshComponent>();
	if (!pMeshComp)
		return;

	bindGeometryPipeline(false);
	updateCameraData();

	ID3D11ShaderResourceView* pTriangleBufferSRV = m_pRayTracer->getTrianglesPos().getSRV();

	const Transform& transformComp = entity.getComponent<Transform>();
	m_renderData.objectWorldMatrix = glm::transpose(transformComp.calculateMatrix());

	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();
	pDevCon->VSSetShaderResources(TRIANGLE_POS_SLOT, 1u, &pTriangleBufferSRV);
	pDevCon->RSSetState(m_pDoubleSideRS);

	uint32_t globalNodeIdx = m_pRayTracer->getGlobalNodeIdx(*pMeshComp, localNodeIdx);
	executeDrawMode(m_bvhDrawMode, m_pRayTracer->getBvhTreeNodes(), globalNodeIdx, 2, &DebugRenderer::drawNodeGeometry);

	pDevCon->RSSetState(nullptr);
}

void DebugRenderer::renderOctTreeNodeBBs(uint32_t nodeIdx)
{
	if (m_octTreeDrawMode == DrawMode::None)
		return;

	updateCameraData();

	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();

	pDevCon->IASetInputLayout(nullptr);
	pDevCon->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	pDevCon->VSSetShader(m_pBoundingBoxVS, nullptr, 0u);
	pDevCon->VSSetConstantBuffers(DBG_RENDER_DATA_SLOT, 1u, &m_pRenderDataBuffer);
	pDevCon->VSSetShaderResources(TRIANGLE_POS_SLOT, 1u, &m_pBvhNodeBuffer);

	pDevCon->RSSetViewports(1u, &m_viewport);

	pDevCon->PSSetShader(m_pBoundingBoxPS, nullptr, 0u);
	pDevCon->PSSetConstantBuffers(DBG_RENDER_DATA_SLOT, 1u, &m_pRenderDataBuffer);

	pDevCon->OMSetRenderTargets(1u, m_pTargetTexture->getRTV(), nullptr);

	const std::vector<GPU_OctTreeNode>& octTreeNodes = m_pRayTracer->getOctTreeNodes();
	uint32_t numChildren = octTreeNodes[nodeIdx].numChildren;
	executeDrawMode(m_octTreeDrawMode, octTreeNodes, nodeIdx, numChildren, &DebugRenderer::drawOctTreeNodeBoundingBox);

	ID3D11ShaderResourceView* pOrigTriangleSRV = m_pRayTracer->getTrianglesPos().getSRV();
	pDevCon->VSSetShaderResources(TRIANGLE_POS_SLOT, 1u, &pOrigTriangleSRV);
}

void DebugRenderer::updateCameraData()
{
	const Entity camEntity = m_pScene->getFirstCamera();
	const Transform& camTra = camEntity.getComponent<Transform>();
	const Camera& camData = camEntity.getComponent<Camera>();

	const glm::mat3 rotationMatrix = glm::toMat3(glm::quat(glm::radians(camTra.rotation)));
	const glm::vec3& camForward = rotationMatrix[2];

	m_renderData.cameraPos = camTra.position;
	m_renderData.cameraDir = camForward;
	m_renderData.cameraViewProjectMatrix = glm::transpose(
		glm::perspectiveFovLH(glm::radians(camData.fov), m_viewport.Width, m_viewport.Height, camData.nearZ, camData.farZ) *
		glm::lookAtLH(camTra.position, camTra.position + camForward, glm::vec3(0.f, 1.f, 0.f)));
}

void DebugRenderer::bindGeometryPipeline(bool clearTarget)
{
	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();
	ID3D11RenderTargetView* pTargetRTV = *m_pTargetTexture->getRTV();
	ID3D11DepthStencilView* pTargetDSV = *m_pTargetTexture->getDSV();

	if (clearTarget)
	{
		static const glm::vec4 clearColor = glm::vec4(0.f, 0.f, 0.f, 1.f);
		pDevCon->ClearRenderTargetView(pTargetRTV, &clearColor.x);
	}
	pDevCon->ClearDepthStencilView(pTargetDSV, D3D11_CLEAR_DEPTH, 1.f, 0);

	pDevCon->IASetInputLayout(nullptr);
	pDevCon->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	pDevCon->VSSetShader(m_pVS, nullptr, 0u);
	pDevCon->RSSetViewports(1u, &m_viewport);
	pDevCon->PSSetShader(m_pPS, nullptr, 0u);
	pDevCon->OMSetRenderTargets(1u, &pTargetRTV, pTargetDSV);

	pDevCon->VSSetConstantBuffers(DBG_RENDER_DATA_SLOT, 1u, &m_pRenderDataBuffer);
	pDevCon->PSSetConstantBuffers(DBG_RENDER_DATA_SLOT, 1u, &m_pRenderDataBuffer);
}

void DebugRenderer::drawNodeBoundingBox(uint32_t nodeIdx, float colourStrength)
{
	OKAY_ASSERT(nodeIdx < (uint32_t)m_pRayTracer->getBvhTreeNodes().size());

	m_renderData.albedo.textureId = Okay::INVALID_UINT;
	m_renderData.albedo.colour = BVH_NODE_COLOUR * colourStrength;
	m_renderData.nodeIdx = nodeIdx;
	m_renderData.mode = 0;

	Okay::updateBuffer(m_pRenderDataBuffer, &m_renderData, sizeof(RenderData));
	Okay::getDeviceContext()->Draw(m_bvhNodeNumVerticies, 0u);
}

void DebugRenderer::drawNodeGeometry(uint32_t nodeIdx, float colourStrength)
{
	OKAY_ASSERT(nodeIdx < (uint32_t)m_pRayTracer->getBvhTreeNodes().size());

	const GPUNode& node = m_pRayTracer->getBvhTreeNodes()[nodeIdx];

	m_renderData.vertStartIdx = node.triStart * 3u;
	m_renderData.albedo.colour = BVH_NODE_GEOMETRY_COLOUR;
	m_renderData.albedo.textureId = Okay::INVALID_UINT;

	Okay::updateBuffer(m_pRenderDataBuffer, &m_renderData, sizeof(RenderData));
	Okay::getDeviceContext()->Draw((node.triEnd - node.triStart) * 3u, 0u);
}

void DebugRenderer::drawOctTreeNodeBoundingBox(uint32_t nodeIdx, float colourStrength)
{
	OKAY_ASSERT(nodeIdx < (uint32_t)m_pRayTracer->getOctTreeNodes().size());

	m_renderData.albedo.textureId = Okay::INVALID_UINT;
	m_renderData.albedo.colour = OCT_NODE_COLOUR /** colourStrength*/;
	m_renderData.nodeIdx = nodeIdx;
	m_renderData.mode = 1;

	Okay::updateBuffer(m_pRenderDataBuffer, &m_renderData, sizeof(RenderData));
	Okay::getDeviceContext()->Draw(m_bvhNodeNumVerticies, 0u);
}

template<typename NodeFunction, typename GPUNodeType, typename... Args>
void DebugRenderer::executeDrawMode(DrawMode drawMode, const std::vector<GPUNodeType>& nodeList, uint32_t nodeIdx, uint32_t numChildren, NodeFunction pDrawFunc, Args... args)
{
	switch (drawMode)
	{
	case DrawMode::DrawSingle:
	{
		(this->*pDrawFunc)(nodeIdx, 1.f - nodeIdx / (float)nodeList.size(), std::forward<Args>(args)...);
		break;
	}
	case DrawMode::DrawWithChildren:
	{
		const GPUNodeType& selectedNode = nodeList[nodeIdx];
		
		(this->*pDrawFunc)(nodeIdx, 1.f - nodeIdx / (float)nodeList.size(), std::forward<Args>(args)...);
		if (selectedNode.firstChildIdx != Okay::INVALID_UINT)
		{
			for (uint32_t i = 0; i < numChildren; i++)
			{
				(this->*pDrawFunc)(selectedNode.firstChildIdx + i, 1.f - (selectedNode.firstChildIdx + i) / (float)nodeList.size(), std::forward<Args>(args)...);
			}
		}
		break;
	}
	case DrawMode::DrawWithDecendants:
	{
		std::stack<uint32_t> nodes;
		nodes.push(nodeIdx);
		
		while (!nodes.empty())
		{
			uint32_t currentIdx = nodes.top();
			const GPUNodeType& currentNode = nodeList[currentIdx];
			nodes.pop();
		
			if (currentNode.firstChildIdx != Okay::INVALID_UINT)
			{
				// Need to solve this numChildren thing, should maybe just make a
				// if constexpr (std::is_same<GPUNodeType, ...>())
				// if that even works..?

				uint32_t numChildren = 0u;
				if constexpr (std::is_same<GPUNodeType, GPU_OctTreeNode>())
					numChildren = currentNode.numChildren;
				else if constexpr (std::is_same<GPUNodeType, GPUNode>())
					numChildren = 2u;

				for (uint32_t i = 0; i < numChildren; i++)
				{
					nodes.push(currentNode.firstChildIdx + i);
				}
			}
		
			(this->*pDrawFunc)(currentIdx, 1.f - currentIdx / (float)nodeList.size(), std::forward<Args>(args)...);
		}
		break;
	}

	default:
		break;
	}
}

void DebugRenderer::onResize()
{
	glm::uvec2 newDims = m_pTargetTexture->getDimensions();
	m_viewport.Width = (float)newDims.x;
	m_viewport.Height = (float)newDims.y;
}