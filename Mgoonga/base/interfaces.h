#ifndef  INTERFACES_H
#define  INTERFACES_H

#include "base.h"
#include "Material.h"

#include <optional>

#include <glm\glm\glm.hpp>
#include <glm\glm\gtc\matrix_transform.hpp>
#include <glm\glm\gtx\transform.hpp>

class Particle;
struct Texture;

//----------------------------------------------------------------------------------------------
class IInputObserver
{
public:
	virtual ~IInputObserver() = default;

	virtual bool OnUpdate() { return false; }
	virtual bool OnKeyPress(const std::vector<bool>, KeyModifiers _modifier)			{ return false; }
	virtual bool OnKeyJustPressed(uint32_t asci, KeyModifiers _modifier)				{ return false; }
	virtual bool OnKeyRelease(ASCII _key, const std::vector<bool>, KeyModifiers _modifier) { return false; }
	virtual bool OnMouseMove(int32_t x, int32_t y, KeyModifiers _modifier)				{ return false; }
	virtual bool OnMousePress(int32_t x, int32_t y, bool left, KeyModifiers _modifier)	{ return false; }
	virtual bool OnMouseDoublePress(int32_t x, int32_t y, bool left, KeyModifiers _modifier) { return false; }
	virtual bool OnMouseRelease(KeyModifiers _modifier)									{ return false; }
	virtual bool OnMouseWheel(int32_t x, int32_t y, KeyModifiers _modifier)				{ return false; }
	virtual bool OnDropEvent(uint32_t x, uint32_t y, const std::string& _file_path) { return false; }
};

class ITransform;

//----------------------------------------------------------------------------------------------
class IParticleSystem
{
public:
	enum Type
	{
		CONE,
		SPHERE //@todo donate or custom model
	};

	IParticleSystem() {}
	virtual ~IParticleSystem() {}

	static const int32_t MAX_PARTICLES = 1'000;

	virtual void							Start() = 0;
	virtual void							GenerateParticles(int64_t _tick)			= 0;
	virtual std::vector<Particle>::iterator PrepareParticles(const glm::vec3& cameraPosition)	= 0;
	virtual std::vector<Particle>&			GetParticles()	= 0;
	
	virtual bool							IsFinished()		= 0;
	virtual bool							IsStarted()			= 0;
	virtual void							Reset()				= 0;

	virtual float&				ConeAngle() = 0;
	virtual glm::vec3&		Scale()	= 0;
	virtual float&				Speed() = 0;
	virtual float&				RandomizeMagnitude() = 0;
	virtual float&				BaseRadius() = 0;
	virtual float&				LifeLength() = 0;
	virtual int&					ParticlesPerSecond() = 0;
	virtual bool&					Loop() = 0;
	virtual float&				Gravity() = 0;
	virtual glm::vec3&		GetSystemCenter() = 0;
	virtual Type&					SystemType() = 0;
	virtual void					SetSizeBezier(std::array<glm::vec3, 4>) = 0;

	virtual const Texture* GetTexture() = 0;
	virtual void SetTexture(Texture*) = 0;

	virtual ITransform*		GetTransform() = 0;
	virtual std::vector<glm::vec3> GetExtremsWorldSpace() const = 0;
};

//----------------------------------------------------------------------------------------------
class ICommand
{
public:
	virtual ~ICommand() = default;
	virtual void Execute() = 0;
};

//----------------------------------------------------------------------------------------------
class IScript : public IInputObserver
{
protected:
	std::weak_ptr<eObject> m_object;
public:
	virtual ~IScript() = default;
	virtual void																			Initialize() {}
	virtual void																			Update(float _tick) = 0;
	virtual void																			CollisionCallback(const eCollision&)	{}
	void																							SetObject(std::shared_ptr<eObject> obj) { m_object = obj; }
	const std::weak_ptr<eObject>&											GetScriptObject() const { return m_object; }
};

//-----------------------------------------------------------------------------
class IAnimation
{
public:
  virtual ~IAnimation() = default;

  virtual void Start() = 0;
  virtual void Stop() = 0;
  virtual void Continue() = 0;
  virtual bool IsPaused() = 0;

  virtual const std::string&	GetName() const = 0;
	virtual int64_t							GetNumFrames() const = 0;
	virtual int&								GetDuration()  = 0;
	virtual float&							GetSpeed() = 0;

