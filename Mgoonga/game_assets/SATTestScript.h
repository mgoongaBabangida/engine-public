#pragma once
#include "game_assets.h"

#include <base/interfaces.h>
#include <base/Object.h>
#include <math/Geometry.h>
#include <math/GeometryFunctions.h>

class eMainContextBase;
class LineMesh;

//-------------------------------------------
class DLL_GAME_ASSETS SATTestScript : public IScript
{
public:
  explicit SATTestScript(eMainContextBase* _game);
  virtual ~SATTestScript();

  virtual void	Update(float _tick) override;
  virtual void  Initialize() override;

  virtual bool  OnKeyJustPressed(uint32_t _asci, KeyModifiers _modifier) override;

protected:
  eMainContextBase* m_game = nullptr;

  LineMesh* m_triangle_mesh = nullptr;
  LineMesh* m_obb_mesh = nullptr;
  LineMesh* m_asix_mesh = nullptr;

  dbb::triangle m_triangle;
  dbb::OBB m_obb;
  DebugInfo m_info;
};