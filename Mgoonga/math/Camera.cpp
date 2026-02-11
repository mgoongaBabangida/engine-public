#include "stdafx.h"
#include "Camera.h"
#include <glm\glm\gtx\transform.hpp>

//---------------------------------------------------------------------
Camera::Camera(float			_width,
							float				_height,
							float				_nearPlane,
							float				_farPlane,
							float				_perspectiveRatio,
							glm::vec3		_position, 
							glm::vec3		_viewDirection,
							glm::vec3 _up)
: m_up(_up)
, m_strafeDirection(1.0f, 0.0f, 0.0f)
, m_projectionOrthoMatrix(glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, _nearPlane, _farPlane))
, m_width(static_cast<int>(_width))
, m_height(static_cast<int>(_height))
, m_nearPlane(_nearPlane)
, m_farPlane(_farPlane)
, m_zoom(_perspectiveRatio)
, m_position(_position)
, m_viewDirection(_viewDirection)
, m_camRay(*this)
, m_rotationMatrix(UNIT_MATRIX)
{
	#define GLM_FORCE_RADIANS
	m_projectionMatrix = glm::perspective(glm::radians(m_zoom), _width/_height, m_nearPlane, m_farPlane);
}

//---------------------------------------------------------------------
Camera::Camera(const Camera & other)
	: m_camRay(*this)
{
	*this = other;
}

//---------------------------------------------------------------------
Camera& Camera::operator=(const Camera& other)
{
	if (&other != this) 
	{
		m_position							= other.m_position;
		m_viewDirection					= other.m_viewDirection;
		m_up										= other.m_up;
		m_oldMousePosition			= other.m_oldMousePosition;
		m_strafeDirection				= other.m_strafeDirection;
		m_rotationMatrix				= other.m_rotationMatrix;
		m_movement_speed				= other.m_movement_speed;
		m_camRay								= other.m_camRay;
		m_projectionMatrix			= other.m_projectionMatrix;
		m_projectionOrthoMatrix	= other.m_projectionOrthoMatrix;
		m_width									= other.m_width;
		m_height								= other.m_height;
		m_nearPlane							= other.m_nearPlane;
		m_farPlane							= other.m_farPlane;
		m_zoom									= other.m_zoom;
		m_strafeThreshold				= other.m_strafeThreshold;
	}
	return *this;
}
//---------------------------------------------------------------------
glm::mat4 Camera::getWorldToViewMatrix() const
{
	return glm::lookAt(m_position, m_position + m_viewDirection, m_up);
}
//---------------------------------------------------------------------
glm::vec3 Camera::getPosition() const
{
	return m_position;
}
//---------------------------------------------------------------------
glm::vec3 Camera::getDirection() const
{
	return m_viewDirection;
}
//---------------------------------------------------------------------
glm::mat3 Camera::getRotationMatrix() const
{
	 return this->m_rotationMatrix;
}
//---------------------------------------------------------------------
const glm::mat4& Camera::getProjectionMatrix() const
{
	return m_useOrtho ? m_projectionOrthoMatrix : m_projectionMatrix;
}
//---------------------------------------------------------------------
glm::mat4 Camera::getProjectionBiasedMatrix() const
{
	 return glm::mat4(glm::vec4(0.5f, 0.0f, 0.0f, 0.0f),
					  glm::vec4(0.0f, 0.5f, 0.0f, 0.0f),
					  glm::vec4(0.0f, 0.0f, 0.5f, 0.0f),
					  glm::vec4(0.5f, 0.5f, 0.5f, 1.0f)) * m_projectionMatrix;
}
//---------------------------------------------------------------------
const glm::mat4& Camera::getProjectionOrthoMatrix() const
{
	 return m_projectionOrthoMatrix;
}

//------------------------------------------------------------------
void Camera::rotateYaw(float degrees)
{
	float radians = glm::radians(degrees);

	// rotate around global up (0,1,0)
	glm::mat4 R = glm::rotate(radians, glm::vec3(0.0f, 1.0f, 0.0f));

	m_viewDirection = glm::mat3(R) * m_viewDirection;
	m_up = glm::mat3(R) * m_up;

	// update strafe + rotationMatrix to stay consistent with mouseUpdate logic
	m_strafeDirection = glm::cross(m_up, m_viewDirection);

	m_rotationMatrix = m_rotationMatrix * glm::mat3(R);
}

//-----------------------------------------------------------------
void Camera::rotatePitch(float degrees)
{
	float radians = glm::radians(degrees);

	// compute right vector based on current orientation
	glm::vec3 right = glm::normalize(glm::cross(m_up, m_viewDirection));

	glm::mat4 R = glm::rotate(radians, right);

	m_viewDirection = glm::mat3(R) * m_viewDirection;
	m_up = glm::mat3(R) * m_up;

	m_strafeDirection = glm::cross(m_up, m_viewDirection);

	m_rotationMatrix = m_rotationMatrix * glm::mat3(R);
}

