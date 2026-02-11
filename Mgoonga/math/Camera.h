#pragma once

#include "math.h"
#include "CameraRay.h"

//---------------------------------------------------------------------
class DLL_MATH Camera
{
public:
	Camera(float width,
				 float height,
				 float nearPlane,
				 float farPlane,
				 float perspectiveRatio = 60.0f,
				 glm::vec3 _position		 = glm::vec3( 0.0f, 3.0f, 0.0f ),
				 glm::vec3 _viewDirection = glm::vec3(0.0f, 0.0f, 1.0f),
				 glm::vec3 _up = glm::vec3(0.0f, 1.0f, 0.0f));

	Camera(const Camera& other);
	Camera&					operator=(const Camera & other);
	~Camera() = default;
	
	void					mouseUpdate(const glm::vec2& newMousePosition);

	void					useOrthographic(bool on) { m_useOrtho = on; }

	// angles in degrees
	void rotateYaw(float degrees);    // turn left/right
	void rotatePitch(float degrees);  // look up/down
	void rotateRoll(float degrees);   // tilt head
	void rotateAroundAxis(const glm::vec3& axis_world, float degrees); // generic

	std::vector<glm::vec4>		getFrustumCornersWorldSpace()	const;
	std::array<dbb::plane, 6> BuildFrustumPlanes() const;
	bool											AABBInFrustum(const extremDots& ext);

	glm::mat4								getWorldToViewMatrix()			const;
	glm::vec3								getPosition()					const;
	glm::vec3								getDirection()					const;
	glm::mat3								getRotationMatrix()				const;
	const glm::mat4&				getProjectionMatrix()			const;
	glm::mat4								getProjectionBiasedMatrix()		const;
	const glm::mat4&				getProjectionOrthoMatrix()		const;
	uint32_t								getWidth()						const	{ return m_width;  }
	uint32_t								getHeight()						const	{ return m_height; }
	float										getNearPlane()					const	{ return m_nearPlane; }
	float										getFarPlane()					const;
	float										getZoom()						const	{ return m_zoom; }
	dbb::CameraRay&					getCameraRay()							{ return m_camRay; }
	glm::vec3								getStrafeDirection() { return m_strafeDirection; }
	glm::vec3								getUpVector() { return m_up; }
	float										getRotationSpeed() { return m_rotation_speed; }

	bool&					VisualiseFrustum()								{ return m_visualise_frustum; }
	const bool&		VisualiseFrustum() const					{ return m_visualise_frustum; }

	void					SetVisualiseFrustum(bool _v)			{ m_visualise_frustum = _v; }
	void					UpdateProjectionMatrix();

	glm::quat GetRotationQuat() const;

	inline glm::vec3 Forward() const { return glm::normalize(m_viewDirection); }
	inline glm::vec3 Right()   const { return glm::normalize(glm::cross(m_up, m_viewDirection)); }
	inline glm::vec3 Up()      const { return glm::normalize(m_up); }

	void					moveForward();
	void					moveBackword();
	void					strafeLeft();
	void					strafeRight();
	void					moveUp();
	void					moveDown();

	void setPosition(glm::vec3 newPos)								{ m_position = newPos;}
	void setDirection(glm::vec3 newDir)								{ m_viewDirection = newDir; }
	void setNearPlane(float near);
	void setFarPlane(float far);
	void setProjectionOrthoMatrix(const glm::mat4&);

	glm::vec3&			PositionRef()							{ return m_position; }
	glm::vec3&			ViewDirectionRef()				{ return m_viewDirection; }
	float&					NearPlaneRef()						{ return m_nearPlane; }
	float&					FarPlaneRef()							{ return m_farPlane; }
	uint32_t&				StrafeThresholdRef()			{ return m_strafeThreshold; }
	float&					MovementSpeedRef()				{ return m_movement_speed; }
	float&					RotationSpeedRef()				{ return m_rotation_speed; }
	float&					ZoomRef()									{ return m_zoom; }
	int&						WidthRef()								{ return m_width; }
	int&						HeightRef()								{ return m_height; }

protected:
	glm::vec3		m_position;
	glm::vec3		m_viewDirection;
	glm::vec3		m_up;
	glm::vec2		m_oldMousePosition;
	glm::vec3		m_strafeDirection;
	glm::mat3		m_rotationMatrix;
	float				m_movement_speed			= 0.000'005f;
	float				m_rotation_speed			= 0.005f;

	glm::mat4		m_projectionMatrix;
	glm::mat4		m_projectionOrthoMatrix;

	int					m_width;
	int					m_height;
	float				m_nearPlane;
	float				m_farPlane;
	float				m_zoom					= 60.0f;

	dbb::CameraRay	m_camRay;

	uint32_t		m_strafeThreshold			= 5;
	bool				m_visualise_frustum =	 false;

	bool m_useOrtho = false;
};


