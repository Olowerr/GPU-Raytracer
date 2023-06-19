#include "Application.h"
#include "DirectX/DX11.h"
#include "Utilities.h"
#include "ECS/Components.h"

Application::Application()
{
	glfwInitHint(GLFW_CLIENT_API, GLFW_NO_API);

	bool glInit = glfwInit();
	OKAY_ASSERT(glInit);

	Okay::initiateDX11();

	m_window.initiate(1600u, 900u, "GPU Raytracer");
	m_renderer.initiate(m_window.getBackBuffer(), &m_scene);
}

Application::~Application()
{
	m_window.shutdown();
	m_renderer.shutdown();

	glfwTerminate();
	Okay::shutdownDX11();
}

void Application::run()
{
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
	
		m_renderer.render();

		m_window.present();
	}
}
