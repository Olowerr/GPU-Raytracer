#include "Application.h"
#include "DirectX/DX11.h"
#include "Utilities.h"

Application::Application()
{
	glfwInitHint(GLFW_CLIENT_API, GLFW_NO_API);

	bool glInit = glfwInit();
	OKAY_ASSERT(glInit);

	Okay::initiateDX11();

	m_window.initiate(1600u, 900u, "GPU Raytracer");
	m_renderer.initiate(m_window.getBackBuffer());
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
	while (m_window.isOpen())
	{
		m_window.processMessages();
	
		m_renderer.render();

		m_window.present();
	}
}
