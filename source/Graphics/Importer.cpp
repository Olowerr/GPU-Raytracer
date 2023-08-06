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

		uint32_t numVerticies = pMesh->mNumFaces * 3u;
		outData.positions.resize(numVerticies);
		outData.normals.resize(numVerticies);
		outData.uvs.resize(numVerticies);

		for (uint32_t i = 0; i < pMesh->mNumFaces; i++)
		{
			uint32_t vertex0Idx = i * 3;
			uint32_t* aiIndices = pMesh->mFaces[i].mIndices;

			for (uint32_t j = 0; j < 3; j++)
			{
				glm::vec3 p = assimpToGlmVec3(pMesh->mVertices[aiIndices[j]]);
				glm::vec3 n = assimpToGlmVec3(pMesh->mNormals[aiIndices[j]]);
				glm::vec2 u = assimpToGlmVec3(pMesh->mTextureCoords[0][aiIndices[j]]);

				outData.positions[vertex0Idx + j] = p;
				outData.normals[vertex0Idx + j] = n;
				outData.uvs[vertex0Idx + j] = u;

				// Perhaps not the best way but works
				outData.boundingBox.min = glm::min(p, outData.boundingBox.min);
				outData.boundingBox.max = glm::max(p, outData.boundingBox.max);
			}	
		}

		return true;
	}
}