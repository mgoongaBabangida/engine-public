#pragma once

#include <base/Object.h>
#include <math/Bezier.h>
#include <math/Transform.h>
#include <math/Clock.h>

#include <game_assets/AnimationSocketScript.h>
#include <game_assets/InteractionData.h>

class eMainContextBase;
class BezierCurveUIController;
class GUIControllerMenuForStairsScript;
class LineMesh;
namespace dbb { class PhysicsSystem; }

//-------------------------------------------------------------------------------------
class AnimationState
{
public:
  virtual void Run() {}
  virtual void Walk() {}
  virtual void WalkBackwards() {}
  virtual void Idle() {}
  virtual void UpStairs() {}
  virtual void SittingOnChair() {}
  virtual void Taking() {}
  virtual void Jump() {}
  virtual void Throw() {}

  virtual void Update(float _tick) {}
};

//-------------------------------------------------------------------------
class CharacterScript : public AnimationSocketScript
{
public:
  explicit CharacterScript(eMainContextBase* _game);

  virtual void Initialize() override;
  virtual void Update(float _tick) override;

  virtual bool OnKeyPress(const std::vector<bool>, KeyModifiers _modifier)		override;
  virtual bool OnKeyJustPressed(uint32_t _asci, KeyModifiers _modifier) override;
  virtual bool OnKeyRelease(ASCII _key, const std::vector<bool>, KeyModifiers _modifier) override;

  void ChangeState(std::unique_ptr<AnimationState> _state) { m_state = std::move(_state); }

  void HandleVertical(float deltaTime);

  const InteractionData&  GetInterationData();
  const dbb::OBB&         GetInterationVolume();

  void Start();
  void Reset();

  eMainContextBase* GetGame() const { return m_game; }

protected:
  void _UpdateWithFirstMethod();
  void _Start();
  void _Reset();
  bool _CanCollide(shObject _obj, bool _check_for_plane = false);

  void _HandleMovement(glm::vec3 _moveDir);

  std::vector<InteractionScript*> m_interaction_scripts;

  bool          m_update_position = false;
  glm::vec3     m_move_vector; //update with 1st meth

  LineMesh*     m_velocity_mesh = nullptr;

  std::unique_ptr<AnimationState> m_state;

  Transform m_initial_transform;

  math::eClock m_clock; // for keys
  float m_cur_speed = 0.f;
  float m_base_speed = 0.007f;

  float m_walkSpeed = 5.0f;
  float m_stepHeight = 0.1f;
  float m_jumpVelocity = 7.5f;
  float m_gravity = -0.00982f;
  float m_verticalVelocity = 0.0f;

  bool m_grounded = false;
  bool m_groundSnap = true;

};

//----------------------------------------------------------
extern "C" __declspec(dllexport)
IScript * CreateScript(eMainContextBase* _context) {
  return new CharacterScript(_context);
}

//----------------------------------------------------------
extern "C" __declspec(dllexport)
const char* GetScriptName() {
  return "CharacterScript";
}

//----------------------------------------------------------
extern "C" __declspec(dllexport)
void DestroyScript(IScript * ptr){
  delete ptr;
}

//-------------------------------------------------------------------------------------
class RunState : public AnimationState
{
public:
  RunState(CharacterScript* _script) : m_script(_script) {}
  virtual void Run() { /*std::cout << "Run::already in the state" << std::endl;*/ }
  virtual void Walk() override;
  virtual void Idle() override;

protected:
  CharacterScript* m_script = nullptr;
};

//-------------------------------------------------------------------------------------
class WalkState : public AnimationState
{
public:
  WalkState(CharacterScript* _script) : m_script(_script) {}
  virtual void Run() override;
  virtual void Walk() { /*std::cout << "Walk::already in the state" << std::endl;*/ }
  virtual void Idle() override;

protected:
  CharacterScript* m_script = nullptr;
};

//-------------------------------------------------------------------------------------
class WalkBackwordsState : public AnimationState
{
public:
  WalkBackwordsState(CharacterScript* _script) : m_script(_script) {}
  virtual void Idle() override;

protected:
  CharacterScript* m_script = nullptr;
};


//-------------------------------------------------------------------------------------
class IdleState : public AnimationState
{
public:
  IdleState(CharacterScript* _script) : m_script(_script) {}
  virtual void Run() override;
  virtual void Walk() override;
  virtual void WalkBackwards() override;
  virtual void Idle() { /*std::cout << "Idle::already in the state" << std::endl;*/ }
  virtual void UpStairs() override;
  virtual void SittingOnChair() override;
  virtual void Taking() override;
  virtual void Jump() override;
  virtual void Throw()  override;

protected:
  CharacterScript* m_script = nullptr;
};

//-------------------------------------------------------------------------------------
class UpStairsState : public AnimationState
{
public:
  enum class Phase { PREPARE, GO };
  UpStairsState(CharacterScript* _script, InteractionData _data) : m_script(_script), m_data(_data) {}

  virtual void Idle() override;

  virtual void Update(float _tick) override;
protected:
  CharacterScript* m_script = nullptr;
  InteractionData m_data;
  float m_time_on_bezier = 0.f; // -> clock
  Phase m_phase = Phase::PREPARE;
};

//-------------------------------------------------------------------------------------
class SittingOnChairState : public AnimationState
{
public:
  enum class Phase { ROTATE, TRANSIT, SITTING, STANDUP };
  SittingOnChairState(CharacterScript* _script, InteractionData _data, dbb::OBB _volume) : m_script(_script), m_data(_data), m_volume(_volume) {}

  virtual void Idle() override;

  virtual void Update(float _tick) override;
protected:
  CharacterScript* m_script = nullptr;
  InteractionData   m_data;
  dbb::OBB          m_volume;
  Phase             m_phase = Phase::ROTATE;
};

//-------------------------------------------------------------------------------------
class TakingState : public AnimationState
{
public:
  TakingState(CharacterScript* _script, InteractionData _data, dbb::OBB _volume) : m_script(_script), m_data(_data), m_volume(_volume) {}

  virtual void Idle() override;

  virtual void Update(float _tick) override;
protected:
  CharacterScript* m_script = nullptr;
  InteractionData m_data;
  dbb::OBB        m_volume;
  bool m_taken = false;
};

//-------------------------------------------------------------------------------------
class ThrowingState : public AnimationState
{
public:
  ThrowingState(CharacterScript* _script) : m_script(_script){}
  virtual void Idle() override;
  virtual void Update(float _tick) override;
protected:
  CharacterScript* m_script = nullptr;
};

//-------------------------------------------------------------------------------------
class JumpingDownState : public AnimationState
{
public:
  JumpingDownState(CharacterScript* _script) : m_script(_script) {}
  virtual void Idle() override;
  virtual void Update(float _tick) override;
protected:
  CharacterScript* m_script = nullptr;
};
