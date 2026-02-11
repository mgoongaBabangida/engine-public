#include "stdafx.h"
#include "Transform.h"

#include <glm\glm\glm.hpp>
#include <glm\glm\gtc\matrix_transform.hpp>
#include <glm\glm\gtx\transform.hpp>
#include <glm\glm/gtc/constants.hpp>
#include <glm/glm/gtx/vector_angle.hpp>

#include <base/base.h>

//-----------------------------------------------------------------------------
void Transform::setRotation(float x, float y, float z)
{
	q_rotation = glm::quat(glm::vec3(x, y, z)); // from euler angles (tutorial 17)
	UpdateModelMatrix();
}

//-----------------------------------------------------------------------------
glm::vec4 Transform::getRotationVector() const
{
	return  glm::toMat4(q_rotation) * glm::vec4(forward, 1.0f);
}

//-----------------------------------------------------------------------------
glm::vec4 Transform::getRotationUpVector() const
{
	return  glm::toMat4(q_rotation) * glm::vec4(Up, 1.0f);
}

//-----------------------------------------------------------------------------
void Transform::UpdateModelMatrix()
{
	glm::mat4 rotatM0 = glm::toMat4(q_rotation);
	totalTransform = glm::translate(m_translation) * rotatM0 * glm::scale(glm::vec3(m_scale.x, m_scale.y, m_scale.z));
}

//-----------------------------------------------------------------------------
const glm::mat4& Transform::getModelMatrix() const
{
	return totalTransform;
}

//-----------------------------------------------------------------------------
void Transform::setModelMatrix(const glm::mat4& M)
{
	totalTransform = M;

	// 1) Translation 4th column
	m_translation = glm::vec3(M[3]);

	// 2) Scale length
	glm::vec3 col0 = glm::vec3(M[0]);
	glm::vec3 col1 = glm::vec3(M[1]);
	glm::vec3 col2 = glm::vec3(M[2]);

	float sx = glm::length(col0);
	float sy = glm::length(col1);
	float sz = glm::length(col2);
	m_scale = glm::vec3(sx, sy, sz);

	// 3) Orthonormal rotation basis (no scale)
	if (sx > 0) col0 /= sx; else col0 = glm::vec3(1, 0, 0);
	if (sy > 0) col1 /= sy; else col1 = glm::vec3(0, 1, 0);
	if (sz > 0) col2 /= sz; else col2 = glm::vec3(0, 0, 1);

	glm::mat3 R;
	R[0] = col0; // local +X -> world
	R[1] = col1; // local +Y -> world
	R[2] = col2; // local +Z -> world

	q_rotation = glm::normalize(glm::quat_cast(R));
}

//-----------------------------------------------------------------------------
bool Transform::isRotationValid()
{
	return q_rotation.x > 0.0f || q_rotation.x < 1.0f 
		&& q_rotation.y > 0.0f || q_rotation.y < 1.0f
		&& q_rotation.z > 0.0f || q_rotation.z < 1.0f;
}

//-----------------------------------------------------------------------------
glm::quat Transform::RotationBetweenVectors(glm::vec3 start, glm::vec3 dest)
{
	start = normalize(start);
	dest = normalize(dest);

	float cosTheta = dot(start, dest);
	glm::vec3 rotationAxis;

	if (cosTheta < -1 + 0.001f) {
		// special case when vectors in opposite directions:
		// there is no "ideal" rotation axis
		// So guess one; any will do as long as it's perpendicular to start
		rotationAxis = glm::cross(glm::vec3(0.0f, 0.0f, 1.0f), start);
		if (glm::length2(rotationAxis) < 0.01) // bad luck, they were parallel, try again!
			rotationAxis = glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), start);

		rotationAxis = normalize(rotationAxis);
		return glm::angleAxis(glm::radians(180.0f), rotationAxis);
	}

	rotationAxis = cross(start,dest);

	float s = sqrt((1 + cosTheta) * 2);
	float invs = 1 / s;

	return glm::quat(
		s * 0.5f,
		rotationAxis.x * invs,
		rotationAxis.y * invs,
		rotationAxis.z * invs
	);
}

//-----------------------------------------------------------------------------
Transform Transform::interpolate(const Transform& first, const Transform& second, float t)
{
	t = glm::clamp(t, 0.0f, 1.0f);

	Transform out;

	// Translation
	out.setTranslation(glm::mix(first.m_translation, second.m_translation, t));

	// Rotation (fix sign + normalize)
	glm::quat qa = glm::normalize(first.q_rotation);
	glm::quat qb = glm::normalize(second.q_rotation);

	// Hemisphere fix: ensure shortest path
	if (glm::dot(qa, qb) < 0.0f)
		qb = -qb;

	// Use slerp (best) or nlerp (fast & usually fine)
	glm::quat q = glm::normalize(glm::slerp(qa, qb, t));
	// glm::quat q = glm::normalize(glm::lerp(qa, qb, t)); // nlerp alternative

	out.q_rotation = q;

	// Scale: don’t force 1 unless you’re 100% sure your data is always 1
	out.m_scale = glm::mix(first.m_scale, second.m_scale, t);

	out.UpdateModelMatrix();
	return out;
}

