// MyScript.cpp (MyScript.dll)
#include "pch.h"
#include "MyScript.h"

#include <game_assets/MainContextBase.h>
#include <game_assets/ObjectFactory.h>
#include <game_assets/BezierCurveUIController.h>
#include <game_assets/ModelManagerYAML.h>
#include <game_assets/GUIController.h>

#include <math/SkeletalAnimation.h>
#include <math/RigAnimator.h>
#include <math/GeometryFunctions.h>
#include <math/PhysicsSystem.h>

#include <opengl_assets/MyMesh.h>
#include <opengl_assets/MyModel.h>

#include <glm/glm/gtx/rotate_vector.hpp>

//---------------------------------------------------------
CharacterScript::CharacterScript(eMainContextBase* _game)
  : AnimationSocketScript(_game), m_state(std::make_unique<IdleState>(this))
{}

//---------------------------------------------------------
void CharacterScript::Initialize()
{
  AnimationSocketScript::Initialize();

  if (shObject object = m_object.lock(); object && object->GetRigger() != nullptr)
  {
    m_initial_transform.setScale(object->GetTransform()->getScaleAsVector());
    m_initial_transform.setTranslation(object->GetTransform()->getTranslation());
    m_initial_transform.setRotation(object->GetTransform()->getRotation());

    //{ // for first method, unused
    //  RigAnimator* rigger = dynamic_cast<RigAnimator*>(object->GetRigger());
    //  int last_frame = rigger->GetAnimations()[0].GetNumFrames() - 1;

    //  // get vector from hip in first frame to hip on last frame 
    //  rigger->GetMatrices("UpStairs", last_frame);
    //  glm::mat4 hip_matrixLast = rigger->GetCurrentMatrixForBone(rigger->GetNameHipsBone());
    //  rigger->GetMatrices("UpStairs", 0);
    //  glm::mat4 hip_matrix0 = rigger->GetCurrentMatrixForBone(rigger->GetNameHipsBone());
    //  m_move_vector = (glm::vec3{ hip_matrixLast[3] } - glm::vec3{ hip_matrix0[3] }); // vector from hip in first frame to hip in last frame
    //}
  }

  //@todo need to add new InteractionScript dynamicly, not just when init
  auto objects = m_game->GetObjects();
  for (auto obj : objects)
  {
    if (InteractionScript* script = dynamic_cast<InteractionScript*>(obj->GetScript()); script)
    {
      m_interaction_scripts.push_back(script);
    }
  }
}

//---------------------------------------------------------
void CharacterScript::Update(float _tick)
{
  AnimationSocketScript::Update(_tick);

 /* if (!m_velocity_mesh)
  {
    ObjectFactoryBase factory;
    m_velocity_mesh = new LineMesh({}, {}, glm::vec4{ 1.0f, 1.0f, 0.0f, 1.0f });
    m_game->AddObject(factory.CreateObject(std::make_shared<SimpleModel>(m_velocity_mesh), eObject::RenderType::LINES, "Velocity mesh"));
  }
  if (shObject object = m_object.lock(); object && object->GetRigger() != nullptr &&
    object->GetRigger()->GetCurrentAnimationName() == "UpStairs")
  {
    m_velocity_mesh->UpdateData({ object->GetTransform()->getTranslation(), object->GetTransform()->getTranslation() + glm::normalize(glm::vec3(object->GetTransform()->getRotationVector())) * 0.5f },
      { 0, 1 },
      { 1.0f, 0.0f ,0.0f, 1.0f });
  }*/
  m_state->Update(_tick);
}

//-------------------------------------------------------------------------------
bool CharacterScript::OnKeyPress(const std::vector<bool> _keys, KeyModifiers _modifier)
{
  if (m_clock.timeElapsedLastFrameMsc() < 10)
    return true;

  std::vector<ASCII> asci{ ASCII_W, ASCII_S, ASCII_D, ASCII_A };

  if (std::any_of(asci.begin(), asci.end(), [&](int index)
    { return index >= 0 && index < _keys.size() && _keys[index]; }))
  {
    float max_speed = 0.03f;

    if (!m_clock.isActive())
      m_clock.restart();

    uint64_t elapsed = m_clock.newFrame();

    if (m_cur_speed < max_speed) //@todo max speed
    {
      float acceleration = ((max_speed / 4 * 3) - m_base_speed) / (1000.f / elapsed);
      m_cur_speed = m_cur_speed + acceleration; //@todo easien function
      m_cur_speed = m_cur_speed > max_speed ? max_speed : m_cur_speed;

      if (m_cur_speed > (max_speed / 4 * 3))
        m_state->Run();
      else
      {
        if (_keys[ASCII_W])
          m_state->Walk();
        else
          m_state->WalkBackwards();
      }
    }
  }

  for (const auto a : asci)
  {
    if (_keys[a])
      OnKeyJustPressed(a, _modifier);
  }
  return false;
}

//-------------------------------------------------------------------------------
bool CharacterScript::OnKeyRelease(ASCII _key, const std::vector<bool> _keys, KeyModifiers _modifier)
{
  if (!_keys[ASCII_W] && !_keys[ASCII_S])
  {
    m_cur_speed = m_base_speed;
    m_clock.reset();
    m_state->Idle();
  }
  return false;
}

