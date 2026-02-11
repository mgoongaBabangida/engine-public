#include "stdafx.h"

#include "LeafMesh.h"
#include "GlDrawContext.h"

#include <algorithm>

//-----------------------------------------------------------
LeafMesh::LeafMesh(std::vector<glm::mat4>&& _transforms, const Material& _material)
  : m_transforms(_transforms), m_material(_material)
{
  // make vertices
  for (unsigned int i = 0; i < 4; ++i)
    m_vertices.push_back(Vertex{});

  m_vertices[0].Position = glm::vec3{ -0.5f, -0.5f, 0.0f };
  m_vertices[1].Position = glm::vec3{ 0.5f, -0.5f, 0.0f };
  m_vertices[2].Position = glm::vec3{ 0.5f,  0.5f, 0.0f };
  m_vertices[3].Position = glm::vec3{ -0.5f,  0.5f, 0.0f };

  //make indices
  m_indices = {
    0, 1, 2,
    0, 2, 3
  };

  setupMesh();
}

//---------------------------------------------------
LeafMesh::~LeafMesh()
{
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteBuffers(1, &EBO);
  glDeleteBuffers(1, &VBOinstanced);
}

//---------------------------------------------------
void LeafMesh::Draw()
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

  if (VAO != -1)
  {
    glBindVertexArray(VAO);
    eGlDrawContext::GetInstance().DrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, m_transforms.size(), "Leaf");
    glBindVertexArray(0);
  }
}

//-----------------------------------------------------
std::vector<TextureInfo> LeafMesh::GetTextures() const
{
  return std::vector<TextureInfo>();
}

//----------------------------------------------------
void LeafMesh::calculatedTangent()
{
}

//-----------------------------------------------------------
void LeafMesh::setupMesh()
{
  glGenVertexArrays(1, &this->VAO);
  glGenBuffers(1, &this->VBO);
  glGenBuffers(1, &this->EBO);

  glBindVertexArray(this->VAO);
  glBindBuffer(GL_ARRAY_BUFFER, this->VBO);

  glBufferData(GL_ARRAY_BUFFER, this->m_vertices.size() * sizeof(Vertex),
    &this->m_vertices[0], GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, this->m_indices.size() * sizeof(GLuint),
    &this->m_indices[0], GL_STATIC_DRAW);

  // Vertex Positions
    glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
    (GLvoid*)0);

  //Instancing
  glGenBuffers(1, &this->VBOinstanced);
  glBindBuffer(GL_ARRAY_BUFFER, VBOinstanced);
  glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4) * m_transforms.size(), m_transforms.data(), GL_STATIC_DRAW);

  std::size_t vec4Size = sizeof(glm::vec4);
  for (int i = 0; i < 4; ++i)
  {
    glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * vec4Size));
    glEnableVertexAttribArray(1 + i);
    glVertexAttribDivisor(1 + i, 1); // Tell OpenGL this attribute advances once per instance
  }

  glBindVertexArray(0);
}
