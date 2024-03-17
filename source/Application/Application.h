#pragma once
#include "Window.h"
#include "Graphics/RayTracer.h"
#include "Graphics/DebugRenderer.h"
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
	RenderTexture m_target;

	DebugRenderer m_debugRenderer;
	bool m_useRasterizer = false;
	bool m_rasterizerDrawObjects = true;
	bool m_drawNodeBBs = true;
	bool m_drawNodeGeometry = true;

	Entity m_selectedEntity;
	uint32_t m_selectedNodeIdx;

	void updateImGui();
	void updateCamera();
	void saveScreenshot();

	float m_accumulationTime;
};