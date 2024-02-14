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

	struct AABB
	{
		AABB() = default;
		AABB(const glm::vec3& min, const glm::vec3& max)
			:min(min), max(max) { }

		glm::vec3 min = glm::vec3(0.f);
		glm::vec3 max = glm::vec3(0.f);

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
	};

	struct Vertex
	{
		glm::vec3 position = glm::vec3(0.f);
		glm::vec3 normal = glm::vec3(0.f);
		glm::vec2 uv = glm::vec2(0.f);
	};

	struct Triangle
	{
		Vertex verticies[3]{};
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
