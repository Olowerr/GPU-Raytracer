#pragma once

#include "Entt/entt.hpp"
#include "Entity.h"


class Scene
{
public:
	Scene();
	~Scene();

	Entity createEntity();
	inline void destroyEntity(const Entity& entity);
	inline void destroyEntity(entt::entity entity);

	inline entt::registry& getRegistry();
	inline const entt::registry& getRegistry() const;

	const Entity getFirstCamera() const;
	Entity getFirstCamera();

private:
	entt::registry registry;

	Entity mainCamera;
	Entity skyLight;
};

inline void Scene::destroyEntity(const Entity& entity)	{ registry.destroy(entity); }
inline void Scene::destroyEntity(entt::entity entity)	{ registry.destroy(entity); }

inline entt::registry& Scene::getRegistry()				{ return registry; }
inline const entt::registry& Scene::getRegistry() const	{ return registry; }
