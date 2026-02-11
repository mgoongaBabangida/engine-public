#include "Bone.h"

//---------------------------------------------------------------------------------
Bone::Bone(size_t index, std::string name, glm::mat4 localBindTransform, bool real)
  : m_index(index)
  , m_name(std::move(name))
  , m_realBone(real)
  , m_has_explicit_inverse_bind(false)
  , m_animatedTransform(1.0f)
  , m_globaltransform(1.0f)
  , m_localBindTransform(localBindTransform)
  , m_inverseBindTransform(1.0f)
  , m_mTransform(localBindTransform)
{}

//----------------------------------------------------------------------------
Bone::Bone()
  : m_index(size_t(-1))
  , m_realBone(true)
  , m_has_explicit_inverse_bind(false)
  , m_animatedTransform(1.0f)
  , m_globaltransform(1.0f)
  , m_localBindTransform(1.0f)
  , m_inverseBindTransform(1.0f)
  , m_mTransform(1.0f)
{
}

//----------------------------------------------------------------------------
std::vector<const IBone*> Bone::GetChildren() const
{
	std::vector<const IBone*> ret_children;
	for (auto& child : m_children)
		ret_children.push_back(child);

	return ret_children;
}

static inline bool IsIdentityApprox(const glm::mat4& M, float eps = 1e-6f)
{
  const glm::mat4 I(1.0f);
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c)
      if (std::fabs(M[r][c] - I[r][c]) > eps) return false;
  return true;
}

void Bone::CalculateInverseBindTransform(const glm::mat4& parentGlobalBindScene,
  const glm::mat4& rootScene)
{
  // global bind in SCENE space from node locals
  const glm::mat4 globalBindScene = parentGlobalBindScene * m_localBindTransform;

  if (!m_has_explicit_inverse_bind)
  {
    // Store an OFFSET (like aiBone::mOffsetMatrix) in mesh space:
    // O = inverse(G_bind_scene) * G_root_scene
    m_inverseBindTransform = glm::inverse(globalBindScene) * rootScene;
  }
  // else: explicit m_inverseBindTransform already holds aiBone::mOffsetMatrix (keep it)

  for (auto* child : m_children)
    child->CalculateInverseBindTransform(globalBindScene, rootScene);
}


