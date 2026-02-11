#pragma once

#include "math.h"
#include "Geometry.h"
#include "BaseCollider.h"

#include <base/base.h>

#include <optional>

namespace dbb
{
  //---------------------------------------------------------------------
  static CollisionManifold FindCollisionFeatures(const ICollider& _A, const ICollider& _B)
  {
    return _A.Dispatch(_B);
  }

  //---------------------------------------------------------------------
  class DLL_MATH OBBCollider : public BaseCollider
  {
  public:
    OBBCollider(const dbb::OBB& _obb) : box(_obb) {}
    OBBCollider(const BaseCollider&, const ITransform& _trans);

    static OBBCollider* CreateFrom(const BaseCollider&, const ITransform&);

    dbb::OBB GetBox() const;

    virtual CollisionManifold       Dispatch(const ICollider& other) const override;
    virtual void                    SynchCollisionVolumes(const glm::vec3& _pos, const glm::vec3& _orientation) override;
    virtual glm::vec4               GetTensor(float mass) const override;

    virtual glm::vec3               GetCenter() const override;
    virtual glm::vec3               GetOrientation() const override;

    virtual void										SetFrom(const ITransform& trans) override;
    virtual void										SetTo(ITransform& trans) const override;

    virtual float										RayCast(const dbb::ray&, RaycastResult& outResult) const override;
  protected:
    virtual CollisionManifold       CollidesWith(const SphereCollider& _other) const override;
    virtual CollisionManifold       CollidesWith(const OBBCollider& _other) const override;
    virtual CollisionManifold       CollidesWith(const EllipseCollider& _other) const override;
    virtual CollisionManifold       CollidesWith(const MeshCollider& _other) const override;
    dbb::OBB box;
  };

  //---------------------------------------------------------------------
  class DLL_MATH SphereCollider : public BaseCollider
  {
  public:
    SphereCollider(const dbb::sphere& _sphere) : sphere(_sphere) {}
    SphereCollider(const BaseCollider&, const ITransform& _trans);

    static SphereCollider* CreateFrom(const BaseCollider&, const ITransform&);

    dbb::sphere GetSphere() const;

    virtual void                        SynchCollisionVolumes(const glm::vec3& _pos, const glm::vec3& _orientation) override;
    virtual glm::vec4                   GetTensor(float mass) const override;

    virtual dbb::point                  GetCenter() const override;
    virtual glm::vec3                   GetOrientation() const override;

    virtual void										    SetFrom(const ITransform& trans) override;
    virtual void										    SetTo(ITransform& trans) const override;

    virtual float                       RayCast(const dbb::ray& _ray, RaycastResult& outResult) const override;

    virtual CollisionManifold Dispatch(const ICollider& other) const override;
  protected:
    virtual CollisionManifold CollidesWith(const SphereCollider& _other) const override;
    virtual CollisionManifold CollidesWith(const OBBCollider& _other) const override;
    virtual CollisionManifold CollidesWith(const EllipseCollider& _other) const override;
    virtual CollisionManifold CollidesWith(const MeshCollider& _other) const override;

    dbb::sphere sphere;
  };

  //---------------------------------------------------------------------
  class DLL_MATH EllipseCollider : public BaseCollider
  {
  public:
    EllipseCollider(const dbb::ellipse& _ellipse) : ellipse(_ellipse) {}
    EllipseCollider(const BaseCollider&, const ITransform& _trans);

    static EllipseCollider* CreateFrom(const BaseCollider&, const ITransform&);

    dbb::ellipse GetEllipse() const;

    virtual void                        SynchCollisionVolumes(const glm::vec3& _pos, const glm::vec3& _orientation) override;
    virtual glm::vec4                   GetTensor(float mass) const override;

    virtual dbb::point                  GetCenter() const override;
    virtual glm::vec3                   GetOrientation() const override;

    virtual void										    SetFrom(const ITransform& trans) override;
    virtual void										    SetTo(ITransform& trans) const override;

    virtual float RayCast(const dbb::ray& _ray, RaycastResult& outResult) const override;

    virtual CollisionManifold Dispatch(const ICollider& other) const override;
  protected:
    virtual CollisionManifold CollidesWith(const SphereCollider& _other) const override;
    virtual CollisionManifold CollidesWith(const OBBCollider& _other) const override;
    virtual CollisionManifold CollidesWith(const EllipseCollider& _other) const override;
    virtual CollisionManifold CollidesWith(const MeshCollider& _other) const override;

    dbb::ellipse ellipse;
  };

  //---------------------------------------------------------------------
  class DLL_MATH MeshCollider : public OBBCollider // @ BaseCollider
  {
  public:
    MeshCollider(const I3DMesh* _mesh, ITransform* _transform);
    const I3DMesh& GetMesh() const { return *m_mesh; }
    const ITransform& GetMeshTransform() const  { return *m_mesh_transform; }

    virtual CollisionManifold Dispatch(const ICollider& other) const override;
  protected:
    virtual CollisionManifold CollidesWith(const OBBCollider& _other) const override;
    virtual CollisionManifold CollidesWith(const SphereCollider& _other) const override;
    virtual CollisionManifold CollidesWith(const EllipseCollider& _other) const override;
    virtual CollisionManifold CollidesWith(const MeshCollider& _other) const override;

    const I3DMesh* m_mesh = nullptr;
    const ITransform* m_mesh_transform = nullptr;
  };
}