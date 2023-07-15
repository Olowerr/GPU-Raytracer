#include "Application.h"
#include "DirectX/DX11.h"
#include "Utilities.h"
#include "Scene/Components.h"

#include "ImGuiHelper.h"

#include "Input.h"

#include "glm/gtx/quaternion.hpp"
#include "glm/gtc/matrix_transform.hpp"

Application::Application()
	:m_pBackBuffer(nullptr)
{
	glfwInitHint(GLFW_CLIENT_API, GLFW_NO_API);

	bool glInit = glfwInit();
	OKAY_ASSERT(glInit);

	Okay::initiateDX11();
	m_window.initiate(1600u, 900u, "GPU Raytracer");
	m_renderer.initiate(m_window.getBackBuffer(), &m_scene);
	m_renderer.toggleAccumulation(true);

	Okay::initiateImGui(m_window.getGLFWWindow());
	Okay::getDevice()->CreateRenderTargetView(m_window.getBackBuffer(), nullptr, &m_pBackBuffer);
}

Application::~Application()
{
	m_window.shutdown();
	m_renderer.shutdown();
	DX11_RELEASE(m_pBackBuffer);

	glfwTerminate();
	Okay::shutdownImGui();
	Okay::shutdownDX11();
}

void Application::run()
{
	static ID3D11RenderTargetView* nullRTV = nullptr;

	m_camera = m_scene.createEntity();
	m_camera.addComponent<Camera>(90.f, 0.1f);
	m_renderer.setCamera(m_camera);

	SphereComponent& ground = m_scene.createEntity().addComponent<SphereComponent>();
	ground.position = glm::vec3(0.f, -1468.f, 91.8f);
	ground.colour = glm::vec3(1.f);
	ground.emission = glm::vec3(0.f);
	ground.emissionPower = 0.f;
	ground.radius = 1465.f;

	SphereComponent& ball = m_scene.createEntity().addComponent<SphereComponent>();
	ball.position = glm::vec3(0.f, -1.4f, 20.f);
	ball.colour = glm::vec3(0.89f, 0.5f, 0.5f);
	ball.emission = glm::vec3(0.f);
	ball.emissionPower = 0.f;
	ball.radius = 7.3f;


	while (m_window.isOpen())
	{
		m_window.processMessages();
		Okay::newFrameImGui();

		static bool accumulate = true;
		m_accumulationTime += ImGui::GetIO().DeltaTime;
		m_accumulationTime *= (float)accumulate;

		if (ImGui::Begin("Data"))
		{
			ImGui::Text("FPS: %.3f", 1.f / ImGui::GetIO().DeltaTime);
			ImGui::Text("MS: %.4f", ImGui::GetIO().DeltaTime * 1000.f);

			ImGui::Separator();

			ImGui::Text("Accumulation Frames: %u", m_renderer.getNumAccumulationFrames());
			ImGui::Text("Accumulation Time:	%.2f", m_accumulationTime);
			if (ImGui::Checkbox("Accumulate", &accumulate))
				m_renderer.toggleAccumulation(accumulate);
			if (ImGui::Button("Reset Accumulation"))
			{
				m_renderer.resetAccumulation();
				m_accumulationTime = 0.f;
			}

			ImGui::Separator();

			if (ImGui::Button("Reload Shaders"))
				m_renderer.reloadShaders();
		}
		ImGui::End();

		if (ImGui::Begin("Spheres"))
		{
			ImGui::PushItemWidth(-120.f);

			if (ImGui::Button("Add Sphere"))
				m_scene.createEntity().addComponent<SphereComponent>();

			ImGui::Separator();

			bool resetAccumulation = false;
			auto sphereView = m_scene.getRegistry().view<SphereComponent>();
			for (entt::entity entity : sphereView)
			{
				SphereComponent& sphere = sphereView[entity];
				const uint32_t entityID = (uint32_t)entity;
				
				ImGui::PushID(entityID);

				ImGui::Text("Sphere: %u", entityID);
				if (ImGui::DragFloat3("Position", &sphere.position.x, 0.1f))				resetAccumulation = true;
				if (ImGui::ColorEdit3("Colour", &sphere.colour.x))							resetAccumulation = true;
				if (ImGui::ColorEdit3("Emission Colour", &sphere.emission.x))				resetAccumulation = true;
				if (ImGui::DragFloat("Emission Power", &sphere.emissionPower, 0.01f))		resetAccumulation = true;
				if (ImGui::DragFloat("Radius", &sphere.radius, 0.1f))						resetAccumulation = true;
				if (ImGui::DragFloat("Smoothness", &sphere.smoothness, 0.01f, 0.f, 1.f))	resetAccumulation = true;
			
				ImGui::Separator();

				ImGui::PopID();
			}
			ImGui::PopItemWidth();

			if (resetAccumulation)
				m_renderer.resetAccumulation();
		}
		ImGui::End();

		updateCamera();
		m_renderer.render();

		Okay::getDeviceContext()->OMSetRenderTargets(1u, &m_pBackBuffer, nullptr);
		Okay::endFrameImGui();
		Okay::getDeviceContext()->OMSetRenderTargets(1u, &nullRTV, nullptr);

		m_window.present();
	}
}

void Application::updateCamera()
{
	static float rotationSpeed = 0.1f;
	static float moveSpeed = 20.f;
	Camera& cameraData = m_camera.getComponent<Camera>();

	if (ImGui::Begin("Camera"))
	{
		ImGui::PushItemWidth(-120.f);

		ImGui::Text("Pos: (%.3f, %.3f, %.3f)", cameraData.position.x, cameraData.position.y, cameraData.position.z);
		ImGui::Text("Rot: (%.3f, %.3f, %.3f)", cameraData.rotation.x, cameraData.rotation.y, cameraData.rotation.z);

		ImGui::Separator();

		ImGui::DragFloat("Rotation Speed", &rotationSpeed, 0.02f);
		ImGui::DragFloat("Move Speed", &moveSpeed, 0.2f);

		ImGui::PopItemWidth();
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

	cameraData.rotation.x += mouseDelta.y * rotationSpeed;
	cameraData.rotation.y += mouseDelta.x * rotationSpeed;
	cameraData.rotation.x = glm::clamp(cameraData.rotation.x, -89.f, 89.f);

	const float xInput = (float)Input::isKeyDown(Key::D) - (float)Input::isKeyDown(Key::A);
	const float yInput = (float)Input::isKeyDown(Key::Space) - (float)Input::isKeyDown(Key::LeftControl);
	const float zInput = (float)Input::isKeyDown(Key::W) - (float)Input::isKeyDown(Key::S);

	const glm::mat3 rotationMatrix = glm::toMat3(glm::quat(glm::radians(cameraData.rotation)));

	const glm::vec3 fwd = rotationMatrix[2];
	const glm::vec3 right = rotationMatrix[0];

	const float frameSpeed = ImGui::GetIO().DeltaTime * moveSpeed * (Input::isKeyDown(Key::LeftShift) ? 3.f : 1.f);

	cameraData.position += (right * xInput + fwd * zInput) * frameSpeed;
	cameraData.position.y += yInput * frameSpeed;


	if (xInput || yInput || zInput || mouseDelta.x || mouseDelta.y)
	{
		m_renderer.resetAccumulation();
		m_accumulationTime = 0.f;
	}
}