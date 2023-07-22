#include "Importer.h"
#include "Mesh.h"

#include "assimp/importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

namespace Importer
{
	glm::vec3 assimpToGlmVec3(const aiVector3D& vector)
	{
		return glm::vec3(vector.x, vector.y, vector.z);
	}

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

		outData.boundingBox.min = glm::vec3(FLT_MAX);
		outData.boundingBox.max = glm::vec3(-FLT_MAX);

		outData.positions.resize(pMesh->mNumFaces * 3u);
		for (uint32_t i = 0; i < pMesh->mNumFaces; i++)
		{
			glm::vec3 vtx0 = assimpToGlmVec3(pMesh->mVertices[pMesh->mFaces[i].mIndices[0]]);
			glm::vec3 vtx1 = assimpToGlmVec3(pMesh->mVertices[pMesh->mFaces[i].mIndices[1]]);
			glm::vec3 vtx2 = assimpToGlmVec3(pMesh->mVertices[pMesh->mFaces[i].mIndices[2]]);

			uint32_t vertex0Idx = i * 3;
			memcpy(&outData.positions[vertex0Idx	 ], &vtx0, sizeof(glm::vec3));
			memcpy(&outData.positions[vertex0Idx + 1u], &vtx1, sizeof(glm::vec3));
			memcpy(&outData.positions[vertex0Idx + 2u], &vtx2, sizeof(glm::vec3));

			// Perhaps not the best way but works
			outData.boundingBox.min = glm::min(vtx0, outData.boundingBox.min);
			outData.boundingBox.min = glm::min(vtx1, outData.boundingBox.min);
			outData.boundingBox.min = glm::min(vtx2, outData.boundingBox.min);

			outData.boundingBox.max = glm::max(vtx0, outData.boundingBox.max);
			outData.boundingBox.max = glm::max(vtx1, outData.boundingBox.max);
			outData.boundingBox.max = glm::max(vtx2, outData.boundingBox.max);
		}

		return true;
	}
}