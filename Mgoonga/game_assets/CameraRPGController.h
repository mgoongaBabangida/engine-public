#pragma once

#include "game_assets.h"

#include <base/interfaces.h>
#include <math/Camera.h>

#include <glm\glm\gtx\transform.hpp>

//-------------------------------------------------------------------------------------
class DLL_GAME_ASSETS CameraRPGController : public IScript
{
public:
  explicit CameraRPGController(Camera& camera, float heightOffset = 4.0f, float distance = 3.0f);

  void Initialize() override;

  void Update(float deltaTime) override;

  virtual bool OnMousePress(int32_t x, int32_t y, bool left, KeyModifiers _modifier) override;
  virtual bool OnMouseRelease(KeyModifiers _modifier) override;
  virtual bool OnMouseMove(int32_t x, int32_t y, KeyModifiers modifier) override;
  virtual bool OnMouseWheel(int32_t _x, int32_t _y, KeyModifiers _modifier) override;

private:
  void UpdateCameraPosition(const ITransform*& transform);
  void RotateAroundObject(float dx, float dy);

protected:
  std::reference_wrapper<Camera> m_camera;

  float m_heightOffset = 5.0f;  // Height above the object
  float m_distance = 5.0f;     // Distance behind the object

  glm::vec2 m_mouse_pos;
  glm::vec3 m_object_pos;
  glm::quat m_object_rotation;
  bool m_need_update = false;
};
