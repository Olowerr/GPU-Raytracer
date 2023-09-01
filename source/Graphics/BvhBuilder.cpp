#include "BvhBuilder.h"

#include "SMath.h"

#include <stack>

BvhBuilder::BvhBuilder(uint32_t maxLeafTriangles, uint32_t maxDepth)
	:m_maxLeafTriangles(maxLeafTriangles), m_maxDepth(maxDepth), m_pMeshTris(nullptr)
{
}

void BvhBuilder::buildTree(const Mesh& mesh)
{
	reset();

	m_pMeshTris = &mesh.getTriangles();
	size_t numTotalTriangles = m_pMeshTris->size();
	// Assert numTotalTriangles?

	// TODO: Calculate rough number of nodes based on numTriangles to reserve memory before starting
	// TODO: Precompute triangle middles (maybe only if high num tris?)

	BvhNode& root = m_nodes.emplace_back();
	root.boundingBox = mesh.getBoundingBox();

	root.triIndicies.resize((uint32_t)numTotalTriangles);
	for (uint32_t i = 0; i < numTotalTriangles; i++)
		root.triIndicies[i] = i;

	// TODO: Use SAH to find splittingPlanes
	Okay::Plane startingPlane{};
	startingPlane.position = OkayMath::getMiddle(root.boundingBox);
	startingPlane.normal = glm::vec3(1.f, 0.f, 0.f);

#if 0
	// TODO: Avoid recursion to decrease risk of stack overflow, or use an arena allocator
	findChildren(0u, startingPlane, 0u);
#else
	// Non recursive approach, uses std::stack
	buildTreeInternal(startingPlane);
#endif
}

void BvhBuilder::findChildren(uint32_t parentNodeIdx, const Okay::Plane& splittingPlane, uint32_t curDepth)
{
	m_nodes[parentNodeIdx].depth = curDepth;
	uint32_t parentNumTris = (uint32_t)m_nodes[parentNodeIdx].triIndicies.size();

	// Reached maxDepth or maxTriangles in node?
	if (curDepth >= m_maxDepth - 1u || parentNumTris <= m_maxLeafTriangles)
		return;

	uint32_t childNodeIdxs[2]{};

	childNodeIdxs[0] = m_nodes[parentNodeIdx].childIdxs[0] = (uint32_t)m_nodes.size();
	childNodeIdxs[1] = m_nodes[parentNodeIdx].childIdxs[1] = (uint32_t)m_nodes.size() + 1u;

	m_nodes.emplace_back();
	m_nodes.emplace_back();

	m_nodes[childNodeIdxs[0]].parentIdx = parentNodeIdx;
	m_nodes[childNodeIdxs[1]].parentIdx = parentNodeIdx;

	for (uint32_t i = 0; i < parentNumTris; i++)
	{
		uint32_t meshTriIndex = m_nodes[parentNodeIdx].triIndicies[i];
		glm::vec3 triToPlane = OkayMath::getMiddle((*m_pMeshTris)[meshTriIndex]) - splittingPlane.position;

		uint32_t localChildIdx = glm::dot(triToPlane, splittingPlane.normal) > 0.f ? 1u : 0u;
		BvhNode& childNode = m_nodes[childNodeIdxs[localChildIdx]];

		childNode.triIndicies.emplace_back(meshTriIndex);
	}
	
	// TODO: Check if 0 tris in children

	findAABB(m_nodes[childNodeIdxs[0]]);
	findAABB(m_nodes[childNodeIdxs[1]]);

	Okay::Plane plane0{};
	Okay::Plane plane1{};

	// -- temp for testing
	plane0.position = OkayMath::getMiddle(m_nodes[childNodeIdxs[0]].boundingBox);
	plane1.position = OkayMath::getMiddle(m_nodes[childNodeIdxs[1]].boundingBox);
	plane0.normal = plane1.normal = (curDepth % 2 ? glm::vec3(1.f, 0.f, 0.f) : glm::vec3(0.f, 0.f, 1.f)); // test random vector
	// --

	findChildren(childNodeIdxs[0], plane0, curDepth + 1u);
	findChildren(childNodeIdxs[1], plane1, curDepth + 1u);
}

