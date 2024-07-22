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

	m_pMeshTris = &mesh.getTrianglesPos();
	size_t numTotalTriangles = m_pMeshTris->size();
	// Assert numTotalTriangles?

	// TODO: Calculate rough number of nodes based on numTriangles to reserve memory before starting

	BvhNode& root = m_nodes.emplace_back();
	root.boundingBox = mesh.getBoundingBox();

	root.triIndicies.resize((uint32_t)numTotalTriangles);
	for (uint32_t i = 0; i < numTotalTriangles; i++)
		root.triIndicies[i] = i;

	// TODO: Use SAH to find splittingPlanes
	Okay::Plane startingPlane{};
	startingPlane.position = OkayMath::getMiddle(root.boundingBox);
	startingPlane.normal = glm::vec3(1.f, 0.f, 0.f);

	// Non recursive approach, uses std::stack
	buildTreeInternal();
}

float BvhBuilder::evaluateSAH(BvhNode& node, uint32_t axis, float pos)
{
	Okay::AABB leftBox, rightBox;
	leftBox = rightBox = Okay::AABB(glm::vec3(FLT_MAX), glm::vec3(-FLT_MAX));

	uint32_t triIndex;
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
			leftBox.growTo(triangle.position[0]);
			leftBox.growTo(triangle.position[1]);
			leftBox.growTo(triangle.position[2]);
		}
		else
		{
			rightCount++;
			rightBox.growTo(triangle.position[0]);
			rightBox.growTo(triangle.position[1]);
			rightBox.growTo(triangle.position[2]);
		}
	}
	float cost = leftCount * leftBox.getArea() + rightCount * rightBox.getArea();
	return cost > 0.f ? cost : FLT_MAX;
}

float BvhBuilder::findBestSplitPlane(BvhNode& node, uint32_t& outAxis, float& outSplitPos)
{
	static const uint32_t NUM_TESTS = 100;

	float bestCost = FLT_MAX;
	for (uint32_t axis = 0; axis < 3; axis++)
	{
		float boundsMin = node.boundingBox.min[axis];
		float boundsMax = node.boundingBox.max[axis];
		if (boundsMin == boundsMax) 
			continue;

		for (uint32_t i = 1; i < NUM_TESTS; i++)
		{
			float candidatePos = glm::mix(boundsMin, boundsMax, i / (float)NUM_TESTS);
			float cost = evaluateSAH(node, axis, candidatePos);
			if (cost < bestCost)
			{
				outSplitPos = candidatePos;
				outAxis = axis;
				bestCost = cost;
			}
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
		pCurrentNode->firstChildIdx = (uint32_t)m_nodes.size() - 2u;

		pChildren[0] = &m_nodes[pCurrentNode->firstChildIdx];
		pChildren[1] = &m_nodes[pCurrentNode->firstChildIdx + 1u];

		pChildren[0]->triIndicies.resize(leftIndicies.size());
		pChildren[1]->triIndicies.resize(rightIndicies.size());

		memcpy(pChildren[0]->triIndicies.data(), leftIndicies.data(), sizeof(uint32_t) * leftIndicies.size());
		memcpy(pChildren[1]->triIndicies.data(), rightIndicies.data(), sizeof(uint32_t) * rightIndicies.size());
		// --


		findAABB(*pChildren[0]);
		findAABB(*pChildren[1]);

		stack.push(NodeStack(pCurrentNode->firstChildIdx, nodeData.depth + 1u));
		stack.push(NodeStack(pCurrentNode->firstChildIdx + 1, nodeData.depth + 1u));
	}

	m_triMiddles.resize(0);
}

void BvhBuilder::findAABB(BvhNode& node)
{
	node.boundingBox.min = glm::vec3(FLT_MAX);
	node.boundingBox.max = glm::vec3(-FLT_MAX);

	uint32_t numTriIndicies = (uint32_t)node.triIndicies.size();
	for (uint32_t i = 0; i < numTriIndicies; i++)
	{
		const Okay::Triangle& currentTri = (*m_pMeshTris)[node.triIndicies[i]];
		for (uint32_t k = 0; k < 3u; k++)
		{
			const glm::vec3& point = currentTri.position[k];

			node.boundingBox.min = glm::min(point, node.boundingBox.min);
			node.boundingBox.max = glm::max(point, node.boundingBox.max);
		}
	}
}

void BvhBuilder::reset()
{
	m_triMiddles.clear();
	m_nodes.clear();
	m_pMeshTris = nullptr;
}