#include "Application.h"
#include "DirectX/DX11.h"

Application::Application()
{
	glfwInitHint(GLFW_CLIENT_API, GLFW_NO_API);

	bool glInit = glfwInit();
	OKAY_ASSERT(glInit);

	Okay::initiateDX11();
	m_window.initiate(1600, 900, "GPU Raytracer");
}

Application::~Application()
{
	glfwTerminate();
	Okay::shutdownDX11();
}

void Application::run()
{
	while (m_window.isOpen())
	{
		m_window.processMessages();
	

		m_window.present();
	}
}
