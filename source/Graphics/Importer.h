#pragma once

#include "Mesh.h"

#include <string_view>

namespace Importer
{
	struct ObjectDecriptionStr
	{
		std::string name;
		MeshData meshData;
		std::string albedoTexturePath;
		std::string rougnessTexturePath;
		std::string metallicTexturePath;
		std::string specularTexturePath;
		std::string normalTexturePath;
	};

	bool loadMesh(std::string_view filePath, MeshData& outMeshData, std::string* pOutname = nullptr);

	bool loadObjects(std::string_view filePath, std::vector<ObjectDecriptionStr>& outObjects, float scale = 1.f);
}