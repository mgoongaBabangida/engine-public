#include "stdafx.h"
#include "ObjectFactory.h"

#include <math/RigAnimator.h>
#include <math/Colliders.h>
#include <math/BoxColliderDynamic.h>
#include <math/RigidBody.h>
#include <math/IAnimatedModel.h>

#include <game_assets/AnimationManagerYAML.h>
#include <game_assets/DynamicBoundingBoxColliderSerializerYAML.h>

//----------------------------------------------------------------------------------
ObjectFactoryBase::ObjectFactoryBase(AnimationManagerYAML* _animationManager)
: m_animationManager(_animationManager)
{}

//----------------------------------------------------------------------------------
std::unique_ptr<eObject> ObjectFactoryBase::CreateObject(std::shared_ptr<IModel> _model,
                                                         eObject::RenderType _render_type,
                                                         const std::string& _name)
{
  auto obj = std::make_unique<eObject>();
  obj->SetModel(_model);
  obj->SetRenderType(_render_type);
  obj->SetTransform(new Transform);
  if (!_model->Get3DMeshes().empty())
  {
    obj->SetCollider(new dbb::OBBCollider(dbb::OBB{}));
    obj->GetCollider()->CalculateExtremDots(obj.get());
  }
  obj->SetRigidBody(new dbb::RigidBody(obj->GetCollider()));
  obj->SetName(_name);
  return obj;
}

//----------------------------------------------------------------------------------
std::unique_ptr<eObject> ObjectFactoryBase::CreateObject(std::shared_ptr<IModel> _model,
                                                         eObject::RenderType _render_type,
                                                         const std::string& _name,
                                                         const std::string& _rigger_path,
                                                         const std::string& _collider_path,
                                                         ColliderType _collider_type)
{
  std::unique_ptr<eObject> obj = CreateObject(_model, _render_type, _name);
  if (m_animationManager)
  {
    if (_rigger_path == "Default")
    {
      obj->SetRigger(new RigAnimator(dynamic_cast<IAnimatedModel*>(_model.get())));
    }
    else if (_rigger_path == "None")
    {
    }
    else
    {
      IRigger* rigger = m_animationManager->DeserializeRigger(_rigger_path);
      obj->SetRigger(rigger);
    }

    if (_collider_type == ColliderType::DYNAMIC_BOX)
    {
      if (_collider_path.empty())
      {
        obj->SetCollider(new BoxColliderDynamic);
       /* DynamicBoundingBoxColliderSerializerYAML boxSerializer;
        boxSerializer.Serialize(dynamic_cast<BoxColliderDynamic*>(obj->GetCollider()), "Soldier3Anim.mgoongaBoxColliderDynamic");*/
      }
      else
      {
        DynamicBoundingBoxColliderSerializerYAML boxSerializer;
        obj->SetCollider(boxSerializer.Deserialize(_collider_path));
      }
      obj->GetCollider()->CalculateExtremDots(obj.get()); // always needs this to set up rigger
    }
    else if (_collider_type == ColliderType::BOX)
    {
      if (!_model->Get3DMeshes().empty())
      {
        obj->SetCollider(new dbb::OBBCollider(dbb::OBB{}));
        obj->GetCollider()->CalculateExtremDots(obj.get());
      }
    }
    else if (_collider_type == ColliderType::MESH)
    {
      obj->SetCollider(new dbb::MeshCollider(_model->Get3DMeshes()[0], obj->GetTransform()));
      obj->GetCollider()->CalculateExtremDots(obj.get());
    }
  }
  return obj;
}

void ObjectFactoryBase::SaveDynamicCollider(shObject obj, const std::string& _collider_path)
{
  DynamicBoundingBoxColliderSerializerYAML boxSerializer;
  boxSerializer.Serialize(dynamic_cast<BoxColliderDynamic*>(obj->GetCollider()), _collider_path);
}
