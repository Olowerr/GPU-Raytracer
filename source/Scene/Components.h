#pragma once
#include "glm/glm.hpp"

struct SphereComponent // Temporary
{
	glm::vec3 position = glm::vec3(0.f);
	glm::vec3 colour = glm::vec3(1.f, 1.f, 1.f);
	glm::vec3 emission = glm::vec3(0.f, 0.f, 0.f);
	float emissionPower = 0.f;
	float radius = 100.f;
};
