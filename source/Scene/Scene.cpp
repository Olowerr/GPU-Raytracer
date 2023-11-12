#include "Scene.h"
#include "Entity.h"
#include "Components.h"

Scene::Scene()
{
}

Scene::~Scene()
{
}

Entity Scene::createEntity()
{
    Entity entity(registry.create(), &registry); 
    entity.addComponent<Transform>();
    return entity;
}

const Entity Scene::getFirstCamera() const
{
	auto cameraView = registry.view<Camera, Transform>();
	Entity camEntity;
	for (entt::entity entity : cameraView)
	{
		camEntity = Entity(entity, const_cast<entt::registry*>(&registry)); // it pains me but idk what to do
		break;
	}
	return camEntity;
}

Entity Scene::getFirstCamera()
{
	auto cameraView = registry.view<Camera, Transform>();
	Entity camEntity;
	for (entt::entity entity : cameraView)
	{
		camEntity = Entity(entity, &registry);
		break;
	}
	return camEntity;
}
