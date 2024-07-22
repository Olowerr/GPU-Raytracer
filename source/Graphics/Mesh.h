#pragma once
#include "Utilities.h"

#include <vector>

struct MeshData
{
	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> tangents;
	std::vector<glm::vec3> bitangents;

	Okay::AABB boundingBox;
};

class Mesh
{
public:
	Mesh(const MeshData& meshData, const std::string& name)
		:m_name(name), m_boundingBox(meshData.boundingBox)
	{
		const uint32_t numVerticies = (uint32_t)meshData.positions.size();
		m_trianglesPos.resize(numVerticies / 3);
		m_trianglesInfo.resize(numVerticies / 3);

		for (uint32_t i = 0; i < numVerticies; i++)
		{
			const uint32_t triangleIdx = i / 3;
			const uint32_t localVertexIdx = i % 3;

			m_trianglesPos[triangleIdx].position[localVertexIdx] = meshData.positions[i];

			Okay::VertexInfo& localVertexInfo = m_trianglesInfo[triangleIdx].vertexInfo[localVertexIdx];
			localVertexInfo.normal = meshData.normals[i];
			localVertexInfo.uv = meshData.uvs[i];
			localVertexInfo.tangent = meshData.tangents[i];
			localVertexInfo.bitangent = meshData.bitangents[i];
		}
	}

	~Mesh() = default;

	inline const std::vector<Okay::Triangle>& getTrianglesPos() const;
	inline const std::vector<Okay::TriangleInfo>& getTrianglesInfo() const;
	inline const Okay::AABB& getBoundingBox() const;

private:
	std::string m_name;

	std::vector<Okay::Triangle> m_trianglesPos;
	std::vector<Okay::TriangleInfo> m_trianglesInfo;
	Okay::AABB m_boundingBox;
};

inline const std::vector<Okay::Triangle>& Mesh::getTrianglesPos() const			{ return m_trianglesPos; }
inline const std::vector<Okay::TriangleInfo>& Mesh::getTrianglesInfo() const	{ return m_trianglesInfo; }

inline const Okay::AABB& Mesh::getBoundingBox() const	{ return m_boundingBox; }