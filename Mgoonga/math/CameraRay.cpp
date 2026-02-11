#include "stdafx.h"
#include "CameraRay.h"
#include "BaseCollider.h"
#include "Camera.h"

#include <glm\glm\glm.hpp>
#include <glm\glm\gtc\matrix_transform.hpp>
#include <glm\glm\gtx\transform.hpp>
#include <glm\glm/gtc/constants.hpp>
#include <map>

namespace dbb
{

	//-------------------------------------------------------------------------------
	CameraRay::CameraRay(Camera& _camera)
		: m_camera(_camera)
	{}

	//------------------------------------------------------------------------------------------------
	std::vector<shObject> CameraRay::OnMove(std::vector<shObject> _objects, float click_x, float click_y)
	{
		const Camera& camera = m_camera.get();
		m_press_curr = glm::vec2(click_x, click_y);
		std::vector<shObject> ret;
		if (m_is_pressed)
		{
			line  line1 = _GetLine(m_press_start);
			line  line2 = _GetLine(m_press_curr);
			line  line3 = _GetLine(glm::vec2(m_press_start.x, m_press_curr.y));
			line  line4 = _GetLine(glm::vec2(m_press_curr.x, m_press_start.y));

			plane left(line1.M, line3.M, line1.M + line1.p);
			plane right(line2.M, line4.M, line2.M + line2.p);
			plane top(line1.M, line4.M, line1.M + line1.p);
			plane bottom(line2.M, line3.M, line2.M + line2.p);

			for (auto& obj : _objects)
			{
				if (!obj->GetCollider())
					continue;

				if (obj->Name() == "GrassPlane") //@todo temp degub !!!
					continue;

				std::vector<glm::vec3> extrems = obj->GetCollider()->GetExtrems(*(obj->GetTransform()));
				extrems.push_back(obj->GetTransform()->getModelMatrix() * glm::vec4(obj->GetCollider()->GetCenterLocalSpace(), 1.0f));

				for (auto& extrem : extrems)
				{
					if (!_IsOpSign(left.A * extrem.x + left.B * extrem.y + left.C * extrem.z + left.D,
						right.A * extrem.x + right.B * extrem.y + right.C * extrem.z + right.D) &&
						!_IsOpSign(top.A * extrem.x + top.B * extrem.y + top.C * extrem.z + top.D,
							bottom.A * extrem.x + bottom.B * extrem.y + bottom.C * extrem.z + bottom.D))
					{
						ret.push_back(obj);
						break;
					}
				}
			}
			//std::cout << "Grabed " << ret.size() << " Objects" << std::endl;
		}
		return ret;
	}

	//------------------------------------------------------------------------------------------------
	std::vector<shObject> CameraRay::FrustumCull(std::vector<shObject> _objects, const std::string& _exclusion)
	{
		const Camera& camera = m_camera.get();
		std::vector<shObject> ret;

		line  line1 = _GetLine({ 0, 0 });
		line  line2 = _GetLine({ camera.getWidth(), camera.getHeight() });
		line  line3 = _GetLine(glm::vec2(0, camera.getHeight()));
		line  line4 = _GetLine(glm::vec2(camera.getWidth(), 0));

		plane left(line1.M, line3.M, line1.M + line1.p);
		plane right(line2.M, line4.M, line2.M + line2.p);
		plane top(line1.M, line4.M, line1.M + line1.p);
		plane bottom(line2.M, line3.M, line2.M + line2.p);
		plane front(line1.M, line2.M, line3.M);
		plane back(line1.M + line1.p, line2.M + line2.p, line3.M + line3.p);

		for (auto& obj : _objects)
		{
			if (!obj->GetCollider())
				continue;

			if (!_exclusion.empty() && obj->Name() == _exclusion)
				continue;

			std::vector<glm::vec3> extrems = obj->GetCollider()->GetExtrems(*(obj->GetTransform()));
			extrems.push_back(obj->GetTransform()->getModelMatrix() * glm::vec4(obj->GetCollider()->GetCenterLocalSpace(), 1.0f));

			for (auto& extrem : extrems)
			{
				if (!_IsOpSign(left.A * extrem.x + left.B * extrem.y + left.C * extrem.z + left.D,
						right.A * extrem.x + right.B * extrem.y + right.C * extrem.z + right.D) &&
					!_IsOpSign(top.A * extrem.x + top.B * extrem.y + top.C * extrem.z + top.D,
						bottom.A * extrem.x + bottom.B * extrem.y + bottom.C * extrem.z + bottom.D) &&
					 _IsOpSign(back.A * extrem.x + back.B * extrem.y + back.C * extrem.z + back.D,
						front.A * extrem.x + front.B * extrem.y + front.C * extrem.z + front.D))
				{
					ret.push_back(obj);
					break;
				}
			}
		}
		return ret;
	}

