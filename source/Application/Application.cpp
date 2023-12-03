#include "Application.h"
#include "DirectX/DX11.h"
#include "Utilities.h"
#include "Scene/Components.h"

#include "ImGuiHelper.h"

#include "Input.h"

#include "glm/gtx/quaternion.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"


Application::Application()
	:m_pBackBuffer(nullptr), m_accumulationTime(0.f)
{
	glfwInitHint(GLFW_CLIENT_API, GLFW_NO_API);

	bool glInit = glfwInit();
	OKAY_ASSERT(glInit);

	Okay::initiateDX11();
	m_window.initiate(1600u, 900u, "GPU Raytracer");

	m_gpuResourceManager.initiate(m_resourceManager);

	m_rayTracer.initiate(m_window.getBackBuffer(), m_gpuResourceManager);
	m_rayTracer.setScene(m_scene);

	m_debugRenderer.initiate(m_window.getBackBuffer(), m_gpuResourceManager);
	m_debugRenderer.setScene(m_scene);

	Okay::initiateImGui(m_window.getGLFWWindow());
	Okay::getDevice()->CreateRenderTargetView(m_window.getBackBuffer(), nullptr, &m_pBackBuffer);

	m_resourceManager.importFile("resources/meshes/room.fbx");	
	m_resourceManager.importFile("resources/textures/RedBlue.png");

	m_resourceManager.importFile("resources/meshes/revolver.fbx");
	m_resourceManager.importFile("resources/textures/rev/rev_albedo.png");

	m_resourceManager.importFile("resources/meshes/Glass.fbx");	

	m_gpuResourceManager.loadResources("resources/environmentMaps/Skybox2.jpg");
}

Application::~Application()
{
	m_window.shutdown();
	m_rayTracer.shutdown();
	DX11_RELEASE(m_pBackBuffer);

	glfwTerminate();
	Okay::shutdownImGui();
	Okay::shutdownDX11();
}

void Application::run()
{
	static ID3D11RenderTargetView* nullRTV = nullptr;

	Entity camera = m_scene.createEntity();
	camera.addComponent<Camera>(90.f, 0.1f);

#if 1
	//Entity ground = m_scene.createEntity();
	//Sphere& groundSphere = ground.addComponent<Sphere>();
	//Transform& groundTra = ground.getComponent<Transform>();
	//groundTra.position = glm::vec3(0.f, -1468.f, 0.f);
	//groundSphere.material.albedo.colour = glm::vec3(1.f);
	//groundSphere.material.emissionColour = glm::vec3(0.f);
	//groundSphere.material.emissionPower = 0.f;
	//groundSphere.material.roughness = 1.f;
	//groundSphere.radius = 1465.f;

	Entity ball = m_scene.createEntity();
	Sphere& ballSphere = ball.addComponent<Sphere>();
	Transform& ballTra = ball.getComponent<Transform>();
	ballTra.position = glm::vec3(0.f, -1.4f, 0.f);
	ballSphere.material.albedo.colour = glm::vec3(0.89f, 0.5f, 0.5f);
	ballSphere.material.emissionColour = glm::vec3(0.f);
	ballSphere.material.emissionPower = 0.f;
	ballSphere.radius = 7.f;

	for (uint32_t i = 0; i < 2; i++)
	{
		Entity meshEntity = m_scene.createEntity();
		MeshComponent& meshComp = meshEntity.addComponent<MeshComponent>();
		meshComp.material = ballSphere.material;
		meshComp.material.albedo.textureId = i;
		meshComp.meshID = i;
	}

	camera.getComponent<Transform>().position.x = 60.f;
	camera.getComponent<Transform>().rotation.y = -90.f;

#elif 0
	glm::vec3 colours[3] = 
	{
		{0.f, 0.f, 1.f},
		{0.f, 1.f, 0.f},
		{1.f, 0.f, 0.f},
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

			sphere.material.albedo.colour = glm::vec3(1.f, 0.1f, 0.1f);// glm::mix(colours[idx1], colours[idx2], tBlend);
		}
	}

	Entity light = m_scene.createEntity();
	Sphere& lightSphere = light.addComponent<Sphere>();
	lightSphere.material.emissionColour = glm::vec3(1.f);
	lightSphere.material.emissionPower = 8.f;
	lightSphere.radius = 55.f;
	light.getComponent<Transform>().position = glm::vec3(244.f, 206.f, -241.f);

