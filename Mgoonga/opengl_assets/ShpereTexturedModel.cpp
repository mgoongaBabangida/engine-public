#include "stdafx.h"
#include "ShpereTexturedModel.h"

#include <opengl_assets\Texture.h>
#include "GlDrawContext.h"
#include "MyMesh.h"

//---------------------------------------------------------------------
SphereTexturedMesh::SphereTexturedMesh()
{
  glGenVertexArrays(1, &sphereVAO);

  glGenBuffers(1, &vbo);
  glGenBuffers(1, &ebo);

  const unsigned int X_SEGMENTS = 64;
  const unsigned int Y_SEGMENTS = 64;
  const float PI = 3.14159265359f;
  for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
  {
    for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
    {
      float xSegment = (float)x / (float)X_SEGMENTS;
      float ySegment = (float)y / (float)Y_SEGMENTS;
      float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
      float yPos = std::cos(ySegment * PI);
      float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

      Vertex vert;
      vert.Position = (glm::vec3(xPos, yPos, zPos));
      vert.TexCoords = (glm::vec2(xSegment, ySegment));
      vert.Normal = glm::normalize((glm::vec3(xPos, yPos, zPos)));
      vertices.push_back(vert);
    }
  }

  bool oddRow = false;
  for (unsigned int y = 0; y < Y_SEGMENTS; ++y)
  {
    if (!oddRow) // even rows: y == 0, y == 2; and so on
    {
      for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
      {
        indices.push_back(y * (X_SEGMENTS + 1) + x);
        indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
      }
    }
    else
    {
      for (int x = X_SEGMENTS; x >= 0; --x)
      {
        indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
        indices.push_back(y * (X_SEGMENTS + 1) + x);
      }
    }
    oddRow = !oddRow;
  }

  for (uint32_t i = 1; i < indices.size()-1; ++i)
  {
    glm::vec3 pos1 = vertices[indices[i - 1]].Position;
    glm::vec3 pos2 = vertices[indices[i]].Position;
    glm::vec3 pos3 = vertices[indices[i + 1]].Position;
    glm::vec2 uv1 = vertices[indices[i - 1]].TexCoords;
    glm::vec2 uv2 = vertices[indices[i]].TexCoords;
    glm::vec2 uv3 = vertices[indices[i + 1]].TexCoords;
    // calculate tangent/bitangent vectors of both triangles
    glm::vec3 tangent1, bitangent1;
    // - triangle 1
    glm::vec3 edge1 = pos2 - pos1;
    glm::vec3 edge2 = pos3 - pos1;
    glm::vec2 deltaUV1 = uv2 - uv1;
    glm::vec2 deltaUV2 = uv3 - uv1;

    GLfloat f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

    tangent1.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
    tangent1.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
    tangent1.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
    tangent1 = glm::normalize(tangent1);

    bitangent1.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
    bitangent1.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
    bitangent1.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
    bitangent1 = glm::normalize(bitangent1);

    vertices[indices[i]].tangent = glm::vec4(tangent1, 1.);
    vertices[indices[i]].bitangent = glm::vec4(bitangent1, 1.);
  }

  indexCount = static_cast<unsigned int>(indices.size());

  glBindVertexArray(this->sphereVAO);
  glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
  glBufferData(GL_ARRAY_BUFFER, this->vertices.size() * sizeof(Vertex),
    &this->vertices[0], GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, this->indices.size() * sizeof(GLuint),
    &this->indices[0], GL_STATIC_DRAW);

  // Vertex Positions
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
    (GLvoid*)0);
  // Vertex Normals
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
    (GLvoid*)offsetof(Vertex, Normal));
  // Vertex Texture Coords
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
    (GLvoid*)offsetof(Vertex, TexCoords));
  // Vertex Tangent
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
    (GLvoid*)offsetof(Vertex, tangent));
  // Vertex Bitangent
  glEnableVertexAttribArray(5);
  glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
    (GLvoid*)offsetof(Vertex, bitangent));
  // Vertex BoneIDs
  glEnableVertexAttribArray(6);
  glVertexAttribIPointer(6, 4, GL_INT, sizeof(Vertex),
    (GLvoid*)offsetof(Vertex, boneIDs));
  // Vertex Weights
  glEnableVertexAttribArray(7);
  glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
    (GLvoid*)offsetof(Vertex, weights));
  glBindVertexArray(0);
}

