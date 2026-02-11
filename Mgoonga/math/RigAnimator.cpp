// RigAnimator.cpp (drop-in refactor: same behavior, debug/log gated, debug helpers at bottom)

#include "stdafx.h"
#include "RigAnimator.h"
#include "AnimationUtils.h"
#include "IAnimatedModel.h"
#include <base/Object.h>
#include <base/Log.h>

#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iterator>
#include <cmath>
#include <unordered_map>
#include <limits>
#include <cstdio>

// ==================== RigAnimator compile switches ====================
// 1 = compile logging/printf; 0 = no output.
// NOTE: Do NOT gate computations that can affect behavior.
#ifndef RIGANIMATOR_ENABLE_LOG
#define RIGANIMATOR_ENABLE_LOG 0
#endif

#ifndef RIGANIMATOR_ENABLE_DEBUG
#define RIGANIMATOR_ENABLE_DEBUG 0
#endif

#if RIGANIMATOR_ENABLE_LOG
#define RIG_LOG_STR(msg) base::Log(msg)
#define RIG_LOGF(fmt, ...) do { char _buf[1024]; std::snprintf(_buf, sizeof(_buf), fmt, __VA_ARGS__); base::Log(_buf); } while(0)
#define RIG_PRINTF(...)  std::printf(__VA_ARGS__)
#else
#define RIG_LOG_STR(msg) ((void)0)
#define RIG_LOGF(...)    ((void)0)
#define RIG_PRINTF(...)  ((void)0)
#endif

#if RIGANIMATOR_ENABLE_DEBUG
#define RIG_DBG(...)     RIG_PRINTF(__VA_ARGS__)
#else
#define RIG_DBG(...)     ((void)0)
#endif

// ============================================================================
// Production helpers (used by behavior / not debug-only)
// ============================================================================
namespace
{
  static std::string NormalizeName(std::string k)
  {
    if (auto p = k.find("_$AssimpFbx$_"); p != std::string::npos) k.erase(p);
    std::replace(k.begin(), k.end(), ':', '_');
    return k;
  }

  static inline glm::vec3 GetT(const glm::mat4& M) { return glm::vec3(M[3]); }
  static inline void SetT(glm::mat4& M, const glm::vec3& t) { M[3] = glm::vec4(t, M[3].w); }

  // Find a bone by substring (case-sensitive, tolerant to FBX prefixes).
  static Bone* FindBoneByNameContains(std::vector<Bone>& bones, const std::string& needle)
  {
    for (auto& b : bones)
      if (b.GetName().find(needle) != std::string::npos) return &b;
    return nullptr;
  }

  static std::vector<Bone*> FindAllBonesByNameContains(std::vector<Bone>& bones, const std::string& needle)
  {
    std::vector<Bone*> matches;
    for (auto& b : bones)
      if (b.GetName().find(needle) != std::string::npos) matches.push_back(&b);
    return matches;
  }

  // Evaluate animated GLOBAL at frame-0 for a target bone, without mutating bones.
  // - Uses SAME local policy as _UpdateAnimation (frame locals if present, else MTransform/bind local).
  // - Excludes m_globalModelTransform on purpose (compare purely in skeleton space).
  static glm::mat4 EvalGlobalAtFrame0_NoSideEffects(
    const Bone& target,
    const Bone* root,
    const Frame* frame0)
  {
    if (!root) return UNIT_MATRIX;

    // Build parent map via DFS (rooted).
    std::unordered_map<std::string, const Bone*> parentOf;
    parentOf.reserve(256);

    std::function<void(const Bone*)> fillParents = [&](const Bone* n)
      {
        for (auto* c : n->GetChildren())
        {
          if (!c) continue;
          parentOf[c->GetName()] = n;
          fillParents(static_cast<const Bone*>(c));
        }
      };
    fillParents(root);

    // Build chain from target -> root using parent map
    std::vector<const Bone*> chain;
    chain.reserve(64);

    const Bone* cur = &target;
    while (cur)
    {
      chain.push_back(cur);
      auto it = parentOf.find(cur->GetName());
      cur = (it == parentOf.end()) ? nullptr : it->second;
    }
    std::reverse(chain.begin(), chain.end());

    // Accumulate
    glm::mat4 G = UNIT_MATRIX;
    for (auto* b : chain)
    {
      glm::mat4 local = b->GetMTransform();
      if (frame0 && frame0->Exists(b->GetName()))
        local = frame0->m_pose.find(b->GetName())->second.getModelMatrix();
      G = G * local;
    }
    return G;
  }

  static inline bool MatFinite(const glm::mat4& M)
  {
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 4; ++c)
        if (!std::isfinite(M[c][r])) return false; // glm is column-major
    return true;
  }
}

