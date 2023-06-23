#include "Application.h"
#include "DirectX/DX11.h"
#include "Utilities.h"
#include "Scene/Components.h"

#include "ImGuiHelper.h"

Application::Application()
	:m_pBackBuffer(nullptr)
{
	glfwInitHint(GLFW_CLIENT_API, GLFW_NO_API);

	bool glInit = glfwInit();
	OKAY_ASSERT(glInit);

	Okay::initiateDX11();
	m_window.initiate(1600u, 900u, "GPU Raytracer");
	m_renderer.initiate(m_window.getBackBuffer(), &m_scene);

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

	Entity camEntity = m_scene.createEntity();
	camEntity.addComponent<Camera>(90.f, 0.1f);
	m_renderer.setCamera(camEntity);

	SphereComponent& sphere1 = m_scene.createEntity().addComponent<SphereComponent>();
	sphere1.position = glm::vec3(4.f, 0.f, 20.f);
	sphere1.colour = glm::vec3(1.f, 0.7f, 0.5f);
	sphere1.emission = glm::vec3(0.f, 0.f, 1.f);
	sphere1.emissionPower = 1.f;
	sphere1.radius = 3.f;

	SphereComponent& sphere2 = m_scene.createEntity().addComponent<SphereComponent>();
	sphere2.position = glm::vec3(-4.f, 0.f, 20.f);
	sphere2.colour = glm::vec3(1.f);
	sphere2.emission = glm::vec3(1.f, 0.f, 0.f);
	sphere2.emissionPower = 1.f;
	sphere2.radius = 2.f;

	while (m_window.isOpen())
	{
		m_window.processMessages();
		Okay::newFrameImGui();

		if (ImGui::Begin("Spheres"))
		{
			ImGui::PushItemWidth(-120.f);

			ImGui::Text("FPS: %.3f", 1.f / ImGui::GetIO().DeltaTime);
			ImGui::Text("MS: %.4f", ImGui::GetIO().DeltaTime * 1000.f);

			ImGui::Separator();

			Camera& camera = camEntity.getComponent<Camera>();
			ImGui::PushID(camEntity.getID());

			ImGui::Text("Camera");
			ImGui::DragFloat3("Position", &camera.position.x, 0.1f);
			ImGui::DragFloat3("Rotation", &camera.rotation.x, 0.1f);
			ImGui::DragFloat("FOV", &camera.fov, 0.5f);
			ImGui::DragFloat("NearZ", &camera.nearZ, 0.001f);
			ImGui::PopID();

			ImGui::Separator();

			static bool accumulate = false;
			ImGui::Text("Num Accumulation Frames: %u", m_renderer.getNumAccumulationFrames());
			if (ImGui::Checkbox("Accumulate", &accumulate))
				m_renderer.toggleAccumulation(accumulate);
			if (ImGui::Button("Reset Accumulation"))
				m_renderer.resetAccumulation();

			ImGui::Separator();

			if (ImGui::Button("Add Sphere"))
				m_scene.createEntity().addComponent<SphereComponent>();

			ImGui::Separator();

			auto sphereView = m_scene.getRegistry().view<SphereComponent>();
			for (entt::entity entity : sphereView)
			{
				SphereComponent& sphere = sphereView[entity];
				const uint32_t entityID = (uint32_t)entity;
				
				ImGui::PushID(entityID);

				ImGui::Text("Sphere: %u", entityID);
				ImGui::DragFloat3("Position", &sphere.position.x, 0.1f);
				ImGui::ColorEdit3("Colour", &sphere.colour.x);
				ImGui::ColorEdit3("Emission Colour", &sphere.emission.x);
				ImGui::DragFloat("Emission Power", &sphere.emissionPower, 0.01f);
				ImGui::DragFloat("Radius", &sphere.radius, 0.1f);
			
				ImGui::Separator();

				ImGui::PopID();
			}
			ImGui::PopItemWidth();
		}
		ImGui::End();

		m_renderer.render();

		Okay::getDeviceContext()->OMSetRenderTargets(1u, &m_pBackBuffer, nullptr);
		Okay::endFrameImGui();
		Okay::getDeviceContext()->OMSetRenderTargets(1u, &nullRTV, nullptr);

		m_window.present();
	}
}
