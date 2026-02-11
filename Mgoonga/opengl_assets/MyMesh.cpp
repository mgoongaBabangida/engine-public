#include "stdafx.h"

#include "MyMesh.h"
#include "GlDrawContext.h"
#include "Windows_Related_api.h"

#include <sstream>
#include <base/Log.h>
#include "mesh_system_utils.h"

//----------------------------------------------------------------------------
Brush::Brush(const std::string& _name)
	: name(_name)
{
	indicesLods.push_back({});
	EBOs.push_back({});
}

//--------------------------------------------------------------------------
Brush::Brush(const std::string& _name, eMesh&& _other, std::vector<Texture*> _textures)
	: MeshBase(std::move((MeshBase&&)_other)), name(_name)
{
	if (!this->m_indices.empty())
		indicesLods.push_back(m_indices);

	this->textures = _textures;

	if (HasCurrentOpenGLContext())
	{
		if (VAO == -1 || VBO == -1 || EBO == -1)
			this->setupMesh();
	}
	else
		base::Log("Warning: tried to init GL in Brush in non GL thread!");
}

//---------------------------------------------------------------------------
Brush::~Brush()
{
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &EBO);
	for(size_t i = 0; i < EBOs.size();++i)
		glDeleteBuffers(1, &EBOs[i]);
}

//----------------------------------------------------------------------------------------------
Brush::Brush(const std::string& _name, std::vector<Vertex> vertices, std::vector<unsigned  int> indices, std::vector<Texture*> textures)
	: name(_name)
{
	indicesLods.push_back({});
	EBOs.push_back({});

	this->m_vertices = vertices;
	this->indicesLods[0] = indices;
	this->textures = textures;

	this->setupMesh();
}

//----------------------------------------------------------------------------------------------
Brush::Brush(const std::string& _name, const ShapeData & data, uint32_t dimensions)
	: name(_name)
{
	indicesLods.push_back({});
	EBOs.push_back({});

	glm::vec2 tex[4];
	tex[0] = glm::vec2(1.0f, 1.0f);
	tex[1] = glm::vec2(0.0f, 1.0f);
	tex[2] = glm::vec2(0.0f, 0.0f);
	tex[3] = glm::vec2(1.0f, 0.0f);
	
	const float checkerRepeats = 5.0f;

	for (uint32_t index = 0; index < data.numVertices; ++index)
	{
		Vertex vert;
		vert.Normal = data.vertices[index].normal;
		vert.Position = data.vertices[index].position;
		vert.TexCoords = tex[index % 4];

		if (dimensions)
		{
			int i = index / dimensions;
			int j = index % dimensions;

			// Normalize to [0, 1]
			float u = static_cast<float>(j) / (dimensions - 1);
			float v = static_cast<float>(i) / (dimensions - 1);

			// Scale to create repetition, then wrap into [0, 1]
			u = std::fmod(u * checkerRepeats, 1.0f);
			v = std::fmod(v * checkerRepeats, 1.0f);

			// Ensure non-negative UVs (fmod can return negative)
			if (u < 0.0f) u += 1.0f;
			if (v < 0.0f) v += 1.0f;

			vert.TexCoords = glm::vec2(u, v);
		}
		m_vertices.push_back(vert);
	}

	for (uint32_t i = 0; i < data.numIndices; ++i)
		indicesLods[0].push_back(data.indices[i]);

	for (uint32_t i = 0; i < indicesLods[0].size(); i += 3)
	{
		glm::vec3 pos1 = m_vertices[indicesLods[0][i]].Position;
		glm::vec3 pos2 = m_vertices[indicesLods[0][i+1]].Position;
		glm::vec3 pos3 = m_vertices[indicesLods[0][i+2]].Position;

		glm::vec2 uv1 = m_vertices[indicesLods[0][i]].TexCoords;
		glm::vec2 uv2 = m_vertices[indicesLods[0][i + 1]].TexCoords;
		glm::vec2 uv3 = m_vertices[indicesLods[0][i + 2]].TexCoords;

		// calculate tangent/bitangent vectors of both triangles
		glm::vec4 tangent1; glm::vec3 bitangent1;
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

		m_vertices[indicesLods[0][i]].tangent = tangent1;
		m_vertices[indicesLods[0][i+1]].tangent = tangent1;
		m_vertices[indicesLods[0][i+2]].tangent = tangent1;
		m_vertices[indicesLods[0][i]].bitangent = bitangent1;
		m_vertices[indicesLods[0][i + 1]].bitangent = bitangent1;
		m_vertices[indicesLods[0][i + 2]].bitangent = bitangent1;
	}

	setupMesh();
}