// ============================================================================
// RigAnimator
// ============================================================================
RigAnimator::RigAnimator(IAnimatedModel* _model)
{
  if (_model)
  {
    for (auto anim : _model->Animations())
      AddAnimation(std::make_shared<SkeletalAnimation>(anim));

    m_bones = _model->Bones();
    m_nameRootBone = _model->RootBoneName();
    m_globalModelTransform = _model->GlobalTransform();

    _Initialize();
  }
}

RigAnimator::RigAnimator(std::vector<SkeletalAnimation> _animations,
  std::vector<Bone> _bones,
  std::string _rootBoneName,
  const glm::mat4& _globaltransform,
  const std::string& _path)
  : m_bones(std::move(_bones))
  , m_nameRootBone(std::move(_rootBoneName))
  , m_globalModelTransform(_globaltransform)
  , m_path(_path)
{
  for (auto anim : _animations)
    AddAnimation(std::make_shared<SkeletalAnimation>(anim));

  _Initialize();
  _FilterAnimationsToSkeleton();
}

//----------------------------------------------------------
RigAnimator* RigAnimator::Clone() const
{
  RigAnimator* animator = new RigAnimator(nullptr);
  animator->m_bones = this->m_bones;
  animator->m_nameRootBone = this->m_nameRootBone;
  animator->m_globalModelTransform = this->m_globalModelTransform;

  // Deep copy animations (do NOT share playback state)
  {
    std::lock_guard lk(this->m_mutex);

    animator->m_animations.clear();
    for (const auto& [name, animPtr] : this->m_animations)
    {
      if (!animPtr) continue;

      auto copy = std::make_shared<SkeletalAnimation>(*animPtr);
      // IMPORTANT: reset runtime playback state in the copy
      copy->ResetPlaybackState();
      animator->m_animations[copy->GetName()] = std::move(copy);
    }
    animator->m_currentAnim.reset(); // don’t inherit current
  }

  animator->_Initialize();
  animator->_FilterAnimationsToSkeleton();
  return animator;
}

RigAnimator::~RigAnimator()
{
  m_is_active = false; // no more sleep
  m_cv.notify_one();   // wake up
  if (m_timer) m_timer->stop();
}

bool RigAnimator::Apply(const std::string& _animation, bool _play_once)
{
  std::shared_ptr<SkeletalAnimation> currentAnimCopy;
  {
    std::lock_guard lk(m_mutex);
    currentAnimCopy = m_currentAnim;
  }

  // Case 1: already playing
  if (currentAnimCopy && currentAnimCopy->GetName() == _animation)
  {
    std::lock_guard lk(m_mutex);
    if (_play_once) m_currentAnim->PlayOnce();
    else            m_currentAnim->Continue();
    m_cv.notify_one();
    return true;
  }

  // Case 2: stop / null => reset DBs to identity and publish
  if (_animation == "Null" || _animation == "NULL")
  {
    {
      std::lock_guard lk(m_mutex);
      m_currentAnim.reset();
    }

    // write identity into back, publish
    {
      auto& backSkin = m_skinDB.begin_write();
      auto& backGlob = m_globalDB.begin_write();
      for (auto& M : backSkin) M = UNIT_MATRIX;
      for (auto& M : backGlob) M = UNIT_MATRIX;
      m_skinDB.publish_written();
      m_globalDB.publish_written();
    }

    m_cv.notify_one();
    return true;
  }

  // Case 3: find and start
  auto it = m_animations.find(_animation);
  if (it != m_animations.end())
  {
    std::lock_guard lk(m_mutex);
    m_currentAnim = it->second;
    if (_play_once) m_currentAnim->PlayOnce();
    else            m_currentAnim->Start();
    m_cv.notify_one();
    return true;
  }

  // Case 4: not found
  return false;
}

void RigAnimator::SetCurrentAnimation(const std::string& _animationName)
{
  auto it = m_animations.find(_animationName);
  if (it != m_animations.end())
  {
    std::lock_guard lk(m_mutex);
    m_currentAnim = it->second;
  }
}

void RigAnimator::Stop()
{
  {
    std::lock_guard lk(m_mutex);
    m_currentAnim = nullptr;
  }
  m_cv.notify_one();
}

bool RigAnimator::ChangeName(const std::string& _oldName, const std::string& _newName)
{
  std::lock_guard lk(m_mutex);
  auto it = m_animations.find(_oldName);
  if (it != m_animations.end())
  {
    if (m_animations.find(_newName) != m_animations.end())
      return false;
    it->second->SetName(_newName);
    m_animations[_newName] = it->second;
    m_animations.erase(it);
    return true;
  }
  return false;
}

