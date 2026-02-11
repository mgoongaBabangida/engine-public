#include "stdafx.h"
#include "MyModel.h"
#include "Texture.h"

#include <glm\glm\glm.hpp>
#include <glm\glm\gtc\matrix_transform.hpp>
#include <glm\glm\gtx\transform.hpp>
#include <glm\glm/gtx/euler_angles.hpp>
#include <glm\glm/gtc/quaternion.hpp>
#include <glm\glm/gtx/quaternion.hpp>
#include <glm\glm/gtx/norm.hpp>

//---------------------------------------------------------------------------------
MyModel::MyModel()
  : m_name("empty")
{}

//---------------------------------------------------------------------------------
MyModel::MyModel(std::shared_ptr<Brush> m, const std::string& _name, 
  const Texture* t, const Texture* t2, const Texture* t3, const Texture* t4)
  : m_mesh(m)
  , m_name(_name)
{
  if (t != nullptr)
  {
    m_material.textures[Material::TextureType::ALBEDO] = t->m_id;
    m_material.used_textures.insert(Material::TextureType::ALBEDO);
  }
  
  if (t2 != nullptr)
  {
    m_material.textures[Material::TextureType::METALLIC] = t2->m_id;
    m_material.used_textures.insert(Material::TextureType::METALLIC);
  }

  if (t3 != nullptr)
  {
    m_material.textures[Material::TextureType::NORMAL] = t3->m_id;
    m_material.used_textures.insert(Material::TextureType::NORMAL);
  }

  if (t4 != nullptr)
  {
    m_material.textures[Material::TextureType::ROUGHNESS] = t4->m_id;
    m_material.used_textures.insert(Material::TextureType::ROUGHNESS);
  }

  m_material.textures[Material::TextureType::EMISSIVE] = Texture::GetTexture1x1(BLACK).m_id;
  m_material.used_textures.insert(Material::TextureType::EMISSIVE); // default emissive ?!

  _InitMaterialWithDefaults();
}

//-----------------------------------------------------------------
MyModel::MyModel(const MyModel& _other) //shallow copy
	: m_mesh(_other.m_mesh)
  , m_material(_other.m_material)
  , m_name(_other.m_name)
  , m_path(_other.m_path)
{
}

//----------------------------------------------------------------------------------------------
MyModel::~MyModel()
{}

//----------------------------------------------------------------------------------------------
std::vector<Brush*> MyModel::getMeshes() const 
{
  std::vector<Brush*> meshes;
  meshes.push_back(m_mesh.get());
  return meshes;
}

//----------------------------------------------------------------------------------------------
void MyModel::SetMaterial(const Material& _material)
{
  m_material = _material;
  _InitMaterialWithDefaults();
}

//--------------------------------------------------------------------------------------------
void MyModel::Draw(int32_t _program)
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

	m_mesh->Draw();
}

//--------------------------------------------------------------------------------------------
void MyModel::DrawInstanced(int32_t _program, int32_t _instances)
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

  m_mesh->DrawInstanced(_instances);
}

//----------------------------------------------------------------
void MyModel::SetUpMeshes()
{
  m_mesh->setupMesh();
}

//----------------------------------------------------------------
void MyModel::ReloadTextures()
{
  //@todo reassign textures
}

//----------------------------------------------------------------
void MyModel::Debug()
{}

//------------------------------------------------------------------------------------
void MyModel::_InitMaterialWithDefaults() // @todo to base class
{
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
  if (!m_material.textures.contains(Material::TextureType::OPACITY))
    m_material.textures[Material::TextureType::OPACITY] = Texture::GetTexture1x1(WHITE).m_id;
}

//-----------------------------------------------------------------------------------
size_t MyModel::GetVertexCount() const
{
  return m_mesh->GetVertexCount();
}

//-----------------------------------------------------------------------------------
std::vector<const I3DMesh*> MyModel::Get3DMeshes() const
{
  return std::vector<const I3DMesh*> { m_mesh.get() };
}

//-----------------------------------------------------------------------------------
std::vector<const IMesh*> MyModel::GetMeshes() const
{
  return std::vector<const IMesh*> { m_mesh.get() };
}

//---------------------------------------------------------------------------------
SimpleModel::SimpleModel(IMesh* _m)
:m_mesh(_m) {}

SimpleModel::~SimpleModel() { delete m_mesh; }

//---------------------------------------------------------------------------------
BezierCurveModel::~BezierCurveModel()
{
  for (auto& m : m_meshs)
    delete m;
}

//---------------------------------------------------------------------------------
void BezierCurveModel::Draw(int32_t _program)
{
  for (auto& m : m_meshs)
    m->Draw();
}

//---------------------------------------------------------------------------------
 std::vector<const IMesh*> BezierCurveModel::GetMeshes() const
{ 
  std::vector<const IMesh*> meshs;
  for(auto m : m_meshs)
    meshs.push_back({ m });
  return meshs;
}