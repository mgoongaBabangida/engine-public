#include "stdafx.h"

#include "Mesh.h"
#include "GlDrawContext.h"
#include "Windows_Related_api.h"
#include "mesh_system_utils.h"

#include <math/Utils.h>

#include <sstream>

using namespace std;

I3DMesh* MakeMesh(std::vector<Vertex> _vertices,
                  std::vector<GLuint> _indices,
                  std::vector<TextureInfo> _textures,
                  const Material& _material,
                  const std::string& _name ,
                  bool _calculate_tangent)
{
  std::vector<Texture> textures;
  for (auto t : _textures)
    textures.emplace_back(t);

  return new eMesh(_vertices, _indices, textures, _material, _name, _calculate_tangent);
}

//-------------------------------------------------------------------------------------------
eMesh::eMesh(vector<Vertex> _vertices,
             vector<GLuint> _indices,
             vector<Texture> _textures,
             const Material& _material,
             const std::string& _name,
             bool _calculate_tangent,
             bool _reload_textures)
{
	m_vertices = _vertices;
	m_indices = _indices;
	m_textures = _textures;
  m_material = _material;
  m_name = _name.empty() ? "Default" : _name;

  {
    uint32_t maxIdx = 0;
    for (auto i : m_indices) maxIdx = std::max(maxIdx, (uint32_t)i);
    assert(maxIdx < m_vertices.size() && "Index out of range!");
    assert(sizeof(GLuint) == 4 && "Expect 32-bit GLuint");
  }

  if (HasCurrentOpenGLContext())
  {
    if (VAO == -1 || VBO == -1 || EBO == -1)
      SetupMesh();
    if(_reload_textures)
      ReloadTextures();
  }

  if(_calculate_tangent)
    this->calculatedTangent();

  if (!m_material.textures.contains(Material::TextureType::ALBEDO))
    m_material.textures[Material::TextureType::ALBEDO] = Texture::GetTexture1x1(GREY).m_id;
  if (!m_material.textures.contains(Material::TextureType::METALLIC))
    m_material.textures[Material::TextureType::METALLIC] = Texture::GetTexture1x1(BLACK).m_id;
  if (!m_material.textures.contains(Material::TextureType::NORMAL))
    m_material.textures[Material::TextureType::NORMAL] = Texture::GetTexture1x1(BLUE).m_id;
  if (!m_material.textures.contains(Material::TextureType::ROUGHNESS))
    m_material.textures[Material::TextureType::ROUGHNESS] = Texture::GetTexture1x1(WHITE).m_id;
  if (!m_material.textures.contains(Material::TextureType::EMISSIVE))
    m_material.textures[Material::TextureType::EMISSIVE] = Texture::GetTexture1x1(BLACK).m_id;
  if (!m_material.textures.contains(Material::TextureType::AO))
    m_material.textures[Material::TextureType::AO] = Texture::GetTexture1x1(WHITE).m_id;
}

//-------------------------------------------------------------------------------------------
eMesh::~eMesh()
{
  if (HasCurrentOpenGLContext())
  {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
  }
}

//-------------------------------------------------------------------------------------------
void eMesh::ReloadTextures()
{
  for (Texture& t : m_textures)
  {
    if(t.m_id == Texture::GetDefaultTextureId())
      t.loadTextureFromFile(t.m_path);

    if (t.m_type == "texture_diffuse")
      m_material.textures[Material::TextureType::ALBEDO] = t.m_id;
    else if (t.m_type == "texture_specular")
      m_material.textures[Material::TextureType::METALLIC] = t.m_id;
    else if (t.m_type == "texture_normal")
      m_material.textures[Material::TextureType::NORMAL] = t.m_id;
    else if (t.m_type == "texture_roughness")
      m_material.textures[Material::TextureType::ROUGHNESS] = t.m_id;
    else if (t.m_type == "texture_emission")
      m_material.textures[Material::TextureType::EMISSIVE] = t.m_id;
  }
}

//-------------------------------------------------------------------------------------------
void eMesh::FreeTextures()
{
  for (auto& t : m_textures)
    t.freeTexture();
}

//-------------------------------------------------------------------------------------------
void eMesh::ReloadVertexBuffer()
{
  if (HasCurrentOpenGLContext())
  {
    glBindVertexArray(this->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, this->VBO);
    glBufferData(GL_ARRAY_BUFFER, this->m_vertices.size() * sizeof(Vertex),
      &this->m_vertices[0], GL_STATIC_DRAW);
    glBindVertexArray(0);
  }
}