void RigAnimator::CreateSocket(const std::shared_ptr<eObject>& _socket_obj,
  const std::string& _boneName,
  glm::mat4 _pretransform,
  bool _map_global)
{
  AnimationSocket socket;
  socket.m_socket_object = _socket_obj.get();

  auto it = std::find_if(m_bones.begin(), m_bones.end(),
    [&_boneName](const Bone& _bone) { return _bone.GetName() == _boneName; });

  if (it != m_bones.end() && _socket_obj.get())
  {
    socket.m_bone_name = it->GetName();
    socket.m_bone_id = (unsigned int)it->GetID();
    socket.m_pre_transform = _pretransform;
    socket.m_map_global = _map_global;
    m_sockets.push_back(std::move(socket));
  }
}

std::vector<AnimationSocket>& RigAnimator::GetSockets() { return m_sockets; }

void RigAnimator::ClearSockets()
{
  m_sockets.clear();
}

const std::array<glm::mat4, MAX_BONES>& RigAnimator::GetMatrices()
{
  return m_skinDB.read();
}

std::array<glm::mat4, MAX_BONES> RigAnimator::GetMatrices(const std::string& _animationName, size_t _frame)
{
  std::shared_ptr<SkeletalAnimation> animPtr;
  {
    std::lock_guard lk(m_mutex);
    auto it = m_animations.find(_animationName);
    if (it == m_animations.end() || it->second->GetNumFrames() <= _frame)
      return {};
    animPtr = it->second;
  }

  std::array<glm::mat4, MAX_BONES> ret;

  auto root = std::find_if(m_bones.begin(), m_bones.end(),
    [this](const Bone& bone) { return m_nameRootBone == bone.GetName(); });

  if (root == m_bones.end())
    return {};

  _UpdateAnimation(*root, animPtr->GetFrameByNumber(_frame), UNIT_MATRIX);

  for (const auto& bone : m_bones)
    ret[bone.GetID()] = bone.GetAnimatedTransform();

  return ret;
}

size_t RigAnimator::GetAnimationCount() const { return m_animations.size(); }
size_t RigAnimator::GetBoneCount() const { return m_bones.size(); }

const std::string& RigAnimator::GetCurrentAnimationName() const
{
  std::lock_guard lk(m_mutex);
  if (m_currentAnim) return m_currentAnim->GetName();
  static const std::string NULL_STR = "NULL";
  return NULL_STR;
}

size_t RigAnimator::GetCurrentAnimationFrameIndex() const
{
  std::lock_guard lk(m_mutex);
  if (m_currentAnim) return m_currentAnim->GetCurFrameIndex();
  return std::numeric_limits<size_t>::max();
}

void RigAnimator::AddAnimation(std::shared_ptr<SkeletalAnimation> anim)
{
  std::lock_guard<std::mutex> m(m_mutex);

  // normalize contained frame bone names just in case
  for (auto& f : anim->m_frames)
  {
    std::map<std::string, Transform> cleaned;
    for (auto& kv : f.m_pose)
      cleaned[NormalizeName(kv.first)] = kv.second;
    f.m_pose.swap(cleaned);
  }

  m_animations[anim->GetName()] = std::move(anim);
}

bool RigAnimator::DeleteAnimation(const std::string& _animationName)
{
  std::lock_guard lk(m_mutex);
  auto it = m_animations.find(_animationName);
  if (it != m_animations.end())
  {
    if (m_currentAnim == it->second) m_currentAnim.reset();
    m_animations.erase(it);
    return true;
  }
  return false;
}

std::vector<std::string> RigAnimator::GetAnimationNames() const
{
  std::lock_guard lk(m_mutex);
  std::vector<std::string> names;
  for (auto& anim : m_animations) names.push_back(anim.second->GetName());
  return names;
}

const Bone* RigAnimator::GetParent(const std::string& _boneName)
{
  auto it = std::find_if(m_bones.begin(), m_bones.end(), [&_boneName](const Bone& bone)
    {
      const auto& children = bone.GetChildren();
      for (unsigned int i = 0; i < children.size(); ++i)
        if (children[i]->GetName() == _boneName) return true;
      return false;
    });
  if (it != m_bones.end()) return &(*it);
  return nullptr;
}

const std::vector<Bone*> RigAnimator::GetChildren(const std::string& _boneName)
{
  auto it = std::find_if(m_bones.begin(), m_bones.end(),
    [&_boneName](const Bone& bone) { return bone.GetName() == _boneName; });
  if (it == m_bones.end()) return {};
  return it->GetChildren();
}

std::map<std::string, std::shared_ptr<SkeletalAnimation>> RigAnimator::GetAnimations() const
{
  std::lock_guard lk(m_mutex);
  return m_animations;
}

std::vector<std::string> RigAnimator::GetBoneNames() const
{
  std::vector<std::string> names;
  names.reserve(m_bones.size());
  for (auto& bone : m_bones) names.push_back(bone.GetName());
  return names;
}

glm::mat4 RigAnimator::GetBindMatrixForBone(const std::string& _boneName) const
{
  auto it = std::find_if(m_bones.begin(), m_bones.end(),
    [&_boneName](const Bone& bone) { return bone.GetName() == _boneName; });
  if (it != m_bones.end()) return it->GetLocalBindTransform();
  return glm::mat4();
}

