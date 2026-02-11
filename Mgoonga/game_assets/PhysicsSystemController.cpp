#include "stdafx.h"

#include "PhysicsSystemController.h"
#include "MainContextBase.h"

#include <math/PhysicsSystem.h>
#include <math/Geometry.h>
#include <math/Colliders.h>
#include <opengl_assets/MyMesh.h>
#include <game_assets/AnimationManagerYAML.h>
#include <game_assets/ModelManagerYAML.h>
#include <game_assets/ObjectFactory.h>

//------------------------------------------------------------------------
PhysicsSystemController::PhysicsSystemController(eMainContextBase* _game)
  : m_game(_game)
  , m_physics_system(m_game->GetPhysicsSystem())
{
}

//------------------------------------------------------------------------
void PhysicsSystemController::Update(float _tick)
{
  if (m_simulation_on)
  {
    m_physics_system->Update(_tick); //Syncronous update to send events in main thread

    std::vector<shObject> objects = m_game->GetObjects();
    for (std::shared_ptr<dbb::RigidBody> body : m_physics_system->GetRigidBodies()) // Set Transform position and orientation from colliders
    {
      auto it = std::find_if(objects.begin(), objects.end(), [body](const shObject& _obj) { return _obj->GetRigidBody() == body; });
      if (it != objects.end())
      {
        body->GetCollider()->SetScale((*it)->GetTransform()->getScaleAsVector().x);
        body->GetCollider()->SetTo(*((*it)->GetTransform()));
      }
    }
    VisualizeOBB();
  }
  else
  {
    std::vector<shObject> objects = m_game->GetObjects();
    for (std::shared_ptr<dbb::RigidBody> body : m_physics_system->GetRigidBodies()) // Set colliders from tramsform
    {
      auto it = std::find_if(objects.begin(), objects.end(), [body](const shObject& _obj) { return _obj->GetRigidBody() == body; });
      if (it != objects.end())
      {
        body->GetCollider()->SetFrom(*((*it)->GetTransform()));
        it->get()->GetRigidBody()->SetCollider(it->get()->GetCollider());
      }
    }
  }
}

//----------------------------------------------------
void PhysicsSystemController::Initialize()
{
  m_timer.reset(new math::Timer([this]()->bool
    {
      if (m_simulation_on)
      {
        m_physics_system->UpdateAsync(17); // @should be real number directly from timer
      }
      return true;
    }));
  m_timer->start(17); //~60 fps
}

//----------------------------------------------------
PhysicsSystemController::~PhysicsSystemController()
{
  m_timer->stop();
}

//----------------------------------------------------
void PhysicsSystemController::VisualizeOBB()
{
  //Visualize OBBs
  static bool first_call = true;
  ObjectFactoryBase factory(m_game->GetAnimationManager());
  std::vector<std::shared_ptr<dbb::RigidBody>> bodies = m_physics_system->GetRigidBodies();
  if(m_boxes_mesh == nullptr)
    m_boxes_mesh = new LineMesh({}, {}, {});
  std::vector<glm::vec3> extrems_total;
  std::vector<unsigned int> indices_total;
  size_t i = 0;
  for (auto body : bodies)
  {
    if(dbb::OBBCollider* collider = dynamic_cast<dbb::OBBCollider*>(body->GetCollider()); collider)
    {
      dbb::OBB obb = collider->GetBox();
      if (obb.IsValid())
      {
        std::vector<dbb::point> extrems = obb.GetVertices();
        extrems_total.insert(extrems_total.end(), extrems.begin(), extrems.end());
        static const std::array<unsigned int, 24> boxEdges = obb.GetIndices();
        for (size_t j = 0; j < sizeof(boxEdges) / sizeof(boxEdges[0]); ++j)
          indices_total.push_back(i * 8 + boxEdges[j]);
        ++i;
      }
    }
  }
  m_boxes_mesh->UpdateData(extrems_total, indices_total, { 1.0f, 0.0f, 0.0f, 1.0f });
  if(first_call)
    m_game->AddObject(factory.CreateObject(std::make_shared<SimpleModel>(m_boxes_mesh), eObject::RenderType::LINES, "Physics OBB"));
  first_call = false;
}
