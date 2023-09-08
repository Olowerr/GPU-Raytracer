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

float BvhBuilder::EvaluateSAH(BvhNode& node, int axis, float pos)
{
	// determine triangle counts and bounds for this split candidate
	Okay::AABB leftBox, rightBox;
	int leftCount = 0, rightCount = 0;
	uint32_t triCount = node.triIndicies.size();

	for (uint32_t i = 0; i < triCount; i++)
	{
		const Okay::Triangle& triangle = (*m_pMeshTris)[node.triIndicies[i]];

		if (OkayMath::getMiddle(triangle)[axis] < pos)
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
	return cost > 0 ? cost : FLT_MAX;
}

void BvhBuilder::buildTreeInternal(const Okay::Plane& startingPlane)
{
	// A NodeStack contains the data a Node needs while it is being processed in the upcoming 'while' loop.
	// It is the same data that each function call would contain in the recursive approach
	struct NodeStack
	{
		NodeStack() = default;
		NodeStack(uint32_t nodeIndex, const Okay::Plane& splittingPlane, uint32_t depth)
			:nodeIndex(nodeIndex), splittingPlane(splittingPlane), depth(depth) { }

		uint32_t nodeIndex;
		Okay::Plane splittingPlane;
		uint32_t depth;
	};

	// Root node
	std::stack<NodeStack> stack;
	stack.push(NodeStack(0u, startingPlane, 0u));

	// Stack variables
	NodeStack nodeData;
	BvhNode* pCurrentNode = nullptr;
	uint32_t nodeNumTris = 0u;
	BvhNode* pChildren[2]{};
	Okay::Plane planes[2]{};

	while (!stack.empty())
	{
		nodeData = stack.top();
		stack.pop();

		pCurrentNode = &m_nodes[nodeData.nodeIndex];
		nodeNumTris = (uint32_t)pCurrentNode->triIndicies.size();

		// Reached maxDepth or maxTriangles in node?
		if (nodeData.depth >= m_maxDepth - 1u || nodeNumTris <= m_maxLeafTriangles)
			continue;

		int bestAxis = -1;
		float bestPos = 0, bestCost = FLT_MAX;
		for (int axis = 0; axis < 3; axis++)
		{
			for (uint32_t i = 0; i < nodeNumTris; i++)
			{
				const Okay::Triangle& triangle = (*m_pMeshTris)[pCurrentNode->triIndicies[i]];

				float candidatePos = OkayMath::getMiddle(triangle)[axis];
				float cost = EvaluateSAH(*pCurrentNode, axis, candidatePos);
				if (cost < bestCost)
					bestPos = candidatePos, bestAxis = axis, bestCost = cost;
			}
		}
		int axis = bestAxis;
		float splitPos = bestPos;
		
		float parentCost = nodeNumTris * pCurrentNode->boundingBox.getArea();
		if (bestCost >= parentCost)
		{
			continue;
		}

		std::vector<uint32_t> leftIndicies;
		std::vector<uint32_t> rightIndicies;

		for (uint32_t i = 0; i < nodeNumTris; i++)
		{
			const Okay::Triangle& triangle = (*m_pMeshTris)[pCurrentNode->triIndicies[i]];

			if (OkayMath::getMiddle(triangle)[axis] < splitPos)
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

		for (uint32_t i = 0; i < nodeNumTris; i++)
		{
			uint32_t meshTriIndex = pCurrentNode->triIndicies[i];
			glm::vec3 triToPlane = OkayMath::getMiddle((*m_pMeshTris)[meshTriIndex]) - nodeData.splittingPlane.position;

			uint32_t localChildIdx = glm::dot(triToPlane, nodeData.splittingPlane.normal) > 0.f ? 1u : 0u;
			
			pChildren[localChildIdx]->triIndicies.emplace_back(meshTriIndex);
		}

		// TODO: Check if 0 tris in children // Shouldn't happen if using SAH (?)

		findAABB(*pChildren[0]);
		findAABB(*pChildren[1]);

		// -- temp for testing before SAH
		planes[0].position = OkayMath::getMiddle(pChildren[0]->boundingBox);
		planes[1].position = OkayMath::getMiddle(pChildren[1]->boundingBox);
		planes[0].normal = planes[1].normal = (nodeData.depth % 2 ? glm::vec3(1.f, 0.f, 0.f) : glm::vec3(0.f, 0.f, 1.f)); // test random vector
		// --

		stack.push(NodeStack(pCurrentNode->childIdxs[0], planes[0], nodeData.depth + 1u));
		stack.push(NodeStack(pCurrentNode->childIdxs[1], planes[1], nodeData.depth + 1u));
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