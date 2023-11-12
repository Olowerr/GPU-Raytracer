#pragma once
#include "Window.h"
#include "Graphics/RayTracer.h"
#include "Graphics/ResourceManager.h"
#include "Graphics/GPUResourceManager.h"
#include "Scene/Scene.h"

class Application
{
public:
	Application();
	~Application();

	void run();

private:
	Window m_window;
	RayTracer m_rayTracer;
	Scene m_scene;
	ResourceManager m_resourceManager;
	GPUResourceManager m_gpuResourceManager;
	ID3D11RenderTargetView* m_pBackBuffer; //Only used for ImGui

	void updateImGui();

	void updateCamera();

	float m_accumulationTime;
};