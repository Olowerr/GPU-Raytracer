#pragma once
#include "Window.h"
#include "Graphics/Renderer.h"

class Application
{
public:
	Application();
	~Application();

	void run();

private:
	Window m_window;
	Renderer m_renderer;
};