//---------------------------------------------------------------------------------
void Brush::Draw()
{
	if (m_render_mode == RenderMode::DEFAULT)
	{
		if (VAO != -1)
		{
			glBindVertexArray(this->VAO);
			eGlDrawContext::GetInstance().DrawElements(GL_TRIANGLES, (GLsizei)this->indicesLods[LOD_index__in_use].size(), GL_UNSIGNED_INT, 0, this->name);
			glBindVertexArray(0);
		}
	}
	else if (m_render_mode == RenderMode::WIREFRAME)
	{
		if (VAO != -1)
		{
			glBindVertexArray(this->VAO);
			eGlDrawContext::GetInstance().DrawElements(GL_LINES, (GLsizei)this->indicesLods[LOD_index__in_use].size(), GL_UNSIGNED_INT, 0, this->name);
			glBindVertexArray(0);
		}
	}
}

//---------------------------------------------------------------------------------
void Brush::DrawInstanced(int32_t _instances)
{
	if (m_render_mode == RenderMode::DEFAULT)
	{
		if (VAO != -1)
		{
			glBindVertexArray(this->VAO);
			eGlDrawContext::GetInstance().DrawElementsInstanced(GL_TRIANGLES, (GLsizei)this->indicesLods[LOD_index__in_use].size(), GL_UNSIGNED_INT, 0, _instances, this->name);
			glBindVertexArray(0);
		}
	}
	else if (m_render_mode == RenderMode::WIREFRAME)
	{
		if (VAO != -1)
		{
			glBindVertexArray(this->VAO);
			eGlDrawContext::GetInstance().DrawElements(GL_LINES, (GLsizei)this->indicesLods[LOD_index__in_use].size(), GL_UNSIGNED_INT, 0, this->name);
			glBindVertexArray(0);
		}
	}
}

std::vector<TextureInfo> Brush::GetTextures() const
{
  std::vector<TextureInfo> ret;
  for (const Texture* t : textures)
    ret.emplace_back(t->m_type, t->m_path);
  return ret;
}

void Brush::setTextures(std::vector<Texture*> _textures)
{
  textures = _textures;
}

void Brush::setupMesh()
{
	if(!m_indices.empty() && !indicesLods.empty()) //@todo indices ebos etc. should be clean !
		indicesLods[0] = m_indices; 

	glGenVertexArrays(1, &this->VAO);
	glGenBuffers(1, &this->VBO);
	glGenBuffers(1, &this->EBO);

	glBindVertexArray(this->VAO);
	glBindBuffer(GL_ARRAY_BUFFER, this->VBO);
	glBufferData(GL_ARRAY_BUFFER, this->m_vertices.size() * sizeof(Vertex),
		&this->m_vertices[0], GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, this->indicesLods[0].size() * sizeof(GLuint),
		&this->indicesLods[0][0], GL_STATIC_DRAW);

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
	glVertexAttribIPointer(6, 4, GL_INT,  sizeof(Vertex),
		(GLvoid*)offsetof(Vertex, boneIDs));
	// Vertex Weights
	glEnableVertexAttribArray(7);
	glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
		(GLvoid*)offsetof(Vertex, weights));
	glBindVertexArray(0);

	if (this->EBO != -1 && EBOs.empty())
		EBOs.push_back(EBO);
}

