#include "Application.h"
#include "DirectX/DX11.h"
#include "Utilities.h"
#include "ECS/Components.h"

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

	SphereComponent& sphere1 = m_scene.createEntity().addComponent<SphereComponent>();
	SphereComponent& sphere2 = m_scene.createEntity().addComponent<SphereComponent>();

	sphere1.m_position = glm::vec3(1600.f * 0.25f, 450.f, 1000.f);
	sphere1.m_colour = glm::vec3(1.f, 0.7f, 0.5f);
	sphere1.m_radius = 200.f;

	sphere2.m_position = glm::vec3(1600.f * 0.75f, 450.f, 1000.f);
	sphere2.m_colour = glm::vec3(0.5f, 0.7f, 0.5f);
	sphere2.m_radius = 100.f;

	while (m_window.isOpen())
	{
		m_window.processMessages();
		Okay::newFrameImGui();

		if (ImGui::Begin("Spheres"))
		{
			ImGui::PushItemWidth(-150.f);
			auto sphereView = m_scene.getRegistry().view<SphereComponent>();
			for (entt::entity entity : sphereView)
			{
				SphereComponent& sphere = sphereView[entity];

				const uint32_t entityID = (uint32_t)entity;
				ImGui::DragFloat3(("Position " + std::to_string(entityID)).c_str(), &sphere.m_position.x);
				ImGui::ColorEdit3(("Colour " + std::to_string(entityID)).c_str(), &sphere.m_colour.x);
				ImGui::ColorEdit3(("Emission Colour " + std::to_string(entityID)).c_str(), &sphere.m_emission.x);
				ImGui::DragFloat(("Emission Power " + std::to_string(entityID)).c_str(), &sphere.m_emissionPower, 0.01f);
				ImGui::DragFloat(("Radius " + std::to_string(entityID)).c_str(), &sphere.m_radius);
			
				ImGui::Separator();
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
