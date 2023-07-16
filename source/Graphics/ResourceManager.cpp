#include "ResourceManager.h"
#include "Importer.h"

bool ResourceManager::importFile(std::string_view filePath)
{
	if (loadMesh(filePath))
		return true;

	return false;
}

bool ResourceManager::loadMesh(std::string_view filePath)
{
	std::string name;
	MeshData meshData;
	if (!Importer::loadMesh(filePath, meshData, &name))
		return false;

	m_meshes.emplace_back(std::move(meshData), name);
	return true;
}
