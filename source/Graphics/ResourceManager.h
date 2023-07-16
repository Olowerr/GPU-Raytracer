#pragma once

#include "Utilities.h"
#include "Mesh.h"

#include <string_view>
#include <vector>

class ResourceManager
{
public:
	ResourceManager() = default;
	~ResourceManager() = default;

	bool importFile(std::string_view filePath);

	template<typename Asset, typename... Args>
	inline Asset& addAsset(Args&&... args);

	template<typename Asset>
	inline Asset& getAsset(uint32_t index);

	template<typename Asset>
	inline const Asset& getAsset(uint32_t index) const;

	template<typename Asset>
	inline uint32_t getCount() const;

	template<typename Asset>
	inline const std::vector<Asset>& getAll() const;

private:
	std::vector<Mesh> m_meshes;

	bool loadMesh(std::string_view path);

	template<typename Asset>
	inline std::vector<Asset>& getAssets();

	template<typename Asset>
	inline const std::vector<Asset>& getAssetsConst() const;
};

#define STATIC_ASSERT_ASSET_TYPE()\
static_assert(std::is_same<Asset, Mesh>(),\
			  "Invalid Asset type")

// Public:
template<typename Asset, typename ...Args>
inline Asset& ResourceManager::addAsset(Args && ...args)
{
	STATIC_ASSERT_ASSET_TYPE();
	return getAssets<Asset>().emplace_back(args...);
}

template<typename Asset>
inline Asset& ResourceManager::getAsset(uint32_t index)
{
	STATIC_ASSERT_ASSET_TYPE();
	std::vector<Asset>& assets = getAssets<Asset>();

	OKAY_ASSERT(index < (uint32_t)assets.size());
	return assets[index];
}

template<typename Asset>
inline const Asset& ResourceManager::getAsset(uint32_t index) const
{
	// Same definition as getAsset(), but just returning getAsset() looks recursive and thats spooky :eyes:

	STATIC_ASSERT_ASSET_TYPE();
	const std::vector<Asset>& assets = getAssets<Asset>();

	OKAY_ASSERT(index < (uint32_t)assets.size());
	return assets[index];
}

template<typename Asset>
inline uint32_t ResourceManager::getCount() const
{
	STATIC_ASSERT_ASSET_TYPE();
	return (uint32_t)getAssetsConst<Asset>().size();
}

template<typename Asset>
inline const std::vector<Asset>& ResourceManager::getAll() const
{
	STATIC_ASSERT_ASSET_TYPE();
	return getAssetsConst<Asset>();
}


// Private
template<typename Asset>
inline std::vector<Asset>& ResourceManager::getAssets()
{
	STATIC_ASSERT_ASSET_TYPE();

	if constexpr (std::is_same<Asset, Mesh>())
		return m_meshes;
}

template<typename Asset>
inline const std::vector<Asset>& ResourceManager::getAssetsConst() const
{
	STATIC_ASSERT_ASSET_TYPE();
	return const_cast<ResourceManager*>(this)->getAssets<Asset>();
}