//---------------------------------------------------------------------------
void Brush::calculatedTangent()
{
	auto& indices = indicesLods[0];

	// Zero-initialize tangents and bitangents
	for (auto& v : m_vertices)
	{
		v.tangent = glm::vec4(0.0f);
		v.bitangent = glm::vec3(0.0f);
	}

	for (size_t i = 0; i + 2 < indices.size(); i += 3)
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

		m_vertices[i0].tangent += glm::vec4(tangent, 0.0f);
		m_vertices[i1].tangent += glm::vec4(tangent, 0.0f);
		m_vertices[i2].tangent += glm::vec4(tangent, 0.0f);

		m_vertices[i0].bitangent += bitangent;
		m_vertices[i1].bitangent += bitangent;
		m_vertices[i2].bitangent += bitangent;
	}

	// Final orthonormalization and handedness
	for (auto& v : m_vertices)
	{
		glm::vec3 N = glm::normalize(v.Normal);
		glm::vec3 T = glm::normalize(glm::vec3(v.tangent));
		glm::vec3 B = glm::normalize(v.bitangent);

		// Orthonormalize T to N
		T = glm::normalize(T - N * glm::dot(N, T));
		// Recompute B to ensure orthogonality
		B = glm::cross(N, T);

		// Handedness: +1 = right-handed, -1 = left-handed
		float handedness = (glm::dot(glm::cross(N, T), B) < 0.0f) ? -1.0f : 1.0f;

		v.tangent = glm::vec4(T, handedness);
		v.bitangent = B;
	}

	gl_utils::DebugPrintTBN(m_vertices);
}

//--------------------------------------------------------------------------------------------------------------
void Brush::ReloadVertexBuffer()
{
	if (VAO != -1)
	{
		glBindVertexArray(this->VAO);
		glBindBuffer(GL_ARRAY_BUFFER, this->VBO);
		glBufferData(GL_ARRAY_BUFFER, this->m_vertices.size() * sizeof(Vertex),
			&this->m_vertices[0], GL_STATIC_DRAW);
		glBindVertexArray(0);
	}
}

//--------------------------------------------------------------------------------------------------------------
bool Brush::SwitchLOD(GLuint _LOD) //starts from one
{
	if (LOD_index__in_use + 1 == _LOD)
		return true;

	if (indicesLods.size() >= _LOD)
	{
		if (EBOs.size() < _LOD) // EBO for this lod does not exists
		{
			unsigned int new_ebos = _LOD - EBOs.size();
			for (unsigned int i = 0; i < new_ebos; ++i)
			{
				EBOs.push_back({});
				glGenBuffers(1, &this->EBOs.back());
			}
		}

		if (VAO != -1)
		{
			glBindVertexArray(this->VAO);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->EBOs[_LOD - 1]);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, this->indicesLods[_LOD - 1].size() * sizeof(GLuint),
				&this->indicesLods[_LOD - 1][0], GL_STATIC_DRAW);
		}

		LOD_index__in_use = _LOD - 1; //!@ starts from zero
		return true;
	}
	return false;
}

//--------------------------------------------------------------------------------------------------------------
GLuint Brush::LODInUse() const
{ return LOD_index__in_use + 1; }

//--------------------------------------------------------------------------------------------------------------
void Brush::SetRenderMode(RenderMode _mode)
{ m_render_mode = _mode; }

//--------------------------------------------------------------------------------------------------------------
Brush::RenderMode Brush::GetRenderMode()
{ return m_render_mode; }

//--------------------------------------------------------------------------------------------------------------
ParticleMesh::ParticleMesh(std::vector<Vertex> vertices, std::vector<GLuint> indices, std::vector<Texture> textures)
{
	this->vertices = vertices;
	this->indices = indices;
	this->textures = textures;

	this->setupMesh();
}

//--------------------------------------------------------------------------------------------------------------
 ParticleMesh::~ParticleMesh()
{
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteBuffers(1, &EBO);
  glDeleteBuffers(1, &VBOinstanced);
}

 //--------------------------------------------------------------------------------------------------------------
