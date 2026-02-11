
#include "Colliders.h"
#include "GeometryFunctions.h"

#include <glm\glm\gtc\quaternion.hpp>
#include <glm\glm\gtx\quaternion.hpp>
#include <glm\glm\gtx\euler_angles.hpp>

namespace dbb
{
  //---------------------------------------------------------------------------------------
  void OBBCollider::SynchCollisionVolumes(const glm::vec3& _pos, const glm::vec3& _orientation)
  {
    box.origin = _pos;
    box.orientation = glm::eulerAngleYXZ(_orientation.y, _orientation.x, _orientation.z);
  }

  //--------------------------------------------------
  glm::vec4 OBBCollider::GetTensor(float _mass) const
  {
    float ix = 0.0f;
    float iy = 0.0f;
    float iz = 0.0f;
    float iw = 0.0f;
    if (_mass != 0)
    {
      glm::vec3 size = box.size * 2.0f;
      float fraction = (1.0f / 12.0f);
      float x2 = size.x * size.x;
      float y2 = size.y * size.y;
      float z2 = size.z * size.z;
      ix = (y2 + z2) * _mass * fraction;
      iy = (x2 + z2) * _mass * fraction;
      iz = (x2 + y2) * _mass * fraction;
      iw = 1.0f;
    }
    return glm::vec4(ix, iy, iz, iw);
  }

  //---------------------------------------------------------------------------------------
  OBBCollider::OBBCollider(const BaseCollider& _base, const ITransform& _trans)
    : BaseCollider(_base)
    , box(_base._GetOBB(_trans))
  {
  }

  //---------------------------------------------------------------------------------------
  OBBCollider* OBBCollider::CreateFrom(const BaseCollider& _base, const ITransform& _trans)
  {
    return new OBBCollider(_base, _trans);
  }

  //---------------------------------------------------------------------------------------
  dbb::OBB OBBCollider::GetBox() const
  {
    return box;
  }

  //---------------------------------------------------------------------------------------
  dbb::point OBBCollider::GetCenter() const
  {
    return box.origin;
  }

  // -------------------------------------------------------------------------------------- -
  glm::vec3 OBBCollider::GetOrientation() const
  {
    return glm::eulerAngles(glm::toQuat(box.orientation)); // pitch(x), yaw(x), roll(x)
    //return { xyz.y, xyz.x, xyz.z }; /// yaw, pitch, roll
  }

  //---------------------------------------------------------------------------------------
  void OBBCollider::SetFrom(const ITransform& _trans)
  {
    box = _GetOBB(_trans);
  }

  //---------------------------------------------------------------------------------------
  void OBBCollider::SetTo(ITransform& _trans) const
  {
    _trans.setRotation(glm::toQuat(box.orientation));
    _trans.setTranslation(box.origin - (_trans.getRotation() * (BaseCollider::GetCenterLocalSpace()*m_scale) ));
  }

  //-------------------------------------------------------------------------
  float OBBCollider::RayCast(const dbb::ray& _ray, RaycastResult& outResult) const
  {
    return _ray.Raycast(box, outResult);
  }

  //---------------------------------------------------------------------------------------
  SphereCollider::SphereCollider(const BaseCollider& _base, const ITransform& _trans)
    : BaseCollider(_base)
    , sphere(_base._GetSphere(_trans))
  {
  }

  //---------------------------------------------------------------------------------------
  SphereCollider* SphereCollider::CreateFrom(const BaseCollider& _base, const ITransform& _trans)
  {
    return new SphereCollider(_base, _trans);
  }

  //---------------------------------------------------------------------------------------
  dbb::sphere SphereCollider::GetSphere() const
  {
    return sphere;
  }

  //---------------------------------------------------------------------------------------
  dbb::point SphereCollider::GetCenter() const
  {
    return sphere.position;
  }

  // -------------------------------------------------------------------------------------- -
  glm::vec3 SphereCollider::GetOrientation() const
  {
    return { 1,1,1 };
  }

  //---------------------------------------------------------------------------------------
  void SphereCollider::SetFrom(const ITransform& _trans)
  {
    sphere = _GetSphere(_trans);
  }

  //---------------------------------------------------------------------------------------
  void SphereCollider::SetTo(ITransform& _trans) const
  {
    //dbb::sphere s1 = sphere1.second->GetCollider()->GetSphere().value();
    //sphere1.first->GetTransform()->setTranslation(s1.position - sphere1.first->GetCollider()->GetCenter());
    _trans.setTranslation(sphere.position - BaseCollider::GetCenterLocalSpace());
  }

