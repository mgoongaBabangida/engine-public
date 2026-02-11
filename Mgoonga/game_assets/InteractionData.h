#pragma once

#include "game_assets.h"

#include <base/interfaces.h>
#include <base/Object.h>
#include <base/Event.h>
#include <math/Bezier.h>
#include <math/Geometry.h>

class eMainContextBase;
class BezierCurveUIController;

//----------------------------------------------------------
struct InteractionData
{
  shObject m_object;
  std::array<dbb::Bezier*, 2> m_beziers;
  shObject m_bezier_object;
  BezierCurveUIController* m_bezier_controller = nullptr;

  std::string m_name;
  float m_speed = 0.f; // m_debug_window->GetSliderOneCallback()(); // 0 < value < 1
  float m_animation_speed = 0.f; // m_debug_window->GetSliderTwoCallback()()
  glm::mat4 m_pretransform;
};

//----------------------------------------------------------
class DLL_GAME_ASSETS InteractionScript : public IScript
{
public:
  explicit InteractionScript(eMainContextBase* _game);
  InteractionScript(eMainContextBase* _game, const InteractionData&, dbb::OBB);
  virtual ~InteractionScript();

  virtual void Initialize() override {}
  virtual void Update(float _tick) override {}

  virtual const InteractionData& GetInteractionData() const { return m_interaction_data; }
  virtual const dbb::OBB& GetInteractionVolume() const { return m_volume; }

  base::Event<std::function<void()>>	Start;
  base::Event<std::function<void()>>	Reset;

protected:
  eMainContextBase* m_game = nullptr;
  InteractionData   m_interaction_data;
  dbb::OBB          m_volume;
};