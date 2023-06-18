#include "Window.h"
#include "DirectX/DX11.h"
#include "Utilities.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

Window::Window()
	:m_pGLWindow(nullptr), m_pDXSwapChain(nullptr), m_pDXBackBuffer(nullptr)
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
	DX11_RELEASE(m_pDXBackBuffer);
}

void Window::initiate(uint32_t width, uint32_t height, std::string_view windowName)
{
	shutdown();

	m_pGLWindow = glfwCreateWindow(width, height, windowName.data(), nullptr, nullptr);
	OKAY_ASSERT(m_pGLWindow);

	Okay::createSwapChain(&m_pDXSwapChain, glfwGetWin32Window(m_pGLWindow));
	OKAY_ASSERT(m_pDXSwapChain);

	m_pDXSwapChain->GetBuffer(0u, __uuidof(ID3D11Texture2D), (void**)&m_pDXBackBuffer);
	OKAY_ASSERT(m_pDXBackBuffer);
}