//----------------------------------------------------------------------------------------------------
float Transform::AngleDegreesBetweenVectors(glm::vec3 _start, glm::vec3 _dest)
{
	return glm::degrees(glm::angle(glm::normalize(_start), glm::normalize(_dest)));
}

//-----------------------------------------------------------------------------
void Transform::billboard(glm::vec3 direction)
{
	// Rotation aroun Y
	float projZ = glm::dot(glm::normalize(direction), glm::vec3(0.0f, 0.0f, 1.0f));
	float projX = glm::dot(glm::normalize(direction), glm::vec3(-1.0f, 0.0f, 0.0f)); //or +1
	glm::vec3 projXZ = glm::normalize(glm::vec3(projX, 0, projZ));
	
	float Yrot = direction.x > 0.0f 
				? acos(glm::dot(projXZ, glm::vec3(0.0f, 0.0f, 1.0f))) 
				: (float)PI * 2 - acos(glm::dot(projXZ, glm::vec3(0.0f, 0.0f, 1.0f)));

	// Rotation aroun X
	float Xrot = glm::dot(glm::normalize(direction), glm::vec3(0.0f, 1.0f, 0.0f)) > 0 
				? -(PI / 2 - acos(glm::dot(direction, glm::vec3(0.0f, 1.0f, 0.0f)))) 
				: -(PI / 2 - acos(glm::dot(direction, glm::vec3(0.0f, 1.0f, 0.0f))));

	setRotation(Xrot, Yrot, 0);
}

//-----------------------------------------------------------------------------
bool Transform::turnToOld(glm::vec3 dest, float speed) //@todo speed should be tested
{	
	glm::vec3 to_target = dest - m_translation;
	if (glm::length(to_target) < 1e-6f)
		return false;

	glm::vec3 target_dir = glm::normalize(to_target);
	glm::vec3 current_forward = glm::normalize(glm::mat3(glm::toMat4(q_rotation)) * forward);
	if (glm::length(current_forward) < 1e-6f)
		return false;

	float angle			= glm::clamp(glm::dot(current_forward, target_dir), -1.0f, 1.0f);
	float angle_rad = glm::acos(angle);

	if (angle_rad < glm::radians(0.0001f)) // Almost aligned
		return false;
	
	if (speed > 0)
	{
		// Clamp speed to never exceed angle between vectors
		float step_rad = glm::radians(glm::min(speed, glm::degrees(angle_rad)));
		angle = cos(step_rad);
	}

	glm::quat rot;
	glm::vec3 axis = glm::cross(current_forward, target_dir);
	if (target_dir + current_forward == glm::vec3{} || (angle == 1.0f && glm::length(axis) == 0)) // 180 degrees
	{ 
		rot = RotationBetweenVectors(current_forward, target_dir); //will not be correct if speed is used
		setRotation(rot * getRotation());
		return true;
	}
	else if (angle > 0.0f)
		{ rot = glm::toQuat(glm::rotate(UNIT_MATRIX, glm::acos(angle), axis));}
	else if (angle < 0.0f)
		{ rot = glm::toQuat(glm::rotate(UNIT_MATRIX, 2 * PI - (glm::acos(angle)), -axis));}
	else 
		{ return false;} // angle inf(nan)

	setRotation(rot * getRotation());
	return true;
}

//-----------------------------------------------------------------------------------------
bool Transform::turnTo(glm::vec3 dest, float speed)  //@todo speed should be tested
{
	glm::vec3 to_target = dest - m_translation;
	if (glm::length(to_target) < 1e-6f)
		return false;

	glm::vec3 target_dir = glm::normalize(to_target);
	glm::vec3 current_forward = glm::normalize(glm::mat3(glm::toMat4(q_rotation)) * forward);
	if (glm::length(current_forward) < 1e-6f)
		return false;

	// Check for parallel or already aligned
	float dot = glm::clamp(glm::dot(current_forward, target_dir), -1.0f, 1.0f);
	float angle_rad = glm::acos(dot);

	if (angle_rad < 0.001f) // Almost aligned
		return false;

	// If speed <= 0, rotate instantly
	if (speed <= 0.0f)
	{
		glm::quat full_rot = glm::rotation(current_forward, target_dir); // glm::rotation is an alias for glm::rotationBetweenVectors if you're using GLM extensions
		setRotation(glm::normalize(full_rot * q_rotation));
		return true;
	}

	// Clamp speed to never exceed angle between vectors
	float step_rad = glm::radians(glm::min(speed, glm::degrees(angle_rad)));

	// Handle 180° case
	glm::vec3 axis = glm::cross(current_forward, target_dir);
	if (glm::length(axis) < 1e-6f)
	{
		// Find any axis perpendicular to current_forward
		axis = glm::normalize(glm::cross(current_forward, glm::vec3(1.0f, 0.0f, 0.0f)));
		if (glm::length(axis) < 1e-6f)
			axis = glm::normalize(glm::cross(current_forward, glm::vec3(0.0f, 1.0f, 0.0f)));
	}

	axis = glm::normalize(axis);
	glm::quat step_rot = glm::angleAxis(step_rad, axis);
	setRotation(glm::normalize(step_rot * q_rotation));
	return true;
}



