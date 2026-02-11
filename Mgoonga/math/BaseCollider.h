
#ifndef BOX_COLLIDER_H
#define BOX_COLLIDER_H

#include <base/interfaces.h>
#include <base/Object.h>
#include <math/Geometry.h>

#include <glm\glm/gtc/constants.hpp>

struct eCollision;

//----------------------------------------------------------------------------------
class DLL_MATH BaseCollider : public ICollider
{
public:
	virtual void CalculateExtremDots(const eObject* _object) override;
	void CalculateExtremDots(IModel* _model);
	virtual bool CollidesWith(const ITransform& trans,
														const ITransform& trans_other,
														const ICollider& other,
														Side moveDirection,
														eCollision& collision) override;
	virtual bool CollidesWith(const ITransform& trans,
														std::vector<shObject> objects,
														Side moveDirection,
														eCollision& collision) override;
	
	virtual bool IsInsideOfAABB(const ITransform& _trans, const ITransform& _trans2, const ICollider& _other) override;

	virtual float										GetRadius() const override;
	virtual float										GetScaledRadius() const override;
	virtual void										SetScale(float) override;

	virtual std::vector<glm::mat3>	GetBoundingTriangles(const ITransform& trans)const override;
	virtual std::vector<glm::vec3>	GetExtrems(const ITransform& trans) const override;

	virtual glm::vec3								GetCenterLocalSpace() const override;
	virtual std::vector<glm::mat3>	GetBoundingTrianglesLocalSpace()const override;
	virtual std::vector<glm::vec3>	GetExtremsLocalSpace() const override;
	virtual extremDots							GetExtremDotsLocalSpace() const override { return m_dots; }

	virtual const std::string&			GetModelName() override { return m_model_name; } // ?
	virtual const std::string&			GetPath() const { return m_path; }
	virtual void										SetPath(const std::string& _path) { m_path = _path; }

	dbb::OBB								_GetOBB(const ITransform& trans) const;
	dbb::sphere							_GetSphere(const ITransform& trans) const;

	// ---------------------------------------------------------------------------------
	virtual void										SetFrom(const ITransform& trans) = 0;
	virtual void										SetTo(ITransform& trans) const = 0;
	virtual CollisionManifold				Dispatch(const ICollider& other) const override = 0;
	virtual void										SynchCollisionVolumes(const glm::vec3& _pos, const glm::vec3& _orientation) override = 0;
	virtual glm::vec4								GetTensor(float mass) const override = 0;
	virtual glm::vec3								GetOrientation() const override = 0;

protected:
	virtual CollisionManifold				CollidesWith(const dbb::SphereCollider& _other) const override = 0;
	virtual CollisionManifold				CollidesWith(const dbb::OBBCollider& _other) const override = 0;
	virtual CollisionManifold				CollidesWith(const dbb::EllipseCollider& _other) const override = 0;
	// --------------------------------- -------------------------------------------------

protected:
	std::vector<dbb::line>	_GetRays(const ITransform& trans, Side moveDirection, std::vector<float>& lengths);
	void										_GetForwardRayLengths(const ITransform& trans, Side moveDirection, std::vector<float>& lengths)	const;
	bool										_CheckByRadius(const ITransform& _trans, const ITransform& _trans_other, ICollider* _other);

	extremDots								m_dots;

	std::string								m_model_name;
	std::string								m_path;
	bool											m_check_sphere_overlap = true;

	float															m_scale = 1.0f;
	mutable std::optional<glm::vec3>	m_center = std::nullopt;
	mutable float											m_radius = 0.0f;
};

#endif