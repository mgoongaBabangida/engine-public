#pragma once
#include "GameScript.h"

#include <base/Object.h>

//-----------------------------------------------
class LightsController : public GameScript
{
public:
  LightsController(eMainContextBase* _game,
    eModelManager* _modelManager,
    eTextureManager* _texManager,
    eSoundManager* _soundManager,
    eOpenGlRenderPipeline& _pipeline,
    Camera& _camera);
  virtual ~LightsController();

  virtual void		Update(float _tick) override;
  virtual void    Initialize() override;

  void OnObjectAddedToScene(shObject);
  void OnCurrentLightChanged(size_t _index);
  Light& GetCurrentLight();

protected:
  shObject	m_light_object;
  Light			m_cur_light;
  size_t		m_cur_light_index = 0;
};