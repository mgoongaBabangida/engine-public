#pragma once

#include <base/interfaces.h>

#include "math.h"
#include "Timer.h"
#include "Bone.h"
#include "SkeletalAnimation.h"
#include "MatricesDB.h"

#include <atomic>
#include <set>

class IAnimatedModel;

//----------------------------------------------------------------------------------------------------
class DLL_MATH RigAnimator : public IRigger
{
public:
  explicit RigAnimator(IAnimatedModel* _model);
  RigAnimator(std::vector<SkeletalAnimation>,std::vector<Bone>, std::string, const glm::mat4&, const std::string& _path = "");
  
  RigAnimator* Clone() const;

  virtual ~RigAnimator();

  // Playback control
  virtual bool        Apply(const std::string& _animation, bool _play_once = false);
  virtual void        Stop();
  virtual bool        ChangeName(const std::string& _oldName, const std::string& _newName);

  // Sockets
  virtual void                                CreateSocket(const std::shared_ptr<eObject>& _socket_obj,
                                                           const std::string& _boneName,
                                                           glm::mat4 _pretransform,
                                                           bool _map_global = false) override;
  virtual std::vector<AnimationSocket>&       GetSockets() override; // non-const ref why?, can race?
  virtual void                                ClearSockets() override;

  // Matrices access
  // Skin (final) matrices: GlobalAnimated * InverseBind — for the shader
  virtual const std::array<glm::mat4, MAX_BONES>& GetMatrices() override; // not safe can race, better use GetChached everywhere
  // Evaluate an arbitrary frame of a specific animation (utility/offline)
  virtual std::array<glm::mat4, MAX_BONES>        GetMatrices(const std::string& _animationName, size_t _frame);

  // Counts / names
  virtual size_t                 GetAnimationCount() const;
  virtual size_t                 GetBoneCount() const;
  const std::string&             GetCurrentAnimationName() const;
  virtual size_t                 GetCurrentAnimationFrameIndex() const;

  virtual const std::string&     GetPath() const { return m_path; }
  virtual void                   SetPath(const std::string& _path) { m_path = _path; }

  virtual bool                   UseFirstFrameAsIdle() override;

  void SplitAnimation(uint32_t index, const std::vector<std::pair<uint32_t, std::string>>& userPattern);

  SkeletalAnimation*              GetCurrentAnimation() const { return m_currentAnim.get(); } // unsafe !
  void                            SetCurrentAnimation(const std::string& _animationName);

  std::vector<std::string>       GetAnimationNames() const;
  void                           AddAnimation(std::shared_ptr<SkeletalAnimation> anim);
  bool                           DeleteAnimation(const std::string& _animationName);

  void                           SetActiveBoneIndex(int32_t _index) { m_activeBoneIndex = _index; }
  int32_t                        GetActiveBoneIndex() { return m_activeBoneIndex; }

  const Bone* GetParent(const std::string& _boneName);
  const std::vector<Bone*>       GetChildren(const std::string& _boneName);

  std::map<std::string, std::shared_ptr<SkeletalAnimation>> GetAnimations() const;
  std::vector<std::string>       GetBoneNames() const;

  // Various matrices (string overloads still supported)
  glm::mat4                      GetBindMatrixForBone(const std::string& _boneName) const;
  glm::mat4                      GetBindMatrixForBone(size_t _boneID) const;

  // For sockets (GLOBAL, model-space, no inverseBind)
  glm::mat4                      GetGlobalMatrixForBone(const std::string& _boneName) const;
  glm::mat4                      GetGlobalMatrixForBone(size_t _boneID) const;

  // Skin final per-bone
  glm::mat4                      GetCurrentMatrixForBone(const std::string& _boneName) const;
  glm::mat4                      GetCurrentMatrixForBone(size_t _boneID) const;

  const std::vector<Bone>&  GetBones() const { return m_bones; }
  const std::string&        GetNameRootBone() const { return m_nameRootBone; }
  const std::string&        GetNameHipsBone() const { return m_nameHipsBone; }
  const glm::mat4&          GetGlobalModelTransform() const { return m_globalModelTransform; }

  float& GetArmOffset() { return m_armOffsetAngle; }

  std::set<std::string>          GetJointNamesFromSkeleton();
  void                           ApplySkeletonOffsetsToAnimation(const std::string&);

  // Legacy cache API kept to satisfy IRigger; now just aliases
  virtual void                                            CacheMatrices() override; // no-op
  virtual const std::array<glm::mat4, MAX_BONES>&         GetCachedMatrices() override;

  enum class RootMotionPolicy { AsAuthored, InPlaceKeepYaw, InPlaceZeroYaw };
  void                           SetRootMotionPolicy(RootMotionPolicy p) { m_rootPolicy = p; }
  
  void                           UpdateOnce(); // per-frame evaluate without timer step

  void                           CorrectInverseBindTransformForBone(const std::string&, const glm::vec3& correction);
  void                           DebugDumpBindTripleForBone(const std::string& boneNameNeedle);
protected:
  void                           _UpdateAnimation(Bone& bone, const Frame& frame, const glm::mat4& ParentTransform);
  void                           _Initialize();
  void                           _FilterAnimationsToSkeleton();
  void                           _NormalizeBoneNames();

  std::map<std::string, std::shared_ptr<SkeletalAnimation>> m_animations;
  std::vector<Bone>                                         m_bones;
  std::vector<AnimationSocket>                              m_sockets;

  std::string                                               m_nameRootBone;
  std::string                                               m_nameHipsBone;
  float                                                     m_armOffsetAngle = 0;
  glm::mat4                                                 m_globalModelTransform;

  std::shared_ptr<SkeletalAnimation>                        m_currentAnim = nullptr;
  int32_t                                                   m_activeBoneIndex = MAX_BONES + 1;

  // Skin (GlobalAnimated * InverseBind)
  MatricesDB<MAX_BONES>                                     m_skinDB;
  // Global bone transforms in MODEL space (for sockets, gizmos, debug)
  MatricesDB<MAX_BONES>                                     m_globalDB;

  RootMotionPolicy                                          m_rootPolicy = RootMotionPolicy::InPlaceKeepYaw;

  std::unique_ptr<math::Timer>                              m_timer;

  mutable std::mutex                                        m_mutex;
  std::condition_variable                                   m_cv;
  std::atomic<bool>                                         m_is_active = true;

  std::string                                               m_path;

  glm::vec3                                                 m_hipFrame0ParityT = glm::vec3(0.0f);
  bool                                                      m_applyHipParityFix = false;
};
