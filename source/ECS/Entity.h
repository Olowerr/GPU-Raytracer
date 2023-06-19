#pragma once
#include "Utilities.h"

#include "Entt/entt.hpp"

class Entity
{
public:
	Entity()
		:m_entityId(entt::null), m_pReg(nullptr) 
	{ }

	Entity(entt::entity id, entt::registry* pReg)
		:m_entityId((entt::entity)id), m_pReg(pReg) 
	{ }
	

	template<typename T, typename... Args>
	inline T& addComponent(Args&&... args);

	template<typename... T>
	inline bool hasComponents() const;

	template<typename T>
	inline T& getComponent();

	template<typename T>
	inline const T& getComponent() const;
	
	template<typename T>
	inline T* tryGetComponent();

	template<typename T>
	inline const T* tryGetComponent() const;

	template<typename T>
	inline bool removeComponent();

	inline operator entt::entity() const	{ return m_entityId; }
	inline uint32_t getID() const			{ return (uint32_t)m_entityId; }

	inline explicit operator bool() const	{ return isValid(); }
	inline bool isValid() const				{ return m_pReg ? m_pReg->valid(m_entityId) : false; }

	inline bool operator== (const Entity& other) { return m_entityId == other.m_entityId; }

private:
	entt::registry* m_pReg; // Hmmmmm
	entt::entity m_entityId;

	// Due to padding, another 4 bytes can fit here for free
	// What to add thooo :thonk:
};


template<typename T, typename... Args>
inline T& Entity::addComponent(Args&&... args)
{
	OKAY_ASSERT(m_pReg);
	return m_pReg->emplace<T>(m_entityId, std::forward<Args>(args)...);
}

template<typename... T>
inline bool Entity::hasComponents() const
{
	OKAY_ASSERT(m_pReg);
	return m_pReg->all_of<T...>(m_entityId);
}

template<typename T>
inline T& Entity::getComponent()
{
	OKAY_ASSERT(m_pReg);
	OKAY_ASSERT(hasComponents<T>());
	return m_pReg->get<T>(m_entityId);
}

template<typename T>
inline const T& Entity::getComponent() const
{
	OKAY_ASSERT(m_pReg);
	OKAY_ASSERT(hasComponents<T>());
	return m_pReg->get<T>(m_entityId);
}

template<typename T>
inline T* Entity::tryGetComponent()
{
	OKAY_ASSERT(m_pReg);
	return m_pReg->try_get<T>(m_entityId);
}

template<typename T>
inline const T* Entity::tryGetComponent() const
{
	OKAY_ASSERT(m_pReg);
	return m_pReg->try_get<T>(m_entityId);
}

template<typename T>
inline bool Entity::removeComponent()
{
	OKAY_ASSERT(m_pReg);
	return m_pReg->remove<T>(m_entityId);
}
