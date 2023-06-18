#pragma once
#include <GLFW/glfw3.h>
#include <string_view>
#include <d3d11.h>

class Window
{
public:
	Window();
	Window(uint32_t width, uint32_t height, std::string_view windowName);
	~Window();

	void initiate(uint32_t width, uint32_t height, std::string_view windowName);

	bool isOpen();

	void processMessages();
	void present();

//(temp) private:
	GLFWwindow* m_pGLWindow;
	IDXGISwapChain* m_pDX11SwapChain;

};