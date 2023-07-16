#pragma once

#include <string_view>

struct MeshData;

namespace Importer
{
	bool loadMesh(std::string_view filePath, MeshData& outMeshData, std::string* pOutname = nullptr);
}