//-----------------------------------------------------------------
void Camera::rotateRoll(float degrees)
{
	float radians = glm::radians(degrees);

	glm::vec3 fwd = glm::normalize(m_viewDirection);

	glm::mat4 R = glm::rotate(radians, fwd);

	m_up = glm::mat3(R) * m_up;

	m_strafeDirection = glm::cross(m_up, m_viewDirection);

	m_rotationMatrix = m_rotationMatrix * glm::mat3(R);
}

//-----------------------------------------------------------------
void Camera::rotateAroundAxis(const glm::vec3& axis_world, float degrees)
{
	float radians = glm::radians(degrees);
	glm::vec3 axis_n = glm::normalize(axis_world);

	glm::mat4 R = glm::rotate(radians, axis_n);

	m_viewDirection = glm::mat3(R) * m_viewDirection;
	m_up = glm::mat3(R) * m_up;

	m_strafeDirection = glm::cross(m_up, m_viewDirection);

	m_rotationMatrix = m_rotationMatrix * glm::mat3(R);
}

//Index  x y z   NDC(x, y, z)       Role
//0      0 0 0   (-1, -1, -1)->near, bottom, left(NBL)
//1      0 0 1   (-1, -1, 1)->far, bottom, left(FBL)
//2      0 1 0   (-1, 1, -1)->near, top, left(NTL)
//3      0 1 1   (-1, 1, 1)->far, top, left(FTL)
//4      1 0 0   (1, -1, -1)->near, bottom, right(NBR)
//5      1 0 1   (1, -1, 1)->far, bottom, right(FBR)
//6      1 1 0   (1, 1, -1)->near, top, right(NTR)
//7      1 1 1   (1, 1, 1)->far, top, right(FTR)
 
//---------------------------------------------------------------------
std::vector<glm::vec4> Camera::getFrustumCornersWorldSpace() const
{
	const auto inv = glm::inverse(m_projectionMatrix * getWorldToViewMatrix());

	std::vector<glm::vec4> frustumCorners;
	for (unsigned int x = 0; x < 2; ++x)
	{
		for (unsigned int y = 0; y < 2; ++y)
		{
			for (unsigned int z = 0; z < 2; ++z)
			{
				const glm::vec4 pt =
					inv * glm::vec4(
						 2.0f * x - 1.0f,
						 2.0f * y - 1.0f,
						 2.0f * z - 1.0f,
						 1.0f);
				frustumCorners.push_back(pt / pt.w);
			}
		}
	}
	return frustumCorners;
}

//------------------------------------------------------------------------------------
std::array<dbb::plane, 6> Camera::BuildFrustumPlanes() const
{
	std::array<dbb::plane, 6> planes{};

	std::vector<glm::vec4> cornersWS = getFrustumCornersWorldSpace();
	if (cornersWS.size() != 8)
		return planes;

	glm::vec3 c[8];
	for (int i = 0; i < 8; ++i)
		c[i] = glm::vec3(cornersWS[i]);

	// NDC corner mapping:
	const int NBL = 0; // near bottom left
	const int FBL = 1; // far  bottom left
	const int NTL = 2; // near top    left
	const int FTL = 3; // far  top    left
	const int NBR = 4; // near bottom right
	const int FBR = 5; // far  bottom right
	const int NTR = 6; // near top    right
	const int FTR = 7; // far  top    right

	auto makePlane = [](const glm::vec3& a,
		const glm::vec3& b,
		const glm::vec3& c) -> dbb::plane
		{
			// Normal from a,b,c (right-handed, CCW)
			glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
			float d = -glm::dot(n, a);
			return dbb::plane(n, d); // A,B,C,D = n.x,n.y,n.z,d
		};

	// --- Build planes from three points each ---
	// The initial orientation doesn't matter; we'll fix it using an inside point.
	planes[0] = makePlane(c[NBL], c[NBR], c[NTR]); // Near
	planes[1] = makePlane(c[FBL], c[FBR], c[FTR]); // Far
	planes[2] = makePlane(c[NBL], c[FBL], c[FTL]); // Left
	planes[3] = makePlane(c[NBR], c[FBR], c[FTR]); // Right
	planes[4] = makePlane(c[NBL], c[FBL], c[FBR]); // Bottom
	planes[5] = makePlane(c[NTL], c[FTL], c[FTR]); // Top

	// Choose a point we KNOW is inside the frustum: its centroid
	glm::vec3 inside(0.0f);
	for (int i = 0; i < 8; ++i)
		inside += c[i];
	inside /= 8.0f;

	for (auto& p : planes)
	{
		glm::vec3 n(p.A, p.B, p.C);
		float s = glm::dot(n, inside) + p.D;

		// We want inside to give s > 0
		if (s < 0.0f)
		{
			p.A = -p.A;
			p.B = -p.B;
			p.C = -p.C;
			p.D = -p.D;
		}

		p.Normalize();
	}

	return planes;
}