glm::mat4 RigAnimator::GetBindMatrixForBone(size_t _boneID) const
{
  if (_boneID < m_bones.size())
    return GetBindMatrixForBone(m_bones[_boneID].GetName());
  return glm::mat4();
}

glm::mat4 RigAnimator::GetGlobalMatrixForBone(const std::string& _boneName) const
{
  auto it = std::find_if(m_bones.begin(), m_bones.end(),
    [&_boneName](const Bone& bone) { return bone.GetName() == _boneName; });
  if (it == m_bones.end()) return glm::mat4(1.0f);
  return m_globalDB.read()[it->GetID()];
}

glm::mat4 RigAnimator::GetGlobalMatrixForBone(size_t _boneID) const
{
  if (_boneID >= m_bones.size()) return glm::mat4(1.0f);
  return m_globalDB.read()[_boneID];
}

glm::mat4 RigAnimator::GetCurrentMatrixForBone(const std::string& _boneName) const
{
  auto it = std::find_if(m_bones.begin(), m_bones.end(),
    [&_boneName](const Bone& bone) { return bone.GetName() == _boneName; });
  if (it == m_bones.end()) return glm::mat4(1.0f);
  return m_skinDB.read()[it->GetID()];
}

glm::mat4 RigAnimator::GetCurrentMatrixForBone(size_t _boneID) const
{
  if (_boneID >= m_bones.size()) return glm::mat4(1.0f);
  return m_skinDB.read()[_boneID];
}

void RigAnimator::_FilterAnimationsToSkeleton()
{
  auto validJointNames = GetJointNamesFromSkeleton();
  for (auto& animation : m_animations)
  {
    for (auto& frame : animation.second->m_frames)
    {
      std::map<std::string, Transform> filtered;
      for (const auto& [jointName, transform] : frame.m_pose)
      {
        std::string normalized = NormalizeName(jointName);
        if (validJointNames.count(normalized))
          filtered[normalized] = transform;
      }
      frame.m_pose = std::move(filtered);
    }
  }
}

void RigAnimator::_NormalizeBoneNames()
{
  for (auto& bone : m_bones)
    bone.SetName(NormalizeName(bone.GetName()));
}

std::set<std::string> RigAnimator::GetJointNamesFromSkeleton()
{
  std::set<std::string> jointNames;

  std::function<void(const IBone&)> traverse;
  traverse = [&](const IBone& bone)
    {
      jointNames.insert(bone.GetName());
      for (const auto* child : bone.GetChildren())
        traverse(*child);
    };

  for (const Bone& b : m_bones)
    traverse(b);

  return jointNames;
}

void RigAnimator::ApplySkeletonOffsetsToAnimation(const std::string& _animationName)
{
  std::shared_ptr<SkeletalAnimation> animPtr;
  {
    std::lock_guard lk(m_mutex);
    auto it = m_animations.find(_animationName);
    if (it == m_animations.end()) return;
    animPtr = it->second;
  }
  animPtr->GetOffsetsFromSkeleton(m_bones);
}

void RigAnimator::CacheMatrices()
{
  // No-op with double-buffer; readers always see a stable front buffer.
}

const std::array<glm::mat4, MAX_BONES>& RigAnimator::GetCachedMatrices()
{
  return m_skinDB.read();
}

void RigAnimator::UpdateOnce()
{
  std::shared_ptr<SkeletalAnimation> anim;
  { std::lock_guard lk(m_mutex); anim = m_currentAnim; }
  if (!anim || anim->IsPaused()) return;

  auto root = std::find_if(m_bones.begin(), m_bones.end(),
    [this](const Bone& bone) { return m_nameRootBone == bone.GetName(); });
  if (root == m_bones.end()) return;

  _UpdateAnimation(*root, anim->GetCurrentFrame(), UNIT_MATRIX);

  auto& skinBack = m_skinDB.begin_write();
  auto& globalBack = m_globalDB.begin_write();

  for (const auto& bone : m_bones)
  {
    globalBack[bone.GetID()] = bone.GetGlobalTransform();
    skinBack[bone.GetID()] = bone.GetAnimatedTransform();
  }

  m_globalDB.publish_written();
  m_skinDB.publish_written();
}

void RigAnimator::CorrectInverseBindTransformForBone(const std::string& _bone, const glm::vec3& _correction)
{
  auto bones = FindAllBonesByNameContains(m_bones, _bone);
  if (!bones.empty())
  {
    for (Bone* b : bones)
    {
      auto inverse = b->GetInverseBindTransform();
      inverse[3][0] += _correction.x;
      inverse[3][1] += _correction.y;
      inverse[3][2] += _correction.z;
      b->SetInverseBindTransform(inverse);
    }
  }
}