void BvhBuilder::buildTreeInternal(const Okay::Plane& startingPlane)
{
	struct NodeStack
	{
		NodeStack() = default;
		NodeStack(uint32_t parentNodeIdx, const Okay::Plane& splittingPlane, uint32_t depth)
			:parentNodeIdx(parentNodeIdx), splittingPlane(splittingPlane), depth(depth) { }

		uint32_t parentNodeIdx;
		Okay::Plane splittingPlane;
		uint32_t depth;
	};

	// Root node
	std::stack<NodeStack> stack;
	stack.push(NodeStack(0u, startingPlane, 0u));

	// Preperation
	NodeStack node;
	uint32_t parentNumTris = 0u;
	uint32_t childNodeIdxs[2]{};
	Okay::Plane plane0{};
	Okay::Plane plane1{};

	while (!stack.empty())
	{
		node = stack.top();
		stack.pop();

		parentNumTris = (uint32_t)m_nodes[node.parentNodeIdx].triIndicies.size();

		// Reached maxDepth or maxTriangles in node?
		if (node.depth >= m_maxDepth - 1u || parentNumTris <= m_maxLeafTriangles)
			continue;

		childNodeIdxs[0] = m_nodes[node.parentNodeIdx].childIdxs[0] = (uint32_t)m_nodes.size();
		childNodeIdxs[1] = m_nodes[node.parentNodeIdx].childIdxs[1] = (uint32_t)m_nodes.size() + 1u;

		m_nodes.emplace_back();
		m_nodes.emplace_back();

		m_nodes[childNodeIdxs[0]].parentIdx = node.parentNodeIdx;
		m_nodes[childNodeIdxs[1]].parentIdx = node.parentNodeIdx;

		for (uint32_t i = 0; i < parentNumTris; i++)
		{
			uint32_t meshTriIndex = m_nodes[node.parentNodeIdx].triIndicies[i];
			glm::vec3 triToPlane = OkayMath::getMiddle((*m_pMeshTris)[meshTriIndex]) - node.splittingPlane.position;

			uint32_t localChildIdx = glm::dot(triToPlane, node.splittingPlane.normal) > 0.f ? 1u : 0u;
			BvhNode& childNode = m_nodes[childNodeIdxs[localChildIdx]];

			childNode.triIndicies.emplace_back(meshTriIndex);
		}

		// TODO: Check if 0 tris in children // Shouldn't happen if using SAH (?)

		findAABB(m_nodes[childNodeIdxs[0]]);
		findAABB(m_nodes[childNodeIdxs[1]]);

		// -- temp for testing before SAH
		plane0.position = OkayMath::getMiddle(m_nodes[childNodeIdxs[0]].boundingBox);
		plane1.position = OkayMath::getMiddle(m_nodes[childNodeIdxs[1]].boundingBox);
		plane0.normal = plane1.normal = (node.depth % 2 ? glm::vec3(1.f, 0.f, 0.f) : glm::vec3(0.f, 0.f, 1.f)); // test random vector
		// --

		stack.push(NodeStack(childNodeIdxs[0], plane0, node.depth + 1u));
		stack.push(NodeStack(childNodeIdxs[1], plane1, node.depth + 1u));
	}
}

void BvhBuilder::findAABB(BvhNode& node)
{
	uint32_t numTriIndicies = (uint32_t)node.triIndicies.size();
	for (uint32_t i = 0; i < numTriIndicies; i++)
	{
		for (uint32_t k = 0; k < 3u; k++)
		{
			const glm::vec3& point = (*m_pMeshTris)[node.triIndicies[i]].verticies[k].position;

			node.boundingBox.min = glm::min(point, node.boundingBox.min);
			node.boundingBox.max = glm::max(point, node.boundingBox.max);
		}
	}
}

void BvhBuilder::reset()
{
	m_nodes.clear();
	m_pMeshTris = nullptr;
}