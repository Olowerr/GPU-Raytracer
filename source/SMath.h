#pragma once

#include "Utilities.h"

// Move these functions to Utilities.h?

namespace OkayMath
{
	inline glm::vec3 getMiddle(const Okay::Triangle& triangle)
	{
		return (triangle.verticies[0].position +
			triangle.verticies[1].position +
			triangle.verticies[2].position) * (1.f / 3.f);
	}

	inline glm::vec3 getMiddle(const Okay::AABB& aabb)
	{
		return (aabb.max + aabb.min) * 0.5f;
	}

	Okay::AABB findAABB(const std::vector<Okay::Triangle>& triangleList)
	{
		Okay::AABB aabb{};
		uint32_t numTriangles = (uint32_t)triangleList.size();

		for (uint32_t i = 0; i < numTriangles; i++)
		{
			// Unroll?
			for (uint32_t k = 0; k < 3u; k++)
			{
				const glm::vec3& point = triangleList[i].verticies[k].position;

				aabb.min = glm::min(point, aabb.min);
				aabb.max = glm::min(point, aabb.max);
			}
		}

		return aabb;
	}
}