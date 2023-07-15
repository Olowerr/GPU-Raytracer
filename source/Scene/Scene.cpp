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