	virtual void								FreezeFrame(size_t) = 0;
};

//----------------------------------------------------------------------------------------------
class IMesh
{
public:
	virtual ~IMesh() = default;
	virtual void															Draw() = 0;
	virtual const std::string&								Name() const = 0;

	virtual bool															HasMaterial() const	{ return false; }
	virtual void															SetMaterial(const Material&) {}
	virtual std::optional<Material>						GetMaterial() const { return std::nullopt; }
};

//----------------------------------------------------------------------------------------------
class I3DMesh : public IMesh
{
public:
	virtual ~I3DMesh() = default;
	virtual size_t														GetVertexCount() const		= 0;
	virtual const std::vector<Vertex>&				GetVertexs() const				= 0;
	virtual const std::vector<unsigned int>&	GetIndices() const				= 0;
	virtual std::vector<TextureInfo>					GetTextures() const				= 0;
	virtual void															AddTexture(Texture*)			= 0;
	virtual void															BindVAO()	const						= 0;
	virtual void															UnbindVAO() const					= 0;
	virtual void															DrawInstanced(int32_t) {}
	virtual void															calculatedTangent()				= 0;
	virtual void															ReloadVertexBuffer()			= 0;

};

//----------------------------------------------------------------------------------------------
class IBone
{
public:
	virtual ~IBone() = default;

	virtual const std::string& GetName()  const							= 0;
	virtual size_t GetID() const														= 0;
	virtual  std::vector<const IBone*> GetChildren() const	= 0;

	virtual const glm::mat4& GetLocalBindTransform() const	= 0;
	virtual const glm::mat4& GetMTransform() const					= 0;
	virtual const glm::mat4& GetInverseBindTransform() const= 0;

	virtual bool IsRealBone() const													= 0;
	virtual bool HasExplicitInverseBind() const							= 0;
};

//----------------------------------------------------------------------------------------------
class IModel
{
public:
	virtual ~IModel() = default;

	virtual void																	Draw(int32_t _program = 0)	 = 0;
	virtual void																	DrawInstanced(int32_t _program, int32_t instances) {}

	virtual void																	SetUpMeshes() {} //@todo
	virtual void																	ReloadTextures() {} //@todo
	virtual std::vector<std::unique_ptr<IModel>>	Split() { return {}; } //@todo

	virtual const std::string&							GetName() const = 0;
	virtual const std::string&							GetPath() const = 0;

	virtual size_t													GetVertexCount() const = 0;
  virtual size_t													GetMeshCount() const = 0;
  virtual std::vector<const IMesh*>				GetMeshes() const = 0;
	virtual std::vector<const I3DMesh*>			Get3DMeshes() const = 0;

	virtual bool														HasBones() const = 0;
	virtual std::vector<const IBone*>				GetBones() const = 0;

  virtual size_t													GetAnimationCount() const = 0;
  virtual std::vector<const IAnimation*>	GetAnimations() const = 0;

	virtual bool														HasMaterial() const { return false; }
	virtual void														SetMaterial(const Material&) {}
	virtual std::optional<Material>					GetMaterial() const { return std::nullopt; }
};

//----------------------------------------------------------------------------------------------
class ITerrainModel : public IModel
{
public:
	virtual ~ITerrainModel() = default;
	virtual float			GetHeight(float, float) = 0;
	virtual glm::vec3	GetNormal(float, float) = 0;
	virtual std::vector<std::vector<glm::vec3>>	GetExtremsOfMeshesLocalSpace() const = 0;
	virtual std::vector<extremDots> GetExtremDotsOfMeshes() const = 0;
};

//-----------------------------------------------------------------------------------------------
class ITransform
{
public:
	virtual ~ITransform() = default;

	virtual void	setRotation(float x, float y, float z) = 0;
	virtual void	setRotation(glm::quat q) = 0;
	virtual void	setTranslation(glm::vec3 tr) = 0;
	virtual void	setScale(glm::vec3 sc) = 0;
	virtual void	setUp(glm::vec3 _up) = 0;
	virtual void	setForward(glm::vec3 _fwd) = 0;
	virtual void	setModelMatrix(const glm::mat4&) = 0;

