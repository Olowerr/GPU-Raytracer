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

	static void aiMeshToMeshData(const aiMesh* pAiMesh, MeshData& outData, float scale = 1.f)
	{
		outData.boundingBox.min = glm::vec3(FLT_MAX);
		outData.boundingBox.max = glm::vec3(-FLT_MAX);

		uint32_t numVerticies = pAiMesh->mNumFaces * 3u;
		outData.positions.resize(numVerticies);
		outData.normals.resize(numVerticies);
		outData.uvs.resize(numVerticies);
		outData.tangents.resize(numVerticies);
		outData.bitangents.resize(numVerticies);

		bool hasUV = pAiMesh->HasTextureCoords(0u);
		bool hasTangent = pAiMesh->HasTangentsAndBitangents();

		for (uint32_t i = 0; i < pAiMesh->mNumFaces; i++)
		{
			uint32_t vertex0Idx = i * 3;
			uint32_t* aiIndices = pAiMesh->mFaces[i].mIndices;

			for (uint32_t j = 0; j < 3; j++)
			{
				glm::vec3 position = assimpToGlmVec3(pAiMesh->mVertices[aiIndices[j]]) * scale;
				glm::vec3 normal = assimpToGlmVec3(pAiMesh->mNormals[aiIndices[j]]);
				glm::vec2 uv = hasUV ? assimpToGlmVec3(pAiMesh->mTextureCoords[0][aiIndices[j]]) : glm::vec2(0.f);
				glm::vec3 tangent = hasTangent ? assimpToGlmVec3(pAiMesh->mTangents[aiIndices[j]]) : glm::vec3(0.f);
				glm::vec3 bitangent = hasTangent ? assimpToGlmVec3(pAiMesh->mBitangents[aiIndices[j]]) : glm::vec3(0.f);

				uint32_t vertexIdx = vertex0Idx + j;

				outData.positions[vertexIdx] = position;
				outData.normals[vertexIdx] = normal;
				outData.uvs[vertexIdx] = uv;
				outData.tangents[vertexIdx] = tangent;
				outData.bitangents[vertexIdx] = bitangent;

				// Perhaps not the best way but works
				outData.boundingBox.min = glm::min(position, outData.boundingBox.min);
				outData.boundingBox.max = glm::max(position, outData.boundingBox.max);
			}
		}
	}

	bool loadMesh(std::string_view filePath, MeshData& outData, std::string* pOutname)
	{
		Assimp::Importer importer;

		const aiScene* pScene = importer.ReadFile(filePath.data(),
			aiProcess_Triangulate | aiProcess_ConvertToLeftHanded | aiProcess_CalcTangentSpace);

		if (!pScene)
			return false;

		aiMesh* pMesh = pScene->mMeshes[0];	// Currenly only supporting one mesh per file
		if (pOutname)
			*pOutname = pMesh->mName.C_Str();

		aiMeshToMeshData(pMesh, outData);

		return true;
	}

	bool loadObjects(std::string_view filePath, std::vector<ObjectDecriptionStr>& outObjects, float scale)
	{
		Assimp::Importer importer;

		const aiScene* pAiScene = importer.ReadFile(filePath.data(),
			aiProcess_Triangulate | aiProcess_ConvertToLeftHanded | aiProcess_CalcTangentSpace); // aiProcess_OptimizeMeshes

		if (!pAiScene)
			return false;

		outObjects.resize(pAiScene->mNumMeshes);

		for (uint32_t i = 0; i < pAiScene->mNumMeshes; i++)
		{
			aiMesh* pAiMesh = pAiScene->mMeshes[i];

			ObjectDecriptionStr& objectDesc = outObjects[i];
			objectDesc.name = pAiMesh->mName.C_Str();
			
			aiMaterial* pAiMaterial = pAiScene->mMaterials[pAiMesh->mMaterialIndex];
			aiString textureStr;
			
			if (pAiMaterial->GetTexture(aiTextureType_DIFFUSE, 0u, &textureStr) == aiReturn_SUCCESS)
				objectDesc.albedoTexturePath = textureStr.C_Str();
			
			if (pAiMaterial->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0u, &textureStr) == aiReturn_SUCCESS)
				objectDesc.rougnessTexturePath = textureStr.C_Str();
			
			if (pAiMaterial->GetTexture(aiTextureType_METALNESS, 0u, &textureStr) == aiReturn_SUCCESS)
				objectDesc.metallicTexturePath = textureStr.C_Str();
			
			if (pAiMaterial->GetTexture(aiTextureType_SPECULAR, 0u, &textureStr) == aiReturn_SUCCESS)
				objectDesc.specularTexturePath = textureStr.C_Str();
			
			if (pAiMaterial->GetTexture(aiTextureType_NORMALS, 0u, &textureStr) == aiReturn_SUCCESS)
				objectDesc.normalTexturePath = textureStr.C_Str();
			else if (pAiMaterial->GetTexture(aiTextureType_DISPLACEMENT, 0u, &textureStr) == aiReturn_SUCCESS)
				objectDesc.normalTexturePath = textureStr.C_Str();

			aiMeshToMeshData(pAiMesh, objectDesc.meshData, scale);
		}

		return true;
	}
}