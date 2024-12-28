#include "Application.h"
#include "DirectX/DX11.h"
#include "Utilities.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Input.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include "glm/gtx/quaternion.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"


Application::Application()
	:m_accumulationTime(0.f), m_debugSelectedBvhNodeIdx(Okay::INVALID_UINT), m_debugSelectedOctNodeIdx(Okay::INVALID_UINT)
{
	glfwInitHint(GLFW_CLIENT_API, GLFW_NO_API);

	bool glInit = glfwInit();
	OKAY_ASSERT(glInit);

	Okay::initiateDX11();
	m_window.initiate(1600u, 900u, "GPU Raytracer");
	Okay::initiateImGui(m_window);

	m_target.initiate(1600u, 900u, TextureFormat::F_8X4);

	m_resourceManager.loadMesh("resources/meshes/cube.fbx");
	//m_resourceManager.loadTexture("resources/textures/wood/whnfeb2_2K_Albedo.jpg");
	//m_resourceManager.loadTexture("resources/textures/wood/whnfeb2_2K_Roughness.jpg");
	//m_resourceManager.loadTexture("resources/textures/wood/whnfeb2_2K_Specular.jpg");
	//m_resourceManager.loadTexture("resources/textures/wood/whnfeb2_2K_Normal.jpg");

	//m_resourceManager.loadMesh("resources/meshes/revolver.fbx");
	//m_resourceManager.loadTexture("resources/textures/rev/rev_albedo.png");
	//m_resourceManager.loadTexture("resources/textures/rev/rev_roughness.png");
	//m_resourceManager.loadTexture("resources/textures/rev/rev_metallic.png");
	//m_resourceManager.loadTexture("resources/textures/rev/rev_normalMap.png");

	//loadMeshesAsEntities("resources/meshes/ikea_glass.obj", "", 10.f);

	loadMeshesAsEntities("resources/Sponza/sponza.obj", "resources/Sponza/", 0.07f);
	//loadMeshesAsEntities("resources/meshes/dragon_80K.obj", "", 20.f);


	m_rayTracer.initiate(m_target, m_resourceManager, "resources/environmentMaps/Skybox2.jpg");
	m_rayTracer.setScene(m_scene);

	m_debugRenderer.initiate(m_target, m_rayTracer);
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
	Entity camera = m_scene.createEntity();
	camera.addComponent<Camera>(90.f, 0.1f);
	camera.getComponent<Transform>().position.x = 60.f;
	camera.getComponent<Transform>().rotation.y = -90.f;

	//{
	//	Entity ent = m_scene.createEntity();
	//
	//	MeshComponent& meshComp = ent.addComponent<MeshComponent>();
	//	meshComp.meshID = 0;
	//
	//	Material& mat = meshComp.material;
	//	mat.albedo.textureId = 0u;
	//	mat.roughness.textureId = 1u;
	//	mat.specular.textureId = 2u;
	//	mat.normalMapIdx = 3u;
	//}

#if 0

	uint32_t num = 5000u;
	glm::vec3 bounds = glm::vec3(500.f, 500.f, 500.f);
	for (uint32_t i = 0; i < num; i++)
	{
		Entity ent = m_scene.createEntity();
		
		MeshComponent& meshComp = ent.addComponent<MeshComponent>();
		meshComp.meshID = 0;
	
		float x = ((float)rand() / RAND_MAX) * bounds.x - bounds.x * 0.5f;
		float y = ((float)rand() / RAND_MAX) * bounds.y - bounds.y * 0.5f;
		float z = ((float)rand() / RAND_MAX) * bounds.z - bounds.z * 0.5f;
		ent.getComponent<Transform>().position = glm::vec3(x, y, z);
	}

#endif

	m_rayTracer.createOctTree(m_scene, m_maxCullingTreeDepth, m_maxCullingTreeLeafEntities);

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
			m_debugRenderer.renderBvhNodeGeometry(m_debugSelectedEntity, m_debugSelectedBvhNodeIdx);
		if (m_drawBvhNodeBBs)
			m_debugRenderer.renderBvhNodeBBs(m_debugSelectedEntity, m_debugSelectedBvhNodeIdx);
		if (m_octTreeDrawMode != DebugRenderer::DrawMode::None && m_debugSelectedOctNodeIdx != Okay::INVALID_UINT)
			m_debugRenderer.renderOctTreeNodeBBs(m_debugSelectedOctNodeIdx);

		Okay::endFrameImGui();
		m_window.present();
	}
}

