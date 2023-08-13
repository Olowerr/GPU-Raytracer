#include "ResourceManager.h"
#include "Importer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

bool ResourceManager::importFile(std::string_view filePath)
{
	if (loadMesh(filePath))
		return true;

	if (loadTexture(filePath))
		return true;

	return false;
}

bool ResourceManager::loadMesh(std::string_view filePath)
{
	std::string name;
	MeshData meshData;
	if (!Importer::loadMesh(filePath, meshData, &name))
		return false;

	m_meshes.emplace_back(meshData, name);
	return true;
}

bool ResourceManager::loadTexture(std::string_view path)
{
	int width, height;
	unsigned char* pData = stbi_load(path.data(), &width, &height, nullptr, STBI_rgb_alpha);

	if (!pData)
		return false;

	m_textures.emplace_back(pData, (uint32_t)width, (uint32_t)height);

	return true;
}