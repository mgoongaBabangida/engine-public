#pragma once
#include <base/interfaces.h>
#include <math/Geometry.h>
#include <math/Camera.h>

//-------------------------------------------------------------------------------
class InputStrategy : public IInputObserver
{
public:
	//IInputObserver
	virtual ~InputStrategy() = default;
	virtual bool OnKeyPress(uint32_t asci) { return false; }
	virtual bool OnMouseMove(uint32_t x, uint32_t y) { return false; }
	virtual bool OnMousePress(uint32_t x, uint32_t y, bool left) { return false; }
	virtual bool OnMouseRelease() { return false; }

	virtual void UpdateInRenderThread() {}
};

//-------------------------------------------------------------------------------
class InputStrategyMoveAlongPlane : public InputStrategy
{
public:

	enum MovePlane
	{
		XZ = 0,
		XY = 1
	};

	InputStrategyMoveAlongPlane(Camera& _camera, std::vector<shObject> _objs, MovePlane _move_plane)
		: m_camera(_camera), m_move_plane(_move_plane)
	{
		for (auto obj : _objs)
			m_objects.push_back(obj);
	}
	~InputStrategyMoveAlongPlane()
	{

	}

	//--------------------------------------------------------------------------------------------
	virtual bool OnMousePress(uint32_t _x, uint32_t _y, bool _left)
	{
		std::vector<shObject> objects;
		for (auto obj : m_objects)
			if (!obj.expired())
				objects.push_back(obj.lock());

		auto [new_focused, intersaction] = m_camera.get().getCameraRay().CalculateIntersaction(objects, static_cast<float>(_x), static_cast<float>(_y));
		if (!_left && new_focused) // grab if left is pressed
		{
			m_grab_translation = new_focused->GetTransform()->getTranslation();
			m_intersaction = intersaction;
			m_grab_camera_line = m_camera.get().getCameraRay().GetLine(static_cast<float>(_x), static_cast<float>(_y));
			m_dragged = new_focused;
			return true;
		}
		return false;
	}

	//--------------------------------------------------------------------------------------------
	virtual bool OnMouseMove(uint32_t _x, uint32_t _y)
	{
		if (m_grab_camera_line != std::nullopt) // button is pressed
		{			
			if (m_move_plane == XZ)
			{
				dbb::plane pl(m_intersaction,
											glm::vec3(0.0f, m_intersaction.y, 1.0f),
											glm::vec3(1.0f, m_intersaction.y, 0.0f)); // arbitrary triangle on xz plane
				glm::vec3 new_intersaction = dbb::intersection(pl, m_camera.get().getCameraRay().GetLine(static_cast<float>(_x), static_cast<float>(_y)));
				m_translation_vector = new_intersaction - m_intersaction;
			}
			else if (m_move_plane == XY)
			{
				dbb::plane pl(m_intersaction,
										  glm::vec3(0.0f, 1.0f, m_intersaction.z),
										  glm::vec3(1.0f, 0.0f, m_intersaction.z)); // arbitrary triangle on xy plane
				glm::vec3 new_intersaction = dbb::intersection(pl, m_camera.get().getCameraRay().GetLine(static_cast<float>(_x), static_cast<float>(_y)));
				m_translation_vector = new_intersaction - m_intersaction;
			}
			return true;
		}
		else
		{
			return false;
		}
	}

	//-----------------------------------------------------------------------------
	virtual bool OnMouseRelease()
	{
		m_grab_camera_line = std::nullopt;
		m_translation_vector = glm::vec3{ 0.0f, 0.0f, 0.0f };
		return true;
	}

	//-----------------------------------------------------------------------------
	virtual void UpdateInRenderThread()
	{
		if (shObject obj = m_dragged.lock(); obj && m_grab_camera_line != std::nullopt && m_translation_vector != glm::vec3{ 0.f,0.f,0.0f })
			obj->GetTransform()->setTranslation(m_grab_translation + m_translation_vector);
	}

protected:
	std::reference_wrapper<Camera>				m_camera;
	std::vector<std::weak_ptr<eObject>>		m_objects;
	std::weak_ptr<eObject>								m_dragged;
	std::optional<dbb::line>							m_grab_camera_line = std::nullopt;
	glm::vec3															m_intersaction;
	glm::vec3															m_grab_translation;
	glm::vec3															m_translation_vector = glm::vec3{ 0.f,0.f,0.0f };
	MovePlane															m_move_plane;
};

// This works together with bezier 2d tool, move controll points with right button
//---------------------------------------------------------------
class InputStrategy2DMove : public InputStrategy
{
public:
	InputStrategy2DMove(IGame*_game, glm::vec2 _screeen_restrictions_horizontal, glm::vec2 _screeen_restrictions_vertical)
		: m_game(_game)
	{
		//represents the gui window in NDC coordenate space with inverted y - coordinate
		// should come in constructor
		x_restriction = _screeen_restrictions_horizontal;
		y_restriction = _screeen_restrictions_vertical;
	}

	~InputStrategy2DMove()
	{

	}

	//-----------------------------------------------------------------------
	virtual bool OnMouseMove(uint32_t _x, uint32_t _y) 
	{
		if (m_grabbed != nullptr)
		{
			float x_ss = (((float)_x / (float)m_game->Width()) - 0.5f) * 2.0f;
			float y_ss = -(((float)_y / (float)m_game->Height()) - 0.5f) * 2.0f;

			if (x_ss > x_restriction.x && x_ss < x_restriction.y
			 && y_ss > y_restriction.x && y_ss < y_restriction.y)
			{
				m_grabbed->GetTransform()->setTranslation({ x_ss, y_ss , 0.0f });
			}
		}
		return true;
	}

	//------------------------------------------------------------------------
	virtual bool OnMousePress(uint32_t _x, uint32_t _y, bool _left)
	{ 
		if (!_left)
		{
			std::vector<std::shared_ptr<eObject>> objs = GetObjectsWithChildren(m_game->GetObjects());
			float x_ss = (((float)_x / (float)m_game->Width()) - 0.5f) * 2.0f;
			float y_ss = -(((float)_y / (float)m_game->Height()) - 0.5f) * 2.0f;
			for (auto& obj : objs)
			{
				if (obj->Is2DScreenSpace())
				{
					float delta_x = abs(obj->GetTransform()->getTranslation().x - x_ss);
					float delta_y = abs(obj->GetTransform()->getTranslation().y - y_ss);
					if (delta_x < m_grab_threshold && delta_y < m_grab_threshold)
					{
						m_grabbed = obj;
						return true;
					}
				}
			}
		}
		return false;
	}

	//-----------------------------------------------------------------
	virtual bool OnMouseRelease()
	{
		m_grabbed = nullptr;
		return false;
	}

protected:
	IGame* m_game = nullptr;
	shObject m_grabbed = nullptr;
	glm::vec2 x_restriction = { -1.0f, 1.0f };
	glm::vec2 y_restriction = { -1.0f, 1.0f };
	float m_grab_threshold = 0.02f;
};
