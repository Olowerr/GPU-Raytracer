#include "DebugRenderer.h"
#include "Scene/Scene.h"
#include "GPUResourceManager.h"
#include "ResourceManager.h"
#include "shaders/ShaderResourceRegisters.h"

#include "Importer.h"

DebugRenderer::DebugRenderer()
	:m_pScene(nullptr), m_pGpuResourceManager(nullptr), m_pResourceManager(nullptr),
	m_pVS(nullptr), m_pPS(nullptr), m_pDSV(nullptr), m_pRTV(nullptr), m_viewport(), m_pRenderDataBuffer(nullptr),
	m_pShereTriBuffer(nullptr), m_sphereNumVerticies(0u)

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
}

void DebugRenderer::initiate(ID3D11Texture2D* pTarget, const GPUResourceManager& pGpuResourceManager)
{
	OKAY_ASSERT(pTarget);

	shutdown();
	m_pGpuResourceManager = &pGpuResourceManager;

	bool success = false;
	ID3D11Device* pDevice = Okay::getDevice();

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
}

template<typename ShaderType>
void reloadShader(std::string_view path, ShaderType** ppShader)
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
}

void DebugRenderer::render()
{
	bindPipeline();

	// Camera data
	{ 
		const Entity camEntity = m_pScene->getFirstCamera();
		const Transform& camTra = camEntity.getComponent<Transform>();
		const Camera& camData = camEntity.getComponent<Camera>();

		const glm::mat3 rotationMatrix = glm::toMat3(glm::quat(glm::radians(camTra.rotation)));
		const glm::vec3& camForward = rotationMatrix[2];
	
		m_renderData.cameraViewProjectMatrix = glm::transpose(
			glm::perspectiveFovLH(glm::radians(camData.fov), m_viewport.Width, m_viewport.Height, camData.nearZ, camData.farZ) *
				glm::lookAtLH(camTra.position, camTra.position + camForward, glm::vec3(0.f, 1.f, 0.f)));
	}

	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();

	const entt::registry& reg = m_pScene->getRegistry();
	const std::vector<MeshDesc>& meshDescs = m_pGpuResourceManager->getMeshDescriptors();

	auto meshView = reg.view<MeshComponent, Transform>();
	for (entt::entity entity : meshView)
	{
		auto [meshComp, transform] = meshView[entity];

		const MeshDesc& desc = meshDescs[meshComp.meshID];

		m_renderData.objectWorldMatrix = glm::transpose(transform.calculateMatrix());
		m_renderData.vertStartIdx = desc.startIdx * 3u;
		m_renderData.albedo = meshComp.material.albedo;

		Okay::updateBuffer(m_pRenderDataBuffer, &m_renderData, sizeof(RenderData));

		pDevCon->Draw((desc.endIdx - desc.startIdx) * 3u, 0u);
	}

	ID3D11ShaderResourceView* pOrigTriangleBuffer = nullptr;
	pDevCon->VSGetShaderResources(RM_TRIANGLE_DATA_SLOT, 1u, &pOrigTriangleBuffer);
	pDevCon->VSSetShaderResources(RM_TRIANGLE_DATA_SLOT, 1u, &m_pShereTriBuffer);

	auto sphereView = reg.view<Sphere, Transform>();
	for (entt::entity entity : sphereView)
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

	pDevCon->VSSetShaderResources(RM_TRIANGLE_DATA_SLOT, 1u, &pOrigTriangleBuffer);
	DX11_RELEASE(pOrigTriangleBuffer);
}

void DebugRenderer::bindPipeline()
{
	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();
	
	static const glm::vec4 clearColor = glm::vec4(0.f, 0.f, 0.f, 0.f);
	pDevCon->ClearRenderTargetView(m_pRTV, &clearColor.x);
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
