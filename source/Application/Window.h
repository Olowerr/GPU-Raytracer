#pragma once
#include "DirectX/DX11.h"

#include "GLFW/glfw3.h"

#include <string_view>

class Window
{
public:
	Window();
	Window(uint32_t width, uint32_t height, std::string_view windowName);
	~Window();

	void shutdown();
	void initiate(uint32_t width, uint32_t height, std::string_view windowName);

	inline ID3D11Texture2D* getBackBuffer();
	HWND getHWND();
	inline GLFWwindow* getGLFWWindow();

	inline bool isOpen();

	void processMessages();
	inline void present();

private:
	GLFWwindow* m_pGLWindow;
	
	IDXGISwapChain* m_pDXSwapChain;
	ID3D11Texture2D* m_pDXBackBuffer;
};

inline ID3D11Texture2D* Window::getBackBuffer()
{
	return m_pDXBackBuffer;
}

inline GLFWwindow* Window::getGLFWWindow()
{
	return m_pGLWindow;
}


inline void Window::present()
{
	m_pDXSwapChain->Present(0u, 0u);
}

inline bool Window::isOpen()
{
	return !glfwWindowShouldClose(m_pGLWindow);
}