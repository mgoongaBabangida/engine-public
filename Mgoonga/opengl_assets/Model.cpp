#include "stdafx.h"

#include "Model.h"
#include <math/Transform.h>

#include <algorithm>
#include <assert.h>
#include <memory>
#include <base/Log.h>

using namespace std;

//---------------------------------------------------------------------------
eModel::eModel(char* _path, const std::string& _name)
	: m_name(_name)
{
	std::string path(_path);
	this->m_directory = path.substr(0, path.find_last_of('/'));
}

// -------------------------------------------------------------------------- -
eModel::eModel(const std::string& _name, std::vector<eMesh>&& _meshes, const std::vector<Bone>& _bones)
	: m_name(_name), m_meshes(std::move(_meshes)), m_bones(_bones)
{
	//@ ? extra init,? extra paramenetrs
}

//---------------------------------------------------------------------------
eModel::~eModel()
{
	for (auto& mesh : m_meshes)
		mesh.FreeTextures();
}

//---------------------------------------------------------------------------
std::string eModel::RootBoneName()
{
	return m_root_bone ? m_root_bone->GetName() : "NoRootBone";
}

//---------------------------------------------------------------------------
void eModel::AddMesh(std::vector<Vertex> _vertices, std::vector<GLuint> _indices, std::vector<TextureInfo> _textures, const Material& _material, const std::string& _name, bool _calculate_tangent)
{
	std::vector<Texture> textures;
	for (auto t : _textures)
		textures.emplace_back(t);

	m_meshes.emplace_back(_vertices, _indices, textures, _material, _name, _calculate_tangent);
}

//---------------------------------------------------------------------------
const eMesh* eModel::GetMeshByIndex(size_t _index) const
{
	if (_index < m_meshes.size())
		return &m_meshes[_index];
	else
		return nullptr;
}

//----------------------------------------------------------
const eMesh* eModel::GetMeshByName(const std::string& _name) const
{
	const auto it = std::find_if(m_meshes.begin(), m_meshes.end(), [&_name](const eMesh& mesh)
		{
			return mesh.Name() == _name;
		});
	return it != m_meshes.end() ? &(*it) : nullptr;
}

//---------------------------------------------------------------------------
std::vector<std::unique_ptr<IModel>> eModel::Split()
{
	std::vector<std::unique_ptr<IModel>> ret;
	for (size_t i = 0; i < m_meshes.size(); ++i)
	{
		std::vector<eMesh> v;
		v.push_back(std::move(m_meshes[i]));  // Move mesh into the new vector
		ret.push_back(std::unique_ptr<IModel>(new eModel(v.back().Name(), std::move(v), {})));
	}
	return ret;
}

//---------------------------------------------------------------------------
void eModel::Draw(int32_t _program)
{
	for (GLuint i = 0; i < this->m_meshes.size(); ++i)
	{
		if (_program != 0)
		{
			// hack fix of an imported mesh offset, better to fix in blender
			uint32_t loc = glGetUniformLocation(_program, "meshOffset");
			glUniformMatrix4fv(loc, 1, GL_FALSE, &m_meshes[i].GetBindCorrection()[0][0]);
			m_meshes[i].Draw();
			glUniformMatrix4fv(loc, 1, GL_FALSE, &UNIT_MATRIX[0][0]);
		}
		else
			m_meshes[i].Draw();
	}
}

//---------------------------------------------------------------------------
void eModel::DrawInstanced(int32_t _program, int32_t _instances)
{
	for (GLuint i = 0; i < this->m_meshes.size(); ++i)
	{
		if (_program != 0)
		{
			uint32_t loc = glGetUniformLocation(_program, "meshOffset");
			glUniformMatrix4fv(loc, 1, GL_FALSE, &m_meshes[i].GetBindCorrection()[0][0]);
			m_meshes[i].DrawInstanced(_instances);
			glUniformMatrix4fv(loc, 1, GL_FALSE, &UNIT_MATRIX[0][0]);
		}
		else
			m_meshes[i].DrawInstanced(_instances);
	}
}

//---------------------------------------------------------------------------
void eModel::SetMaterial(const Material& _m)
{
	for (auto& mesh : m_meshes)
		mesh.SetMaterial(_m);
}

