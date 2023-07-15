#pragma once
#include "glm/glm.hpp"

struct SphereComponent // Temporary
{
	glm::vec3 position = glm::vec3(0.f);
	glm::vec3 colour = glm::vec3(1.f, 1.f, 1.f);
	glm::vec3 emission = glm::vec3(0.f, 0.f, 0.f);
	float emissionPower = 0.f;
	float radius = 20.f;
	float smoothness = 0.f;
	float specularProbability = 0.f;
	glm::vec3 specularColour = glm::vec3(1.f);
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