#include "stdafx.h"
#include "CSMRender.h"
#include "GlBufferContext.h"

#include <map>

#include <math/Transform.h>

void CalculateLogarithmicSplits(float nearClip, float farClip, int cascadeCount, std::vector<float>& cascadeSplits);
void CalculateLogarithmicUniformSplits(float nearClip, float farClip, int cascadeCount, std::vector<float>& cascadeSplits, float lambda = 0.5f);

//---------------------------------------------------------------------------------------------------
eCSMRender::eCSMRender(const std::string& vS, const std::string& fS, const std::string& gS)
{
  csmShader.installShaders(vS.c_str(), fS.c_str()/*, gS.c_str()*/); // no gs option
  glUseProgram(csmShader.ID());

  ModelLocationDir = glGetUniformLocation(csmShader.ID(), "model");
  BonesMatLocationDir = glGetUniformLocation(csmShader.ID(), "gBones");

  m_shadowCascadeLevels = { 2.5f, 7.0f, 13.0f, 20.0f };
  //CalculateLogarithmicSplits(0.01f, 40.f, 4, m_shadowCascadeLevels); //@todo parameters should be from camera
  //CalculateLogarithmicUniformSplits(0.01f, 20.f, 4, m_shadowCascadeLevels, 0.5f);
  
  // configure UBO
// --------------------
  glGenBuffers(1, &matricesUBO);
  glBindBuffer(GL_UNIFORM_BUFFER, matricesUBO);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4x4) * 16 * 2, nullptr, GL_STATIC_DRAW);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, matricesUBO);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

//---------------------------------------------------------------------------------------------------
eCSMRender::~eCSMRender()
{
}

//---------------------------------------------------------------------------------------------------
void eCSMRender::Render(const Camera& _camera, const Light& _light, const std::vector<shObject>& _objects)
{
  glUseProgram(csmShader.ID());

  _updatetLightSpaceMatrices(_camera, _light);
  glBindBuffer(GL_UNIFORM_BUFFER, matricesUBO);
  for (size_t i = 0; i < m_viewProjectionMatrices.size(); ++i)
  {
    glBufferSubData(GL_UNIFORM_BUFFER, i * sizeof(glm::mat4x4), sizeof(glm::mat4x4), &m_viewProjectionMatrices[i]);
    glBufferSubData(GL_UNIFORM_BUFFER, (i + m_viewProjectionMatrices.size()) * sizeof(glm::mat4x4), sizeof(glm::mat4x4), &m_projectionMatrices[i]);
  }
  glBindBuffer(GL_UNIFORM_BUFFER, 0);

  eGlBufferContext::GetInstance().BindSSBO(eSSBO::MODEL_TO_WORLD_MATRIX);

  // Render each cascade into its own layer
  glDrawBuffer(GL_NONE); // ?
  glReadBuffer(GL_NONE); // ?

  const GLuint depthArrayTex = eGlBufferContext::GetInstance().GetTexture(eBuffer::BUFFER_SHADOW_CSM).m_id;
  const int numCascades = static_cast<int>(m_shadowCascadeLevels.size()) + 1;

  //RENDER DEPTH
  for (int c = 0; c < numCascades; ++c)
  {
    // Attach layer c of the array as the depth attachment
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthArrayTex, 0, c);
    
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      // Log and bail this cascade; otherwise any clear/draw will error
      // printf("FBO incomplete on cascade %d: 0x%x\n", c, status);
      continue;
    }
    
    glClear(GL_DEPTH_BUFFER_BIT);
    csmShader.SetUniformData("cascadeIndex", c);

    std::map<std::string, std::vector<shObject>> instanced;
    // 1) Non-instanced
    for (auto& object : _objects)
    {
      if (object->GetInstancingTag().empty())
      {
        csmShader.SetUniformData("isInstanced", false);
        csmShader.SetUniformData("instanceIndex", 0);

        /*const glm::mat4 model = object->GetTransform()->getModelMatrix();
        glUniformMatrix4fv(ModelLocationDir, 1, GL_FALSE, &model[0][0]);*/

        eGlBufferContext::GetInstance().UploadSSBOData(eSSBO::MODEL_TO_WORLD_MATRIX, { object->GetTransform()->getModelMatrix() });

        if (object->GetRigger() != nullptr)
          bone_matrices = object->GetRigger()->GetMatrices();
        else
          for (auto& m : bone_matrices) m = UNIT_MATRIX;

        glUniformMatrix4fv(BonesMatLocationDir, MAX_BONES, GL_FALSE,
          &bone_matrices[0][0][0]);

        object->GetModel()->Draw(); // non-instanced
      }
      else
        instanced[object->GetInstancingTag()].push_back(object);
    }

    // 2) Instanced
    for (auto& node : instanced)
    {
      csmShader.SetUniformData("isInstanced", true);

      std::vector<glm::mat4> modelToWorld;
      modelToWorld.reserve(node.second.size());
      for (auto& obj : node.second)
        modelToWorld.push_back(obj->GetTransform()->getModelMatrix());

      // Upload instance transforms (SSBO). Consider orphaning/persistent mapping later.
      eGlBufferContext::GetInstance().UploadSSBOData(eSSBO::MODEL_TO_WORLD_MATRIX, modelToWorld);

      // Bones: single set for all (depth-only). If needed per-object, split batches.
      for (auto& m : bone_matrices) m = UNIT_MATRIX;
      glUniformMatrix4fv(BonesMatLocationDir, MAX_BONES, GL_FALSE, &bone_matrices[0][0][0]);

      const GLsizei instanceCount = static_cast<GLsizei>(node.second.size());
      node.second[0]->GetModel()->DrawInstanced(csmShader.ID(), instanceCount);
    }
  }
}

