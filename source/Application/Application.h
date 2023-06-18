#pragma once
#include "Window.h"

class Application
{
public:
	Application();
	~Application();

	void run();

private:
	Window m_window;
};