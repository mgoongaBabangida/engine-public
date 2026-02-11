#pragma once

#include "game_assets.h"

#include <base/interfaces.h>

class Camera;
class eMainContextBase;
class eModelManager;
class eTextureManager;
class eSoundManager;
class eOpenGlRenderPipeline;

//--------------------------------------------------
class DLL_GAME_ASSETS GameScript : public IScript
{
public:
  GameScript(eMainContextBase* _game,
             eModelManager* _modelManager,
             eTextureManager* _texManager,
             eSoundManager* _soundManager,
             eOpenGlRenderPipeline& _pipeline,
             Camera& _camera);
  virtual ~GameScript();

protected:
  eMainContextBase*   m_game = nullptr;
  eModelManager*      m_modelManager = nullptr;
  eTextureManager*    m_texManager = nullptr;
  eSoundManager*      m_soundManager = nullptr;

  std::reference_wrapper<eOpenGlRenderPipeline>   m_pipeline;
  std::reference_wrapper<Camera>                  m_camera;
};