//-------------------------------------------------------------------------------------------
void eMesh::Draw()
{
  _BindMaterialTextures(); // _BindRawTextures();
	// Draw mesh
  if (VAO != -1)
  {
    glBindVertexArray(VAO);
    eGlDrawContext::GetInstance().DrawElements(GL_TRIANGLES, (GLsizei)this->m_indices.size(), GL_UNSIGNED_INT, 0, this->m_name);
    glBindVertexArray(0);
  }
}

//-------------------------------------------------------------------------------------------
void eMesh::DrawInstanced(int32_t instances)
{
  _BindMaterialTextures(); // _BindRawTextures();
  // Draw mesh
  if (VAO != -1)
  {
    glBindVertexArray(VAO);
    eGlDrawContext::GetInstance().DrawElementsInstanced(GL_TRIANGLES, (GLsizei)this->m_indices.size(), GL_UNSIGNED_INT, (GLvoid*)(0), instances, this->m_name);
    glBindVertexArray(0);
  }
}

//-------------------------------------------------------------------------------------------
std::vector<TextureInfo> eMesh::GetTextures() const
{
   std::vector<TextureInfo> ret;
   for (auto& t : m_textures)
     ret.emplace_back(t.m_type, t.m_path);
   return ret;
}

//-------------------------------------------------------------------------------------------
void eMesh::AddTexture(Texture* _texture)
{
  if (_texture->m_type == "texture_diffuse")
    m_material.textures[Material::TextureType::ALBEDO] = _texture->m_id;
  else if (_texture->m_type == "texture_specular")
    m_material.textures[Material::TextureType::METALLIC] = _texture->m_id;
  else if (_texture->m_type == "texture_normal")
    m_material.textures[Material::TextureType::NORMAL] = _texture->m_id;
  else if (_texture->m_type == "texture_roughness")
    m_material.textures[Material::TextureType::ROUGHNESS] = _texture->m_id;
  else if (_texture->m_type == "texture_emission")
    m_material.textures[Material::TextureType::EMISSIVE] = _texture->m_id;

  m_textures.push_back(*_texture);
}

//-------------------------------------------------------------------------------------------
void eMesh::SetMaterial(const Material& _material)
{
  m_material = _material;
}

//-------------------------------------------------------------------------------------------
std::optional<Material> eMesh::GetMaterial() const
{
  return m_material;
}

//-------------------------------------------------------------------------------------------
void eMesh::calculatedTangent()
{
  auto& indices = m_indices;

  // Initialize tangents and bitangents to zero
  for (auto& v : m_vertices)
  {
    v.tangent = glm::vec4(0.0f);         // xyz: tangent, w: handedness
    v.bitangent = glm::vec3(0.0f);
  }

  // Triangle-wise tangent/bitangent computation
  for (size_t i = 0; i < indices.size(); i += 3)
  {
    GLuint i0 = indices[i];
    GLuint i1 = indices[i + 1];
    GLuint i2 = indices[i + 2];

    const glm::vec3& pos1 = m_vertices[i0].Position;
    const glm::vec3& pos2 = m_vertices[i1].Position;
    const glm::vec3& pos3 = m_vertices[i2].Position;

    const glm::vec2& uv1 = m_vertices[i0].TexCoords;
    const glm::vec2& uv2 = m_vertices[i1].TexCoords;
    const glm::vec2& uv3 = m_vertices[i2].TexCoords;

    glm::vec3 edge1 = pos2 - pos1;
    glm::vec3 edge2 = pos3 - pos1;
    glm::vec2 deltaUV1 = uv2 - uv1;
    glm::vec2 deltaUV2 = uv3 - uv1;

    float denom = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
    if (std::abs(denom) < 1e-6f)
      continue;

    float f = 1.0f / denom;

    glm::vec3 tangent = f * (deltaUV2.y * edge1 - deltaUV1.y * edge2);
    glm::vec3 bitangent = f * (-deltaUV2.x * edge1 + deltaUV1.x * edge2);

    // Accumulate per-vertex
    m_vertices[i0].tangent += glm::vec4(tangent, 0.0f);
    m_vertices[i1].tangent += glm::vec4(tangent, 0.0f);
    m_vertices[i2].tangent += glm::vec4(tangent, 0.0f);

    m_vertices[i0].bitangent += bitangent;
    m_vertices[i1].bitangent += bitangent;
    m_vertices[i2].bitangent += bitangent;
  }

  // Normalize and compute tangent handedness
  for (auto& v : m_vertices)
  {
    glm::vec3 N = glm::normalize(v.Normal);
    glm::vec3 T = glm::normalize(glm::vec3(v.tangent));
    glm::vec3 B = glm::normalize(v.bitangent);

    // Compute handedness: 1.0 = right-handed, -1.0 = left-handed
    float handedness = (glm::dot(glm::cross(N, T), B) < 0.0f) ? -1.0f : 1.0f;
    v.tangent = glm::vec4(T, handedness);
  }

  // Optional debug output
  //gl_utils::DebugPrintTBN(m_vertices);
}

