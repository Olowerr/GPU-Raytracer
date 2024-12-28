#pragma once
#include "glm/glm.hpp"

#include <cassert>
#include <stdint.h>
#include <memory>
#include <fstream>

#ifdef DIST
#define OKAY_ASSERT(condition) 
#else
#define OKAY_ASSERT(condition) if (!(condition)) {printf("ASSERTION FAILED: %s  |  FILE: %s  |  LINE: %d\n", #condition, __FILE__, __LINE__); __debugbreak(); }0
#endif

#define DX11_RELEASE(X)		 if (X) {(X)->Release(); } (X) = nullptr
#define OKAY_DELETE(X)		 if (X) { delete (X);	 } (X) = nullptr
#define OKAY_DELETE_ARRAY(X) if (X) { delete[](X);	 } (X) = nullptr
#define CHECK_BIT(X, pos)	((X) & 1<<(pos))

#define SHADER_PATH "resources/shaders/"

namespace Okay
{
	constexpr uint32_t INVALID_UINT = ~0u;

	static bool readBinary(std::string_view binPath, std::string& output)
	{
		std::ifstream reader(binPath.data(), std::ios::binary);
		if (!reader)
			return false;

		reader.seekg(0, std::ios::end);
		output.reserve((size_t)reader.tellg());
		reader.seekg(0, std::ios::beg);

		output.assign(std::istreambuf_iterator<char>(reader), std::istreambuf_iterator<char>());

		return true;
	}

	static std::string_view getFileEnding(std::string_view path)
	{
		return path.substr(path.find_last_of('.'));
	}

	static std::string_view getFileName(std::string_view path)
	{
		size_t slashPos = path.find_last_of("/");

		if (slashPos == std::string::npos)
			slashPos = path.find_last_of("\\");

		if (slashPos == std::string::npos)
			slashPos = 1;
		else
			slashPos++;

		// Can exclude file ending by passing in this as 2nd argument to substr()
		size_t dot = path.find_last_of(".") - slashPos;
		return path.substr(slashPos);
	}

	struct AABB
	{
		AABB() = default;
		AABB(const glm::vec3& min, const glm::vec3& max)
			:min(min), max(max) { }

		glm::vec3 min = glm::vec3(FLT_MAX);
		glm::vec3 max = glm::vec3(-FLT_MAX);

		void growTo(const glm::vec3& point)
		{
			min = glm::min(point, min);
			max = glm::max(point, max);
		}

		float getArea() const
		{
			glm::vec3 extents = max - min;
			return extents.x * extents.y + extents.y * extents.z + extents.z * extents.x;
		}

		static bool intersects(const AABB& a, const AABB& b)
		{
			glm::vec3 aToBCenter = glm::abs((b.max + b.min) * 0.5f - (a.max + a.min) * 0.5f);
			glm::vec3 extentsSum = (a.max - a.min) * 0.5f + (b.max - b.min) * 0.5f;

			return (aToBCenter.x < extentsSum.x) && (aToBCenter.y < extentsSum.y) && (aToBCenter.z < extentsSum.z);
		}
		
		static bool intersects(const AABB& box, const glm::vec3& point)
		{
			glm::vec3 boxCenter = (box.max + box.min) * 0.5f;
			glm::vec3 pointToBox = boxCenter - point;

			glm::vec3 boxExtents = (box.max - box.min) * 0.5f;

			return glm::dot(pointToBox, pointToBox) <= glm::dot(boxExtents, boxExtents);
		}
	};

	struct Triangle // Rename?
	{
		glm::vec3 position[3];
	};

	struct VertexInfo
	{
		glm::vec3 normal = glm::vec3(0.f);
		glm::vec2 uv = glm::vec2(0.f);
		glm::vec3 tangent = glm::vec3(0.f);
		glm::vec3 bitangent = glm::vec3(0.f);
	};

	struct TriangleInfo
	{
		VertexInfo vertexInfo[3];
	};

	struct Plane
	{
		glm::vec3 position;
		glm::vec3 normal;
	};
}

struct AssetID
{
	AssetID() = default;
	AssetID(uint32_t id) :id(id) { };
	AssetID(size_t id) :id((uint32_t)id) { };

	operator uint32_t() const { return id; };
	operator size_t() const { return (size_t)id; };
	operator bool() const { return id != Okay::INVALID_UINT; }
	AssetID& operator= (uint32_t newId) { id = newId; return *this; }

private:
	uint32_t id = Okay::INVALID_UINT;
};
