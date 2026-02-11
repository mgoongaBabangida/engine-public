#include "stdafx.h"
#include "CameraSecondScript.h"

//----------------------------------------------------
CameraSecondScript::CameraSecondScript(Camera* _camera, IGame* _game)
  : m_camera(_camera)
  , m_game(_game)
{

}

//----------------------------------------------------
void	CameraSecondScript::Update(float _tick)
{
  if (shObject object = m_object.lock(); object)
  {
    object->GetTransform()->setTranslation(m_camera->getPosition());
    object->GetTransform()->setRotation(glm::toQuat(m_camera->getRotationMatrix()));
    if(m_camera->VisualiseFrustum())
      m_game->SetFramed(m_camera->getCameraRay().FrustumCull(m_game->GetObjects()));
  }
}