// ============================================================================
// Core: evaluate one frame recursively
// Assumptions (matches your current setup):
// - LocalBind / MTransform are LOCAL in SCENE space
// - InverseBindTransform is inverse bind in MODEL space (Assimp offset, rebased)
// - m_globalModelTransform == rootInv == inverse(sceneRootTransform)
// - parentLocalAccum is parent's GLOBAL in SCENE space
//
// Writes:
// - bone.GlobalTransform = globalModel (MODEL space) (used by sockets via m_globalDB)
// - bone.AnimatedTransform = skin (MODEL space) (used by shader via m_skinDB)
// ============================================================================
void RigAnimator::_UpdateAnimation(Bone& bone, const Frame& frame, const glm::mat4& parentLocalAccum)
{
  // 1) LOCAL in SCENE space
  glm::mat4 localScene = bone.GetLocalBindTransform();
  if (frame.Exists(bone.GetName()))
    localScene = frame.m_pose.find(bone.GetName())->second.getModelMatrix();
  else
    localScene = bone.GetLocalBindTransform();

  // @todo: keep as-is (your parity fix hook)
  /*if (bone.GetName() == "horse_BIP" && m_applyHipParityFix)
  {
    localScene[3][0] -= m_hipFrame0ParityT.x;
    localScene[3][1] -= m_hipFrame0ParityT.y;
    localScene[3][2] -= m_hipFrame0ParityT.z;
  }*/

  // --- ROOT MOTION REMOVAL ON HIPS ---
  if (m_currentAnim && bone.GetName() == m_nameHipsBone)
  {
    glm::vec3 tBind = GetT(bone.GetLocalBindTransform());
    glm::vec3 tAnim = GetT(localScene);

    glm::vec3 d = tAnim - tBind;
    if (m_currentAnim->GetFixHipsY()) d.y = std::min(d.y, 0.0f); // prevent going UP, allow crouch
    if (m_currentAnim->GetFixHipsZ()) d.z = 0.0f;                // prevent any forward/back
    SetT(localScene, tBind + d);
  }

  // Optional safety (kept commented as before)
  // if (!MatFinite(localScene)) localScene = bone.GetLocalBindTransform();

  // 2) GLOBAL in SCENE space
  const glm::mat4 globalScene = parentLocalAccum * localScene;

  // 3) GLOBAL in MODEL space
  const glm::mat4 globalModel = m_globalModelTransform * globalScene;
  bone.SetGlobalTransform(globalModel);

  // 4) Skin matrix in MODEL space
  const glm::mat4 invBindModel = bone.GetInverseBindTransform();
  const glm::mat4 skin = globalModel * invBindModel;
  bone.SetAnimatedTransform(skin);

  // 5) Recurse children (pass GLOBAL SCENE, not MODEL)
  for (Bone* child : bone.GetChildren())
  {
    if (!child) continue;
    _UpdateAnimation(*child, frame, globalScene);
  }
}

bool RigAnimator::UseFirstFrameAsIdle()
{
  if (m_animations.empty()) return false;

  if (m_currentAnim == nullptr)
    Apply(m_animations.begin()->second->GetName(), false);

  m_currentAnim->FreezeFrame(0);
  return true;
}

