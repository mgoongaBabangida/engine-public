#include "stdafx.h"
#include "LightsController.h"
#include "MainContextBase.h"

#include <math/Camera.h>
#include <opengl_assets/openglrenderpipeline.h>
#include <opengl_assets/ModelManager.h>
#include <opengl_assets/TextureManager.h>
#include <opengl_assets/SoundManager.h>

//-----------------------------------------------------------------
LightsController::LightsController(eMainContextBase* _game, eModelManager* _modelManager, eTextureManager* _texManager, eSoundManager* _soundManager, eOpenGlRenderPipeline& _pipeline, Camera& _camera)
  : GameScript(_game, _modelManager, _texManager, _soundManager, _pipeline, _camera)
{
}

//-------------------------------------------
LightsController::~LightsController()
{
}

//------------------------------------------------------------------------
void LightsController::Update(float _tick)
{
	if (m_light_object)
	{
		m_light_object->GetTransform()->setTranslation(m_game->m_scene.m_lights[m_cur_light_index].light_position);
		m_light_object->GetTransform()->billboard(m_camera.get().getDirection());
	}
	m_game->m_scene.m_lights[m_cur_light_index] = m_cur_light;
}

//--------------------------------------------------------------
void LightsController::Initialize()
{
	m_cur_light = m_game->m_scene.m_lights[0];
}

//--------------------------------------------------------------------------
void LightsController::OnObjectAddedToScene(shObject _object)
{
	if (_object->Name() == "LightObject")
	{
		m_light_object = _object;
		m_light_object->SetTransparent(true);
	}
}

//-------------------------------------------------------------
void LightsController::OnCurrentLightChanged(size_t _index)
{
	if (m_game->m_scene.m_lights.size() > _index)
	{
		m_cur_light = m_game->m_scene.m_lights[_index];
		m_cur_light_index = _index;
	}
}

//------------------------------------------------------------
Light& LightsController::GetCurrentLight()
{
	return m_cur_light;
}