  //-------------------------------------------------------------------------
  float SphereCollider::RayCast(const dbb::ray& _ray, RaycastResult& outResult) const
  {
    return _ray.Raycast(sphere, outResult);
  }

  //---------------------------------------------------------------------------------------
  CollisionManifold OBBCollider::CollidesWith(const SphereCollider& _sphere) const
  {
    CollisionManifold cm = FindCollision(box, _sphere.GetSphere()); //@todo check optional
    cm.normal = -cm.normal;
    return cm;
  }

  //---------------------------------------------------------------------------------------
  CollisionManifold OBBCollider::CollidesWith(const OBBCollider& _box) const
  {
    return FindCollision(_box.GetBox(), box);
  }

  //------------------------------------------------------------------------------
  CollisionManifold OBBCollider::CollidesWith(const EllipseCollider& _ellipse) const
  {
    return FindCollision(GetBox(), _ellipse.GetEllipse());
  }

  //--------------------------------------------------------------------------------
  CollisionManifold OBBCollider::CollidesWith(const MeshCollider& _other) const
  {
    if (this->GetScaledRadius() + _other.GetScaledRadius() < glm::length(_other.GetBox().origin - this->box.origin))
      return CollisionManifold{};
    return FindCollision(_other.GetMesh(), _other.GetMeshTransform(), box);
  }

  //--------------------------------------------------------------------------------
  CollisionManifold SphereCollider::CollidesWith(const SphereCollider& _sphere) const
  {
    return FindCollision(_sphere.GetSphere(), sphere);
  }

  //--------------------------------------------------------------------------------
  CollisionManifold SphereCollider::CollidesWith(const OBBCollider& _box) const
  {
    return FindCollision(_box.GetBox(), sphere);
  }

  //---------------------------------------------------------------------------------------
  CollisionManifold SphereCollider::CollidesWith(const EllipseCollider& ellipse) const
  {
    return FindCollision(GetSphere(), ellipse.GetEllipse());
  }

  //------------------------------------------------------------------------------------------
  CollisionManifold SphereCollider::CollidesWith(const MeshCollider& _other) const
  {
    return FindCollision(_other.GetMesh(), GetSphere());
  }

  //------------------------------------------------------------------------------------------
  void SphereCollider::SynchCollisionVolumes(const glm::vec3& _pos, const glm::vec3& _orientation)
  {
    sphere.position = _pos;
  }

  //---------------------------------------------------------------------------------------
  CollisionManifold OBBCollider::Dispatch(const ICollider& _other) const
  {
    return _other.CollidesWith(*this);
  }

  //---------------------------------------------------------------------------------------
  CollisionManifold SphereCollider::Dispatch(const ICollider& _other) const
  {
    return _other.CollidesWith(*this);
  }

  //--------------------------------------------------
  glm::vec4 SphereCollider::GetTensor(float _mass) const
  {
    float ix = 0.0f;
    float iy = 0.0f;
    float iz = 0.0f;
    float iw = 0.0f;
    if (_mass != 0)
    {
      float r2 = sphere.radius * sphere.radius;
      float fraction = (2.0f / 5.0f);
      ix = r2 * _mass * fraction;
      iy = r2 * _mass * fraction;
      iz = r2 * _mass * fraction;
      iw = 1.0f;
    }
    return glm::vec4(ix, iy, iz, iw);
  }

  //------------------------------------------------------------------------
  EllipseCollider::EllipseCollider(const BaseCollider& _base, const ITransform& _trans)
    : BaseCollider(_base)
  {
    SetFrom(_trans);
  }

  //------------------------------------------------------------------------
  EllipseCollider* EllipseCollider::CreateFrom(const BaseCollider& _base, const ITransform& _trans)
  {
    return new EllipseCollider(_base, _trans);
  }

  //------------------------------------------------------------------------
  dbb::ellipse EllipseCollider::GetEllipse() const
  {
    return ellipse;
  }

  //------------------------------------------------------------------------
  void EllipseCollider::SynchCollisionVolumes(const glm::vec3& _pos, const glm::vec3& _orientation)
  {
    ellipse.center = _pos;
    ellipse.orientation = glm::eulerAngleYXZ(_orientation.y, _orientation.x, _orientation.z);
  }

