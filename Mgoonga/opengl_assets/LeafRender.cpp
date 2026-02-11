#include "stdafx.h"
#include "LeafRender.h"
#include "GlPipelineState.h"

std::vector<glm::mat4> ExtractQuadTransforms(const I3DMesh* mesh);

//-------------------------------------------------------------------------
eLeafRender::eLeafRender(const std::string& _vS, const std::string& _fS)
{
  m_shader.installShaders(_vS.c_str(), _fS.c_str());
  m_shader.GetUniformInfoFromShader();
}

//----------------------------------------------------------------------------
void eLeafRender::Render(const Camera& _camera, std::vector<shObject> _objects)
{
  glUseProgram(m_shader.ID());
  eGlPipelineState::GetInstance().EnableBlend();
  glm::mat4 worldToProjectionMatrix = _camera.getProjectionMatrix() * _camera.getWorldToViewMatrix();
  for (auto& object : _objects)
  {
    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    if (object->IsBackfaceCull())
      eGlPipelineState::GetInstance().EnableCullFace();
    else
      eGlPipelineState::GetInstance().DisableCullFace();

    glm::mat4 modelToProjectionMatrix = worldToProjectionMatrix * object->GetTransform()->getModelMatrix();
    m_shader.SetUniformData("MVP", modelToProjectionMatrix);
    static float time = 0;
    time += 0.01f;
    m_shader.SetUniformData("time", time);
    if (auto it = m_meshes.find(object->Name()); it == m_meshes.end())
    {
      m_meshes[object->Name()] = std::make_unique<LeafMesh>(ExtractQuadTransforms(const_cast<I3DMesh*>(object->GetModel()->Get3DMeshes()[0])),
                                                            object->GetModel()->Get3DMeshes()[0]->GetMaterial().value());
    }
    m_meshes[object->Name()]->Draw();
  }
  eGlPipelineState::GetInstance().DisableBlend();
  glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
}

//----------------------------------------------------------------------
std::vector<glm::mat4> ExtractQuadTransforms(const I3DMesh* mesh)
{
  std::vector<glm::mat4> transforms;

  const std::vector<Vertex>& verts = mesh->GetVertexs();
  const std::vector<unsigned int>& inds = mesh->GetIndices();

  for (size_t i = 0; i + 5 < inds.size(); i += 6)
  {
    // Get the four unique indices that form a quad (0,1,2,0,2,3 -> quad = 0,1,2,3)
    glm::vec3 p0 = verts[inds[i + 0]].Position;
    glm::vec3 p1 = verts[inds[i + 1]].Position;
    glm::vec3 p2 = verts[inds[i + 2]].Position;
    glm::vec3 p3 = verts[inds[i + 5]].Position; // usually the missing one

    // Compute center of the quad
    glm::vec3 center = (p0 + p1 + p2 + p3) * 0.25f;

    // Basis vectors
    glm::vec3 xAxis = glm::normalize(p1 - p0);      // Right
    glm::vec3 yApprox = glm::normalize(p3 - p0);     // Temporary Up
    glm::vec3 zAxis = glm::normalize(glm::cross(xAxis, yApprox)); // Normal
    glm::vec3 yAxis = glm::normalize(glm::cross(zAxis, xAxis));   // Corrected Up

    // Dimensions (projected onto axes for accuracy)
    float width = glm::dot(p1 - p0, xAxis);
    float height = glm::dot(p3 - p0, yAxis);

    // Build model matrix
    glm::mat4 transform(1.0f);
    transform[0] = glm::vec4(xAxis * width, 0.0f);
    transform[1] = glm::vec4(yAxis * height, 0.0f);
    transform[2] = glm::vec4(zAxis, 0.0f);
    transform[3] = glm::vec4(center, 1.0f);

    transforms.push_back(transform);
  }
  return transforms;
}
