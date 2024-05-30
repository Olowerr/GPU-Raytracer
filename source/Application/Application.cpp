#include "Application.h"
#include "DirectX/DX11.h"
#include "Utilities.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Input.h"

#include "stb/stb_image_write.h"

#include "glm/gtx/quaternion.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"


Application::Application()
	:m_accumulationTime(0.f), m_debugSelectedNodeIdx(Okay::INVALID_UINT)
{
	glfwInitHint(GLFW_CLIENT_API, GLFW_NO_API);

	bool glInit = glfwInit();
	OKAY_ASSERT(glInit);

	Okay::initiateDX11();
	m_window.initiate(1600u, 900u, "GPU Raytracer");
	Okay::initiateImGui(m_window);

	m_target.initiate(1600u, 900u, TextureFormat::F_8X4);

	m_resourceManager.importFile("resources/meshes/cube.fbx");
	m_resourceManager.importFile("resources/textures/wood/whnfeb2_2K_Albedo.jpg");
	m_resourceManager.importFile("resources/textures/wood/whnfeb2_2K_Roughness.jpg");
	m_resourceManager.importFile("resources/textures/wood/whnfeb2_2K_Specular.jpg");
	m_resourceManager.importFile("resources/textures/wood/whnfeb2_2K_Normal.jpg");

	//m_resourceManager.importFile("resources/meshes/revolver.fbx");
	m_resourceManager.importFile("resources/textures/rev/rev_albedo.png");
	m_resourceManager.importFile("resources/textures/rev/rev_roughness.png");
	m_resourceManager.importFile("resources/textures/rev/rev_metallic.png");
	m_resourceManager.importFile("resources/textures/rev/rev_normalMap.png");

	m_gpuResourceManager.initiate(m_resourceManager);
	m_gpuResourceManager.loadResources("resources/environmentMaps/Skybox2.jpg");

	m_rayTracer.initiate(m_target, m_gpuResourceManager);
	m_rayTracer.setScene(m_scene);

	m_debugRenderer.initiate(m_target, m_gpuResourceManager);
	m_debugRenderer.setScene(m_scene);
}

Application::~Application()
{
	m_window.shutdown();
	m_rayTracer.shutdown();

	glfwTerminate();
	Okay::shutdownImGui();
	Okay::shutdownDX11();
}

void Application::run()
{
	static ID3D11RenderTargetView* nullRTV = nullptr;

	Entity camera = m_scene.createEntity();
	camera.addComponent<Camera>(90.f, 0.1f);
	camera.getComponent<Transform>().position.x = 60.f;
	camera.getComponent<Transform>().rotation.y = -90.f;

	{
		Entity ent = m_scene.createEntity();
		
		MeshComponent& meshComp = ent.addComponent<MeshComponent>();
		meshComp.meshID = 0;

		Material& mat = meshComp.material;
		mat.albedo.textureId = 0u;
		mat.roughness.textureId = 1u;
		mat.specular.textureId = 2u;
		mat.normalMapIdx = 3u;
	}
	
	//{
	//	Entity ent = m_scene.createEntity();
	//	
	//	MeshComponent& meshComp = ent.addComponent<MeshComponent>();
	//	meshComp.meshID = 1;
	//
	//	Material& mat = meshComp.material;
	//	mat.albedo.textureId = 4u;
	//	mat.roughness.textureId = 5u;
	//	mat.metallic.textureId = 6u;
	//	mat.normalMapIdx = 7u;
	//}

#if 1
	glm::vec3 colours[3] =
	{
		{0.05f, 0.05f, 0.95f},
		{0.05f, 0.95f, 0.05f},
		{0.95f, 0.05f, 0.05f},
	};

	uint32_t num = 10u;
	float dist = 3.f;
	float offset = (num - 1u) * 0.5f * dist;
	for (uint32_t y = 0; y < num; y++)
	{
		for (uint32_t x = 0; x < num; x++)
		{
			Entity entity = m_scene.createEntity();

			Sphere& sphere = entity.addComponent<Sphere>();
			sphere.radius = dist * 0.4f;
			sphere.material.roughness = x / (num - 1.f);
			sphere.material.metallic = y / (num - 1.f);

			Transform& tra = entity.getComponent<Transform>();
			tra.position = glm::vec3(x * dist - offset, y * dist - offset, 10.f);


			uint32_t id = x + y * num;
			float t = (float)id / float(num * num);
			int idx1 = int(t * (3 - 1));
			int idx2 = (idx1 + 1) % 3;
			float tBlend = (t * (3 - 1)) - idx1;

			sphere.material.albedo.colour = glm::mix(colours[idx1], colours[idx2], tBlend); // glm::vec3(0.8f, 0.8f, 0.8f);//
		}
	}
#endif

	while (m_window.isOpen())
	{
		m_window.processMessages();
		Okay::newFrameImGui();

		updateImGui();
		updateCamera();

		if (m_useRasterizer)
			m_debugRenderer.render(m_rasterizerDrawObjects);
		else
			m_rayTracer.render();

		if (m_drawNodeGeometry)
			m_debugRenderer.renderNodeGeometry(m_debugSelectedEntity, m_debugSelectedNodeIdx);
		if (m_drawNodeBBs)
			m_debugRenderer.renderNodeBBs(m_debugSelectedEntity, m_debugSelectedNodeIdx);

		Okay::endFrameImGui();
		m_window.present();
	}
}

