#include "stdafx.h"

#include "TerrainModel.h"
#include "TerrainMesh.h"
#include "Texture.h"

#include <math/Camera.h>
#include <math/GeometryFunctions.h>

#include <algorithm>
#include<cmath> 

//@todo improve constructor - initialization
//----------------------------------------------------------------
TerrainModel::TerrainModel()
{
	_InitMaterialWithDefaults();
}

//----------------------------------------------------------------
TerrainModel::TerrainModel(Texture* diffuse,
													 Texture* specular,
													 Texture* normal,
													 Texture* heightMap)
{
	if (diffuse != nullptr)
		m_material.textures[Material::TextureType::ALBEDO] = diffuse->m_id;

	if (specular != nullptr)
		m_material.textures[Material::TextureType::METALLIC] = specular->m_id;

	if (normal != nullptr)
		m_material.textures[Material::TextureType::NORMAL] = normal->m_id;

	_InitMaterialWithDefaults();

	m_meshes.push_back(new TerrainMesh("terrain0 0"));
	TerrainMesh* mesh = m_meshes.back();
	
	mesh->MakePlaneVerts(heightMap->m_width,heightMap->m_height, false);
	mesh->MakePlaneIndices(heightMap->m_width, heightMap->m_height);
	mesh->AssignHeights(m_height);
	mesh->GenerateNormals(heightMap->m_width, heightMap->m_height)->m_id;
	m_material.textures[Material::TextureType::NORMAL] = mesh->GetNormalMapId();
	mesh->calculatedTangent();
	mesh->setupMesh();
}

//------------------------------------------------------------
TerrainModel::TerrainModel(Texture* color)
{
	if (color != nullptr)
		m_material.textures[Material::TextureType::ALBEDO] = color->m_id;

	if (color != nullptr)
		m_material.textures[Material::TextureType::METALLIC] = color->m_id;

	_InitMaterialWithDefaults();

	m_meshes.push_back(new TerrainMesh("terrain0 0"));
	TerrainMesh* mesh = m_meshes.back();
	mesh->MakePlaneVerts(10);
	mesh->MakePlaneIndices(10);
	m_material.textures[Material::TextureType::NORMAL] = mesh->GenerateNormals(10)->m_id;
	mesh->setupMesh();
	mesh->calculatedTangent();
}

//----------------------------------------------------------------
TerrainModel::TerrainModel(const TerrainModel& _other)
  : m_material(_other.m_material)
{
	for (auto& mesh : _other.m_meshes)
		m_meshes.push_back(mesh);
	_InitMaterialWithDefaults();
}

//----------------------------------------------------------------
void TerrainModel::Initialize(Texture* _diffuse,
														  Texture* _specular,
														  Texture* _normal,
														  Texture* _heightMap,
														  bool spreed_texture,
															float _height_scale,
															float _max_height,
															float _min_height,
															int32_t _normal_sharpness,
															unsigned int _tessellation_coef,
															unsigned int _cpu_lods)
{
	if (_diffuse != nullptr)
		m_material.textures[Material::TextureType::ALBEDO] = _diffuse->m_id;

	if (_specular != nullptr)
		m_material.textures[Material::TextureType::METALLIC] = _specular->m_id;

	if (_normal != nullptr)
		m_material.textures[Material::TextureType::NORMAL] = _normal->m_id;

	_InitMaterialWithDefaults();

	if (m_meshes.empty())
		m_meshes.push_back(new TerrainMesh("terrain0 0"));

	TerrainMesh* mesh = m_meshes.back();
	if (_heightMap != nullptr)
	{
		m_height = *_heightMap;

		mesh->MakePlaneVerts(_heightMap->m_width / _tessellation_coef, _heightMap->m_height / _tessellation_coef, spreed_texture);

		for (size_t i = 1; i <= _cpu_lods; ++i)
			mesh->MakePlaneIndices(_heightMap->m_width / _tessellation_coef, _heightMap->m_height / _tessellation_coef, i);

		mesh->AssignHeights(*_heightMap, _height_scale, _max_height, _min_height, _normal_sharpness);
		if(Texture* generated_normals_texture = mesh->GenerateNormals(_heightMap->m_width / _tessellation_coef, _heightMap->m_height / _tessellation_coef))
			m_material.textures[Material::TextureType::NORMAL] = generated_normals_texture->m_id;
		mesh->GenerateTessellationData();
	}
	else
	{
		mesh->MakePlaneVerts(_diffuse->m_height, _diffuse->m_height, spreed_texture);
		mesh->MakePlaneIndices(_diffuse->m_height);
		m_material.textures[Material::TextureType::NORMAL] = mesh->GenerateNormals(mesh->Size() / _tessellation_coef)->m_id;
	}

	mesh->calculatedTangent();
	mesh->setupMesh();
	if (m_camera)
		mesh->SetCamera(m_camera);
}