//---------------------------------------------------------------------------------------------------
std::vector<float> eCSMRender::GetCascadePlaneDistances() const
{
  return m_shadowCascadeLevels;
}

//---------------------------------------------------------------------------------------------------
void eCSMRender::_updatetLightSpaceMatrices(const Camera& _camera, const Light& _light)
{
  const size_t cascadeCount = m_shadowCascadeLevels.size();
  const float nearPlane = _camera.getNearPlane();
  const float farPlane = _camera.getFarPlane();

  float prevSplit = nearPlane;
  float overlapRatio = 1.01f;

  for (size_t i = 0; i <= cascadeCount; ++i)
  {
    float currSplit = (i < cascadeCount) ? m_shadowCascadeLevels[i] : farPlane;

    // Slightly extend the cascade range to avoid gaps
    float extendedNear = prevSplit / overlapRatio;
    float extendedFar = currSplit * overlapRatio;

    _updateLightSpaceMatrix(_camera, _light, extendedNear, extendedFar, i);

    prevSplit = currSplit;
  }
}

//---------------------------------------------------------------------------------------------------
void eCSMRender::_updateLightSpaceMatrix(const Camera& _camera, const Light& _light, float _nearPlane, float _farPlane, size_t _index)
{
  Camera cascadeCamera = _camera;
  cascadeCamera.setNearPlane(_nearPlane);
  cascadeCamera.setFarPlane(_farPlane);
  const auto frustumCornersWS = cascadeCamera.getFrustumCornersWorldSpace();

  glm::vec3 center(0.0f);
  for (const auto& corner : frustumCornersWS)
    center += glm::vec3(corner);
  center /= static_cast<float>(frustumCornersWS.size());

  glm::vec3 lightDir = glm::normalize(glm::vec3(_light.light_direction));
  glm::vec3 eye = center - lightDir * m_light_placement_distance_coef;
  glm::mat4 lightView = glm::lookAt(eye, center, glm::vec3(0.0f, 1.0f, 0.0f));

  float minX = std::numeric_limits<float>::max();
  float maxX = std::numeric_limits<float>::lowest();
  float minY = minX, maxY = maxX;
  float minZ = minX, maxZ = maxX;

  for (const auto& corner : frustumCornersWS)
  {
    glm::vec4 transformed = lightView * corner;
    minX = std::min(minX, transformed.x);
    maxX = std::max(maxX, transformed.x);
    minY = std::min(minY, transformed.y);
    maxY = std::max(maxY, transformed.y);
    minZ = std::min(minZ, transformed.z);
    maxZ = std::max(maxZ, transformed.z);
  }

  // Slightly expand Z to prevent clipping
  float zMultMin = (minZ < 0) ? zMult : 1.0f / zMult;
  float zMultMax = (maxZ < 0) ? 1.0f / zMult : zMult;

  minZ *= zMultMin;
  maxZ *= zMultMax;

  //@todo test this code instead of zMult
  // 4. Additive Z padding instead of zMult
  //float zPaddingBase = 10.0f;
  //float cascadeLerp = float(_index) / float(m_shadowCascadeLevels.size());
  //float zPadding = glm::mix(zPaddingBase, zPaddingBase * 2.0f, cascadeLerp); // more padding in distant cascades
  //minZ -= zPadding;
  //maxZ += zPadding;

  // Snap projection bounds to texel size
  constexpr float SHADOW_MAP_RESOLUTION = 2048.0f; // Use your actual shadow map resolution
  float worldUnitsPerTexelX = (maxX - minX) / SHADOW_MAP_RESOLUTION;
  float worldUnitsPerTexelY = (maxY - minY) / SHADOW_MAP_RESOLUTION;

  minX = std::floor(minX / worldUnitsPerTexelX) * worldUnitsPerTexelX;
  maxX = std::floor(maxX / worldUnitsPerTexelX) * worldUnitsPerTexelX;
  minY = std::floor(minY / worldUnitsPerTexelY) * worldUnitsPerTexelY;
  maxY = std::floor(maxY / worldUnitsPerTexelY) * worldUnitsPerTexelY;

  glm::mat4 lightProj = glm::ortho(minX, maxX, minY, maxY, -maxZ, -minZ);

  m_projectionMatrices[_index] = lightProj;
  m_viewProjectionMatrices[_index] = lightProj * lightView;
}

//----------------------------------------------------------------------------------------------------------
void CalculateLogarithmicSplits(float nearClip, float farClip, int cascadeCount, std::vector<float>& cascadeSplits)
{
  cascadeSplits.resize(cascadeCount);
  float clipRange = farClip - nearClip;
  float logNear = std::log(nearClip + 1.0f); // Add 1 to avoid log(0)
  float logFar = std::log(farClip + 1.0f);
  float logRange = logFar - logNear;

  for (int i = 0; i < cascadeCount; ++i) {
    float p = (i + 1) / static_cast<float>(cascadeCount);
    float logZ = logNear + p * logRange;
    cascadeSplits[i] = std::exp(logZ) - 1.0f; // Subtract 1 to match original range
  }
}

//----------------------------------------------------------------------------------------------------------
void CalculateLogarithmicUniformSplits(float nearClip, float farClip, int cascadeCount, std::vector<float>& cascadeSplits, float lambda)
{
  cascadeSplits.resize(cascadeCount);
  float clipRange = farClip - nearClip;

  for (int i = 0; i < cascadeCount; ++i)
  {
    float p = (i + 1) / static_cast<float>(cascadeCount);
    float logSplit = nearClip * std::pow(farClip / nearClip, p);
    float uniformSplit = nearClip + clipRange * p;
    cascadeSplits[i] = lambda * logSplit + (1.0f - lambda) * uniformSplit;
  }
}