//
void RigAnimator::SplitAnimation(
  uint32_t index,
  const std::vector<std::pair<uint32_t, std::string>>& userPattern)
{
  if (m_animations.empty() || userPattern.empty())
    return;

  // ---- 1) Locate source animation by index in ordered map ----
  auto it = m_animations.begin();
  if (index >= m_animations.size())
    return;
  std::advance(it, index);

  const std::string srcName = it->first;
  std::shared_ptr<SkeletalAnimation> src = it->second;
  if (!src) return;

  const uint32_t srcFrames = static_cast<uint32_t>(src->GetNumFrames());
  if (srcFrames == 0) return;

  const int   srcDurationMs = src->GetDuration(); // total ms of original anim
  const float srcSpeed = src->GetSpeed();
  const bool  fixZ = src->GetFixHipsZ();
  const bool  fixY = src->GetFixHipsY();

  // ---- 2) Sanitize & sort pattern ----
  std::vector<std::pair<uint32_t, std::string>> pattern = userPattern;
  pattern.erase(std::remove_if(pattern.begin(), pattern.end(),
    [&](const auto& p) { return p.first >= srcFrames || p.second.empty(); }),
    pattern.end());
  if (pattern.empty()) return;

  std::sort(pattern.begin(), pattern.end(),
    [](auto& a, auto& b) { return a.first < b.first; });
  pattern.erase(std::unique(pattern.begin(), pattern.end(),
    [](auto& a, auto& b) { return a.first == b.first; }), pattern.end());

  // ---- 3) Unique name helper ----
  auto makeUnique = [&](std::string base)->std::string {
    if (base.empty()) base = "Anim";
    std::string candidate = base;
    int suffix = 1;
    while (m_animations.find(candidate) != m_animations.end()) {
      candidate = base + "_" + std::to_string(suffix++);
    }
    return candidate;
    };

  // ---- 4) Build slices [start..end] ----
  struct Slice { uint32_t start, end; std::string name; };
  std::vector<Slice> slices;
  slices.reserve(pattern.size());
  for (size_t i = 0; i < pattern.size(); ++i) {
    const uint32_t start = pattern[i].first;
    const uint32_t end = (i + 1 < pattern.size())
      ? (pattern[i + 1].first == 0 ? 0 : pattern[i + 1].first - 1)
      : (srcFrames - 1);
    if (start > end || start >= srcFrames) continue;

    Slice s;
    s.start = start;
    s.end = std::min(end, srcFrames - 1);
    s.name = makeUnique(pattern[i].second);
    slices.push_back(std::move(s));
  }
  if (slices.empty()) return;

  // If currently playing the source, stop it
  {
    std::lock_guard lk(m_mutex);
    if (m_currentAnim == src) {
      m_currentAnim.reset();
      m_cv.notify_one();
    }
  }

  // ---- 5) Create new animations from slices ----
  for (const auto& s : slices) {
    const uint32_t sliceCount = (s.end - s.start + 1);

    const int newDurationMs = std::max<int>(
      1,
      static_cast<int>(std::llround(
        static_cast<double>(srcDurationMs) *
        static_cast<double>(sliceCount) /
        static_cast<double>(std::max<uint32_t>(1, srcFrames))
      ))
    );

    auto anim = std::make_shared<SkeletalAnimation>(*src);
    anim->SetName(s.name);

    std::vector<Frame> sliced;
    sliced.reserve(sliceCount);
    for (uint32_t f = s.start; f <= s.end; ++f)
      sliced.push_back(src->m_frames[static_cast<size_t>(f)]);

    int64_t t0 = 0, t1 = 0;
    if (!sliced.empty()) {
      t0 = sliced.front().m_timeStamp;
      t1 = sliced.back().m_timeStamp;
    }
    const int64_t span = std::max<int64_t>(1, t1 - t0);

    for (size_t i = 0; i < sliced.size(); ++i) {
      const int64_t oldTs = sliced[i].m_timeStamp;
      const int64_t shifted = std::max<int64_t>(0, oldTs - t0);
      const int64_t mapped = static_cast<int64_t>(
        std::llround(static_cast<long double>(shifted) *
          static_cast<long double>(newDurationMs) /
          static_cast<long double>(span))
        );
      sliced[i].m_timeStamp = std::clamp<int64_t>(mapped, 0, newDurationMs);
    }

    if (!sliced.empty())
      sliced.back().m_timeStamp = newDurationMs;

    anim->m_frames.swap(sliced);
    anim->m_duration = newDurationMs;
    anim->m_play_once = false;
    anim->m_cur_frame_index = 0;
    anim->m_freeze_frame = std::numeric_limits<size_t>::max();
    anim->m_speed = srcSpeed;
    anim->m_fix_hips_z_movement = fixZ;
    anim->m_fix_hips_y_movement = fixY;

    m_animations[anim->GetName()] = std::move(anim);
  }

  // ---- 6) Remove original animation ----
  m_animations.erase(srcName);
}

