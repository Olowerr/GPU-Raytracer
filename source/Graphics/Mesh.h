#pragma once
#include "Utilities.h"

#include <vector>

struct MeshData
{
	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec2> uvs;

	Okay::AABB boundingBox;
};

class Mesh
{
public:
	Mesh(const MeshData& meshData, const std::string& name)
		:m_name(name), m_boundingBox(meshData.boundingBox)
	{
		const uint32_t numVerticies = (uint32_t)meshData.positions.size();
		m_triangles.resize(numVerticies / 3);

		for (uint32_t i = 0; i < numVerticies; i++)
		{
			const uint32_t triangleIdx = i / 3;
			const uint32_t localVertexIdx = i % 3;

			Okay::Vertex& localVertex = m_triangles[triangleIdx].verticies[localVertexIdx];

			localVertex.position = meshData.positions[i];
			localVertex.normal = meshData.normals[i];
			localVertex.uv = meshData.uvs[i];
			
			// Ensure all UVs are between [0, 1]
			localVertex.uv -= glm::floor(localVertex.uv);
		}
	}

	~Mesh() = default;

	inline const std::vector<Okay::Triangle>& getTriangles() const;	
	inline const Okay::AABB& getBoundingBox() const;

private:
	std::string m_name;

	std::vector<Okay::Triangle> m_triangles;
	Okay::AABB m_boundingBox;
};

inline const std::vector<Okay::Triangle>& Mesh::getTriangles() const	{ return m_triangles; }
inline const Okay::AABB& Mesh::getBoundingBox() const	{ return m_boundingBox; }