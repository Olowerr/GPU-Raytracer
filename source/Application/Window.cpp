#include "Window.h"
#include "DirectX/DX11Utilities.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

Window::Window()
	:m_pGLWindow(nullptr), m_pDX11SwapChain(nullptr)
{
}

Window::Window(uint32_t width, uint32_t height, std::string_view windowName, ID3D11Device* pDevice)
{
	initiate(width, height, windowName, pDevice);
}

Window::~Window()
{
	if (m_pGLWindow)
		glfwDestroyWindow(m_pGLWindow);

	DX11_RELEASE(m_pDX11SwapChain);
}

void Window::initiate(uint32_t width, uint32_t height, std::string_view windowName, ID3D11Device* pDevice)
{
	m_pGLWindow = glfwCreateWindow(width, height, windowName.data(), nullptr, nullptr);
	OKAY_ASSERT(m_pGLWindow);

	OkayDX11::createSwapChain(pDevice, &m_pDX11SwapChain, glfwGetWin32Window(m_pGLWindow));
	OKAY_ASSERT(m_pDX11SwapChain);
}

void Window::processMessages()
{
	glfwPollEvents(); // Maybe shouldn't be here? Fine for now tho
}

void Window::present()
{
	m_pDX11SwapChain->Present(0u, 0u);
}

bool Window::isOpen()
{
	return !glfwWindowShouldClose(m_pGLWindow);
}