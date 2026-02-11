#pragma once
#include <math/Geometry.h>
#include <game_assets/InteractionData.h>

class GUIControllerMenuForStairsScript;

//------------------------------------------------------------
class StairsScript : public InteractionScript
{
public:
  explicit StairsScript(eMainContextBase* _game);

  virtual void Initialize() override;
  virtual void Update(float _tick) override;

  void SetDebugWindow(GUIControllerMenuForStairsScript* _wnd) { m_debug_window = _wnd; }

protected:
  void _CreateBezier();

  GUIControllerMenuForStairsScript* m_debug_window = nullptr;
};