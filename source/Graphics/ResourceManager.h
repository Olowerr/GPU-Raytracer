#pragma once

#include "Utilities.h"

#include "Mesh.h"
#include "Texture.h"

#include <string_view>
#include <vector>

class ResourceManager
{
public:
	ResourceManager() = default;
	~ResourceManager() = default;

	AssetID importFile(std::string_view filePath);

	template<typename Asset, typename... Args>
	inline Asset& addAsset(Args&&... args);

	template<typename Asset>
	inline Asset& getAsset(AssetID id);

	template<typename Asset>
	inline const Asset& getAsset(AssetID id) const;

	template<typename Asset>
	inline uint32_t getCount() const;

	template<typename Asset>
	inline const std::vector<Asset>& getAll() const;

private:
	std::vector<Mesh> m_meshes;
	std::vector<Texture> m_textures;

	AssetID loadMesh(std::string_view path);
	AssetID loadTexture(std::string_view path);

	template<typename Asset>
	inline std::vector<Asset>& getAssets();

	template<typename Asset>
	inline const std::vector<Asset>& getAssetsConst() const;
};

#define STATIC_ASSERT_ASSET_TYPE()\
static_assert(std::is_same<Asset, Mesh>() || \
			  std::is_same<Asset, Texture>(),\
			  "Invalid Asset type")

// Public:
template<typename Asset, typename ...Args>
inline Asset& ResourceManager::addAsset(Args && ...args)
{
	STATIC_ASSERT_ASSET_TYPE();
	return getAssets<Asset>().emplace_back(args...);
}

template<typename Asset>
inline Asset& ResourceManager::getAsset(AssetID id)
{
	STATIC_ASSERT_ASSET_TYPE();
	std::vector<Asset>& assets = getAssets<Asset>();

	OKAY_ASSERT((uint32_t)id < (uint32_t)assets.size());
	return assets[id];
}

template<typename Asset>
inline const Asset& ResourceManager::getAsset(AssetID id) const
{
	// Same definition as getAsset(), but just returning getAsset() looks recursive and thats spooky :eyes:

	STATIC_ASSERT_ASSET_TYPE();
	const std::vector<Asset>& assets = getAssets<Asset>();

	OKAY_ASSERT((uint32_t)id < (uint32_t)assets.size());
	return assets[id];
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