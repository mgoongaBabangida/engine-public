#include "stdafx.h"
#include "CameraRPGController.h"

//-----------------------------------------------------------------------
CameraRPGController::CameraRPGController(Camera& camera, float heightOffset, float distance)
  : m_camera(camera), m_heightOffset(heightOffset), m_distance(distance) {}

//-------------------------------------------------------------------------------
void CameraRPGController::Initialize()
{
  if (auto object = m_object.lock())
  {
    glm::vec3 cameraPosition = object->GetTransform()->getTranslation() - glm::vec3(object->GetTransform()->getRotationVector()) * m_distance;
    cameraPosition.y += m_heightOffset;
    glm::vec3 focusPoint = object->GetTransform()->getTranslation();
    m_camera.get().setPosition(cameraPosition);
    m_camera.get().setDirection(glm::normalize(focusPoint - m_camera.get().getPosition()));

    m_object_pos = object->GetTransform()->getTranslation();
    m_object_rotation = object->GetTransform()->getRotation();
    m_need_update = false;
  }
}

//-----------------------------------------------------------
bool CameraRPGController::OnMousePress(int32_t x, int32_t y, bool left, KeyModifiers _modifier)
{
  m_camera.get().getCameraRay().Press(x, y, left);
  m_camera.get().MovementSpeedRef() = 0.f;
  m_camera.get().RotationSpeedRef() = 0.f;
  return false;
}

//-----------------------------------------------------------
bool CameraRPGController::OnMouseRelease(KeyModifiers _modifier)
{
  m_camera.get().getCameraRay().Release();
  m_camera.get().MovementSpeedRef() = 0.000'005f;
  m_camera.get().RotationSpeedRef() = 0.005f;
  return false;
}

//-------------------------------------------------------------------------------
void CameraRPGController::Update(float deltaTime)
{
  if (auto object = m_object.lock())
  {
    if (object->GetTransform()->getTranslation() != m_object_pos
      || object->GetTransform()->getRotation() != m_object_rotation
      || m_need_update)
    {
      const ITransform* transform = object->GetTransform();
      UpdateCameraPosition(transform);
    }
  }
}

//-------------------------------------------------------------------------------
bool CameraRPGController::OnMouseMove(int32_t _x, int32_t _y, KeyModifiers _modifiers)
{
  if (_modifiers == KeyModifiers::SHIFT)
    m_camera.get().StrafeThresholdRef() = 5.0f;
  else
    m_camera.get().StrafeThresholdRef() = 0.0f;

  m_camera.get().mouseUpdate(glm::vec2(_x, _y));
  return true;
}

//-----------------------------------------------------------
bool CameraRPGController::OnMouseWheel(int32_t _x, int32_t _y, KeyModifiers _modifier)
{
  if (_y > 0)
  {
    m_heightOffset += 0.05f;
    m_distance += 0.05f;
    m_need_update = true;
  }
  else if (_y < 0)
  {
    m_heightOffset -= 0.05f;
    m_distance -= 0.05f;
    m_need_update = true;
  }
  return true;
}

//-------------------------------------------------------------------------------
void CameraRPGController::UpdateCameraPosition(const ITransform*& transform)
{
  if (!transform) return;

  glm::vec3 objectPosition = transform->getTranslation();
  glm::vec3 objectForward = transform->getRotationVector();

  // Calculate camera position
  glm::vec3 cameraPosition = objectPosition - objectForward * m_distance;
  cameraPosition.y += m_heightOffset;

  // Update the camera
  float focusPointDistance = std::sqrt(m_distance * m_distance + m_heightOffset * m_heightOffset);
  glm::vec3 focusPoint = m_camera.get().getPosition() + m_camera.get().getDirection() * focusPointDistance;
  m_camera.get().setPosition(cameraPosition);
  m_camera.get().setDirection(glm::normalize(focusPoint - cameraPosition));

  m_object_pos = transform->getTranslation();
  m_object_rotation = transform->getRotation();
  m_need_update = false;

}

//--------------------------------------------------------------------------------
void CameraRPGController::RotateAroundObject(float dx, float dy)
{
  glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), dx * m_camera.get().getRotationSpeed(), m_camera.get().getUpVector());
  rotation = glm::rotate(rotation, dy * m_camera.get().getRotationSpeed(), m_camera.get().getStrafeDirection());

  glm::vec3 offset = m_camera.get().getPosition() - m_object.lock()->GetTransform()->getTranslation();
  offset = glm::vec3(rotation * glm::vec4(offset, 1.0f));

  //m_camera.get().setPosition(m_object.lock()->GetTransform()->getTranslation() + offset);
  m_camera.get().setDirection(glm::normalize(-offset));
}