//----------------------------------------------------------------
void TerrainModel::AddOrUpdate(glm::ivec2 _pos,
	const TessellationRenderingInfo& _tess_info_in,
	const Texture* _diffuse,
	const Texture* _heightMap,
	bool spreed_texture,
	int32_t _normal_sharpness,
	bool _apply_normal_blur,
	unsigned int _tessellation_coef,
	unsigned int _cpu_lods)
{
	if (_diffuse != nullptr)
		m_material.textures[Material::TextureType::ALBEDO] = _diffuse->m_id;

	//if (_specular != nullptr)
	//	m_material.textures[Material::TextureType::METALLIC] = _specular->m_id;

	//if (_normal != nullptr)
	//	m_material.textures[Material::TextureType::NORMAL] = _normal->m_id;

	_InitMaterialWithDefaults();

	if (m_meshes.empty())
		m_meshes.push_back(new TerrainMesh("terrain0 0"));

	TerrainMesh* mesh = nullptr;
	for (auto* m : m_meshes)
		if (m->GetPosition() == _pos) { mesh = m; break; }

	if (!mesh)
	{
		m_meshes.push_back(new TerrainMesh("terrain" + std::to_string(_pos.x) + " " + std::to_string(_pos.y)));
		mesh = m_meshes.back();
	}
	mesh->SetPosition(_pos);

	// Ensure sane placement defaults
	TessellationRenderingInfo ti = _tess_info_in;
	if (ti.chunk_scale_xz == glm::vec2(0.0f))
		ti.chunk_scale_xz = glm::vec2(1.0f);

	mesh->SetTessellationRenderingInfo(ti);

	if (_heightMap)
	{
		m_height = *_heightMap;

		mesh->MakePlaneVerts(_heightMap->m_width / _tessellation_coef,
			_heightMap->m_height / _tessellation_coef,
			spreed_texture);

		for (size_t i = 1; i <= _cpu_lods; ++i)
			mesh->MakePlaneIndices(_heightMap->m_width / _tessellation_coef,
				_heightMap->m_height / _tessellation_coef, i);

		mesh->AssignHeights(*_heightMap, ti.height_scale, ti.max_height, ti.min_height,
			_normal_sharpness, _apply_normal_blur);

		mesh->GenerateNormals(_heightMap->m_width / _tessellation_coef,
			_heightMap->m_height / _tessellation_coef);

		m_material.textures[Material::TextureType::NORMAL] = mesh->GetNormalMapId();
		mesh->GenerateTessellationData();
	}
	else
	{
		mesh->MakePlaneVerts(_diffuse->m_height, _diffuse->m_height, spreed_texture);
		mesh->MakePlaneIndices(_diffuse->m_height);
		m_material.textures[Material::TextureType::NORMAL] =
			mesh->GenerateNormals(mesh->Size() / _tessellation_coef)->m_id;
	}

	mesh->calculatedTangent();
	mesh->setupMesh();
	if (m_camera)
		mesh->SetCamera(m_camera);
}

//----------------------------------------------------------------
void TerrainModel::SetAABBtoChunk(extremDots _dots, glm::ivec2 _pos)
{
	for (auto mesh : m_meshes)
		if (mesh->GetPosition() == _pos)
			mesh->SetExtremDots(_dots);
}

//----------------------------------------------------------------
void TerrainModel::EnableTessellation(bool _enable)
{
	m_tessellation_enabled = _enable;
}

//----------------------------------------------------------------
void TerrainModel::SetCamera(Camera* _camera)
{
	m_camera = _camera;
	for (auto* mesh : m_meshes)
		mesh->SetCamera(_camera);
}

//----------------------------------------------------------------
void TerrainModel::SetTessellationInfoUpdater(const TesellationInfoUpdater& _updater)
{
	m_tessellation_info_updater = _updater;
}

//----------------------------------------------------------------
void TerrainModel::SetTessellationInfo(glm::ivec2 _pos, const TessellationRenderingInfo& _info)
{
	for (auto* m : m_meshes)
	{
		if (m->GetPosition() == _pos)
		{
			m->SetTessellationRenderingInfo(_info);
			break;
		}
	}
}