//-------------------------------------------------------------------------------------------
void eMesh::_BindRawTextures()
{
  static const std::unordered_map<std::string, GLenum> textureMap = {
         {"texture_diffuse", GL_TEXTURE2},
         {"texture_specular", GL_TEXTURE3},
         {"texture_normal", GL_TEXTURE4},
         {"texture_roughness", GL_TEXTURE5},
         {"texture_emission", GL_TEXTURE6}
  };

  bool diffuseNr = false, specularNr = false, normalNr = false, emissionNr = false;

  for (const auto& t : m_textures)
  {
    auto it = textureMap.find(t.m_type);
    if (it != textureMap.end())
    {
      glActiveTexture(it->second);
      glBindTexture(GL_TEXTURE_2D, t.m_id);

      // Track which textures are assigned
      if (t.m_type == "texture_diffuse")   diffuseNr = true;
      if (t.m_type == "texture_specular")  specularNr = true;
      if (t.m_type == "texture_normal")    normalNr = true;
      if (t.m_type == "texture_emission")  emissionNr = true;
    }
  }

  // Ensure all required textures are assigned
  if (!diffuseNr || !specularNr || !normalNr || !emissionNr)
    assert(false && "Some required textures are not assigned!");

  glActiveTexture(GL_TEXTURE0);
}

//-------------------------------------------------------------------------------------------
void eMesh::_BindMaterialTextures()  //@todo move to base class
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
}

//-------------------------------------------------------------------------------------------
void eMesh::MergeMesh(eMesh&& _other)
{
  size_t old_vertex_size = this->m_vertices.size();
  size_t old_index_size = this->m_indices.size();
  dbb::Unite(this->m_vertices, std::move(_other.m_vertices));
  dbb::Unite(this->m_indices, std::move(_other.m_indices));
  for (size_t i = old_index_size; i < this->m_indices.size(); ++i)
    m_indices[i] += old_vertex_size;

  SetupMesh();
}

//-------------------------------------------------------------------------------------------
void eMesh::SetupMesh()
{
  if(this->VAO == -1)
	  glGenVertexArrays(1, &this->VAO);
  if(this->VBO == -1)
	  glGenBuffers(1, &this->VBO);
  if(this->EBO == -1)
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
void SaveBinary(const MeshBase& mesh, const std::string& path)
{
  std::ofstream out(path, std::ios::binary);
  if (!out) throw std::runtime_error("Failed to open file for writing");

  // Write vertices
  size_t vertexCount = mesh.m_vertices.size();
  out.write(reinterpret_cast<const char*>(&vertexCount), sizeof(vertexCount));
  if (vertexCount > 0) {
    out.write(reinterpret_cast<const char*>(mesh.m_vertices.data()), sizeof(Vertex) * vertexCount);
  }

  // Write indices
  size_t indexCount = mesh.m_indices.size();
  out.write(reinterpret_cast<const char*>(&indexCount), sizeof(indexCount));
  if (indexCount > 0) {
    out.write(reinterpret_cast<const char*>(mesh.m_indices.data()), sizeof(GLuint) * indexCount);
  }
}

//-----------------------------------------------------------------------------------
void LoadBinary(MeshBase& mesh, const std::string& path)
{
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("Failed to open file for reading");

  // Read vertices
  size_t vertexCount;
  in.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
  mesh.m_vertices.resize(vertexCount);
  if (vertexCount > 0) {
    in.read(reinterpret_cast<char*>(mesh.m_vertices.data()), sizeof(Vertex) * vertexCount);
  }

  // Read indices
  size_t indexCount;
  in.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));
  mesh.m_indices.resize(indexCount);
  if (indexCount > 0) {
    in.read(reinterpret_cast<char*>(mesh.m_indices.data()), sizeof(GLuint) * indexCount);
  }
}
