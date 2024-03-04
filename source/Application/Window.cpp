#include "Window.h"
#include "DirectX/DX11.h"
#include "Utilities.h"
#include "Input.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

Window::Window()
	:m_pGLWindow(nullptr), m_pDXSwapChain(nullptr)
{
}

Window::Window(uint32_t width, uint32_t height, std::string_view windowName)
{
	initiate(width, height, windowName);
}

Window::~Window()
{
	shutdown();
}

void Window::shutdown()
{
	if (m_pGLWindow)
	{
		glfwDestroyWindow(m_pGLWindow);
		m_pGLWindow = nullptr;
	}

	DX11_RELEASE(m_pDXSwapChain);
	m_texture.shutdown();
}

void Window::initiate(uint32_t width, uint32_t height, std::string_view windowName)
{
	shutdown();

	m_pGLWindow = glfwCreateWindow(width, height, windowName.data(), nullptr, nullptr);
	OKAY_ASSERT(m_pGLWindow);

	Okay::createSwapChain(&m_pDXSwapChain, glfwGetWin32Window(m_pGLWindow));
	OKAY_ASSERT(m_pDXSwapChain);

	ID3D11Texture2D* pBackBuffer = nullptr;
	m_pDXSwapChain->GetBuffer(0u, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
	OKAY_ASSERT(pBackBuffer);

	m_texture.initiate(pBackBuffer);
	DX11_RELEASE(pBackBuffer);

	if (GLFWmonitor* pMonitor = glfwGetPrimaryMonitor())
	{
		int monitorWidth, monitorHeight;
		glfwGetMonitorWorkarea(pMonitor, nullptr, nullptr, &monitorWidth, &monitorHeight);
		glfwSetWindowPos(m_pGLWindow, monitorWidth / 2 - width / 2, monitorHeight / 2 - height / 2);
	}

	Input::pWindow = m_pGLWindow;

	glfwSetKeyCallback(m_pGLWindow, [](GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		switch (action)
		{
		case GLFW_PRESS:
			Input::setKeyDown(key);
			break;

		case GLFW_RELEASE:
			Input::setKeyUp(key);
			break;
		}
	});
}

void Window::processMessages()
{
	Input::update();
	glfwPollEvents(); 
}

HWND Window::getHWND()
{
	return glfwGetWin32Window(m_pGLWindow);
}