// ============================================================================
// _Initialize (same behavior; logs/debug gated)
// ============================================================================
void RigAnimator::_Initialize()
{
  // 1) Rewire children to point into m_bones (same as before)
  std::unordered_map<std::string, Bone*> byName;
  byName.reserve(m_bones.size());
  for (auto& b : m_bones) byName[b.GetName()] = &b;

  std::vector<std::vector<std::string>> childNames(m_bones.size());
  for (size_t i = 0; i < m_bones.size(); ++i)
    for (Bone* c : m_bones[i].GetChildren())
      if (c) childNames[i].push_back(c->GetName());

  for (auto& b : m_bones) b.GetChildren().clear();

  for (size_t i = 0; i < m_bones.size(); ++i)
  {
    for (const auto& cname : childNames[i])
    {
      auto it = byName.find(cname);
      if (it != byName.end())
        m_bones[i].AddChild(it->second);
    }
  }

  // 2) Use root provided by loader; do NOT auto-detect
  auto root = std::find_if(
    m_bones.begin(), m_bones.end(),
    [this](const Bone& bone) { return m_nameRootBone == bone.GetName(); }
  );

  if (root != m_bones.end() && !root->GetChildren().empty())
  {
    m_nameHipsBone = root->GetChildren().back()->GetName();
    root->CalculateInverseBindTransform(UNIT_MATRIX, glm::inverse(m_globalModelTransform));
  }
  else
  {
    if (root == m_bones.end() && !m_bones.empty())
    {
      root = m_bones.begin();
      m_nameRootBone = root->GetName();
      if (!root->GetChildren().empty())
        m_nameHipsBone = root->GetChildren().back()->GetName();
      root->CalculateInverseBindTransform(UNIT_MATRIX, glm::inverse(m_globalModelTransform));
    }
  }

  // -------- Identity test / parity baseline (COMPUTATION NOT GATED) --------
  {
    const Frame* frame0 = nullptr;
    std::shared_ptr<SkeletalAnimation> an0;
    {
      std::lock_guard lk(m_mutex);
      if (!m_animations.empty())
        an0 = m_animations.begin()->second;
    }
    if (an0 && an0->GetNumFrames() > 0)
      frame0 = &an0->GetFrameByNumber(0);

    Bone* rootPtr = (root != m_bones.end()) ? &(*root) : nullptr;

    if (rootPtr && frame0)
    {
      // Probe pelvis for baseline
      Bone* pelvis = FindBoneByNameContains(m_bones, "Pelvis");
      if (pelvis)
      {
        glm::mat4 G0 = EvalGlobalAtFrame0_NoSideEffects(*pelvis, rootPtr, frame0);
        glm::mat4 J = pelvis->GetInverseBindTransform();
        glm::mat4 Delta = G0 * J;

        // store baseline translation (same behavior)
        m_hipFrame0ParityT = glm::vec3(Delta[3]);

#if RIGANIMATOR_ENABLE_LOG
        RIG_LOGF("[BindParity] Pelvis baseline fix set: (%.6f, %.6f, %.6f)",
          m_hipFrame0ParityT.x, m_hipFrame0ParityT.y, m_hipFrame0ParityT.z);
#endif
      }
    }
  }
  // -------- end parity baseline --------

  // Initialize matrix buffers (unchanged)
  {
    auto& skinBack = m_skinDB.begin_write();
    auto& globalBack = m_globalDB.begin_write();

    for (auto& M : skinBack)   M = glm::mat4(1.0f);
    for (auto& M : globalBack) M = glm::mat4(1.0f);

    for (const auto& b : m_bones)
    {
      globalBack[b.GetID()] = b.GetGlobalTransform();
      skinBack[b.GetID()] = b.GetAnimatedTransform();
    }

    m_globalDB.publish_written();
    m_skinDB.publish_written();
  }

#if RIGANIMATOR_ENABLE_DEBUG
  DebugDumpBindTripleForBone("LeftHandMiddle1");
#endif

  // 4) Timer loop (unchanged behavior)
  m_timer.reset(new math::Timer([this]()->bool
    {
      std::shared_ptr<SkeletalAnimation> anim;
      { std::lock_guard lk(m_mutex); anim = m_currentAnim; }
      if (anim != nullptr && !anim->IsPaused())
      {
        auto rootIt = std::find_if(m_bones.begin(), m_bones.end(),
          [this](const Bone& bone) { return m_nameRootBone == bone.GetName(); });

        if (rootIt != m_bones.end())
        {
          if (!rootIt->GetChildren().empty())
            m_nameHipsBone = rootIt->GetChildren().back()->GetName();

          _UpdateAnimation(*rootIt, anim->GetCurrentFrame(), UNIT_MATRIX);

          auto& skinBack = m_skinDB.begin_write();
          auto& globalBack = m_globalDB.begin_write();

          for (const auto& bone : m_bones)
          {
            globalBack[bone.GetID()] = bone.GetGlobalTransform();
            skinBack[bone.GetID()] = bone.GetAnimatedTransform();
          }

          m_globalDB.publish_written();
          m_skinDB.publish_written();
        }
      }
      else if (m_is_active)
      {
        std::unique_lock lk(m_mutex);
        m_cv.wait(lk);
      }
      return true;
    }));

  m_timer->start(33); // 30fps
}

// ============================================================================
// Debug API entrypoint (kept; compiles to no-op when debug disabled)
// ============================================================================
void RigAnimator::DebugDumpBindTripleForBone(const std::string& boneNameNeedle)
{
#if RIGANIMATOR_ENABLE_DEBUG
  // implemented at end of file
  extern void RigAnimator_DebugDumpBindTriple_Impl(RigAnimator * self, const std::string & needle);
  RigAnimator_DebugDumpBindTriple_Impl(this, boneNameNeedle);
#else
  (void)boneNameNeedle;
#endif
}

// ============================================================================
// DEBUG SECTION (safe to move into RigAnimatorDebug.cpp later)
// ============================================================================
#if RIGANIMATOR_ENABLE_DEBUG

namespace rig_anim_debug
{
  static glm::vec3 ExtractT(const glm::mat4& M) { return glm::vec3(M[3]); }

  static glm::vec3 ExtractS(const glm::mat4& M)
  {
    return glm::vec3(glm::length(glm::vec3(M[0])),
      glm::length(glm::vec3(M[1])),
      glm::length(glm::vec3(M[2])));
  }

