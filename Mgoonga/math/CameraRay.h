#pragma once

#include <base/Object.h>

#include "math.h"

#include "Transform.h"
#include "Geometry.h"

class Camera;

namespace dbb
{
	//------------------------------------------------------------------------------------
	class DLL_MATH CameraRay
	{
	public:
		explicit CameraRay(Camera&);
		CameraRay() = default;

		std::pair<shObject, glm::vec3>   CalculateIntersaction(std::vector<shObject> objects, float _click_x, float _click_y);
		std::vector<shObject>            OnMove(std::vector<shObject> objects, float _click_x, float _click_y);

		std::vector<shObject>						 FrustumCull(std::vector<shObject> _objects, const std::string& _exclusion = "");
		bool														 IsInFrustum(const std::vector<glm::vec3>&);

		bool									           IsPressed() const { return m_is_pressed; }
		bool									           IsPressedWithLeft() const { return m_is_pressed && m_left; }
		bool									           IsPressedWithRight() const { return m_is_pressed && !m_left; }

		dbb::line												 GetLine(float _x, float _y) { return _GetLine({_x, _y}); };
		std::pair<glm::vec2, glm::vec2>  GetFrame() const { return std::pair<glm::vec2, glm::vec2>{ m_press_start, m_press_curr };}
		
		void														 Press(float click_x, float click_y, bool _left);
		void														 Release();

	protected:
		dbb::line				_GetLine(glm::vec2);
		bool					  _IsOpSign(float, float);
		
		std::reference_wrapper<Camera> m_camera;

		Transform			m_transform;

		bool					m_is_pressed = false;
		bool					m_left = false;
		glm::vec2			m_press_start;
		glm::vec2			m_press_curr;
	};
}