	//------------------------------------------------------------------------------------------------
	bool	CameraRay::IsInFrustum(const std::vector<glm::vec3>& _extrems)
	{
		if (_extrems.size() < 8)
			return false;  // error

		const Camera& camera = m_camera.get();

		// frustum corner rays
		line  line1 = _GetLine({ 0, 0 });
		line  line2 = _GetLine({ camera.getWidth(), camera.getHeight() });
		line  line3 = _GetLine(glm::vec2(0, camera.getHeight()));
		line  line4 = _GetLine(glm::vec2(camera.getWidth(), 0));

		//frustum planes
		plane left(line1.M, line3.M, line1.M + line1.p);
		plane right(line2.M, line4.M, line2.M + line2.p);
		plane top(line1.M, line4.M, line1.M + line1.p);
		plane bottom(line2.M, line3.M, line2.M + line2.p);
		plane front(line1.M, line2.M, line3.M);
		plane back(line1.M + line1.p,
							 line2.M + line2.p,
							 line3.M + line3.p);

		//if at least one of the extrems is inside frustum box -> true
		for (auto& extrem : _extrems)
		{
			if (!_IsOpSign(left.A * extrem.x + left.B * extrem.y + left.C * extrem.z + left.D,
										right.A * extrem.x + right.B * extrem.y + right.C * extrem.z + right.D) &&
					!_IsOpSign(top.A * extrem.x + top.B * extrem.y + top.C * extrem.z + top.D,
										bottom.A * extrem.x + bottom.B * extrem.y + bottom.C * extrem.z + bottom.D) &&
					 _IsOpSign(back.A * extrem.x + back.B * extrem.y + back.C * extrem.z + back.D,
										front.A * extrem.x + front.B * extrem.y + front.C * extrem.z + front.D))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	std::pair<shObject, glm::vec3> CameraRay::CalculateIntersaction(std::vector<shObject> objects, float _click_x, float _click_y)
	{
		std::multimap<shObject, glm::vec3> intersections;
		dbb::line	 ray = _GetLine({ _click_x , _click_y });

		for (auto& obj : objects)
		{
			if (!obj->GetCollider() || !obj->IsPickable())
				continue;

			std::vector<glm::mat3> boundings = obj->GetCollider()->GetBoundingTriangles(*(obj->GetTransform()));
			line line;
			line.p = glm::vec4(ray.p, 1.0f);
			line.M = glm::vec4(ray.M, 1.0f);

			for (auto& triangle : boundings)
			{
				plane plane(triangle);
				glm::vec3 inters = intersection(plane, line);
				if (IsInside(triangle, inters) && glm::dot(line.p, glm::vec3(inters - line.M)) > 0.0f) //check if behind 
				{
					intersections.insert(std::pair<shObject, glm::vec3>(obj, inters));
				}
			}
		}

		if (intersections.empty())
		{
			return {};
		}
		else
		{
			float length = 1000.0f; //@ todo should be in line with far plane
			std::pair<shObject, glm::vec3> obj;
			for (auto& inter : intersections)
			{
				if (glm::length(ray.M - inter.second) < length)
				{
					length = glm::length(ray.M - inter.second);
					obj = inter;
				}
			}
			return obj;
		}
	}

	//------------------------------------------------------------------------------------------------
	void CameraRay::Press(float click_x, float click_y, bool _left)
	{
		m_press_start = m_press_curr = glm::vec2(click_x, click_y);
		m_is_pressed = true;
		m_left = _left;
	}

	//------------------------------------------------------------------------------------------------
	void CameraRay::Release()
	{
		m_press_start = m_press_curr = glm::vec2(-1, -1);
		m_is_pressed = false;
	}

	//------------------------------------------------------------------------------------------------
	line CameraRay::_GetLine(glm::vec2 _click)
	{
		line line;
		const Camera& camera = m_camera.get();
		m_transform.billboard(camera.getDirection());
		m_transform.setTranslation(camera.getPosition());

		float W = (float)camera.getWidth();
		float H = (float)camera.getHeight();

		float XOffsetCoef = _click.x / W;
		float YOffsetCoef = _click.y / H;

		float heightN = 2 * tan(glm::radians(camera.getZoom() / 2)) * camera.getNearPlane(); // zoom is Y view angle 
		float heightF = 2 * tan(glm::radians(camera.getZoom() / 2)) * camera.getFarPlane();

		float widthN = heightN * (W / H);
		float widthF = heightF * (W / H);

		glm::vec3 dot1 = glm::vec3(widthN / 2 - XOffsetCoef * widthN, heightN / 2 - YOffsetCoef * heightN, camera.getNearPlane());
		glm::vec3 dot2 = glm::vec3(widthF / 2 - XOffsetCoef * widthF, heightF / 2 - YOffsetCoef * heightF, camera.getFarPlane());

		glm::vec3 dir = dot2 - dot1;
		line.M = m_transform.getModelMatrix() * glm::vec4(dot1, 1.0f); //origin
		line.p = glm::mat3(m_transform.getRotation()) * dir;  //direction (should not be normalized to use it to find dot on far plane)

		return line;
	}

	//------------------------------------------------------------------------------------------------
	bool CameraRay::_IsOpSign(float a, float b)
	{
		if (a > 0 && b < 0)
			return true;
		if (b > 0 && a < 0)
			return true;
		return false;
	}
}

