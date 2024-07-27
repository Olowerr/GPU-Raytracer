#pragma once
#include "Window.h"
#include "Graphics/RayTracer.h"
#include "Graphics/DebugRenderer.h"
#include "Graphics/ResourceManager.h"
#include "Graphics/GPUResourceManager.h"
#include "Scene/Scene.h"

#include "ImGuiHelper.h"

class Application
{
public:
	Application();
	~Application();

	void run();

private:
	void loadMeshesAsEntities(std::string_view filePath, std::string_view texturesPath = "", float scale = 1.f);

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

	Entity m_debugSelectedEntity;
	uint32_t m_debugSelectedNodeIdx;

	void updateImGui();
	void updateCamera();
	void saveScreenshot();

	void displayComponents(Entity entity);
	Entity m_selectedEntity;

	template<typename ComponentType>
	void createComponentSelection(Entity entity, std::string_view componentName);

	float m_accumulationTime;
};

template<typename ComponentType>
void Application::createComponentSelection(Entity entity, std::string_view componentName)
{
	if (entity.hasComponents<ComponentType>())
	{
		return;
	}

	if (ImGui::Selectable(componentName.data()))
	{
		entity.addComponent<ComponentType>();
	}
}