#else

	typedef glm::vec3 Color;
	auto getMat = [&](Color col, float smoothness = 0.f, float specProb = 0.f, float transp = 0.f) 
	{
		Material mat;
		mat.albedo.colour = col;
		mat.metallic.colour = specProb;
		mat.roughness = 1.f - smoothness;
		mat.transparency = transp;
		return mat;
	};
	
	auto entityWithMat = [&](Material& mat, glm::vec3 pos, float radius)
	{
		Entity entity = m_scene.createEntity();
		entity.getComponent<Transform>().position = pos;
		Sphere& sphere = entity.addComponent<Sphere>();
		sphere.material = mat;
		sphere.radius = radius;
	};

	srand((uint32_t)time(0));
	auto randomFloat = []()
	{
		return rand() / (float)RAND_MAX;
	};

	auto randomFloat2 = [&](float min, float max)
	{
		return min + (max - min) * randomFloat();
	};

	auto randomColour = [&]()
	{
		return glm::vec3(randomFloat(), randomFloat(), randomFloat());
	};
	
	auto randomColour2 = [&](float min, float max)
	{
		return glm::vec3(randomFloat2(min, max), randomFloat2(min, max), randomFloat2(min, max));
	};


	Material groundMat = getMat(Color(0.5, 0.5, 0.5));
	entityWithMat(groundMat, glm::vec3(0.f, -1000.f, 0.f), 1000.f);

	for (int a = -11; a < 11; a++) 
	{
		for (int b = -11; b < 11; b++)
		{
			auto choose_mat = randomFloat();
			glm::vec3 center(a + 0.9 * randomFloat(), 0.2, b + 0.9 * randomFloat());

			if ((center - glm::vec3(4, 0.2, 0)).length() > 0.9)
			{
				Material sphere_material;

				if (choose_mat < 0.8) 
				{
					// diffuse
					auto albedo = randomColour() * randomColour();
					float smoothness = randomFloat2(0.f, 0.3f);
					float specProb = randomFloat2(0.f, 0.3f);
					sphere_material = getMat(albedo, smoothness, specProb);
					entityWithMat(sphere_material, center, 0.2f);
				}
				else if (choose_mat < 0.95) 
				{
					// metal
					auto albedo = randomColour2(0.5, 1);
					auto fuzz = randomFloat2(0.5f, 1.f);
					sphere_material = getMat(albedo, fuzz, fuzz);
					entityWithMat(sphere_material, center, 0.2f);
				}
				else 
				{
					// glass
					auto albedo = Color(1.f);
					sphere_material = getMat(albedo, 0.f, 0.f, 0.9f);
					sphere_material.indexOfRefraction = 1.5f;
					entityWithMat(sphere_material, center, 0.2f);
				}
			}
		}
	}

	auto material1 = getMat(Color(1.f), 1.f, 0.f);
	entityWithMat(material1, glm::vec3(-4, 1, 0), 1.0);

	auto material2 = getMat(Color(1.f), 0.f, 0.f, 1.f);
	material2.indexOfRefraction = 1.5f;
	entityWithMat(material2, glm::vec3(0, 1, 0), 1.0);

	auto material3 = getMat(Color(0.7, 0.6, 0.5), 1.f, 1.f);
	entityWithMat(material3, glm::vec3(4, 1, 0), 1.0);

	Transform& camTra = m_camera.getComponent<Transform>();
	camTra.position = glm::vec3(6.149f, 1.414f, -1.912f);
	camTra.rotation = glm::vec3(16.502f, -66.649f, 0.f);
#endif

	while (m_window.isOpen())
	{
		m_window.processMessages();
		Okay::newFrameImGui();

		updateImGui();
		updateCamera();

		m_debugRenderer.render();

		Okay::getDeviceContext()->OMSetRenderTargets(1u, &m_pBackBuffer, nullptr);
		Okay::endFrameImGui();
		Okay::getDeviceContext()->OMSetRenderTargets(1u, &nullRTV, nullptr);

		m_window.present();
	}
}

