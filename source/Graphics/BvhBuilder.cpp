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

#define RECURSIVE 0

#if RECURSIVE
	// TODO: Avoid recursion to decrease risk of stack overflow, or use an arena allocator
	findChildren(0u, startingPlane, 0u);
#else
	// Non recursive approach, uses std::stack
	buildTreeInternal();
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

		glm::vec3 triToPlane = m_triMiddles[meshTriIndex] - splittingPlane.position;

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

float BvhBuilder::evaluateSAH(BvhNode& node, uint32_t axis, float pos)
{
	uint32_t triIndex;
	Okay::AABB leftBox, rightBox;
	uint32_t leftCount = 0, rightCount = 0;
	uint32_t triCount = (uint32_t)node.triIndicies.size();

	for (uint32_t i = 0; i < triCount; i++)
	{
		triIndex = node.triIndicies[i];

		const glm::vec3& middle = m_triMiddles[triIndex];
		const Okay::Triangle& triangle = (*m_pMeshTris)[triIndex];

		if (middle[axis] < pos)
		{
			leftCount++;
			leftBox.growTo(triangle.verticies[0].position);
			leftBox.growTo(triangle.verticies[1].position);
			leftBox.growTo(triangle.verticies[2].position);
		}
		else
		{
			rightCount++;
			rightBox.growTo(triangle.verticies[0].position);
			rightBox.growTo(triangle.verticies[1].position);
			rightBox.growTo(triangle.verticies[2].position);
		}
	}
	float cost = leftCount * leftBox.getArea() + rightCount * rightBox.getArea();
	return cost > 0.f ? cost : FLT_MAX;
}

float BvhBuilder::findBestSplitPlane(BvhNode& node, uint32_t& outAxis, float& outSplitPos)
{
	float bestCost = FLT_MAX;
	for (uint32_t axis = 0; axis < 3; axis++)
	{
		float boundsMin = node.boundingBox.min[axis];
		float boundsMax = node.boundingBox.max[axis];
		if (boundsMin == boundsMax) 
			continue;

		float scale = (boundsMax - boundsMin) / 100;
		for (uint32_t i = 1; i < 100; i++)
		{
			float candidatePos = boundsMin + i * scale;
			float cost = evaluateSAH(node, axis, candidatePos);
			if (cost < bestCost)
				outSplitPos = candidatePos, outAxis = axis, bestCost = cost;
		}
	}
	return bestCost;
}

void BvhBuilder::buildTreeInternal()
{
	// A NodeStack contains the data a Node needs while it is being processed in the upcoming 'while' loop.
	// It is the same data that each function call would contain in the recursive approach
	struct NodeStack
	{
		NodeStack() = default;
		NodeStack(uint32_t nodeIndex, uint32_t depth)
			:nodeIndex(nodeIndex), depth(depth) { }

		uint32_t nodeIndex;
		uint32_t depth;
	};

	// Precalculate the middle of all triangles
	uint32_t numMeshTris = (uint32_t)m_pMeshTris->size();
	m_triMiddles.resize(numMeshTris);
	for (uint32_t i = 0; i < numMeshTris; i++)
	{
		m_triMiddles[i] = OkayMath::getMiddle((*m_pMeshTris)[i]);
	}

	// Root node
	std::stack<NodeStack> stack;
	stack.push(NodeStack(0u, 0u));

	// Stack variables
	NodeStack nodeData;
	BvhNode* pCurrentNode = nullptr;
	uint32_t nodeNumTris = 0u;
	BvhNode* pChildren[2]{};
	uint32_t axis;
	float splitPos;

	std::vector<uint32_t> leftIndicies;
	std::vector<uint32_t> rightIndicies;

	while (!stack.empty())
	{
		nodeData = stack.top();
		stack.pop();

		pCurrentNode = &m_nodes[nodeData.nodeIndex];
		nodeNumTris = (uint32_t)pCurrentNode->triIndicies.size();

		// Reached maxDepth or maxTriangles in node?
		if (nodeData.depth >= m_maxDepth - 1u || nodeNumTris <= m_maxLeafTriangles)
			continue;

		
		float splitCost = findBestSplitPlane(*pCurrentNode, axis, splitPos);

		float parentCost = nodeNumTris * pCurrentNode->boundingBox.getArea();
		if (splitCost >= parentCost)
		{
			continue;
		}

		leftIndicies.clear();
		rightIndicies.clear();

		uint32_t triIndex;

		for (uint32_t i = 0; i < nodeNumTris; i++)
		{
			triIndex = pCurrentNode->triIndicies[i];

			const glm::vec3& middle = m_triMiddles[triIndex];
			const Okay::Triangle& triangle = (*m_pMeshTris)[triIndex];

			if (middle[axis] < splitPos)
			{
				leftIndicies.emplace_back(pCurrentNode->triIndicies[i]);
			}
			else
			{
				rightIndicies.emplace_back(pCurrentNode->triIndicies[i]);
			}
		}

		if (!leftIndicies.size() || !rightIndicies.size())
			continue;

		m_nodes.emplace_back();
		m_nodes.emplace_back();

		// Need to get the new pointer for the node again incase the m_nodes std::vector had to reallocate
		pCurrentNode = &m_nodes[nodeData.nodeIndex];
		

		// TODO: Rewrite into for loop? Maybe it will unroll on complie if possible?
		pCurrentNode->childIdxs[0] = (uint32_t)m_nodes.size() - 2u;
		pCurrentNode->childIdxs[1] = (uint32_t)m_nodes.size() - 1u;

		pChildren[0] = &m_nodes[pCurrentNode->childIdxs[0]];
		pChildren[1] = &m_nodes[pCurrentNode->childIdxs[1]];

		pChildren[0]->parentIdx = nodeData.nodeIndex;
		pChildren[1]->parentIdx = nodeData.nodeIndex;

		pChildren[0]->triIndicies.resize(leftIndicies.size());
		pChildren[1]->triIndicies.resize(rightIndicies.size());

		memcpy(pChildren[0]->triIndicies.data(), leftIndicies.data(), sizeof(uint32_t) * leftIndicies.size());
		memcpy(pChildren[1]->triIndicies.data(), rightIndicies.data(), sizeof(uint32_t) * rightIndicies.size());
		// --


		findAABB(*pChildren[0]);
		findAABB(*pChildren[1]);

		stack.push(NodeStack(pCurrentNode->childIdxs[0], nodeData.depth + 1u));
		stack.push(NodeStack(pCurrentNode->childIdxs[1], nodeData.depth + 1u));
	}

	m_triMiddles.resize(0);
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