//----------------------------------------------------------------
void TerrainModel::Draw(int32_t _program)
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

	if (m_albedo_texture_array != nullptr)
	{
		glActiveTexture(GL_TEXTURE12);
		glBindTexture(GL_TEXTURE_2D_ARRAY, m_albedo_texture_array->m_id);
		glBindTextureUnit(12, m_albedo_texture_array->m_id);
	}
	if (m_normal_texture_array != nullptr)
	{
		glActiveTexture(GL_TEXTURE13);
		glBindTexture(GL_TEXTURE_2D_ARRAY, m_normal_texture_array->m_id);
		glBindTextureUnit(13, m_normal_texture_array->m_id);
	}
	if (m_metallic_texture_array != nullptr)
	{
		glActiveTexture(GL_TEXTURE14);
		glBindTexture(GL_TEXTURE_2D_ARRAY, m_metallic_texture_array->m_id);
		glBindTextureUnit(14, m_metallic_texture_array->m_id);
	}
	if (m_roughness_texture_array != nullptr)
	{
		glActiveTexture(GL_TEXTURE15);
		glBindTexture(GL_TEXTURE_2D_ARRAY, m_roughness_texture_array->m_id);
		glBindTextureUnit(15, m_roughness_texture_array->m_id);
	}
	if (m_ao_texture_array != nullptr)
	{
		glActiveTexture(GL_TEXTURE16);
		glBindTexture(GL_TEXTURE_2D_ARRAY, m_ao_texture_array->m_id);
		glBindTextureUnit(16, m_ao_texture_array->m_id);
	}

	// legacy may be needed for square chunks
	//if (false) // if local  mesh->GetExtrems() local corners
  //{
  //	const glm::vec2 off = mesh->GetWorlOffset(); // world placement
  //	for (auto& p : extrems) { p.x += off.x; p.z += off.y; }
  //}

	std::vector<TerrainMesh*> rendered_meshes;
	//@todo cache frustum plance per call not per AABB
	if (m_camera)
	{
		const dbb::point camPos = m_camera->getPosition();
		for (auto* mesh : m_meshes)
		{
			const auto& ext = mesh->GetExtremDots();
			bool visible = m_camera->AABBInFrustum(ext);

			// Safety: always render if camera is inside chunk
			if (!visible)
			{
				dbb::AABB chunkAABB = dbb::AABB::FromMinMax(
					{ ext.MinX, ext.MinY, ext.MinZ },
					{ ext.MaxX, ext.MaxY, ext.MaxZ }
				);
				if (dbb::IsPointInAABB(camPos, chunkAABB))
					visible = true;
			}

			if (visible)
				rendered_meshes.push_back(mesh);
		}
	}
	else
	{
		// No camera -> render everything
		rendered_meshes.assign(m_meshes.begin(), m_meshes.end());
	}

	for (auto* mesh : rendered_meshes)
	{
		if (!m_tessellation_enabled)
				mesh->Draw();
		else
				mesh->DrawTessellated(m_tessellation_info_updater);
	}
}

//----------------------------------------------------------------
std::vector<Brush*>	TerrainModel::getMeshes()	const
{ 
	std::vector<Brush*> meshes;
	for (auto& mesh : m_meshes)
		meshes.push_back(mesh);
  return meshes;
}

//------------------------------------------------------------
void	TerrainModel::setDiffuse(uint32_t _id)
{
	m_material.textures[Material::TextureType::ALBEDO] = _id;
}

//------------------------------------------------------------
void	TerrainModel::setSpecular(uint32_t _id)
{
	m_material.textures[Material::TextureType::METALLIC] = _id;
}

//----------------------------------------------------------------
void TerrainModel::setAlbedoTextureArray(const Texture* _t)
{
	m_albedo_texture_array = _t;
}

//----------------------------------------------------------------
void TerrainModel::setNormalTextureArray(const Texture* _t)
{
	m_normal_texture_array = _t;
}

//----------------------------------------------------------------
void TerrainModel::setMetallicTextureArray(const Texture* _t)
{
	m_metallic_texture_array = _t;
}

//----------------------------------------------------------------
void TerrainModel::setRoughnessTextureArray(const Texture* _t)
{
	m_roughness_texture_array = _t;
}

//----------------------------------------------------------------
void TerrainModel::setAOTextureArray(const Texture* _t)
{
	m_ao_texture_array = _t;
}

//--------------------------------------------------------------
float TerrainModel::GetHeight(float x, float z)
{
	for (auto* mesh : m_meshes) {
		const glm::vec2 off = mesh->GetWorlOffset();
		if (auto v = mesh->FindVertex(x - off.x, z - off.y)) return v->Position.y;
	}
	return -1.f;
}

//------------------------------------------------------------
glm::vec3 TerrainModel::GetNormal(float x, float z)
{
	for (auto* mesh : m_meshes) {
		const glm::vec2 off = mesh->GetWorlOffset();
		if (auto v = mesh->FindVertex(x - off.x, z - off.y)) return v->Normal;
	}
	return {};
}

