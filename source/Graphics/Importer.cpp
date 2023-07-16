#include "Importer.h"
#include "Mesh.h"

#include "assimp/importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

namespace Importer
{
	bool loadMesh(std::string_view filePath, MeshData& outData, std::string* pOutname)
	{
		Assimp::Importer importer;

		const aiScene* pScene = importer.ReadFile(filePath.data(),
			aiProcess_Triangulate | aiProcess_ConvertToLeftHanded);

		if (!pScene)
			return false;

		aiMesh* pMesh = pScene->mMeshes[0];	// Currenly only supporting one mesh per file
		if (pOutname)
			*pOutname = pMesh->mName.C_Str();

		outData.positions.resize(pMesh->mNumFaces * 3u);
		for (uint32_t i = 0; i < pMesh->mNumFaces; i++)
		{
			uint32_t idx0 = pMesh->mFaces[i].mIndices[0];
			uint32_t idx1 = pMesh->mFaces[i].mIndices[1];
			uint32_t idx2 = pMesh->mFaces[i].mIndices[2];

			uint32_t vertex0Idx = i * 3;
			memcpy(&outData.positions[vertex0Idx	 ], &pMesh->mVertices[idx0], sizeof(glm::vec3));
			memcpy(&outData.positions[vertex0Idx + 1u], &pMesh->mVertices[idx1], sizeof(glm::vec3));
			memcpy(&outData.positions[vertex0Idx + 2u], &pMesh->mVertices[idx2], sizeof(glm::vec3));
		}

		return true;
	}
}