//---------------------------------------------------------------------------
void	eModel::SetUpMeshes()
{
	for (auto& mesh : m_meshes)
		mesh.SetupMesh();
}

//---------------------------------------------------------------------------
void eModel::ReloadTextures()
{
	std::vector<Texture> unique_textures;
	for (auto& mesh : m_meshes)
	{
		for (auto& t : mesh.m_textures)
		{
			if (auto it = std::find_if(unique_textures.begin(), unique_textures.end(), [&t](const Texture& _t) { return t.m_path == _t.m_path; });
				it == unique_textures.end())
			{
				if(t.m_id == Texture::GetDefaultTextureId())
					t.loadTextureFromFile(t.m_path);
				unique_textures.push_back(t);
			}
			else // it != unique_textures.end()
			{
				t = *it;
			}
			if (t.m_type == "texture_diffuse")
				mesh.m_material.textures[Material::TextureType::ALBEDO] = t.m_id;
			else if (t.m_type == "texture_specular")
				mesh.m_material.textures[Material::TextureType::METALLIC] = t.m_id;
			else if (t.m_type == "texture_normal")
				mesh.m_material.textures[Material::TextureType::NORMAL] = t.m_id;
			else if (t.m_type == "texture_roughness")
				mesh.m_material.textures[Material::TextureType::ROUGHNESS] = t.m_id;
			else if (t.m_type == "texture_emission")
				mesh.m_material.textures[Material::TextureType::EMISSIVE] = t.m_id;
			else if (t.m_type == "texture_opacity")
				mesh.m_material.textures[Material::TextureType::OPACITY] = t.m_id;
			else
				base::Log("eModel::ReloadTextures: unused texture type: " + t.m_type);
		}
	}
}

//---------------------------------------------------------------------------
size_t eModel::GetVertexCount() const
{
  size_t count = 0;
  for (auto& mesh : m_meshes)
    count += mesh.m_vertices.size();
  return count;
}

//----------------------------------------------------------------------------
std::vector<const I3DMesh*> eModel::Get3DMeshes() const
{
	std::vector<const I3DMesh*> ret;
	for (const eMesh& mesh : m_meshes)
		ret.push_back(&mesh);
	return ret;
}

//----------------------------------------------------------------------------
std::vector<const IMesh*> eModel::GetMeshes() const
{
  std::vector<const IMesh*> ret;
  for (const eMesh& mesh : m_meshes)
    ret.push_back(&mesh);
  return ret;
}

//----------------------------------------------------------------------------
std::vector<const IBone*> eModel::GetBones() const
{
	std::vector<const IBone*> bones;
	for (auto& bone : m_bones)
		bones.push_back(&bone);
	return bones;
}

//----------------------------------------------------------------------------
std::vector<const IAnimation*> eModel::GetAnimations() const
{
  std::vector<const IAnimation*> ret;
  for (auto& anim : m_animations)
  {
    ret.push_back(&anim);
  }
  return ret;
}

//-------------------------------------------------------------------------------------------
void eModel::mapMehsesToNodes()
{
	for (int32_t i = 0; i < m_meshes.size(); ++i)
	{
		std::vector<Bone>::iterator meshBoneIter = std::find_if(m_bones.begin(), m_bones.end(), [this, i](const Bone& bone)
			{ return bone.GetName() == m_meshes[i].Name(); });
		if (meshBoneIter != m_bones.end())
		{
			for (int32_t j = 0; j < m_meshes[i].m_vertices.size(); ++j)
			{
				m_meshes[i].m_vertices[j].boneIDs[0] = (glm::i32)meshBoneIter->GetID();
				m_meshes[i].m_vertices[j].weights[0] = 1.0f;
			}
			m_meshes[i].ReloadVertexBuffer();
		}
	}
}

//-----------------------------------------------------------------------
void eModel::VertexBoneData::AddBoneData(int BoneID, float Weight)
 {
		++numTries;
		for (int i = 0; i < NUM_BONES_PER_VEREX; i++)
		{
			//ARRAY_SIZE_IN_ELEMENTS(IDs)
			if (Weights[i] == 0.0)
			{
				IDs[i] = BoneID;
				Weights[i] = Weight;
				return;
			}
		}
		// should never get here - more bones than we have space for 
		//  assert(0);
}