//----------------------------------------------------------------
std::vector<std::vector<glm::vec3>> TerrainModel::GetExtremsOfMeshesLocalSpace() const
{
	std::vector<std::vector<glm::vec3>> ret;
	for (auto& mesh : m_meshes)
		ret.push_back(mesh->GetExtrems());
	return ret;
}

//----------------------------------------------------------------
std::vector<extremDots> TerrainModel::GetExtremDotsOfMeshes() const
{
	std::vector<extremDots> ret;
	for (auto& mesh : m_meshes)
		ret.push_back(mesh->GetExtremDots());
	return ret;
}

//----------------------------------------------------------------
const std::string& TerrainModel::GetName() const
{
	return m_path;
}

//----------------------------------------------------------------
size_t TerrainModel::GetVertexCount() const
{
	size_t vertexCount = 0;
	if (!m_meshes.empty())
	{
		for (auto* mesh : m_meshes)
			vertexCount += mesh->m_vertices.size();
	}
  return vertexCount;
}

//----------------------------------------------------------------
std::vector<const IMesh*> TerrainModel::GetMeshes() const
{
	if (!m_meshes.empty())
	{
		std::vector<const IMesh*> meshes;
		for (auto* mesh : m_meshes)
			meshes.push_back(mesh);
		return meshes;
	}
	else
		return {};
}

//----------------------------------------------------------------
std::vector<const I3DMesh*> TerrainModel::Get3DMeshes() const
{
	if (!m_meshes.empty())
	{
		std::vector<const I3DMesh*> meshes;
		for (auto* mesh : m_meshes)
			meshes.push_back(mesh);
		return meshes;
	}
	else
		return {};
}

//----------------------------------------------------------------
void TerrainModel::SetUpMeshes()
{
	for (auto mesh : m_meshes)
		mesh->setupMesh();
}

//----------------------------------------------------------------
void TerrainModel::SetMaterial(const Material& _m)
{
	m_material = _m;
	_InitMaterialWithDefaults();
}

//----------------------------------------------------------------
void TerrainModel::_InitMaterialWithDefaults() // @todo to base class
{
	if (m_material.textures.find(Material::TextureType::ALBEDO) == m_material.textures.end())
	{
		m_material.textures[Material::TextureType::ALBEDO] = Texture::GetTexture1x1(GREY).m_id;
	}
	if (m_material.textures.find(Material::TextureType::METALLIC) != m_material.textures.end())
	{
		m_material.textures[Material::TextureType::METALLIC] = Texture::GetTexture1x1(BLACK).m_id;
	}
	if (m_material.textures.find(Material::TextureType::NORMAL) != m_material.textures.end())
	{
		m_material.textures[Material::TextureType::NORMAL] = Texture::GetTexture1x1(BLUE).m_id;
	}

	if (m_material.textures.find(Material::TextureType::ROUGHNESS) != m_material.textures.end())
	{
		m_material.textures[Material::TextureType::ROUGHNESS] = Texture::GetTexture1x1(WHITE).m_id;
	}
	if (m_material.textures.find(Material::TextureType::EMISSIVE) != m_material.textures.end())
	{
		m_material.textures[Material::TextureType::EMISSIVE] = Texture::GetTexture1x1(BLACK).m_id;
	}
	if (m_material.textures.find(Material::TextureType::AO) != m_material.textures.end())
	{
		m_material.textures[Material::TextureType::AO] = Texture::GetTexture1x1(WHITE).m_id;
	}
	if (m_material.textures.find(Material::TextureType::OPACITY) != m_material.textures.end())
	{
		m_material.textures[Material::TextureType::OPACITY] = Texture::GetTexture1x1(WHITE).m_id;
	}
}

//----------------------------------------------------------------
std::vector<glm::vec3> TerrainModel::GetPositions() const
{
	std::vector<glm::vec3> ret;
	if (m_meshes.empty())
		return ret;

	for (auto& vert : m_meshes[0]->m_vertices)
		ret.push_back(vert.Position);
	return ret; // @todo to improve
}

//----------------------------------------------------------------
std::vector<GLuint> TerrainModel::GetIndeces() const
{
	return m_meshes[0]->GetIndicesLods()[0];
}

//----------------------------------------------------------------
TerrainModel::~TerrainModel()
{
	for (auto* mesh : m_meshes)
		delete mesh;
	//custom normals
 /* normal.freeTexture(); !!!*/
}

//----------------------------------------------------------------
bool operator<(const TerrainType& _one, const TerrainType& _two)
{
	// use threshold_start to sort
	return _one.threshold_start < _two.threshold_start;
}

//----------------------------------------------------------------
bool operator==(const TerrainType& _one, const TerrainType& _two)
{
	// use name as a key
	return _one.name == _two.name;
}