//----------------------------------------------------------------------------
SphereTexturedMesh::~SphereTexturedMesh()
{
  glDeleteVertexArrays(1, &sphereVAO);
  glDeleteBuffers(1, &vbo);
  glDeleteBuffers(1, &ebo);
}

//----------------------------------------------------------------------------
void SphereTexturedMesh::Draw()
{
  if (m_material.textures.find(Material::TextureType::ALBEDO) != m_material.textures.end())
  {
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_material.textures[Material::TextureType::ALBEDO]);
  }

  if (m_material.textures.find(Material::TextureType::METALLIC) != m_material.textures.end())
  {
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_material.textures[Material::TextureType::METALLIC]);
  }

  if (m_material.textures.find(Material::TextureType::NORMAL) != m_material.textures.end())
  {
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, m_material.textures[Material::TextureType::NORMAL]);
  }

  if (m_material.textures.find(Material::TextureType::ROUGHNESS) != m_material.textures.end())
  {
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, m_material.textures[Material::TextureType::ROUGHNESS]);
  }
  if (m_material.textures.find(Material::TextureType::EMISSIVE) != m_material.textures.end())
  {
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, m_material.textures[Material::TextureType::EMISSIVE]);
  }
  if (m_material.textures.find(Material::TextureType::AO) != m_material.textures.end())
  {
    glActiveTexture(GL_TEXTURE16);
    glBindTexture(GL_TEXTURE_2D, m_material.textures[Material::TextureType::AO]);
  }
  if (m_material.textures.find(Material::TextureType::OPACITY) != m_material.textures.end())
  {
    glActiveTexture(GL_TEXTURE18);
    glBindTexture(GL_TEXTURE_2D, m_material.textures[Material::TextureType::OPACITY]);
  }

  if (sphereVAO != -1)
  {
    glBindVertexArray(sphereVAO);
    eGlDrawContext::GetInstance().DrawElements(GL_TRIANGLE_STRIP, indexCount, GL_UNSIGNED_INT, 0, "SphereTexturedMesh");
    glBindVertexArray(0);
  }
}

//----------------------------------------------------------------------------
void SphereTexturedMesh::DrawInstanced(int32_t _instances)
{
  if (m_material.textures.find(Material::TextureType::ALBEDO) != m_material.textures.end())
  {
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_material.textures[Material::TextureType::ALBEDO]);
  }

  if (m_material.textures.find(Material::TextureType::METALLIC) != m_material.textures.end())
  {
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_material.textures[Material::TextureType::METALLIC]);
  }

  if (m_material.textures.find(Material::TextureType::NORMAL) != m_material.textures.end())
  {
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, m_material.textures[Material::TextureType::NORMAL]);
  }

  if (m_material.textures.find(Material::TextureType::ROUGHNESS) != m_material.textures.end())
  {
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, m_material.textures[Material::TextureType::ROUGHNESS]);
  }
  if (m_material.textures.find(Material::TextureType::EMISSIVE) != m_material.textures.end())
  {
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, m_material.textures[Material::TextureType::EMISSIVE]);
  }
  if (m_material.textures.find(Material::TextureType::AO) != m_material.textures.end())
  {
    glActiveTexture(GL_TEXTURE16);
    glBindTexture(GL_TEXTURE_2D, m_material.textures[Material::TextureType::AO]);
  }
  if (m_material.textures.find(Material::TextureType::OPACITY) != m_material.textures.end())
  {
    glActiveTexture(GL_TEXTURE18);
    glBindTexture(GL_TEXTURE_2D, m_material.textures[Material::TextureType::OPACITY]);
  }

  // Draw mesh
  if (sphereVAO != -1)
  {
    glBindVertexArray(sphereVAO);
    eGlDrawContext::GetInstance().DrawElementsInstanced(
      GL_TRIANGLES, (GLsizei)indexCount, GL_UNSIGNED_INT, (GLvoid*)(0), _instances, "SphereTexturedMesh");
    glBindVertexArray(0);
  }
}

