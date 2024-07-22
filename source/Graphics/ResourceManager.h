#pragma once

#include "Utilities.h"

#include "Mesh.h"
#include "Texture.h"

#include <string_view>
#include <vector>

class ResourceManager
{
public:
	struct ObjectDecription
	{
		std::string name;
		AssetID meshId;
		AssetID albedoTextureId;
		AssetID rougnessTextureId;
		AssetID metallicTextureId;
		AssetID specularTextureId;
		AssetID normalTextureId;
	};

public:
	ResourceManager() = default;
	~ResourceManager() = default;

	AssetID loadMesh(std::string_view path);
	AssetID loadTexture(std::string_view path);
	AssetID findOrLoadTexture(std::string_view path);

	bool importAssets(std::string_view filePath, std::vector<ObjectDecription>& outObjects, std::string_view texturePath = "", float scale = 1.f);

	template<typename Asset, typename... Args>
	inline Asset& addAsset(Args&&... args);

	template<typename Asset>
	inline Asset& getAsset(AssetID id);

	template<typename Asset>
	inline const Asset& getAsset(AssetID id) const;

	template<typename Asset>
	inline AssetID getAssetID(std::string_view name);

	template<typename Asset>
	inline uint32_t getCount() const;

	template<typename Asset>
	inline const std::vector<Asset>& getAll() const;

private:
	std::vector<Mesh> m_meshes;
	std::vector<Texture> m_textures;


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
	STATIC_ASSERT_ASSET_TYPE();
	const std::vector<Asset>& assets = getAssetsConst<Asset>();

	OKAY_ASSERT((uint32_t)id < (uint32_t)assets.size());
	return assets[id];
}

template<typename Asset>
inline AssetID ResourceManager::getAssetID(std::string_view name)
{
	STATIC_ASSERT_ASSET_TYPE();
	std::vector<Asset>& assets = getAssets<Asset>();

	for (uint32_t i = 0; i < (uint32_t)assets.size(); i++)
	{
		const Asset& asset = assets[i];

		if (asset.getName() == name)
		{
			return AssetID(i);
		}
	}

	return AssetID();
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

	else if constexpr (std::is_same<Asset, Texture>())
		return m_textures;
}

template<typename Asset>
inline const std::vector<Asset>& ResourceManager::getAssetsConst() const
{
	STATIC_ASSERT_ASSET_TYPE();
	return const_cast<ResourceManager*>(this)->getAssets<Asset>();
}