void Application::loadMeshesAsEntities(std::string_view filePath, std::string_view texturesPath, float scale)
{
	std::vector<ResourceManager::ObjectDecription> objectDescriptions;
	bool success = m_resourceManager.importAssets(filePath, objectDescriptions, texturesPath, scale);
	OKAY_ASSERT(success);

	for (uint32_t i = 0; i < (uint32_t)objectDescriptions.size(); i++)
	{
		ResourceManager::ObjectDecription& objectDesc = objectDescriptions[i];

		Entity entity = m_scene.createEntity();
		MeshComponent& meshComp = entity.addComponent<MeshComponent>();
		Material& material = meshComp.material;

		meshComp.meshID = objectDesc.meshId;
		material.albedo.textureId = objectDesc.albedoTextureId;
		material.roughness.textureId = objectDesc.rougnessTextureId;
		material.metallic.textureId = objectDesc.metallicTextureId;
		material.specular.textureId = objectDesc.specularTextureId;
		material.normalMapIdx = objectDesc.normalTextureId;
	}
}

void Application::displayComponents(Entity entity)
{
	bool resetAcu = false;
	int imguiId = 0;

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
		ImGui::PushID(imguiId++);

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

		ImGui::PopID();
	}

	else if (MeshComponent* pMeshComp = entity.tryGetComponent<MeshComponent>())
	{
		ImGui::PushID(imguiId++);

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
		
		ImGui::PopID();
	}

	if (pMaterial)
	{
		ImGui::PushID(imguiId++);

		ImGui::Separator();

		ImGui::Text("Material");

		if (ImGui::ColorEdit3("Colour", glm::value_ptr(pMaterial->albedo.colour)))					resetAcu = true;
		if (ImGui::ColorEdit3("Emission Colour", glm::value_ptr(pMaterial->emissionColour)))		resetAcu = true;
		if (ImGui::DragFloat("Emission Power", &pMaterial->emissionPower, 0.01f, 0.f, FLT_MAX))		resetAcu = true;
		if (ImGui::DragFloat("Roughness", &pMaterial->roughness.colour, 0.01f, 0.f, 1.f))			resetAcu = true;
		if (ImGui::DragFloat("Metallic", &pMaterial->metallic.colour, 0.01f, 0.f, 1.f))				resetAcu = true;
		if (ImGui::DragFloat("Specular", &pMaterial->specular.colour, 0.01f, 0.f, 1.f))				resetAcu = true;
		if (ImGui::DragFloat("Transparency", &pMaterial->transparency, 0.01f, 0.f, 1.f))			resetAcu = true;
		if (ImGui::DragFloat("Refraction Idx", &pMaterial->indexOfRefraction, 0.01f, 1.f, 5.f))		resetAcu = true;

		ImGui::PopID();
	}

	if (DirectionalLight* pDirLight = entity.tryGetComponent<DirectionalLight>())
	{
		ImGui::PushID(imguiId++);

		ImGui::Text("Directional Light");
		ImGui::SameLine();
		bool deleted = ImGui::Button("X");

		if (ImGui::ColorEdit3("Colour", glm::value_ptr(pDirLight->colour)))								resetAcu = true;
		if (ImGui::DragFloat("Intensity", &pDirLight->intensity, 0.01f, 0.f, FLT_MAX))					resetAcu = true;
		if (ImGui::DragFloat("Effective Angle", &pDirLight->effectiveAngle, 0.001f, 0.f, FLT_MAX))		resetAcu = true;

		if (deleted)
		{
			entity.removeComponent<DirectionalLight>();
			resetAcu = true;
		}

		ImGui::PopID();
	}

	if (PointLight* pPointLight = entity.tryGetComponent<PointLight>())
	{
		ImGui::PushID(imguiId++);

		ImGui::Text("Point Light");
		ImGui::SameLine();
		bool deleted = ImGui::Button("X");

		if (ImGui::ColorEdit3("Colour", glm::value_ptr(pPointLight->colour)))				resetAcu = true;
		if (ImGui::DragFloat("Intensity", &pPointLight->intensity, 0.01f, 0.f, FLT_MAX))	resetAcu = true;
		if (ImGui::DragFloat("Radius", &pPointLight->radius, 0.01f, 0.f, FLT_MAX))			resetAcu = true;

		if (deleted)
		{
			entity.removeComponent<PointLight>();
			resetAcu = true;
		}

		ImGui::PopID();
	}

	if (SpotLight* pSpotLight = entity.tryGetComponent<SpotLight>())
	{
		ImGui::PushID(imguiId++);

		ImGui::Text("Spot Light");
		ImGui::SameLine();
		bool deleted = ImGui::Button("X");

		if (ImGui::ColorEdit3("Colour", glm::value_ptr(pSpotLight->colour)))			resetAcu = true;
		if (ImGui::DragFloat("Intensity", &pSpotLight->intensity, 0.01f, 0.f, FLT_MAX))	resetAcu = true;
		if (ImGui::DragFloat("Max angle", &pSpotLight->maxAngle, 0.01f, 0.f, 360.f))	resetAcu = true;

		if (deleted)
		{
			entity.removeComponent<SpotLight>();
			resetAcu = true;
		}

		ImGui::PopID();
	}

	if (resetAcu)
	{
		m_rayTracer.resetAccumulation();
		m_rayTracer.createOctTree(m_scene, m_maxCullingTreeDepth, m_maxCullingTreeLeafEntities);
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

		static float avgTimer = 0.f;
		static uint32_t frameCount = 0;

		static float avgFpsValue = 0.f;
		static float avgMsValue = 0.f;

		static float avgFpsDisplayValue = 0.f;
		static float avgMsDisplayValue = 0.f;

		float dt = ImGui::GetIO().DeltaTime;
		
		avgTimer += dt;
		frameCount++;

		float fps = 1.f / dt;
		float ms = dt * 1000.f;

		avgFpsValue += fps;
		avgMsValue += ms;

		if (avgTimer >= 1.f)
		{
			avgFpsDisplayValue = avgFpsValue / (float)frameCount;
			avgMsDisplayValue = avgMsValue / (float)frameCount;

			avgTimer = avgFpsValue = avgMsValue = 0.f;
			frameCount = 0;
		}

		ImGui::Text("FPS: %.3f (%.3f)", avgFpsDisplayValue, fps);
		ImGui::Text("MS: %.3f (%.3f)", avgMsDisplayValue, ms);

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

		ImGui::DragInt("BVH Max triangles", (int*)&m_maxBvhLeafTriangles, 1, 1, Okay::INVALID_UINT / 2);
		ImGui::DragInt("BVH Max depth", (int*)&m_maxBvhDepth, 0.3f, 1, Okay::INVALID_UINT / 2);

		if (ImGui::Button("Rebuild BVH tree"))
		{
			m_rayTracer.loadMeshAndBvhData(m_maxBvhDepth, m_maxBvhLeafTriangles);
		}


		ImGui::Separator();

		ImGui::DragInt("Oct Tree max leaf objects", (int*)&m_maxCullingTreeLeafEntities, 0.1f, 0, INT_MAX);
		ImGui::DragInt("Oct Tree max depth", (int*)&m_maxCullingTreeDepth, 0.1f, 0, INT_MAX);

		if (ImGui::Button("Rebuild oct tree"))
		{
			m_rayTracer.createOctTree(m_scene, m_maxCullingTreeDepth, m_maxCullingTreeLeafEntities);
		}

		ImGui::Separator();

		static int debugDisplayMode = RayTracer::DebugDisplayMode::None;
		int oldModeValue = debugDisplayMode;

		ImGui::Text("Debug Display Mode");
		ImGui::RadioButton("None", &debugDisplayMode, RayTracer::DebugDisplayMode::None);
		ImGui::RadioButton("BB Check Count", &debugDisplayMode, RayTracer::DebugDisplayMode::BBCheckCount);
		ImGui::RadioButton("Tri Check Count", &debugDisplayMode, RayTracer::DebugDisplayMode::TriCheckCount);

		bool changedMaxCount = ImGui::DragInt("Debug Check Max Count", (int*)&m_rayTracer.getDebugMaxCount(), 0.1f, 0, 100000);

		if ((debugDisplayMode != oldModeValue) || changedMaxCount)
		{
			m_rayTracer.setDebugMode(RayTracer::DebugDisplayMode(debugDisplayMode));
			m_rayTracer.resetAccumulation();
		}

		ImGui::Separator();

		{ // Mesh rendering debugging
			if (ImGui::Checkbox("Rasterizer", &m_useRasterizer)) resetAcu = true;

			ImGui::BeginDisabled(!m_useRasterizer);
			ImGui::Checkbox("Draw Objects", &m_rasterizerDrawObjects);
			ImGui::EndDisabled();

			static int mode = DebugRenderer::DrawMode::DrawWithDecendants;
			static int enttID = -1;

			if (ImGui::DragInt("Entity", &enttID, 0.1f, -1, (int)m_scene.getRegistry().alive()))
			{
				m_debugSelectedEntity = enttID != -1 ? Entity(entt::entity(enttID), &m_scene.getRegistry()) : Entity();
				m_debugSelectedBvhNodeIdx = 0u;
			}

			const MeshComponent* pMeshComp = m_debugSelectedEntity ? m_debugSelectedEntity.tryGetComponent<MeshComponent>() : nullptr;
			uint32_t nodeCap = pMeshComp ? m_rayTracer.getMeshDescriptors()[pMeshComp->meshID].numBvhNodes : 1u;
			
			ImGui::BeginDisabled(!pMeshComp);
			ImGui::DragInt("Node Idx", (int*)&m_debugSelectedBvhNodeIdx, 0.1f, 0, nodeCap - 1);
			m_debugSelectedBvhNodeIdx = nodeCap == 1 && m_debugSelectedBvhNodeIdx > 0 ? 0u : m_debugSelectedBvhNodeIdx;
			ImGui::EndDisabled();

			ImGui::Checkbox("Draw Node BBs", &m_drawBvhNodeBBs);
			ImGui::Checkbox("Draw Node Gemoetry", &m_drawNodeGeometry);

			ImGui::Text("Node Draw Mode");
			ImGui::RadioButton("Draw None", &mode, DebugRenderer::DrawMode::None);
			ImGui::RadioButton("Draw Single", &mode, DebugRenderer::DrawMode::DrawSingle);
			ImGui::RadioButton("Draw With Children", &mode, DebugRenderer::DrawMode::DrawWithChildren);
			ImGui::RadioButton("Draw With Decendants", &mode, DebugRenderer::DrawMode::DrawWithDecendants);
			m_debugRenderer.setBvhNodeDrawMode(DebugRenderer::DrawMode(mode));

			ImGui::Separator();
		}

		{
			static int mode = m_octTreeDrawMode;

			ImGui::PushID("octNodes");

			const std::vector<GPU_OctTreeNode>& octTreeNodes = m_rayTracer.getOctTreeNodes();

			ImGui::DragInt("Node Idx", (int*)&m_debugSelectedOctNodeIdx, 0.1f, 0, (int)octTreeNodes.size() - 1);
			m_debugSelectedOctNodeIdx = octTreeNodes.size() == 1 && m_debugSelectedOctNodeIdx > 0 ? 0u : m_debugSelectedOctNodeIdx;

			ImGui::Text("Oct Node Draw Mode");
			ImGui::RadioButton("Draw None", &mode, DebugRenderer::DrawMode::None);
			ImGui::RadioButton("Draw Single", &mode, DebugRenderer::DrawMode::DrawSingle);
			ImGui::RadioButton("Draw With Children", &mode, DebugRenderer::DrawMode::DrawWithChildren);
			ImGui::RadioButton("Draw With Decendants", &mode, DebugRenderer::DrawMode::DrawWithDecendants);

			m_octTreeDrawMode = DebugRenderer::DrawMode(mode);
			m_debugRenderer.setOctTreeNodeDrawMode(m_octTreeDrawMode);

			ImGui::PopID();
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

		auto transformView = m_scene.getRegistry().view<Transform>(entt::exclude<Camera>);
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
		m_rayTracer.createOctTree(m_scene, m_maxCullingTreeDepth, m_maxCullingTreeLeafEntities);
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
			m_rayTracer.createOctTree(m_scene, m_maxCullingTreeDepth, m_maxCullingTreeLeafEntities);
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

	void* textureData = nullptr;
	Okay::getCPUTextureData(sourceBuffer, &textureData);

	stbi_write_png("render.png", (int)desc.Width, (int)desc.Height, 4, textureData, desc.Width * 4);

	OKAY_DELETE_ARRAY(textureData);
}
