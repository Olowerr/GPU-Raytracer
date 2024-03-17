#pragma once
#include "DirectX/RenderTexture.h"

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

	inline const RenderTexture& getTexture();
	HWND getHWND();
	inline GLFWwindow* getGLFWWindow();

	inline bool isOpen();

	void processMessages();
	inline void present();

	void onResize();

private:
	GLFWwindow* m_pGLWindow;
	
	IDXGISwapChain* m_pDXSwapChain;
	RenderTexture m_texture;
};

inline const RenderTexture& Window::getTexture()
{
	return m_texture;
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