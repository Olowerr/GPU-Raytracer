#pragma once
#include "glm/glm.hpp"

struct SphereComponent // Temporary
{
	glm::vec3 m_position = glm::vec3(0.f);
	glm::vec3 m_colour = glm::vec3(1.f, 1.f, 1.f);
	glm::vec3 m_emission = glm::vec3(0.f, 0.f, 0.f);
	float m_emissionPower = 0.f;
	float m_radius = 100.f;
};