void Application::displayComponents(Entity entity)
{
	bool resetAcu = false;

	ImGui::Text("Transform");

	Transform& transform = entity.getComponent<Transform>();
	if (ImGui::DragFloat3("Position", &transform.position.x, 0.1f))
		resetAcu = true;
	if (ImGui::DragFloat3("Rotation", &transform.rotation.x, 0.1f))
		resetAcu = true;
	if (ImGui::DragFloat3("Scale", &transform.scale.x, 0.1f))
		resetAcu = true;


	ImGui::Separator();

	Material* pMaterial = nullptr;

	if (Sphere* pSphere = entity.tryGetComponent<Sphere>())
	{
		ImGui::Text("Sphere");
		ImGui::SameLine();
		bool deleted = ImGui::Button("X");

		if (ImGui::DragFloat("Radius", &pSphere->radius, 0.1f))
			resetAcu = true;

		pMaterial = &pSphere->material;
		if (deleted)
		{
			entity.removeComponent<Sphere>();
			resetAcu = true;
		}
		else
		{
			pMaterial = &pSphere->material;
		}
	}

	else if (MeshComponent* pMeshComp = entity.tryGetComponent<MeshComponent>())
	{
		ImGui::Text("Mesh");
		ImGui::SameLine();
		bool deleted = ImGui::Button("X");

		const uint32_t maxMeshId = m_resourceManager.getCount<Mesh>() - 1;
		if (ImGui::DragInt("MeshID", (int*)&pMeshComp->meshID, 0.1f, 0, maxMeshId))
			resetAcu = true;

		if (maxMeshId == 0) // If maxMeshId is 0 then ImGui thinks there should be no max value for the input
		{
			pMeshComp->meshID = 0;
		}

		if (deleted)
		{
			entity.removeComponent<MeshComponent>();
			resetAcu = true;
		}
		else
		{
			pMaterial = &pMeshComp->material;
		}
	}

	if (pMaterial)
	{
		ImGui::Separator();

		ImGui::Text("Material");
		
		if (ImGui::ColorEdit3("Colour", glm::value_ptr(pMaterial->albedo.colour)))					resetAcu = true;
		if (ImGui::ColorEdit3("Emission Colour", glm::value_ptr(pMaterial->emissionColour)))		resetAcu = true;
		if (ImGui::DragFloat("Emission Power", &pMaterial->emissionPower, 0.01f))					resetAcu = true;
		if (ImGui::DragFloat("Roughness", &pMaterial->roughness.colour, 0.01f, 0.f, 1.f))			resetAcu = true;
		if (ImGui::DragFloat("Metallic", &pMaterial->metallic.colour, 0.01f, 0.f, 1.f))				resetAcu = true;
		if (ImGui::DragFloat("Specular", &pMaterial->specular.colour, 0.01f, 0.f, 1.f))				resetAcu = true;
		if (ImGui::DragFloat("Transparency", &pMaterial->transparency, 0.01f, 0.f, 1.f))			resetAcu = true;
		if (ImGui::DragFloat("Refraction Idx", &pMaterial->indexOfRefraction, 0.01f, 1.f, 5.f))		resetAcu = true;
	}

	if (DirectionalLight* pDirLight = entity.tryGetComponent<DirectionalLight>())
	{
		ImGui::Text("Directional Light");
		ImGui::SameLine();
		bool deleted = ImGui::Button("X");

		if (ImGui::ColorEdit3("Colour", glm::value_ptr(pDirLight->colour)))										resetAcu = true;
		if (ImGui::DragFloat("Intensity", &pDirLight->intensity, 0.01f, 0.f, 10.f))								resetAcu = true;
		if (ImGui::DragFloat("Specular Strength", &pDirLight->specularStrength, 0.f, 10.f))						resetAcu = true;
		if (ImGui::DragFloat("Penumbra size factor", &pDirLight->penumbraSizeModifier, 0.001f, 0.f, 10.f))		resetAcu = true;

		if (deleted)
		{
			entity.removeComponent<DirectionalLight>();
			resetAcu = true;
		}
	}

	if (PointLight* pPointLight = entity.tryGetComponent<PointLight>())
	{
		ImGui::Text("Point Light");
		ImGui::SameLine();
		bool deleted = ImGui::Button("X");

		if (ImGui::ColorEdit3("Colour", glm::value_ptr(pPointLight->colour)))								resetAcu = true;
		if (ImGui::DragFloat("Intensity", &pPointLight->intensity, 0.01f, 0.f, 10.f))						resetAcu = true;
		if (ImGui::DragFloat("Specular Strength", &pPointLight->specularStrength, 0.01f, 0.f, 10.f))		resetAcu = true;
		if (ImGui::DragFloat2("Attenuation", glm::value_ptr(pPointLight->attenuation), 0.01f, 0.f, 1.f))	resetAcu = true;
		if (ImGui::DragFloat("Penumbra radius", &pPointLight->penumbraRadius, 0.01f, 0.f, 10.f))			resetAcu = true;

		if (deleted)
		{
			entity.removeComponent<PointLight>();
			resetAcu = true;
		}
	}
	
	if (SpotLight* pSpotLight = entity.tryGetComponent<SpotLight>())
	{
		ImGui::Text("Spot Light");
		ImGui::SameLine();
		bool deleted = ImGui::Button("X");

		if (ImGui::ColorEdit3("Colour", glm::value_ptr(pSpotLight->colour)))								resetAcu = true;
		if (ImGui::DragFloat("Intensity", &pSpotLight->intensity, 0.01f, 0.f, 10.f))						resetAcu = true;
		if (ImGui::DragFloat("Specular Strength", &pSpotLight->specularStrength, 0.01f, 0.f, 10.f))			resetAcu = true;
		if (ImGui::DragFloat2("Attenuation", glm::value_ptr(pSpotLight->attenuation), 0.01f, 0.f, 1.f))		resetAcu = true;
		if (ImGui::DragFloat("Penumbra radius", &pSpotLight->penumbraRadius, 0.01f, 0.f, 10.f))				resetAcu = true;
		if (ImGui::DragFloat("Max angle", &pSpotLight->maxAngle, 0.01f, 0.f, 360.f))						resetAcu = true;

		if (deleted)
		{
			entity.removeComponent<SpotLight>();
			resetAcu = true;
		}
	}

	if (resetAcu)
	{
		m_rayTracer.resetAccumulation();
		m_accumulationTime = 0.f;
	}
}

