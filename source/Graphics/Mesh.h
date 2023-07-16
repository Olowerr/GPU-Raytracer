#pragma once
#include <vector>
#include "glm/glm.hpp"

struct MeshData
{
	std::vector<glm::vec3> positions;
	//std::vector<glm::vec3> normals;
	//std::vector<glm::vec2> uvs;
};

class Mesh
{
public:
	Mesh(MeshData&& meshData)
		:m_meshData(std::move(meshData)) { }

	~Mesh() = default;

	inline MeshData& getMeshData();
	inline const MeshData& getMeshData() const;

private:
	MeshData m_meshData;
};

inline MeshData& Mesh::getMeshData()				{ return m_meshData; }
inline const MeshData& Mesh::getMeshData() const	{ return m_meshData; }