#pragma once
#include "Window.h"
#include "Graphics/Renderer.h"
#include "Scene/Scene.h"

class Application
{
public:
	Application();
	~Application();

	void run();

private:
	Window m_window;
	Renderer m_renderer;
	Scene m_scene;
	ID3D11RenderTargetView* m_pBackBuffer; //Only used for ImGui

	Entity m_camera;
	void updateCamera();

	float m_accumulationTime;
};