void Application::updateImGui()
{
	static bool accumulate = true;
	m_accumulationTime += ImGui::GetIO().DeltaTime;
	m_accumulationTime *= (float)accumulate * (float)!m_useRasterizer;

	bool resetAcu = false;

	if (ImGui::Begin("Main"))
	{
		glm::uvec2 textureDims = m_target.getDimensions();
		float aspectRatio = textureDims.x / (float)textureDims.y;
		float windowWidth = ImGui::GetWindowWidth();

		ImVec2 imageDims = ImVec2(windowWidth, windowWidth / aspectRatio);
		ImGui::Image(*m_target.getSRV(), imageDims);
	}
	ImGui::End();

	if (ImGui::Begin("Rendering"))
	{
		ImGui::PushItemWidth(-120.f);

		ImGui::Text("FPS: %.3f", 1.f / ImGui::GetIO().DeltaTime);
		ImGui::Text("MS: %.3f", ImGui::GetIO().DeltaTime * 1000.f);

		ImGui::Separator();

		static const char* resolutionLables[] = { "1024x576", "1600x900", "1920x1080", "3840x2160", "7680x4320" };
		static const glm::uvec2 resolutionValues[] = { {1024, 576}, {1600, 900}, {1920, 1080}, {3840, 2160}, {7680, 4320} };
		static int currentIdx = 1;

		if (ImGui::Combo("Resolution", &currentIdx, resolutionLables, IM_ARRAYSIZE(resolutionLables)))
		{
			const glm::uvec2& resolution = resolutionValues[currentIdx];
			m_target.resize(resolution.x, resolution.y);
			m_rayTracer.onResize();
			m_debugRenderer.onResize();
		}

		if (ImGui::Button("Save screenshot"))
		{
			saveScreenshot();
		}

		ImGui::Separator();

		if (ImGui::Button("Reload Shaders"))
		{
			m_rayTracer.reloadShaders();
			m_debugRenderer.reloadShaders();
			resetAcu = true;
		}

		ImGui::Separator();

		ImGui::DragInt("BVH Max triangles", (int*)&m_gpuResourceManager.getMaxBvhLeafTriangles(), 1, 1, Okay::INVALID_UINT / 2);
		ImGui::DragInt("BVH Max depth", (int*)&m_gpuResourceManager.getMaxBvhDepth(), 0.3f, 1, Okay::INVALID_UINT / 2);

		if (ImGui::Button("Rebuild BVH tree"))
		{
			m_gpuResourceManager.loadMeshAndBvhData();
		}

		ImGui::Separator();

		{ // Mesh rendering debugging
			if (ImGui::Checkbox("Rasterizer", &m_useRasterizer)) resetAcu = true;

			ImGui::BeginDisabled(!m_useRasterizer);
			ImGui::Checkbox("Draw Objects", &m_rasterizerDrawObjects);
			ImGui::EndDisabled();

			static int mode = DebugRenderer::BvhNodeDrawMode::DrawWithDecendants;
			static int enttID = -1;

			if (ImGui::DragInt("Entity", &enttID, 0.1f, -1, (int)m_scene.getRegistry().alive()))
			{
				m_debugSelectedEntity = enttID != -1 ? Entity(entt::entity(enttID), &m_scene.getRegistry()) : Entity();
				m_debugSelectedNodeIdx = 0u;
			}

			const MeshComponent* pMeshComp = m_debugSelectedEntity ? m_debugSelectedEntity.tryGetComponent<MeshComponent>() : nullptr;
			uint32_t nodeCap = pMeshComp ? m_gpuResourceManager.getMeshDescriptors()[pMeshComp->meshID].numBvhNodes : 1u;
			
			ImGui::BeginDisabled(!pMeshComp);
			ImGui::DragInt("Node Idx", (int*)&m_debugSelectedNodeIdx, 0.1f, 0, nodeCap - 1);
			m_debugSelectedNodeIdx = nodeCap == 1 && m_debugSelectedNodeIdx > 0 ? 0u : m_debugSelectedNodeIdx;
			ImGui::EndDisabled();

			ImGui::Checkbox("Draw Node BBs", &m_drawNodeBBs);
			ImGui::Checkbox("Draw Node Gemoetry", &m_drawNodeGeometry);

			ImGui::Text("Node Draw Mode");
			ImGui::RadioButton("Draw None", &mode, DebugRenderer::BvhNodeDrawMode::None);
			ImGui::RadioButton("Draw Single", &mode, DebugRenderer::BvhNodeDrawMode::DrawSingle);
			ImGui::RadioButton("Draw With Children", &mode, DebugRenderer::BvhNodeDrawMode::DrawWithChildren);
			ImGui::RadioButton("Draw With Decendants", &mode, DebugRenderer::BvhNodeDrawMode::DrawWithDecendants);
			m_debugRenderer.setBvhNodeDrawMode(DebugRenderer::BvhNodeDrawMode(mode));

			ImGui::Separator();
		}

		{ // Raytracer
			ImGui::Text("Accumulation Frames: %u", m_rayTracer.getNumAccumulationFrames());
			ImGui::Text("Accumulation Time: %.2f", m_accumulationTime);
			if (ImGui::Checkbox("Accumulate", &accumulate))
			{
				m_rayTracer.toggleAccumulation(accumulate);
			}

			if (ImGui::Button("Reset Accumulation")) resetAcu = true;

			ImGui::Separator();

			if (ImGui::DragFloat("DOF Strength", &m_rayTracer.getDOFStrength(), 0.05f, 0.f, 10.f)) resetAcu = true;
			if (ImGui::DragFloat("DOF Distance", &m_rayTracer.getDOFDistance(), 0.05f, 0.f, 1000.f)) resetAcu = true;

			ImGui::Separator();
		}

		ImGui::PopItemWidth();
	}
	ImGui::End();


	if (ImGui::Begin("Entities"))
	{
		if (ImGui::Button("Create"))
		{
			m_selectedEntity = m_scene.createEntity();
			resetAcu = true;
		}

		ImGui::SameLine();

		ImGui::BeginDisabled(!m_selectedEntity);
		if (ImGui::Button("Remove"))
		{
			m_scene.destroyEntity(m_selectedEntity);
			m_selectedEntity = Entity();
			resetAcu = true;
		}
		ImGui::EndDisabled();

		ImGui::Separator();

		auto transformView = m_scene.getRegistry().view<Transform>();
		for (entt::entity entity : transformView)
		{
			if (ImGui::Selectable(std::to_string((uint32_t)entity).c_str(), entity == (entt::entity)m_selectedEntity.getID()))
			{
				m_selectedEntity = Entity(entity, &m_scene.getRegistry());
			}
		}
	}
	ImGui::End();

	if (ImGui::Begin("Components"))
	{
		if (m_selectedEntity)
		{
			if (ImGui::BeginCombo("##AddComps", "Add Component"))
			{
				createComponentSelection<MeshComponent>(m_selectedEntity, "MeshComponent");
				createComponentSelection<Sphere>(m_selectedEntity, "Sphere");
				createComponentSelection<PointLight>(m_selectedEntity, "PointLight");
				createComponentSelection<DirectionalLight>(m_selectedEntity, "DirectionalLight");
				createComponentSelection<SpotLight>(m_selectedEntity, "SpotLight");

				ImGui::EndCombo();
			}

			ImGui::Separator();

			displayComponents(m_selectedEntity);
		}
	}
	ImGui::End();

	if (resetAcu)
	{
		m_rayTracer.resetAccumulation();
		m_accumulationTime = 0.f;
	}
}

