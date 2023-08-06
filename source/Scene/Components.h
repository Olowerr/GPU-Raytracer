#pragma once
#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"

struct Transform
{
	glm::vec3 position = glm::vec3(0.f);
	glm::vec3 rotation = glm::vec3(0.f);
	glm::vec3 scale = glm::vec3(1.f);

	glm::mat4 calculateMatrix() const
	{
		return glm::translate(glm::mat4(1.f), position) *
			glm::toMat4(glm::quat(rotation)) *
			glm::scale(glm::mat4(1.f), scale);
	}
};

struct Material
{
	glm::vec3 albedoColour = glm::vec3(1.f);

	glm::vec3 specularColour = glm::vec3(1.f);
	float smoothness = 0.f;
	float specularProbability = 0.f;

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
	uint32_t meshID = 0u;
};

// TODO: Find better system for sharing structs between GPU & CPU.
// We can define them in a Header file but then we need to typedef float3...
// Maybe not a problem tho? But probably best if mostly/only included in source files
struct GPU_MeshComponent 
{
	glm::mat4 transformMatrix;

	uint32_t triStart;
	uint32_t triCount;
	Okay::AABB boundingBox;

	Material material;
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