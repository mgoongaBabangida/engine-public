#pragma once

#include "opengl_assets.h"

#include <base/interfaces.h>
#include <math/Camera.h>
#include <math/Decal.h>

#include "GUI.h"

//--------------------------------------------
struct DLL_OPENGL_ASSETS Scene
{
	Scene() = default;
	Scene(const Scene&) = delete;
	Scene& operator=(const Scene&) = delete;

	Scene(Scene&&) noexcept = default;
	Scene& operator=(Scene&&) noexcept = default;

	std::vector<Light>											m_lights;
	std::vector<Camera>											m_cameras;
	size_t																	m_cur_camera = 0;

	shObject																m_focused;
	shObject																m_hovered;

	std::shared_ptr<std::vector<shObject>>	m_framed;
	std::vector<std::shared_ptr<GUI>>				m_guis;
	std::vector<std::shared_ptr<Text>>			m_texts;
	std::vector<Decal>											m_decals;
	std::shared_ptr<GUI>										m_welcome; //splash screen

	std::vector<shObject>										m_objects; //@todo protect access

	std::string															m_drop_path;
	bool																		m_update_hovered = false;
	//debuging
	shObject																m_camera_obj;

	void InitalizeMainLightAndCamera(size_t _width, size_t _height);
};