	virtual const glm::mat4&	getModelMatrix() const = 0;
	virtual glm::mat4					getScale()			const = 0;
  virtual glm::vec3					getScaleAsVector() const = 0;
	virtual glm::vec3					getTranslation()	const = 0;
	virtual glm::vec3&				getTranslationRef() = 0;
	virtual glm::quat					getRotation()		const = 0;
	virtual glm::vec4					getRotationVector() const = 0;
	virtual glm::vec4					getRotationUpVector() const = 0;
	virtual glm::vec3					getUp()				const = 0;
	virtual glm::vec3					getForward()		const = 0;

	virtual void		incrementScale() = 0;
	virtual void		decrementScale() = 0;
	virtual void		billboard(glm::vec3 direction) = 0;
	virtual bool		turnTo(glm::vec3 dest, float speed = 0.f) = 0;
	virtual bool		isRotationValid() = 0;
};

namespace dbb
{
	class SphereCollider;
	class OBBCollider;
	class EllipseCollider;
	class MeshCollider;
	struct ray;
}

//----------------------------------------------------------------------------------------------
class ICollider
{
public:
	virtual ~ICollider() = default;

	virtual void CalculateExtremDots(const eObject* _object) = 0;

	// old api
	virtual bool CollidesWith(const ITransform& trans1,
														const ITransform& trans2,
														const ICollider& other,
														Side moveDirection,
														eCollision& collision) = 0;

	virtual bool CollidesWith(const ITransform& _trans,
														std::vector<std::shared_ptr<eObject>> _objects,
														Side _moveDirection,
														eCollision& _collision) = 0;
	//

	virtual bool IsInsideOfAABB(const ITransform& _trans, const ITransform& trans2, const ICollider& other) = 0;

	virtual float										GetRadius() const = 0;
	virtual float										GetScaledRadius() const = 0;
	virtual void										SetScale(float) = 0;

	virtual const std::string&			GetModelName() = 0;
	virtual const std::string&			GetPath() const = 0;
	virtual void										SetPath(const std::string&) = 0;

	virtual std::vector<glm::mat3>	GetBoundingTriangles(const ITransform& trans)const = 0;
	virtual std::vector<glm::vec3>	GetExtrems(const ITransform& trans) const = 0;

	//Local space
	virtual glm::vec3								GetCenterLocalSpace() const = 0;
	virtual std::vector<glm::mat3>	GetBoundingTrianglesLocalSpace()const = 0;
	virtual std::vector<glm::vec3>	GetExtremsLocalSpace() const = 0;
	virtual extremDots							GetExtremDotsLocalSpace() const = 0;

	// new functions
	//----------------------------------------------------------------------------------
	virtual void										SetFrom(const ITransform& trans) = 0;
	virtual void										SetTo(ITransform& trans) const = 0;

	virtual CollisionManifold				Dispatch(const ICollider& other) const = 0;
	virtual void										SynchCollisionVolumes(const glm::vec3& _pos, const glm::vec3& _orientation) = 0;
	virtual glm::vec4								GetTensor(float mass) const = 0;

	virtual glm::vec3               GetCenter() const = 0;
	virtual glm::vec3								GetOrientation() const = 0;

	virtual float										RayCast(const dbb::ray&, RaycastResult& outResult) const = 0;

	virtual CollisionManifold				CollidesWith(const dbb::SphereCollider& _other) const = 0;
	virtual CollisionManifold				CollidesWith(const dbb::OBBCollider& _other) const = 0;
	virtual CollisionManifold				CollidesWith(const dbb::EllipseCollider& _other) const = 0;
	virtual CollisionManifold				CollidesWith(const dbb::MeshCollider& _other) const = 0;
};

//----------------------------------------------------------------------------------------------
class IRigidBody
{
public:
	virtual void  AddLinearImpulse(const glm::vec3& impulse) = 0;
	virtual void  AddRotationalImpulse(const glm::vec3& point, const glm::vec3& impulse) = 0;

	virtual ICollider*		GetCollider() const = 0;
	virtual void					SetCollider(ICollider*) = 0;

	virtual void    SetMass(float) = 0;
	virtual float   GetMass() const = 0;

	virtual void    SetCoefOfRestitution(float) = 0;
	virtual float   GetCoefOfRestitution() const = 0;

	virtual void    SetFriction(float) = 0;
	virtual float   GetFriction() const = 0;

	virtual void    SetGravityApplicable(bool) = 0;
	virtual bool    GetGravityApplicable() const = 0;

