#include "stdafx.h"
#include "GameScript.h"

#include <base/Object.h>
#include <math/Camera.h>
#include <opengl_assets/openglrenderpipeline.h>
#include <opengl_assets/ModelManager.h>
#include <opengl_assets/TextureManager.h>
#include <opengl_assets/SoundManager.h>

//#include "MainContextBase.h"

//------------------------------------------------
GameScript::GameScript(eMainContextBase* _game,
                       eModelManager* _modelManager,
                       eTextureManager* _texManager,
                       eSoundManager* _soundManager,
                       eOpenGlRenderPipeline& _pipeline,
                       Camera& _camera)
  : m_game(_game)
  , m_modelManager(_modelManager)
  , m_texManager(_texManager)
  , m_soundManager(_soundManager)
  , m_pipeline(_pipeline)
  , m_camera(_camera)
{
}

//------------------------------------------------
GameScript::~GameScript()
{

}