//----------------------------------------------------------------------------
void SphereTexturedMesh::BindVAO() const
{
  if(sphereVAO != -1)
    glBindVertexArray(this->sphereVAO);
}

//----------------------------------------------------------------------------
void SphereTexturedMesh::UnbindVAO() const
{
   glBindVertexArray(0);
}

//----------------------------------------------------------------------------
void SphereTexturedMesh::SetMaterial(const Material& _m)
{
  m_material = _m;
}

//----------------------------------------------------------------------------
std::optional<Material> SphereTexturedMesh::GetMaterial() const
{
  return  m_material;
}

//-------------------------------------------------------------------
void SphereTexturedMesh::calculatedTangent()
{
  for (uint32_t i = 1; i < indices.size() - 1; ++i)
  {
    glm::vec3 pos1 = vertices[indices[i - 1]].Position;
    glm::vec3 pos2 = vertices[indices[i]].Position;
    glm::vec3 pos3 = vertices[indices[i + 1]].Position;
    glm::vec2 uv1 = vertices[indices[i - 1]].TexCoords;
    glm::vec2 uv2 = vertices[indices[i]].TexCoords;
    glm::vec2 uv3 = vertices[indices[i + 1]].TexCoords;
    // calculate tangent/bitangent vectors of both triangles
    glm::vec3 tangent1, bitangent1;
    // - triangle 1
    glm::vec3 edge1 = pos2 - pos1;
    glm::vec3 edge2 = pos3 - pos1;
    glm::vec2 deltaUV1 = uv2 - uv1;
    glm::vec2 deltaUV2 = uv3 - uv1;

    GLfloat f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

    tangent1.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
    tangent1.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
    tangent1.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
    tangent1 = glm::normalize(tangent1);

    bitangent1.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
    bitangent1.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
    bitangent1.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
    bitangent1 = glm::normalize(bitangent1);

    vertices[indices[i]].tangent = glm::vec4(tangent1, 1.);
    vertices[indices[i]].bitangent = bitangent1;
  }
}

//-------------------------------------------------------------------
void SphereTexturedMesh::ReloadVertexBuffer()
{
  glGenVertexArrays(1, &sphereVAO);

  glGenBuffers(1, &vbo);
  glGenBuffers(1, &ebo);

  indexCount = static_cast<unsigned int>(indices.size());

  glBindVertexArray(this->sphereVAO);
  glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
  glBufferData(GL_ARRAY_BUFFER, this->vertices.size() * sizeof(Vertex),
    &this->vertices[0], GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, this->indices.size() * sizeof(GLuint),
    &this->indices[0], GL_STATIC_DRAW);

  // Vertex Positions
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
    (GLvoid*)0);
  // Vertex Color
    glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
    (GLvoid*)offsetof(Vertex, Color));
  // Vertex Normals
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
    (GLvoid*)offsetof(Vertex, Normal));
  // Vertex Texture Coords
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
    (GLvoid*)offsetof(Vertex, TexCoords));
  // Vertex Tangent
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
    (GLvoid*)offsetof(Vertex, tangent));
  // Vertex Bitangent
  glEnableVertexAttribArray(5);
  glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
    (GLvoid*)offsetof(Vertex, bitangent));
  // Vertex BoneIDs
  glEnableVertexAttribArray(6);
  glVertexAttribIPointer(6, 4, GL_INT, sizeof(Vertex),
    (GLvoid*)offsetof(Vertex, boneIDs));
  // Vertex Weights
  glEnableVertexAttribArray(7);
  glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
    (GLvoid*)offsetof(Vertex, weights));
  glBindVertexArray(0);
}

//-------------------------------------------------------------------
void SphereTexturedModel::Draw(int32_t _program)
{
  m_mesh->Draw();
}

//-------------------------------------------------------------------
void SphereTexturedModel::DrawInstanced(int32_t _program, int32_t _instances)
{
  m_mesh->DrawInstanced(_instances);
}

//-------------------------------------------------------------------
void SphereTexturedModel::SetUpMeshes()
{
  m_mesh->ReloadVertexBuffer();
  m_mesh->calculatedTangent();
}