	virtual bool IsKinematic() const = 0;
	virtual void SetKinematic() = 0;

	//debug
	virtual float& GetDamping() = 0;
	virtual float& GetDampingAngular() = 0;

	virtual ~IRigidBody() = default;
};

//----------------------------------------------------------------------------------------------
class IRigger
{
public:
	virtual ~IRigger() = default;
	virtual bool													Apply(const std::string& _animation, bool _play_once)																															= 0;
	virtual void													Stop()																																																						= 0;
	virtual bool													ChangeName(const std::string& _oldName, const std::string& _newName)																							= 0;
	virtual void													CreateSocket(const std::shared_ptr<eObject>& _socket_obj, const std::string& _boneName, glm::mat4 _pretransform, bool _map_global = false)	= 0;
	virtual std::vector<AnimationSocket>&	GetSockets() 																																			= 0;
	virtual void													ClearSockets()																																		= 0;
	virtual const std::array<glm::mat4,MAX_BONES>&	GetMatrices()																																						= 0;
	virtual size_t																	GetAnimationCount() const																																= 0;
	virtual IAnimation*															GetCurrentAnimation() const																															= 0;
	virtual std::vector<std::string>								GetAnimationNames() const																																= 0;
	virtual const std::string&											GetCurrentAnimationName() const																													= 0;
	virtual size_t																	GetCurrentAnimationFrameIndex() const																										= 0;
	virtual size_t																	GetBoneCount() const																																		= 0;
	virtual std::array<glm::mat4,MAX_BONES>					GetMatrices(const std::string& _animationName, size_t _frame)														= 0;
	virtual const std::string&											GetPath() const																																					= 0;
	virtual void																		SetPath(const std::string&)																															= 0;

	virtual bool																		UseFirstFrameAsIdle()																																		= 0;

	virtual void																		CacheMatrices()																																					= 0;
	virtual const std::array<glm::mat4, MAX_BONES>& GetCachedMatrices()																																			= 0;
};

//---------------------------------------------------------------------------
class ISound
{
public:
  virtual ~ISound() = default;
	virtual void Play()		 = 0;
	virtual bool isPlaying() = 0;
	virtual void Stop()		 = 0;
};

//-----------------------------------------------------------------------------
class IGame
{
public:
	enum class GameState
	{
		UNINITIALIZED = 0,
		LOADING = 1,
		MODEL_RELOAD = 2,
		LOADED = 3,
	};

	virtual ~IGame() = default;

	virtual void		InitializeGL() = 0;
	virtual void		InitializeScene() = 0;
	virtual void		PaintGL() = 0;
	virtual uint32_t GetFinalImageId() = 0;

	virtual GameState GetState() const = 0;

	virtual std::shared_ptr<eObject> GetFocusedObject() = 0; //const ref?
	virtual const std::vector<std::shared_ptr<eObject>>& GetObjects() const = 0;

	virtual std::vector<std::shared_ptr<IParticleSystem> >  GetParticleSystems() = 0;

	virtual void AddObject(std::shared_ptr<eObject>) = 0;
	virtual void DeleteObject(std::shared_ptr<eObject>) = 0;

	virtual void AddGlobalScript(std::shared_ptr<IScript> _script) = 0;

	virtual void	SetFocused(std::shared_ptr<eObject>) = 0;
	virtual void	SetFramed(const std::vector<std::shared_ptr<eObject>>&) = 0;

	virtual const Texture* GetTexture(const std::string& _name) const = 0;
	virtual Light& GetMainLight() = 0;

	virtual glm::mat4 GetMainCameraViewMatrix() = 0;
	virtual glm::mat4 GetMainCameraProjectionMatrix() = 0;

	virtual glm::vec3 GetMainCameraPosition() const = 0;
	virtual glm::vec3 GetMainCameraDirection() const = 0;

	virtual void AddInputObserver(IInputObserver* _observer, ePriority _priority) = 0;
	virtual void DeleteInputObserver(IInputObserver* _observer) = 0;

	virtual bool& UseGizmo() = 0;
	virtual uint32_t CurGizmoType() = 0;

	virtual bool& ShowFPS() = 0;
	virtual bool& ShowMeasurementGrid() = 0;

	virtual uint32_t	Width() const = 0 ;
	virtual uint32_t	Height() const = 0 ;
};

#endif