//---------------------------------------------------------
bool CharacterScript::OnKeyJustPressed(uint32_t _asci, KeyModifiers _modifier)
{
  glm::vec3 translation = glm::normalize(m_object.lock()->GetTransform()->getRotationVector()) * m_cur_speed;
  shObject object = m_object.lock();
  switch (_asci)
  {
  case ASCII_W:
  {
    object->GetTransform()->setTranslation(object->GetTransform()->getTranslation() + translation);
    auto objs = m_game->GetObjects();
    for (std::shared_ptr<eObject> other_obj : objs)
    {
      if (_CanCollide(other_obj))
      {
        // set collider from transform
        other_obj->GetCollider()->SetFrom(*other_obj->GetTransform());
        object->GetCollider()->SetFrom(*object->GetTransform());

        CollisionManifold collision = other_obj->GetCollider()->Dispatch(*object->GetCollider());
        if (collision.colliding && (collision.contacts.empty()
          || Transform::AngleDegreesBetweenVectors(glm::vec3(object->GetTransform()->getRotationVector()),
            collision.contacts[0] - object->GetTransform()->getTranslation()) < 90.f))
        {
          object->GetTransform()->setTranslation(object->GetTransform()->getTranslation() - translation);
          break;
        }
      }
    }
    return true;
  }
  case ASCII_S:
  {
    translation = -translation;
    object->GetTransform()->setTranslation(object->GetTransform()->getTranslation() + translation);
    auto objs = m_game->GetObjects();
    for (std::shared_ptr<eObject> other_obj : objs)
    {
      if (_CanCollide(other_obj))
      {
        // set collider from transform
        other_obj->GetCollider()->SetFrom(*other_obj->GetTransform());
        object->GetCollider()->SetFrom(*object->GetTransform());

        CollisionManifold collision = other_obj->GetCollider()->Dispatch(*object->GetCollider());
        if (collision.colliding && !collision.contacts.empty()
          && Transform::AngleDegreesBetweenVectors(glm::vec3(object->GetTransform()->getRotationVector()),
            collision.contacts[0] - object->GetTransform()->getTranslation()) > 90.f)
        {
          object->GetTransform()->setTranslation(object->GetTransform()->getTranslation() - translation);
          break;
        }
      }
    }
    return true;
  }
  case ASCII_D:
  {
    auto rot = glm::toQuat(glm::rotate(UNIT_MATRIX, glm::radians(-1.0f / 1.f), glm::vec3(0.0f, 1.0f, 0.0f)));
    m_object.lock()->GetTransform()->setRotation(rot * m_object.lock()->GetTransform()->getRotation());
    return true;
  }
  case ASCII_A:
  {
    auto rot = glm::toQuat(glm::rotate(UNIT_MATRIX, glm::radians(1.0f / 1.f), glm::vec3(0.0f, 1.0f, 0.0f)));
    m_object.lock()->GetTransform()->setRotation(rot * m_object.lock()->GetTransform()->getRotation());
    return true;
  }
  case ASCII_E:
  {
    for (auto interaction_script : m_interaction_scripts)
    {
      if (dbb::IsPointInOBB(m_object.lock()->GetTransform()->getTranslation(),
        interaction_script->GetInteractionVolume()))
      {
        if (interaction_script->GetInteractionData().m_name == "upstairs")
          m_state->UpStairs();
        else if (interaction_script->GetInteractionData().m_name == "chair")
          m_state->SittingOnChair();
        else if (interaction_script->GetInteractionData().m_name == "taking")
          m_state->Taking();
        else if (interaction_script->GetInteractionData().m_name == "animated")
        {
          IRigger* rigger = interaction_script->GetInteractionData().m_object->GetRigger();
          rigger->Apply(rigger->GetCurrentAnimationName(), true);
          rigger->GetCurrentAnimation()->FreezeFrame(-1);
        }
      }
    }
    return true;
  }
  case ASCII_J:
  {
    m_state->Jump();
    break;
  }
  case ASCII_T:
  {
    m_state->Throw();
    break;
  }
  case ASCII_K:
  {
    //_Reset();
    auto& sockets = object->GetRigger()->GetSockets();
    auto socket = std::find_if(sockets.begin(), sockets.end(), [](const AnimationSocket& _socket) { return _socket.m_socket_object->Name() == "Jar"; });
    if (socket != sockets.end())
    {
      // glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(5.0f), glm::vec3(1.0f, 0.0f, 0.0f));
      glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(-0.05f, 0, 0));
      socket->m_pre_transform = socket->m_pre_transform * translation;
    }
    break;
  }
  case ASCII_U:
  {
    //_Reset();
    auto& sockets = object->GetRigger()->GetSockets();
    auto socket = std::find_if(sockets.begin(), sockets.end(), [](const AnimationSocket& _socket) { return _socket.m_socket_object->Name() == "Jar"; });
    if (socket != sockets.end())
    {
      // glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(5.0f), glm::vec3(1.0f, 0.0f, 0.0f));
      glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(0.05f, 0, 0));
      socket->m_pre_transform = socket->m_pre_transform * translation;
    }
    break;
  }
  case ASCII_L:
  {
    /* m_interaction_data.m_bezier_controller->FlipInputStrategy();*/

    auto& sockets = object->GetRigger()->GetSockets();
    auto socket = std::find_if(sockets.begin(), sockets.end(), [](const AnimationSocket& _socket) { return _socket.m_socket_object->Name() == "Jar"; });
    if (socket != sockets.end())
    {
      //glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(5.0f), glm::vec3(0.0f, 1.0f, 0.0f));
      glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, -0.05f, 0));
      socket->m_pre_transform = socket->m_pre_transform * translation;
    }
    break;
  }
  case ASCII_Z:
  {
    /* m_interaction_data.m_bezier_controller->FlipInputStrategy();*/

    auto& sockets = object->GetRigger()->GetSockets();
    auto socket = std::find_if(sockets.begin(), sockets.end(), [](const AnimationSocket& _socket) { return _socket.m_socket_object->Name() == "Jar"; });
    if (socket != sockets.end())
    {
      //glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(5.0f), glm::vec3(0.0f, 1.0f, 0.0f));
      glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.05f, 0));
      socket->m_pre_transform = socket->m_pre_transform * translation;
    }
    break;
  }
  case ASCII_X:
  {
    //_Start();

    auto& sockets = object->GetRigger()->GetSockets();
    auto socket = std::find_if(sockets.begin(), sockets.end(), [](const AnimationSocket& _socket) { return _socket.m_socket_object->Name() == "Jar"; });
    if (socket != sockets.end())
    {
      //glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(5.0f), glm::vec3(0.0f, 0.0f, 1.0f));
      glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.0f, -0.05f));
      socket->m_pre_transform = socket->m_pre_transform * translation;
    }

    break;
  }
  default: return false;
  }
  return true;
}

