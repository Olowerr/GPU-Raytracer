#include "Scene.h"
#include "Entity.h"

Scene::Scene()
{
}

Scene::~Scene()
{
}

Entity Scene::createEntity()
{
    Entity entity(registry.create(), &registry); 
    return entity;
}