  //------------------------------------------------------------------------
  glm::vec4 EllipseCollider::GetTensor(float _mass) const
  {
    // Calculate moments of inertia components
    float Ixx = (_mass / 5.0f) * (ellipse.radii.y * ellipse.radii.y + ellipse.radii.z * ellipse.radii.z); // Moment of inertia around x-axis
    float Iyy = (_mass / 5.0f) * (ellipse.radii.x * ellipse.radii.x + ellipse.radii.z * ellipse.radii.z); // Moment of inertia around y-axis
    float Izz = (_mass / 5.0f) * (ellipse.radii.x * ellipse.radii.x + ellipse.radii.y * ellipse.radii.y); // Moment of inertia around z-axis

    // The off-diagonal elements (product of inertia) are assumed to be zero for an axis-aligned ellipse
    return { Ixx , Iyy , Izz, 1.0f};
  }

  //----------------------------------------------------------------------------------------------------
  dbb::point EllipseCollider::GetCenter() const
  {
    return ellipse.center;
  }

  //---------------------------------------------------------------------------------
  glm::vec3 EllipseCollider::GetOrientation() const
  {
    return glm::eulerAngles(glm::toQuat(ellipse.orientation)); // pitch(x), yaw(x), roll(x)
   //return { xyz.y, xyz.x, xyz.z }; /// yaw, pitch, roll
  }

  //-----------------------------------------------------------------------------
  void EllipseCollider::SetFrom(const ITransform& _trans)
  {
    dbb::OBB box = _GetOBB(_trans);
    ellipse.center = box.origin;
    ellipse.orientation = box.orientation;
    ellipse.radii = box.size; //@! check
  }

  //--------------------------------------------------------------
  void EllipseCollider::SetTo(ITransform& _trans) const
  {
    _trans.setRotation(glm::toQuat(ellipse.orientation)); //@todo works incorrectly!!!!!!
    _trans.setTranslation(ellipse.center - (_trans.getRotation() * BaseCollider::GetCenterLocalSpace())); //@!?
  }

  //-------------------------------------------------------------------------
  float EllipseCollider::RayCast(const dbb::ray& _ray, RaycastResult& outResult) const
  {
    //@todo implement
    //return _ray.Raycast(ellipse, outResult);
    return -1;
  }

  //--------------------------------------------------------------
  CollisionManifold EllipseCollider::Dispatch(const ICollider& _other) const
  {
    return _other.CollidesWith(*this);
  }

  //--------------------------------------------------------------
  CollisionManifold EllipseCollider::CollidesWith(const SphereCollider& _sphere) const
  {
    return FindCollision(_sphere.GetSphere(), ellipse);
  }

  //--------------------------------------------------------------
  CollisionManifold EllipseCollider::CollidesWith(const OBBCollider& _obb) const
  {
    return FindCollision(_obb.GetBox(), ellipse);
  }

  //--------------------------------------------------------------
  CollisionManifold EllipseCollider::CollidesWith(const EllipseCollider& _other) const
  {
    return FindCollision(_other.GetEllipse(), ellipse);
  }

  //--------------------------------------------------------------
  CollisionManifold EllipseCollider::CollidesWith(const MeshCollider& _other) const
  {
    return FindCollision(_other.GetMesh(), ellipse);
  }

  //----------------------------------------------------------
  dbb::MeshCollider::MeshCollider(const I3DMesh* _mesh, ITransform* _transform)
    : OBBCollider(OBB{}),
    m_mesh(_mesh),
    m_mesh_transform(_transform)
  {
    //@todo _other.GetMeshTransform().getScaleAsVector().x only works when scale is uniform.  -> add GetRadius(ITransform*) or calculate here correctly
    if(_transform)
      SetScale(_transform->getScaleAsVector().x);
  }

  //-----------------------------------------------------------
  CollisionManifold MeshCollider::Dispatch(const ICollider& _other) const
  {
    return _other.CollidesWith(*this);
  }

  //-----------------------------------------------------------
  CollisionManifold MeshCollider::CollidesWith(const OBBCollider& _other) const
  {
    if (this->GetScaledRadius() + _other.GetScaledRadius() < glm::length(_other.GetBox().origin - this->box.origin))
      return CollisionManifold{};
    return FindCollision(*m_mesh, *m_mesh_transform, _other.GetBox());
  }

  //-----------------------------------------------------------
  CollisionManifold MeshCollider::CollidesWith(const SphereCollider& _other) const
  {
    return FindCollision(*m_mesh, _other.GetSphere());
  }

  //-----------------------------------------------------------
  CollisionManifold MeshCollider::CollidesWith(const EllipseCollider& _other) const
  {
    return FindCollision(*m_mesh, _other.GetEllipse());
  }

  //-----------------------------------------------------------
  CollisionManifold MeshCollider::CollidesWith(const MeshCollider& _other) const
  {
    return CollisionManifold(); //@todo implement
  }

}