void Application::updateCamera()
{
	static float rotationSpeed = 0.1f;
	static float moveSpeed = 20.f;
	Entity camera = m_scene.getFirstCamera();
	OKAY_ASSERT(camera.isValid());

	Transform& camTra = camera.getComponent<Transform>();

	if (ImGui::Begin("Camera"))
	{
		bool resetAcu = false;

		ImGui::PushItemWidth(-120.f);

		if (ImGui::DragFloat3("Camera Pos", glm::value_ptr(camTra.position), 0.01f))	resetAcu = true;
		if (ImGui::DragFloat3("Camera Rot", glm::value_ptr(camTra.rotation), 0.1f))		resetAcu = true;

		ImGui::Separator();

		ImGui::DragFloat("Rotation Speed", &rotationSpeed, 0.02f);
		ImGui::DragFloat("Move Speed", &moveSpeed, 0.2f);

		ImGui::PopItemWidth();

		if (resetAcu)
		{
			m_rayTracer.resetAccumulation();
			m_accumulationTime = 0.f;
		}
	}
	ImGui::End();


	static MouseMode mouseMode = MouseMode::Normal;
	if (Input::isKeyPressed(Key::E))
	{
		mouseMode = mouseMode == MouseMode::Normal ? MouseMode::Locked : MouseMode::Normal;
		Input::setMouseMode(mouseMode);
	}

	if (mouseMode == MouseMode::Normal)
		return;

	const glm::vec2 mouseDelta = Input::getMouseDelta();

	camTra.rotation.x += mouseDelta.y * rotationSpeed;
	camTra.rotation.y += mouseDelta.x * rotationSpeed;
	camTra.rotation.x = glm::clamp(camTra.rotation.x, -89.f, 89.f);

	const float xInput = (float)Input::isKeyDown(Key::D) - (float)Input::isKeyDown(Key::A);
	const float yInput = (float)Input::isKeyDown(Key::Space) - (float)Input::isKeyDown(Key::LeftControl);
	const float zInput = (float)Input::isKeyDown(Key::W) - (float)Input::isKeyDown(Key::S);

	const glm::mat3 rotationMatrix = glm::toMat3(glm::quat(glm::radians(camTra.rotation)));

	const glm::vec3& fwd = rotationMatrix[2];
	const glm::vec3& right = rotationMatrix[0];

	const float frameSpeed = ImGui::GetIO().DeltaTime * moveSpeed * (Input::isKeyDown(Key::LeftShift) ? 3.f : 1.f);

	camTra.position += (right * xInput + fwd * zInput) * frameSpeed;
	camTra.position.y += yInput * frameSpeed;


	if (xInput || yInput || zInput || mouseDelta.x || mouseDelta.y)
	{
		m_rayTracer.resetAccumulation();
		m_accumulationTime = 0.f;
	}
}

void Application::saveScreenshot()
{
	ID3D11Texture2D* sourceBuffer = *m_target.getBuffer();

	D3D11_TEXTURE2D_DESC desc{};
	sourceBuffer->GetDesc(&desc);
	desc.Usage = D3D11_USAGE_STAGING;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.BindFlags = 0u;

	ID3D11Texture2D* stagingBuffer = nullptr;
	Okay::getDevice()->CreateTexture2D(&desc, nullptr, &stagingBuffer);
	if (!stagingBuffer)
		return;

	ID3D11DeviceContext* pDevCon = Okay::getDeviceContext();
	pDevCon->CopyResource(stagingBuffer, sourceBuffer);

	D3D11_MAPPED_SUBRESOURCE sub{};
	pDevCon->Map(stagingBuffer, 0u, D3D11_MAP_READ, 0u, &sub);

	stbi_write_png("render.png", (int)desc.Width, (int)desc.Height, 4, sub.pData, (int)sub.RowPitch);

	pDevCon->Unmap(stagingBuffer, 0u);
	DX11_RELEASE(stagingBuffer);
}
