#include "stdafx.h"

#include "Scene.h"

//--------------------------------------------------------------------------
void Scene::InitalizeMainLightAndCamera(size_t _width, size_t _height)
{
	//init main light
	m_lights.push_back({});

	m_lights[0].light_position = glm::vec4(0.5f, 2.0f, -4.0f, 1.0f);
	m_lights[0].light_direction = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	m_lights[0].intensity = glm::vec4{ 10.0f, 10.0f ,10.0f, 1.0f };
	m_lights[0].type = eLightType::DIRECTION;
	m_lights[0].constant = 0.9f;
	m_lights[0].linear = 0.5f;
	m_lights[0].quadratic = 0.03f;
	m_lights[0].radius = 100.f;

	std::array<glm::vec4, 4> points = { // for area light
	glm::vec4(-0.33f, -0.33f, 0.0f, 1.0f),
	glm::vec4(-0.33f, 0.33f, 0.0f, 1.0f),
	glm::vec4(0.33f, 0.33f, 0.0f, 1.0f),
	glm::vec4(0.33f, -0.33f, 0.0f, 1.0f) };

	m_lights[0].points = points;

	//area light
	m_lights.push_back({ m_lights[0] });
	m_lights.back().type = eLightType::AREA_LIGHT;
	m_lights.back().ambient = glm::vec4{ 1.0f, 1.0f , 0.0f, 1.0f };
	m_lights.back().intensity = glm::vec4{ 0.75f, 0.75f, 0.5f, 1.0f };
	m_lights.back().light_position = glm::vec4(10.0f, 3.7f, 3.5f, 1.0f);
	m_lights.back().radius = 6.5f;

	//init main camera
	m_cameras.emplace_back(_width, _height, 0.01f, 40.0f);
	m_cameras[0].setDirection(glm::vec3(0.6f, -0.10f, 0.8f));
	m_cameras[0].setPosition(glm::vec3(0.0f, 4.0f, -4.0f));
}