  static float MaxAbsDiff(const glm::mat4& A, const glm::mat4& B)
  {
    float m = 0.f;
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 4; ++c)
        m = std::max(m, std::fabs(A[c][r] - B[c][r]));
    return m;
  }

  static bool MatHasNaNInf(const glm::mat4& M)
  {
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 4; ++c)
        if (!std::isfinite(M[c][r])) return true;
    return false;
  }

  static void PrintTRSLine(const char* tag, const glm::mat4& M)
  {
    glm::vec3 t = ExtractT(M);
    glm::vec3 s = ExtractS(M);

    float maxAbs = 0.f;
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 4; ++c)
        maxAbs = std::max(maxAbs, std::fabs(M[c][r]));

    RIG_PRINTF("%-12s T=(% .6g % .6g % .6g)  S=(%.6g %.6g %.6g)  maxAbs=%.6g NaN=%d\n",
      tag, t.x, t.y, t.z, s.x, s.y, s.z, maxAbs, (int)MatHasNaNInf(M));
  }
}

// This is the original DebugDumpBindTripleForBone body moved out-of-line.
// Keeping it external avoids header changes and makes moving to a new .cpp trivial.
void RigAnimator_DebugDumpBindTriple_Impl(RigAnimator* self, const std::string& boneNameNeedle)
{
  using namespace rig_anim_debug;

  if (!self) return;

  // Find bone by substring (so you can pass "LeftHandMiddle1")
  Bone* target = nullptr;
  for (auto& b : self->m_bones)
    if (b.GetName().find(boneNameNeedle) != std::string::npos) { target = &b; break; }

  if (!target) {
    RIG_PRINTF("[BindTriple] Bone containing '%s' not found\n", boneNameNeedle.c_str());
    return;
  }

  Bone* root = nullptr;
  for (auto& b : self->m_bones)
    if (b.GetName() == self->m_nameRootBone) { root = &b; break; }

  if (!root) {
    RIG_PRINTF("[BindTriple] root '%s' not found\n", self->m_nameRootBone.c_str());
    return;
  }

  // Build parent map
  std::unordered_map<std::string, Bone*> parentOf;
  parentOf.reserve(self->m_bones.size());

  std::function<void(Bone*)> buildParents = [&](Bone* p)
    {
      for (Bone* c : p->GetChildren())
      {
        if (!c) continue;
        parentOf[c->GetName()] = p;
        buildParents(c);
      }
    };
  buildParents(root);

  // Build chain root -> target
  std::vector<Bone*> chain;
  for (Bone* cur = target; cur; )
  {
    chain.push_back(cur);
    auto it = parentOf.find(cur->GetName());
    cur = (it == parentOf.end() ? nullptr : it->second);
  }
  std::reverse(chain.begin(), chain.end());

  glm::mat4 accumScene(1.f);
  glm::mat4 I(1.f);

  RIG_PRINTF("\n=== BindTriple '%s' (chain length=%zu) ===\n",
    target->GetName().c_str(), chain.size());
  RIG_PRINTF("m_globalModelTransform (rootInv?)\n");
  PrintTRSLine("rootInv", self->m_globalModelTransform);

  for (Bone* b : chain)
  {
    glm::mat4 L = b->GetLocalBindTransform();
    glm::mat4 Gscene = accumScene * L;
    glm::mat4 Gmodel = self->m_globalModelTransform * Gscene;
    glm::mat4 O = b->GetInverseBindTransform();
    glm::mat4 P = Gmodel * O;

    RIG_PRINTF("\n[Bone] %s  id=%zu explicit=%d\n",
      b->GetName().c_str(), b->GetID(), (int)b->HasExplicitInverseBind());
    PrintTRSLine("Lbind", L);
    PrintTRSLine("Gscene", Gscene);
    PrintTRSLine("Gmodel", Gmodel);
    PrintTRSLine("Offset", O);
    PrintTRSLine("G*Off", P);
    RIG_PRINTF("err(G*Off vs I)=%.6g\n", MaxAbsDiff(P, I));

    glm::mat4 ExpectedGmodel = glm::inverse(O);
    glm::mat4 DeltaG = glm::inverse(ExpectedGmodel) * Gmodel;

    PrintTRSLine("ExpGmdl", ExpectedGmodel);
    PrintTRSLine("DeltaG", DeltaG);
    RIG_PRINTF("err(Gmodel vs ExpGmdl)=%.6g\n", MaxAbsDiff(Gmodel, ExpectedGmodel));
    RIG_PRINTF("err(DeltaG vs I)     =%.6g\n", MaxAbsDiff(DeltaG, glm::mat4(1.f)));

    accumScene = Gscene;
  }

  RIG_PRINTF("\n=== End BindTriple ===\n");
}

#endif // RIGANIMATOR_ENABLE_DEBUG
