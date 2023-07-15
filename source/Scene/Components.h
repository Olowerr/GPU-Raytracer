#pragma once
#include "glm/glm.hpp"

struct Transform
{
	glm::vec3 position = glm::vec3(0.f);
	glm::vec3 rotation = glm::vec3(0.f);
	glm::vec3 scale = glm::vec3(1.f);
};

struct Material
{
	glm::vec3 albedoColour = glm::vec3(1.f);

	glm::vec3 specularColour = glm::vec3(1.f);
	float smoothness = 0.f;
	float specularProbability = 1.f;

	glm::vec3 emissionColour = glm::vec3(1.f);
	float emissionPower = 0.f;
};

struct Sphere
{
	float radius = 4.f;
};

struct Camera
{
	Camera(float fov, float nearZ)
		:fov(fov), nearZ(nearZ), farZ(1000.f)
	{ }

	glm::vec3 position = glm::vec3(0.f);
	glm::vec3 rotation = glm::vec3(0.f); // Degress
	float fov; // Degress
	float nearZ;
	float farZ; // Only used in Cherno way
};