ParticleMesh::ParticleMesh(const ShapeData& data)
{
	glm::vec2 tex[4];
	tex[0] = glm::vec2(1.0f, 1.0f);
	tex[1] = glm::vec2(0.0f, 1.0f);
	tex[2] = glm::vec2(0.0f, 0.0f);
	tex[3] = glm::vec2(1.0f, 0.0f);

	for (unsigned int i = 0; i < data.numVertices; ++i)
	{
		Vertex vert;
		vert.Normal = data.vertices[i].normal;
		vert.Position = data.vertices[i].position;
		vert.TexCoords = tex[i % 4];
		vertices.push_back(vert);
	}

	for (unsigned int i = 0; i < data.numIndices; ++i)
		indices.push_back(data.indices[i]);

	for (unsigned int i = 0; i < indices.size(); i += 3)
	{
		glm::vec3 pos1 = vertices[indices[i]].Position;
		glm::vec3 pos2 = vertices[indices[i + 1]].Position;
		glm::vec3 pos3 = vertices[indices[i + 2]].Position;
		glm::vec2 uv1 = vertices[indices[i]].TexCoords;
		glm::vec2 uv2 = vertices[indices[i + 1]].TexCoords;
		glm::vec2 uv3 = vertices[indices[i + 2]].TexCoords;
	}
	this->setupMesh();
}

//--------------------------------------------------------------------------------------------------------------
void ParticleMesh::Draw()
{
	if (VAO != -1)
	{
		glBindVertexArray(this->VAO);
		eGlDrawContext::GetInstance().DrawElementsInstanced(GL_TRIANGLES, (GLsizei)this->indices.size(), GL_UNSIGNED_INT, 0, instances, "ParticleMesh");
		glBindVertexArray(0);
	}
}

//--------------------------------------------------------------------------------------------------------------
std::vector<TextureInfo> ParticleMesh::GetTextures() const
{
  std::vector<TextureInfo> ret;
  for (const Texture& t : textures)
    ret.emplace_back(t.m_type, t.m_path);
  return ret;
}

//--------------------------------------------------------------------------------------------------------------
void ParticleMesh::setupMesh()
{
	glGenVertexArrays(1, &this->VAO);
	glGenBuffers(1, &this->VBO);
	glGenBuffers(1, &this->EBO);

	glBindVertexArray(this->VAO);
	glBindBuffer(GL_ARRAY_BUFFER, this->VBO);

	glBufferData(GL_ARRAY_BUFFER, this->vertices.size() * sizeof(Vertex),
		&this->vertices[0], GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, this->indices.size() * sizeof(GLuint),
		&this->indices[0], GL_STATIC_DRAW);

	// Vertex Positions
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
		(GLvoid*)0);

	//Instancing
	glGenBuffers(1, &this->VBOinstanced);
	glBindBuffer(GL_ARRAY_BUFFER, this->VBOinstanced);

	glBufferData(GL_ARRAY_BUFFER, SIZEOF * MAXPARTICLES, 0, GL_DYNAMIC_DRAW);

	for (int i = 0; i < 4; ++i) 
	{
		glEnableVertexAttribArray(i+6);
		glVertexAttribPointer(i+6, 4, GL_FLOAT, GL_FALSE, SIZEOF, (const GLvoid*)((sizeof(float) * 4)*i));
		glVertexAttribDivisor(i+6, 1);
	}

	glEnableVertexAttribArray(10);
	glVertexAttribPointer(10, 2, GL_FLOAT, GL_FALSE, SIZEOF, (GLvoid*)(sizeof(float) * 16));
	glVertexAttribDivisor(10, 1);

	glEnableVertexAttribArray(11);
	glVertexAttribPointer(11, 2, GL_FLOAT, GL_FALSE, SIZEOF, (GLvoid*)(sizeof(float) * 18));
	glVertexAttribDivisor(11, 1);

	glEnableVertexAttribArray(12);
	glVertexAttribPointer(12, 2, GL_FLOAT, GL_FALSE, SIZEOF, (GLvoid*)(sizeof(float) * 20));
	glVertexAttribDivisor(12, 1);

	glBindVertexArray(0);
}

