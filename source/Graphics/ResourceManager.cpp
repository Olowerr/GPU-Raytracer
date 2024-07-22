#include "ResourceManager.h"
#include "Importer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

AssetID ResourceManager::loadMesh(std::string_view filePath)
{
	std::string name;
	MeshData meshData;

	bool success = Importer::loadMesh(filePath, meshData, &name);
	OKAY_ASSERT(success);

	m_meshes.emplace_back(meshData, name);
	return AssetID(m_meshes.size() - 1);
}

AssetID ResourceManager::loadTexture(std::string_view path)
{
	int width, height;
	unsigned char* pData = stbi_load(path.data(), &width, &height, nullptr, STBI_rgb_alpha);

	OKAY_ASSERT(pData);

	std::string_view name = Okay::getFileName(path);

	m_textures.emplace_back(pData, (uint32_t)width, (uint32_t)height, name);

	return AssetID(m_textures.size() - 1);
}

AssetID ResourceManager::findOrLoadTexture(std::string_view path)
{
	std::string_view name = Okay::getFileName(path);
	AssetID assetId = getAssetID<Texture>(name);

	if (assetId)
		return assetId;

	return loadTexture(path);
}

bool ResourceManager::importAssets(std::string_view filePath, std::vector<ObjectDecription>& outObjects, std::string_view texturePath, float scale)
{
	std::vector<Importer::ObjectDecriptionStr> outAssets;

	if (!Importer::loadObjects(filePath, outAssets, scale))
		return false;

	outObjects.resize(outAssets.size());

	std::string texturePathStr = texturePath.data();

	for (uint32_t i = 0; i < outAssets.size(); i++)
	{
		ObjectDecription& outObjectDesc = outObjects[i];

		Importer::ObjectDecriptionStr& objectDescStr = outAssets[i];

		m_meshes.emplace_back(objectDescStr.meshData, objectDescStr.name);
		outObjectDesc.meshId = AssetID(m_meshes.size() - 1);

		if (objectDescStr.albedoTexturePath != "")
			outObjectDesc.albedoTextureId = findOrLoadTexture(texturePathStr == "" ? objectDescStr.albedoTexturePath : texturePathStr + objectDescStr.albedoTexturePath);

		if (objectDescStr.rougnessTexturePath != "")
			outObjectDesc.rougnessTextureId = findOrLoadTexture(texturePathStr == "" ? objectDescStr.rougnessTexturePath : texturePathStr + objectDescStr.rougnessTexturePath);

		if (objectDescStr.metallicTexturePath != "")
			outObjectDesc.metallicTextureId = findOrLoadTexture(texturePathStr == "" ? objectDescStr.metallicTexturePath : texturePathStr + objectDescStr.metallicTexturePath);

		if (objectDescStr.specularTexturePath != "")
			outObjectDesc.specularTextureId = findOrLoadTexture(texturePathStr == "" ? objectDescStr.specularTexturePath : texturePathStr + objectDescStr.specularTexturePath);

		if (objectDescStr.normalTexturePath != "")
			outObjectDesc.normalTextureId = findOrLoadTexture(texturePathStr == "" ? objectDescStr.normalTexturePath : texturePathStr + objectDescStr.normalTexturePath);
	}

	return true;
}
