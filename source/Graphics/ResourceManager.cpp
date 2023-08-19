#include "ResourceManager.h"
#include "Importer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

AssetID ResourceManager::importFile(std::string_view filePath)
{
	AssetID id;

	// Assimp throws "Deadly import error" on fail, doesn't seem to affect anything tho lol
	// Assimp just returns nullptr, maybe there's is way to surpress?
	if (id = loadMesh(filePath))
		return id;

	if (id = loadTexture(filePath))
		return id;

	return id;
}

AssetID ResourceManager::loadMesh(std::string_view filePath)
{
	std::string name;
	MeshData meshData;
	if (!Importer::loadMesh(filePath, meshData, &name))
		return AssetID();

	m_meshes.emplace_back(meshData, name);
	return AssetID(m_meshes.size() - 1);
}

AssetID ResourceManager::loadTexture(std::string_view path)
{
	int width, height;
	unsigned char* pData = stbi_load(path.data(), &width, &height, nullptr, STBI_rgb_alpha);

	if (!pData)
		return AssetID();

	m_textures.emplace_back(pData, (uint32_t)width, (uint32_t)height);
	//stbi_image_free(pData);

	return AssetID(m_textures.size() - 1);
}