//------------------------------------------------------------
void ParticleMesh::updateInstancedData(std::vector<float>& buffer)
{
	//glBindVertexArray(this->VAO);
	glBindBuffer(GL_ARRAY_BUFFER, this->VBOinstanced);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float)*buffer.size()/*size*/, &buffer[0]/*data*/, GL_DYNAMIC_DRAW);
	//glBindVertexArray(0);
}

//------------------------------------------------------------
SimpleGeometryMesh::SimpleGeometryMesh(const std::vector<glm::vec3>& _dots, float _radius, GeometryType _type, glm::vec3 _color)
	: m_dots(_dots)
	, m_radius(_radius)
	, m_type(_type)
	, m_color(_color, 1.0f)
{
	// Setup hex VAO
	if (!m_dots.empty())
	{
		glGenVertexArrays(1, &hexVAO);
		glGenBuffers(1, &hexVBO);
		glBindVertexArray(hexVAO);
		glBindBuffer(GL_ARRAY_BUFFER, hexVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * m_dots.size(), &m_dots[0], GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
		glBindVertexArray(0);
	}
}

//---------------------------------------------------------------------------------------
SimpleGeometryMesh::~SimpleGeometryMesh()
{
	glDeleteVertexArrays(1, &hexVAO);
	glDeleteBuffers(1, &hexVBO);
}

//-------------------------------------------------------------------------------------
void SimpleGeometryMesh::SetColor(glm::vec4 _c)
{
	m_color = _c;
}

//-------------------------------------------------------------------------------------
void SimpleGeometryMesh::SetDots(const std::vector<glm::vec3>& _dots)
{
	m_dots = _dots;
	glBindBuffer(GL_ARRAY_BUFFER, hexVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * m_dots.size(), &m_dots[0], GL_DYNAMIC_DRAW);
}

//-----------------------------------------------------------
void SimpleGeometryMesh::Draw()
{
	if (hexVAO != -1)
	{
		glBindVertexArray(hexVAO);
		eGlDrawContext::GetInstance().DrawArrays(GL_POINTS, 0, (GLsizei)m_dots.size(), "SimpleGeometryMesh");
		glBindVertexArray(0);
	}
}

//-------------------------------------------
BezierCurveMesh::BezierCurveMesh(const dbb::Bezier& _bezier, bool _is2D)
	: m_bezier(_bezier)
	, m_2d(_is2D)
{
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * 4, &m_bezier, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
	glBindVertexArray(0);
}

//-------------------------------------------
BezierCurveMesh::~BezierCurveMesh()
{
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
}

//-------------------------------------------
void BezierCurveMesh::Draw()
{
	if (VAO != -1)
	{
		glPatchParameteri(GL_PATCH_VERTICES, 4);
		glBindVertexArray(VAO);
		eGlDrawContext::GetInstance().DrawArrays(GL_PATCHES, 0, 4, "BezierCurveMesh");
		glBindVertexArray(0);
	}
}

//-------------------------------------------
void BezierCurveMesh::Update()
{
	if (VAO != -1)
	{
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * 4, &m_bezier, GL_DYNAMIC_DRAW);
	}
}

//-------------------------------------------
LineMesh::LineMesh(const std::vector<glm::vec3>& _verices, const std::vector<GLuint>& _indices, glm::vec4 _color)
	: m_verices(_verices)
	, m_indices(_indices)
	, m_color(_color)
{
	// Setup VAO
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
	glBindVertexArray(0);
}

//-------------------------------------------
LineMesh::~LineMesh()
{
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &EBO);
}

//-------------------------------------------
void LineMesh::UpdateData(const std::vector<glm::vec3>& _verices, const std::vector<GLuint>& _indices, glm::vec4 _color)
{
	m_verices = _verices;
	m_indices = _indices;
	m_color = _color;
}

//---------------------------------------------------------------------------------------------------
void LineMesh::Draw()
{
	if (VAO != -1)
	{
		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * m_verices.size(), &m_verices[0], GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(GLuint), &m_indices[0], GL_DYNAMIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);

		eGlDrawContext::GetInstance().DrawElements(GL_LINES, (GLsizei)m_indices.size(), GL_UNSIGNED_INT, 0, "LineMesh");
		glBindVertexArray(0);
	}
}