//-------------------------------------------------------
void CharacterScript::_HandleMovement(glm::vec3 _desiredMove)
{
  if (!m_object.lock()->GetRigidBody() || !m_object.lock()->GetCollider()) return;

 shObject object = m_object.lock();
  /*glm::vec3 right = glm::normalize(glm::cross(object->GetTransform()->getForward(), glm::vec3(0, 1, 0)));
  glm::vec3 moveDir = m_input.y * object->GetTransform()->getForward() + m_input.x * right;
  if (glm::length2(moveDir) < 0.0001f)
    return;

  moveDir = glm::normalize(moveDir);
  glm::vec3 desiredMove = moveDir * m_walkSpeed * deltaTime;*/

  // Simple sweep: check if desired move collides
  auto body = m_object.lock()->GetRigidBody();
  glm::vec3 currentPos = object->GetTransform()->getTranslation();
  glm::vec3 newPos = currentPos + _desiredMove;

  CollisionManifold hit;
  if (hit.colliding)
  {
    glm::vec3 normal = hit.normal;
    glm::vec3 slideDir = _desiredMove - glm::dot(_desiredMove, normal) * normal;
    newPos = currentPos + slideDir;
  }
  object->GetTransform()->setTranslation(newPos);
}

//--------------------------------------------------------------------
void CharacterScript::HandleVertical(float deltaTime)
{
  if (!m_object.lock()->GetRigidBody()) return;

  glm::vec3 pos = m_object.lock()->GetTransform()->getTranslation();
  glm::vec3 rayOrigin = pos + glm::vec3(0, 0.05f, 0);
  glm::vec3 rayDir = glm::vec3(0, -1, 0);
  float maxDist = m_stepHeight + 0.05f; // @todo use max dist in raycast

  // Check for ground using ray
  dbb::ray groundRay(rayOrigin, rayDir);
  RaycastResult result;
  m_grounded = false;

  auto objs = m_game->GetObjects();
  for (std::shared_ptr<eObject> other_obj : objs)
  {
    if (_CanCollide(other_obj), true) //ground should be checked too
    {
      if (!other_obj->GetCollider() || other_obj== m_object.lock())
        continue;
      if (other_obj->GetCollider()->RayCast(groundRay, result) > 0) // falling at this object
      {
        std::cout << "Raycast " << other_obj->Name() << result.point.y << std::endl;
        if ((result.hit && result.t == result.tmin && result.tmin >= 0.0f && result.tmin < maxDist) // got there , Ray entered from outside at tmin
          || (result.point.y == 0))
        {
          m_grounded = true;
          if (m_groundSnap /*&& result.t < m_stepHeight*/)
          {
            pos.y = result.point.y;
            std::cout << "grounded " << other_obj->Name() << "result.tmin " << result.tmin <<" "<< pos.y << std::endl;
            m_verticalVelocity = 0.0f;
            m_object.lock()->GetTransform()->setTranslation(pos);
          }
          break;
        }
      }
    }
  }

  if (!m_grounded)
  {
    m_verticalVelocity += m_gravity * (deltaTime/1'000.f);
    pos.y += m_verticalVelocity * deltaTime;
    m_object.lock()->GetTransform()->setTranslation(pos);
  }
}

//-------------------------------------------------------
const InteractionData& CharacterScript::GetInterationData()
{
  for (auto interaction_script : m_interaction_scripts)
  {
    if (dbb::IsPointInOBB(m_object.lock()->GetTransform()->getTranslation(),
      interaction_script->GetInteractionVolume()))
      return interaction_script->GetInteractionData();
  }
  return m_interaction_scripts[0]->GetInteractionData(); // @todo exception or ?
}

//-------------------------------------------------------
const dbb::OBB& CharacterScript::GetInterationVolume()
{
  for (auto interaction_script : m_interaction_scripts)
  {
    if (dbb::IsPointInOBB(m_object.lock()->GetTransform()->getTranslation(),
      interaction_script->GetInteractionVolume()))
      return interaction_script->GetInteractionVolume();
  }
  return m_interaction_scripts[0]->GetInteractionVolume(); // @todo exception or ?
}

//--------------------- Debug -----------------------------

//---------------------------------------------------------
void CharacterScript::Start()
{
  //first method
 /* object->GetRigger()->Apply(0, true);
 object->GetRigger()->GetCurrentAnimation()->FreezeFrame(-1);*/

  m_state->UpStairs();
}

//---------------------------------------------------------
void CharacterScript::Reset()
{
  if (shObject object = m_object.lock(); object && object->GetRigger() != nullptr)
  {
    object->GetTransform()->setTranslation(m_initial_transform.getTranslation());
    object->GetTransform()->setScale(m_initial_transform.getScaleAsVector());
    object->GetTransform()->setRotation(m_initial_transform.getRotation());
    /* m_time_on_bezier = 0.f;*/
  }
  m_state->Idle();
}

//---------------------------------------------------------
void CharacterScript::_Start()
{
  if (shObject object = m_object.lock(); object && object->GetRigger() != nullptr)
  {
    //first method
   /* object->GetRigger()->Apply(0, true);
    object->GetRigger()->GetCurrentAnimation()->FreezeFrame(-1);*/

    //second method
    RigAnimator* rigger = dynamic_cast<RigAnimator*>(object->GetRigger());
    if (rigger)
    {
      object->GetRigger()->Apply("UpStairs", false);
      rigger->GetCurrentAnimation()->GetFixHipsY() = true;
      rigger->GetCurrentAnimation()->GetFixHipsZ() = true;
    }
  }
}

//---------------------------------------------------------
void CharacterScript::_Reset()
{
  if (shObject object = m_object.lock(); object && object->GetRigger() != nullptr)
  {
    object->GetTransform()->setTranslation(m_initial_transform.getTranslation());
    object->GetTransform()->setScale(m_initial_transform.getScaleAsVector());
    object->GetTransform()->setRotation(m_initial_transform.getRotation());
    /* m_time_on_bezier = 0.f;*/
  }
}

//-------------------------------------------------------
bool CharacterScript::_CanCollide(shObject _obj, bool _should_collider_with_plane)
{
  if(!_should_collider_with_plane && _obj->Name() == "GrassPlane")
    return false;

  if (_obj->GetRigidBody()->GetCollider()
      && m_object.lock()->GetCollider()
      && _obj != m_object.lock()
      && _obj->IsVisible())
    return true;
  else
    return false;
}

// ---------------------STATES----------------------------------------------

//--------------------------------------------------------------------------
void IdleState::Run()
{
  shObject object = m_script->GetScriptObject().lock();
  if (object.get() != nullptr)
  {
    RigAnimator* rigger = dynamic_cast<RigAnimator*>(object->GetRigger());
    if (rigger)
    {
      object->GetRigger()->Apply("Running", false);
      rigger->GetCurrentAnimation()->GetFixHipsY() = true;
      rigger->GetCurrentAnimation()->GetFixHipsZ() = true;
      m_script->ChangeState(std::make_unique<RunState>(m_script));

      std::cout << "From IdleState to RunState" << std::endl;
    }
  }
}

//-------------------------------------------------------------
void IdleState::Walk()
{
  shObject object = m_script->GetScriptObject().lock();
  if (object.get() != nullptr)
  {
    RigAnimator* rigger = dynamic_cast<RigAnimator*>(object->GetRigger());
    if (rigger)
    {
      object->GetRigger()->Apply("Walking", false);
      rigger->GetCurrentAnimation()->GetFixHipsY() = true;
      rigger->GetCurrentAnimation()->GetFixHipsZ() = true;
      m_script->ChangeState(std::make_unique<WalkState>(m_script));

      std::cout << "From IdleState to WalkState" << std::endl;
    }
  }
}

//--------------------------------------------------------------------
void IdleState::WalkBackwards()
{
  shObject object = m_script->GetScriptObject().lock();
  if (object.get() != nullptr)
  {
    RigAnimator* rigger = dynamic_cast<RigAnimator*>(object->GetRigger());
    if (rigger)
    {
      object->GetRigger()->Apply("WalkingBackwards", false);
      rigger->GetCurrentAnimation()->GetFixHipsY() = true;
      rigger->GetCurrentAnimation()->GetFixHipsZ() = true;
      m_script->ChangeState(std::make_unique<WalkBackwordsState>(m_script));

      std::cout << "From IdleState to WalkBackwords State" << std::endl;
    }
  }
}

//-------------------------------------------------------------------
void IdleState::UpStairs()
{
  m_script->ChangeState(std::make_unique<UpStairsState>(m_script, m_script->GetInterationData()));
  std::cout << "From IdleState to UpStairsState" << std::endl;
}

//--------------------------------------------------
void IdleState::SittingOnChair()
{
  m_script->ChangeState(std::make_unique<SittingOnChairState>(m_script, m_script->GetInterationData(), m_script->GetInterationVolume()));
  std::cout << "From IdleState to SittingOnChairState" << std::endl;
}

//------------------------------------------------
void IdleState::Taking()
{
  shObject object = m_script->GetScriptObject().lock();
  if (object.get() != nullptr)
  {
    RigAnimator* rigger = dynamic_cast<RigAnimator*>(object->GetRigger());
    if (rigger)
    {
      object->GetRigger()->Apply("TakingItem", true);
      rigger->GetCurrentAnimation()->GetFixHipsY() = true;
      rigger->GetCurrentAnimation()->GetFixHipsZ() = true;
      rigger->GetCurrentAnimation()->FreezeFrame(-1);
      m_script->ChangeState(std::make_unique<TakingState>(m_script, m_script->GetInterationData(), m_script->GetInterationVolume()));

      std::cout << "From IdleState to Taking" << std::endl;
    }
  }
}

//--------------------------------------------------
void IdleState::Jump()
{
  shObject object = m_script->GetScriptObject().lock();
  if (object.get() != nullptr)
  {
    RigAnimator* rigger = dynamic_cast<RigAnimator*>(object->GetRigger());
    if (rigger)
    {
      object->GetRigger()->Apply("JumpingDown", true);
     /* rigger->GetCurrentAnimation()->GetFixHipsY() = true;*/
      rigger->GetCurrentAnimation()->GetFixHipsZ() = true;
      m_script->ChangeState(std::make_unique<JumpingDownState>(m_script));

      std::cout << "From IdleState to JumpingDownState" << std::endl;
    }
  }
}

//--------------------------------------------------
void IdleState::Throw()
{
  shObject object = m_script->GetScriptObject().lock();
  if (object.get() != nullptr)
  {
    RigAnimator* rigger = dynamic_cast<RigAnimator*>(object->GetRigger());
    if (rigger && !object->GetRigger()->GetSockets().empty())
    {
      object->GetRigger()->Apply("Throw", true);
     /* rigger->GetCurrentAnimation()->GetFixHipsY() = true;
      rigger->GetCurrentAnimation()->GetFixHipsZ() = true;*/
      m_script->ChangeState(std::make_unique<ThrowingState>(m_script));

      std::cout << "From RunState to ThrowingState" << std::endl;
    }
  }
}

//--------------------------------------------------
void RunState::Walk()
{
  shObject object = m_script->GetScriptObject().lock();
  if (object.get() != nullptr)
  {
    RigAnimator* rigger = dynamic_cast<RigAnimator*>(object->GetRigger());
    if (rigger)
    {
      object->GetRigger()->Apply("Walking", false);
      rigger->GetCurrentAnimation()->GetFixHipsY() = true;
      rigger->GetCurrentAnimation()->GetFixHipsZ() = true;
      m_script->ChangeState(std::make_unique<WalkState>(m_script));

      std::cout << "From RunState to WalkState" << std::endl;
    }
  }
}

//----------------------------------------------------------
void RunState::Idle()
{
  shObject object = m_script->GetScriptObject().lock();
  if (object.get() != nullptr)
  {
    RigAnimator* rigger = dynamic_cast<RigAnimator*>(object->GetRigger());
    if (rigger)
    {
      if (rigger->GetCurrentAnimationName() != "IdleBreath")
      {
        object->GetRigger()->Apply("IdleBreath", false);
        rigger->GetCurrentAnimation()->GetFixHipsY() = true;
        rigger->GetCurrentAnimation()->GetFixHipsZ() = true;
        m_script->ChangeState(std::make_unique<IdleState>(m_script));

        std::cout << "From RunState to IdleState" << std::endl;
      }
    }
  }
}

//--------------------------------------------------------
void WalkState::Run()
{
  shObject object = m_script->GetScriptObject().lock();
  if (object.get() != nullptr)
  {
    RigAnimator* rigger = dynamic_cast<RigAnimator*>(object->GetRigger());
    if (rigger)
    {
      object->GetRigger()->Apply("Running", false);
      rigger->GetCurrentAnimation()->GetFixHipsY() = true;
      rigger->GetCurrentAnimation()->GetFixHipsZ() = true;
      m_script->ChangeState(std::make_unique<RunState>(m_script));

      std::cout << "From WalkState to RunState" << std::endl;
    }
  }
}

//----------------------------------------------------------
void WalkState::Idle()
{
  shObject object = m_script->GetScriptObject().lock();
  if (object.get() != nullptr)
  {
    RigAnimator* rigger = dynamic_cast<RigAnimator*>(object->GetRigger());
    if (rigger)
    {
      object->GetRigger()->Apply("IdleBreath", false);
      rigger->GetCurrentAnimation()->GetFixHipsY() = true;
      rigger->GetCurrentAnimation()->GetFixHipsZ() = true;
      m_script->ChangeState(std::make_unique<IdleState>(m_script));

      std::cout << "From WalkState to IdleState" << std::endl;
    }
  }
}

//---------------------------------------------------------
void UpStairsState::Idle()
{
  shObject object = m_script->GetScriptObject().lock();
  if (object.get() != nullptr)
  {
    RigAnimator* rigger = dynamic_cast<RigAnimator*>(object->GetRigger());
    if (rigger)
    {
      object->GetRigger()->Apply("IdleBreath", false);
      rigger->GetCurrentAnimation()->GetFixHipsY() = true;
      rigger->GetCurrentAnimation()->GetFixHipsZ() = true;
      m_script->ChangeState(std::make_unique<IdleState>(m_script));

      std::cout << "From UpStairsState to IdleState" << std::endl;
    }
  }
}

//---------------------------------------------------------
void UpStairsState::Update(float _tick)
{
  if (m_phase == Phase::PREPARE)
  {
    shObject object = m_script->GetScriptObject().lock();
    if (object.get() != nullptr)
    {
      RigAnimator* rigger = dynamic_cast<RigAnimator*>(object->GetRigger());
      if (rigger)
      {
        glm::vec3 move_vec = m_data.m_beziers[0]->p0 - object->GetTransform()->getTranslation();
        if (glm::length(move_vec) > 0.01f)
        {
          object->GetRigger()->Apply("Walking", false);
          object->GetTransform()->getTranslationRef() += glm::normalize(move_vec) * 0.01f;
          glm::vec3 new_dir = GetVelocity(*m_data.m_beziers[0], 0);
          glm::vec3 right = glm::cross(glm::normalize(new_dir), object->GetTransform()->getUp());
          new_dir = glm::cross(object->GetTransform()->getUp(), right);
          object->GetTransform()->turnTo(object->GetTransform()->getTranslation() + glm::normalize(new_dir) * 0.1f, 1.f);
        }
        else
        {
          object->GetRigger()->Apply("UpStairs", false);
          rigger->GetCurrentAnimation()->GetFixHipsY() = true;
          rigger->GetCurrentAnimation()->GetFixHipsZ() = true;
          m_phase = Phase::GO;
        }
      }
    }
  }
  else if (m_phase == Phase::GO)
  {
    m_time_on_bezier += glm::mix(0.0003f, 0.003f, m_data.m_speed*3);

    if (shObject object = m_script->GetScriptObject().lock();
      object && object->GetRigger() != nullptr &&
      object->GetRigger()->GetCurrentAnimationName() == "UpStairs")
    {
      RigAnimator* rigger = dynamic_cast<RigAnimator*>(object->GetRigger());
      rigger->GetCurrentAnimation()->GetSpeed() = m_data.m_animation_speed * 1.5f;

      if (m_time_on_bezier < 2.0f)
      {
        glm::vec3 new_place; glm::vec3 new_dir;
        if (m_time_on_bezier < 1.0f) // on first part of bezier
        {
          new_place = GetPoint(*m_data.m_beziers[0], m_time_on_bezier);
          new_dir = GetVelocity(*m_data.m_beziers[0], m_time_on_bezier);
        }
        else // on second part of bezier
        {
          new_place = GetPoint(*m_data.m_beziers[1], m_time_on_bezier - 1.f);
          new_dir = GetVelocity(*m_data.m_beziers[1], m_time_on_bezier - 1.f);
        }

        object->GetTransform()->setTranslation(new_place);
        glm::vec3 right = glm::cross(glm::normalize(new_dir), object->GetTransform()->getUp());
        new_dir = glm::cross(object->GetTransform()->getUp(), right);
        object->GetTransform()->turnTo(object->GetTransform()->getTranslation() + glm::normalize(new_dir) * 0.1f);
      }
    }
  }
}

//----------------------------------------------------------
void SittingOnChairState::Idle()
{
  if (m_phase != Phase::SITTING && m_phase != Phase::ROTATE)
    return;

  //temp
  shObject obj = m_script->GetScriptObject().lock();
  if (obj.get() != nullptr)
  {
    //create transit animation
    RigAnimator* rigger = dynamic_cast<RigAnimator*>(obj->GetRigger());
    rigger->DeleteAnimation("Sit");

    if (rigger->GetCurrentAnimationName() != "IdleBreath")
    {
      std::shared_ptr<SkeletalAnimation> transit = std::make_shared<SkeletalAnimation>(SkeletalAnimation::CreateByInterpolation(
        rigger->GetCurrentAnimation()->GetCurrentFrame(),
        rigger->GetAnimations()["IdleBreath"]->GetFrameByNumber(0),
        500, // duration
        20, // num frames
        "Standup"));
      rigger->AddAnimation(transit);
      m_phase = Phase::STANDUP;
    }
    else
    {
      m_script->ChangeState(std::make_unique<IdleState>(m_script));
      std::cout << "From SittingOnChairState to IdleState" << std::endl;
    }
  }
}

//---------------------------------------------------------
void SittingOnChairState::Update(float _tick)
{
  shObject obj = m_script->GetScriptObject().lock();
  RigAnimator* rigger = dynamic_cast<RigAnimator*>(obj->GetRigger());
  if (m_phase == Phase::ROTATE) // rotate to volume origin
  {
    //if? // this is for the character not to try sitting from behind the chair
    glm::vec3 relative_pos = glm::normalize(m_volume.origin - obj->GetTransform()->getTranslation());
    if (float res = Transform::AngleDegreesBetweenVectors(glm::vec3(0, 0, -1), relative_pos); res < 45.f)
    {
      Idle();
      return;
    }

    glm::vec3 new_direction = glm::normalize(obj->GetTransform()->getTranslation() - m_volume.origin);
    glm::vec3 focus_point = obj->GetTransform()->getTranslation() + new_direction;
    glm::vec3 cur_direction = glm::normalize(obj->GetTransform()->getRotationVector());

    if (Transform::AngleDegreesBetweenVectors(glm::normalize(new_direction), glm::normalize(cur_direction)) > 0.1f)
    {
      obj->GetTransform()->turnTo(focus_point, 3.f);
    }
    else
    {
      //create transit animation
      std::shared_ptr<SkeletalAnimation> transit = std::make_shared<SkeletalAnimation>(SkeletalAnimation::CreateByInterpolation(
        rigger->GetCurrentAnimation()->GetCurrentFrame(),
        rigger->GetAnimations()["Sitting"]->GetFrameByNumber(0),
        1000, // duration
        20, // num frames
        "Sit"));
      rigger->AddAnimation(transit);
      obj->GetTransform()->setTranslation(m_volume.origin + new_direction * 0.3f);
      m_phase = Phase::TRANSIT;
    }
  }
  else if (m_phase == Phase::TRANSIT) // play transit animation
  {
    if (rigger->GetCurrentAnimationName() != "Sit")
      obj->GetRigger()->Apply("Sit", true);
    else if (obj->GetRigger()->GetCurrentAnimationFrameIndex() == 19) // transit num frames -1
      m_phase = Phase::SITTING;
  }
  else if (m_phase == Phase::SITTING) // play sit animation
  {
    if (rigger->GetCurrentAnimationName() != "Sitting")
      obj->GetRigger()->Apply("Sitting", false);
  }
  else if (m_phase == Phase::STANDUP)
  {
    if (rigger->GetCurrentAnimationName() != "Standup")
      obj->GetRigger()->Apply("Standup", true);
    else if (obj->GetRigger()->GetCurrentAnimationFrameIndex() == 19) // transit num frames -1
    {
      rigger->DeleteAnimation("Standup");
      obj->GetRigger()->Apply("IdleBreath", false);
      rigger->GetCurrentAnimation()->GetFixHipsY() = true;
      rigger->GetCurrentAnimation()->GetFixHipsZ() = true;
      m_script->ChangeState(std::make_unique<IdleState>(m_script));

      std::cout << "From SittingOnChairState to IdleState" << std::endl;
    }
  }
}

//----------------------------------------------------------
void TakingState::Idle()
{
  shObject object = m_script->GetScriptObject().lock();
  if (object.get() != nullptr)
  {
    RigAnimator* rigger = dynamic_cast<RigAnimator*>(object->GetRigger());
    if (rigger)
    {
      if (rigger->GetCurrentAnimationName() != "IdleBreath")
      {
        object->GetRigger()->Apply("IdleBreath", false);
        rigger->GetCurrentAnimation()->GetFixHipsY() = true;
        rigger->GetCurrentAnimation()->GetFixHipsZ() = true;
        rigger->GetArmOffset() = 10.f;
        m_script->ChangeState(std::make_unique<IdleState>(m_script));

        std::cout << "From TakingState to IdleState" << std::endl;
      }
    }
  }
}

//-----------------------------------------------------------
void TakingState::Update(float _tick)
{
  shObject obj = m_script->GetScriptObject().lock();
  RigAnimator* rigger = dynamic_cast<RigAnimator*>(obj->GetRigger());

  glm::vec3 new_direction = glm::normalize(m_volume.origin - obj->GetTransform()->getTranslation());
  new_direction = glm::rotateY(new_direction, glm::radians(45.0f)); //@ animation dep
  glm::vec3 focus_point = obj->GetTransform()->getTranslation() + new_direction;
  glm::vec3 cur_direction = glm::normalize(obj->GetTransform()->getRotationVector());

  if (Transform::AngleDegreesBetweenVectors(glm::normalize(new_direction), cur_direction) > 0.1f)
    obj->GetTransform()->turnTo(focus_point, 3.f);
  if (rigger->GetArmOffset() < 40.f)
    rigger->GetArmOffset() += 1.0f;

  if (!m_taken && rigger->GetCurrentAnimationFrameIndex() >= 45 && m_data.m_object)
  {
    glm::mat4 pretransform = glm::inverse(obj->GetTransform()->getScale()) * m_data.m_pretransform;
    obj->GetRigger()->CreateSocket(m_data.m_object, "mixamorig_RightHandIndex1", pretransform, true);
    m_taken = true;
  }
}

//----------------------------------------------------------------
void WalkBackwordsState::Idle()
{
  shObject object = m_script->GetScriptObject().lock();
  if (object.get() != nullptr)
  {
    RigAnimator* rigger = dynamic_cast<RigAnimator*>(object->GetRigger());
    if (rigger)
    {
      object->GetRigger()->Apply("IdleBreath", false);
      rigger->GetCurrentAnimation()->GetFixHipsY() = true;
      rigger->GetCurrentAnimation()->GetFixHipsZ() = true;
      m_script->ChangeState(std::make_unique<IdleState>(m_script));

      std::cout << "From WalkBackwords State to IdleState" << std::endl;
    }
  }
}

//---------------------------------------------------------------
void ThrowingState::Idle()
{
  shObject object = m_script->GetScriptObject().lock();
  if (object.get() != nullptr)
  {
    RigAnimator* rigger = dynamic_cast<RigAnimator*>(object->GetRigger());
    if (rigger)
    {
      object->GetRigger()->Apply("IdleBreath", false);
      rigger->GetCurrentAnimation()->GetFixHipsY() = true;
      rigger->GetCurrentAnimation()->GetFixHipsZ() = true;
      m_script->ChangeState(std::make_unique<IdleState>(m_script));

      std::cout << "From ThrowingState State to IdleState" << std::endl;
    }
  }
}

//-----------------------------------------------------------------
void ThrowingState::Update(float _tick)
{
  shObject object = m_script->GetScriptObject().lock();
  RigAnimator* rigger = dynamic_cast<RigAnimator*>(object->GetRigger());
  if (rigger)
  {
    if (rigger->GetCurrentAnimationFrameIndex() >= 25 && !rigger->GetSockets().empty())
    {
      std::vector<AnimationSocket>& sockets = rigger->GetSockets();
      eObject* child = sockets[0].m_socket_object;

      m_script->GetGame()->AddObject(object->GetChildrenObjects().front());
      object->GetChildrenObjects().clear();
      sockets.clear();

      child->GetCollider()->SetFrom(*child->GetTransform());
      child->GetRigidBody()->SetCollider(child->GetCollider());
      m_script->GetGame()->GetPhysicsSystem()->AddRigidbody(std::dynamic_pointer_cast<dbb::RigidBody>(child->GetRigidBody()));//@todo
      child->GetRigidBody()->AddLinearImpulse(object->GetTransform()->getRotationVector() * 25.f);
    }
  }
}

//-----------------------------------------------------------------
void JumpingDownState::Idle()
{
  shObject object = m_script->GetScriptObject().lock();
  if (object.get() != nullptr)
  {
    RigAnimator* rigger = dynamic_cast<RigAnimator*>(object->GetRigger());
    if (rigger)
    {
      object->GetRigger()->Apply("IdleBreath", false);
      rigger->GetCurrentAnimation()->GetFixHipsY() = true;
      rigger->GetCurrentAnimation()->GetFixHipsZ() = true;
      m_script->ChangeState(std::make_unique<IdleState>(m_script));

      std::cout << "From JumpingDownState State to IdleState" << std::endl;
    }
  }
}

//-----------------------------------------------------------------
void JumpingDownState::Update(float _tick)
{
  if(m_script)
    m_script->HandleVertical(_tick);
}

//-------------------------------------------------------
// Debug
//---------------------------------------------------------
void CharacterScript::_UpdateWithFirstMethod()
{
  if (shObject object = m_object.lock(); object && object->GetRigger() != nullptr &&
    object->GetRigger()->GetCurrentAnimationName() == "UpStairs")
  {
    if (object->GetRigger()->GetCurrentAnimation() &&
      object->GetRigger()->GetCurrentAnimationFrameIndex() ==
      object->GetRigger()->GetCurrentAnimation()->GetNumFrames() - 1) // last frame of the animation is now
    {
      object->GetRigger()->Apply("UpStairs", true);
      m_update_position = true;
    }
    else if (object->GetRigger()->GetCurrentAnimation() && m_update_position) //first frame -> time to update position
    {
      glm::vec3 move_vector = glm::toMat4(object->GetTransform()->getRotation()) * glm::vec4(m_move_vector, 1.0f);
      glm::vec3 new_place = object->GetTransform()->getTranslation() + (move_vector * object->GetTransform()->getScaleAsVector().x);
      object->GetTransform()->setTranslation(new_place);
      m_update_position = false;
    }
    // update rotation
    static glm::quat starting_rotation = object->GetTransform()->getRotation();
    float height = object->GetTransform()->getTranslation().y - (-2.f); //-2 is starting height
    float rotation = height / 5.5 * 360.f; // 5.5 is total height, 360 is total rotation
    auto rot = glm::toQuat(glm::rotate(UNIT_MATRIX, glm::radians(rotation), glm::vec3(0.0f, 1.0f, 0.0f)));
    object->GetTransform()->setRotation(rot * starting_rotation);

    if (float height = object->GetTransform()->getTranslation().y > 3.5f) // object is on top , stop the animation
    {
      object->GetRigger()->Apply("NULL", false);
    }
  }
}
