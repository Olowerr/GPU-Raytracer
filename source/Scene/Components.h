#pragma once
#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"

#include "Utilities.h"

struct Transform
{
	glm::vec3 position = glm::vec3(0.f);
	glm::vec3 rotation = glm::vec3(0.f);
	glm::vec3 scale = glm::vec3(1.f);

	glm::mat4 calculateMatrix() const
	{
		return glm::translate(glm::mat4(1.f), position) *
			glm::toMat4(glm::quat(glm::radians(rotation))) *
			glm::scale(glm::mat4(1.f), scale);
	}
};

struct MaterialColour3
{
	glm::vec3 colour = glm::vec3(1.f);
	AssetID textureId = Okay::INVALID_UINT;
};

struct MaterialColour1
{
	MaterialColour1() = default;
	MaterialColour1(float colour)
		:colour(colour) { }

	float colour = 1.f;
	AssetID textureId = Okay::INVALID_UINT;
};

struct Material
{
	MaterialColour3 albedo;
	MaterialColour1 roughness;
	MaterialColour1 metallic = MaterialColour1(0.f);

	glm::vec3 specularColour = glm::vec3(1.f);

	glm::vec3 emissionColour = glm::vec3(1.f);
	float emissionPower = 0.f;
};

struct Sphere
{
	Material material;
	float radius = 4.f;
};

struct MeshComponent
{
	Material material;
	AssetID meshID = 0u;
};

// TODO: Find better system for sharing structs between GPU & CPU.
// We can define them in a Header file but then we need to typedef float3...
// Maybe not a problem tho? But probably best if mostly/only included in source files
struct GPU_MeshComponent 
{
	glm::mat4 transformMatrix;

	uint32_t triStart;
	uint32_t triEnd;
	Okay::AABB boundingBox;

	Material material;
	uint32_t bvhNodeStartIdx;
};

struct Camera
{
	Camera(float fov, float nearZ)
		:fov(fov), nearZ(nearZ), farZ(1000.f)
	{ }

	float fov; // Degress
	float nearZ;
	float farZ; // Only used in Cherno way
};