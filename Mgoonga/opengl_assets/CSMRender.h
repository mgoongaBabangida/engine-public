#pragma once

#include "Shader.h"
#include <math/Camera.h>
#include <base/Object.h>
#include <base/base.h>

//---------------------------------------------------------------
class eCSMRender
{
public:
  eCSMRender(const std::string& vS, const std::string& fS, const std::string& gS);
  ~eCSMRender();

  void Render(const Camera& camera,
              const Light& light,
              const std::vector<shObject>& objects);

  Shader& GetShader() { return csmShader; }

  float& ZMult() { return zMult; }
  float& GetFirstCascadePlaneDistance() { return m_shadowCascadeLevels[0]; }
  float& GetLightPlacementCoef() { return m_light_placement_distance_coef; }

  std::vector<float> GetCascadePlaneDistances() const;
protected:
  void _updatetLightSpaceMatrices(const Camera& camera, const Light& light);
  void _updateLightSpaceMatrix(const Camera& camera, const Light& light, float _cameraNearPlane, float _cameraFarPlane, size_t _index);

  Shader csmShader;
  std::array<glm::mat4, MAX_BONES> bone_matrices;

  std::vector<float> m_shadowCascadeLevels;

  unsigned int matricesUBO;

  std::array<glm::mat4, 16> m_viewProjectionMatrices;
  std::array<glm::mat4, 16> m_projectionMatrices;

  GLuint			ModelLocationDir;
  GLuint			BonesMatLocationDir;
  float       zMult = 1.2f;
  float       m_light_placement_distance_coef = 1.0f; //@todo twick
};