//-----------------------------------------------------------------------
bool Camera::AABBInFrustum(const extremDots& ext)
{
	auto planes = BuildFrustumPlanes();

	glm::vec3 center(
		0.5f * (ext.MinX + ext.MaxX),
		0.5f * (ext.MinY + ext.MaxY),
		0.5f * (ext.MinZ + ext.MaxZ)
	);
	glm::vec3 extents(
		0.5f * (ext.MaxX - ext.MinX),
		0.5f * (ext.MaxY - ext.MinY),
		0.5f * (ext.MaxZ - ext.MinZ)
	);

	for (const auto& p : planes)
	{
		glm::vec3 n(p.A, p.B, p.C); // already normalized

		// Radius of the box projected onto this plane normal
		float r =
			extents.x * std::abs(n.x) +
			extents.y * std::abs(n.y) +
			extents.z * std::abs(n.z);

		float s = glm::dot(n, center) + p.D;

		// If even the furthest point (center + r) is behind the plane,
		// the box is completely outside the frustum.
		if (s + r < 0.0f)
			return false;
	}
	return true;
}

//---------------------------------------------------------------------
void Camera::mouseUpdate(const glm::vec2& newMousePosition)
{
	glm::vec2 mouseDelta = newMousePosition - m_oldMousePosition;
	if(glm::length(mouseDelta) > m_strafeThreshold)
	{
		m_oldMousePosition = newMousePosition;
	}
	else
	{
		m_strafeDirection = glm::cross(m_up, m_viewDirection);
		m_viewDirection = glm::mat3(
			glm::rotate(-mouseDelta.x * m_rotation_speed, m_up) *
			glm::rotate(mouseDelta.y * m_rotation_speed, m_strafeDirection)) * m_viewDirection;

		m_oldMousePosition = newMousePosition;

		m_rotationMatrix = m_rotationMatrix * glm::mat3(glm::rotate(-mouseDelta.x * m_rotation_speed, m_up) *
																								glm::rotate(mouseDelta.y * m_rotation_speed, m_strafeDirection));
	}
}
//---------------------------------------------------------------------
float Camera::getFarPlane() const
{
	return m_farPlane;
}//---------------------------------------------------------------------

void Camera::moveForward()
{
	m_position += m_movement_speed * m_viewDirection;
}
//---------------------------------------------------------------------
void Camera::moveBackword()
{
	m_position -= m_movement_speed * m_viewDirection;
}
//---------------------------------------------------------------------
void Camera::strafeLeft()
{
	m_position += -m_movement_speed * m_strafeDirection;
}
//---------------------------------------------------------------------
void Camera::strafeRight()
{
	m_position += m_movement_speed * m_strafeDirection;
}
//---------------------------------------------------------------------
void Camera::moveUp() 
{
	m_position += m_movement_speed * m_up;
}
//---------------------------------------------------------------------
void Camera::moveDown() 
{
	m_position += -m_movement_speed * m_up;
}
//---------------------------------------------------------------------
void Camera::setNearPlane(float _near)
{
	m_nearPlane			= _near;
	m_projectionMatrix	= glm::perspective(glm::radians(m_zoom), ((float)m_width) / m_height, m_nearPlane, m_farPlane);
}
//---------------------------------------------------------------------
void Camera::setFarPlane(float _far)
{
	m_farPlane = _far;
	m_projectionMatrix = glm::perspective(glm::radians(m_zoom), ((float)m_width) / m_height, m_nearPlane, m_farPlane);
}
//------------------------------------------------------------------------
void Camera::setProjectionOrthoMatrix(const glm::mat4& _ortho)
{
	m_projectionOrthoMatrix = _ortho;
}
//---------------------------------------------------------------------
void Camera::UpdateProjectionMatrix()
{
	m_projectionMatrix = glm::perspective(glm::radians(m_zoom), ((float)m_width) / m_height, m_nearPlane, m_farPlane);
}

//---------------------------------------------------------------------
glm::quat Camera::GetRotationQuat() const
{
	// Step 1: build camera basis
	glm::vec3 fwd = glm::normalize(m_viewDirection);      // camera forward
	glm::vec3 up = glm::normalize(m_up);       // camera up
	glm::vec3 right = glm::normalize(glm::cross(up, fwd));   // camera right

	// Re-orthonormalize up to kill drift
	up = glm::normalize(glm::cross(fwd, right));

	// Step 2: build a 3x3 (or 4x4) rotation matrix using these as columns
	glm::mat3 rotM;
	rotM[0][0] = right.x; rotM[0][1] = right.y; rotM[0][2] = right.z;
	rotM[1][0] = up.x;    rotM[1][1] = up.y;    rotM[1][2] = up.z;
	rotM[2][0] = fwd.x;   rotM[2][1] = fwd.y;   rotM[2][2] = fwd.z;

	// Step 3: convert to quaternion
	glm::quat q = glm::quat_cast(rotM); // same as glm::toQuat for mat3/mat4

	// Optional: normalize to avoid float creep
	q = glm::normalize(q);

	return q;
}
