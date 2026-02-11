#pragma once
#include "game_assets.h"

#include <base/interfaces.h>
#include <base/Object.h>

#include <math/Timer.h>

class eMainContextBase;

namespace dbb
{
  class RigidBody;
  class PhysicsSystem;
  struct CollisionPair;
}
class LineMesh;

//-------------------------------------------
class DLL_GAME_ASSETS PhysicsSystemController : public IScript
{
public:
  explicit PhysicsSystemController(eMainContextBase* _game);
  ~PhysicsSystemController();

  void VisualizeOBB();

  PhysicsSystemController(const PhysicsSystemController&) = delete;
  PhysicsSystemController& operator=(const PhysicsSystemController&) = delete;

  virtual void	Update(float _tick) override;
  virtual void  Initialize() override;

  void RunPhysics() { m_simulation_on = true; }
  void StopPhysics() { m_simulation_on = false; }

protected:
  eMainContextBase*             m_game = nullptr;
  dbb::PhysicsSystem*           m_physics_system = nullptr;
  std::unique_ptr<math::Timer>  m_timer;
  bool                          m_simulation_on = false;
  LineMesh*                     m_boxes_mesh = nullptr;
};