void Application::updateImGui()
{
	static bool accumulate = true;
	m_accumulationTime += ImGui::GetIO().DeltaTime;
	m_accumulationTime *= (float)accumulate;

	bool resetAcu = false;

	if (ImGui::Begin("Rendering"))
	{
		ImGui::PushItemWidth(-120.f);

		ImGui::Text("FPS: %.3f", 1.f / ImGui::GetIO().DeltaTime);
		ImGui::Text("MS: %.3f", ImGui::GetIO().DeltaTime * 1000.f);

		ImGui::Separator();

		ImGui::Text("Accumulation Frames: %u", m_rayTracer.getNumAccumulationFrames());
		ImGui::Text("Accumulation Time:	%.2f", m_accumulationTime);
		if (ImGui::Checkbox("Accumulate", &accumulate))
			m_rayTracer.toggleAccumulation(accumulate);

		if (ImGui::Button("Reset Accumulation"))
		{
			resetAcu = true;
		}

		ImGui::Separator();

		if (ImGui::DragFloat("DOF Strength", &m_rayTracer.getDOFStrength(), 0.05f, 0.f, 10.f)) resetAcu = true;
		if (ImGui::DragFloat("DOF Distance", &m_rayTracer.getDOFDistance(), 0.05f, 0.f, 1000.f)) resetAcu = true;

		ImGui::Separator();

		if (ImGui::Button("Reload Shaders"))
		{
			m_rayTracer.reloadShaders();
			resetAcu = true;
		}

		ImGui::Separator();

		ImGui::DragInt("BVH Max triangles", (int*)&m_gpuResourceManager.getMaxBvhLeafTriangles(), 1, 0, Okay::INVALID_UINT / 2);
		ImGui::DragInt("BVH Max depth", (int*)&m_gpuResourceManager.getMaxBvhDepth(), 1, 0, Okay::INVALID_UINT / 2);

		if (ImGui::Button("Rebuild BVH tree"))
		{
			m_gpuResourceManager.loadMeshAndBvhData();
		}

		ImGui::PopItemWidth();
	}
	ImGui::End();


	if (ImGui::Begin("Spheres"))
	{
		ImGui::PushItemWidth(-120.f);

		if (ImGui::Button("Add Sphere"))
			m_scene.createEntity().addComponent<Sphere>();

		ImGui::Separator();

		auto sphereView = m_scene.getRegistry().view<Sphere, Transform>();
		for (entt::entity entity : sphereView)
		{
			auto [sphere, transform] = sphereView.get<Sphere, Transform>(entity);
			Material& mat = sphere.material;

			const uint32_t entityID = (uint32_t)entity;

			ImGui::PushID(entityID);

			// TODO: Switch to glm::value_ptr
			ImGui::Text("Sphere: %u", entityID);
			if (ImGui::DragFloat3("Position", &transform.position.x, 0.1f))							resetAcu = true;
			if (ImGui::ColorEdit3("Colour", &mat.albedo.colour.x))									resetAcu = true;
			if (ImGui::ColorEdit3("Emission Colour", &mat.emissionColour.x))						resetAcu = true;
			if (ImGui::DragFloat("Emission Power", &mat.emissionPower, 0.01f))						resetAcu = true;
			if (ImGui::DragFloat("Radius", &sphere.radius, 0.1f))									resetAcu = true;
			if (ImGui::DragFloat("Roughness", &mat.roughness.colour, 0.01f, 0.f, 1.f))				resetAcu = true;
			if (ImGui::DragFloat("Metallic", &mat.metallic.colour, 0.01f, 0.f, 1.f))				resetAcu = true;
			if (ImGui::ColorEdit3("Specular Colour", &mat.specularColour.x))						resetAcu = true;
			if (ImGui::DragFloat("Transparency", &mat.transparency, 0.01f, 0.f, 1.f))				resetAcu = true;
			if (ImGui::DragFloat("Refraction Idx", &mat.indexOfRefraction, 0.01f, 1.f, 5.f))		resetAcu = true;

			ImGui::Separator();

			ImGui::PopID();
		}
		ImGui::PopItemWidth();
	}
	ImGui::End();

	if (ImGui::Begin("Meshes"))
	{
		ImGui::PushItemWidth(-120.f);

		if (ImGui::Button("Add Mesh entity"))
			m_scene.createEntity().addComponent<MeshComponent>();

		ImGui::Separator();

		const uint32_t maxMeshId = m_resourceManager.getCount<Mesh>() - 1;
		auto meshView = m_scene.getRegistry().view<MeshComponent, Transform>();
		for (entt::entity entity : meshView)
		{
			auto [mesh, transform] = meshView.get<MeshComponent, Transform>(entity);
			Material& mat = mesh.material;

			const uint32_t entityID = (uint32_t)entity;

			ImGui::PushID(entityID);

			// TODO: Switch to glm::value_ptr
			ImGui::Text("Entity: %u", entityID);
			if (ImGui::DragFloat3("Position", &transform.position.x, 0.1f))							resetAcu = true;
			if (ImGui::DragFloat3("Rotation", &transform.rotation.x, 0.1f))							resetAcu = true;
			if (ImGui::DragFloat3("Scale", &transform.scale.x, 0.1f))								resetAcu = true;
			if (ImGui::ColorEdit3("Colour", &mat.albedo.colour.x))									resetAcu = true;
			if (ImGui::ColorEdit3("Emission Colour", &mat.emissionColour.x))						resetAcu = true;
			if (ImGui::DragFloat("Emission Power", &mat.emissionPower, 0.01f))						resetAcu = true;
			if (ImGui::DragFloat("Roughness", &mat.roughness.colour, 0.01f, 0.f, 1.f))				resetAcu = true;
			if (ImGui::DragFloat("Metallic", &mat.metallic.colour, 0.01f, 0.f, 1.f))				resetAcu = true;
			if (ImGui::ColorEdit3("Specular Colour", &mat.specularColour.x))						resetAcu = true;
			if (ImGui::DragFloat("Transparency", &mat.transparency, 0.01f, 0.f, 1.f))				resetAcu = true;
			if (ImGui::DragFloat("Refraction Idx", &mat.indexOfRefraction, 0.01f, 1.f, 5.f))		resetAcu = true;
			if (ImGui::DragInt("MeshID", (int*)&mesh.meshID, 0.1f, 0, maxMeshId))					resetAcu = true;

			ImGui::Separator();

			ImGui::PopID();
		}
		ImGui::PopItemWidth();
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

		if (ImGui::DragFloat("Rotation Speed", &rotationSpeed, 0.02f))			resetAcu = true;
		if (ImGui::DragFloat("Move Speed", &moveSpeed, 0.2f))					resetAcu = true;

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