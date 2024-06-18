#pragma once

#include "Mesh.h"

struct BvhNode
{
	BvhNode() = default;
	BvhNode(BvhNode&& other) noexcept // rmv?
		:triIndicies(std::move(other.triIndicies))
	{
		boundingBox = other.boundingBox;
		firstChildIdx = other.firstChildIdx;
	}

	inline bool isLeaf() const { return firstChildIdx == Okay::INVALID_UINT; }

	Okay::AABB boundingBox;
	std::vector<uint32_t> triIndicies;
	uint32_t firstChildIdx = Okay::INVALID_UINT;
};

constexpr uint32_t ads = sizeof(std::vector<uint32_t>);
constexpr uint32_t ads2 = sizeof(BvhNode);

class BvhBuilder
{
public:
	BvhBuilder(uint32_t maxLeafTriangles, uint32_t maxDepth = Okay::INVALID_UINT);
	~BvhBuilder() = default;

	inline void setMaxLeafTriangles(uint32_t maxLeafTriangles);
	inline void setMaxDepth(uint32_t maxDepth);

	void buildTree(const Mesh& mesh);
	inline const std::vector<BvhNode>& getTree() const;

private:
	uint32_t m_maxLeafTriangles;
	uint32_t m_maxDepth;

	const std::vector<Okay::Triangle>* m_pMeshTris;
	std::vector<BvhNode> m_nodes;
	std::vector<glm::vec3> m_triMiddles;

	void findAABB(BvhNode& node);
	void reset();

	// Recursive approach for building the node tree
	void findChildren(uint32_t parentNodeIdx, const Okay::Plane& splittingPlane, uint32_t curDepth);
	
	// std::stack approach for building the node tree
	void buildTreeInternal();

	float evaluateSAH(BvhNode& node, uint32_t axis, float pos);
	float findBestSplitPlane(BvhNode& node, uint32_t& axis, float& splitPos);
};

inline void BvhBuilder::setMaxLeafTriangles(uint32_t minimumTriangles)	{ m_maxLeafTriangles = minimumTriangles; }
inline void BvhBuilder::setMaxDepth(uint32_t maxDepth)					{ m_maxDepth = maxDepth; }

inline const std::vector<BvhNode>& BvhBuilder::getTree() const	{ return m_nodes; }

