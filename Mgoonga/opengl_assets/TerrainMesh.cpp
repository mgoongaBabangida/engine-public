#include "stdafx.h"

#include "TerrainMesh.h"

#include <base/Log.h>
#include <math/Camera.h>
#include <opengl_assets/GlDrawContext.h>

#include "Windows_Related_api.h"

namespace {
	inline void RecomputeNormalsFromIndices(std::vector<Vertex>& verts, const std::vector<unsigned>& indices)
	{
		for (auto& v : verts) v.Normal = glm::vec3(0.0f);

		for (size_t i = 0; i + 2 < indices.size(); i += 3)
		{
			const glm::vec3& p1 = verts[indices[i + 0]].Position;
			const glm::vec3& p2 = verts[indices[i + 1]].Position;
			const glm::vec3& p3 = verts[indices[i + 2]].Position;

			glm::vec3 n = glm::cross(p2 - p1, p3 - p1);
			// avoid NaNs on degenerate tris
			if (glm::length2(n) > 0.0f) n = glm::normalize(n);

			verts[indices[i + 0]].Normal += n;
			verts[indices[i + 1]].Normal += n;
			verts[indices[i + 2]].Normal += n;
		}

		for (auto& v : verts)
		{
			if (glm::length2(v.Normal) > 0.0f) v.Normal = glm::normalize(v.Normal);
			else v.Normal = glm::vec3(0, 1, 0);
		}
	}
}

//-------------------------------------------------------------------------
static inline glm::vec2 CubeToWorldXZ(const glm::ivec3& cube,
																			math::Hex::Orientation o,
																			float R /* outer radius */)
{
	const int q = cube.x, r = cube.z;
	if (o == math::Hex::Orientation::Flat)
	{
		return { R * 1.5f * q, R * sqrtf(3.0f) * (r + 0.5f * q) };
	}
	else
	{ // Pointy
		return { R * sqrtf(3.0f) * (q + 0.5f * r), R * 1.5f * r };
	}
}

//---------------------------------------------------
TerrainMesh::TerrainMesh(const std::string& _name)
  : Brush(_name) , m_position(0, 0), m_world_offset(0, 0)
{
	/*m_normalMap = Texture::GetTexture1x1(TColor::BLUE);*/
}

//----------------------------------------------------------------------------
void TerrainMesh::Draw()
{
	if (m_camera && !indicesLods.empty())
	{
		glm::vec3 center = {
			0.5f * (m_extrems.MinX + m_extrems.MaxX) + m_world_offset.x,
			0.5f * (m_extrems.MinY + m_extrems.MaxY),
			0.5f * (m_extrems.MinZ + m_extrems.MaxZ) + m_world_offset.y
		};

		float dist = glm::length(m_camera->getPosition() - center);
		for (size_t i = indicesLods.size(); i > 0; --i)
		{
			if (dist >= m_LOD_Step * i)
			{
				SwitchLOD(i);
				break;
			}
		}
	}

	Brush::Draw();
}

//----------------------------------------------------------------------------
void TerrainMesh::MakePlaneVerts(unsigned int _dimensions, bool _spread_texture)
{
	m_rows = _dimensions; m_columns = _dimensions; m_size = _dimensions;
	m_vertices.resize(_dimensions * _dimensions);

	const float half = float(_dimensions - 1) * 0.5f;

	m_extrems.MinX = m_extrems.MinZ = +std::numeric_limits<float>::max();
	m_extrems.MaxX = m_extrems.MaxZ = -std::numeric_limits<float>::max();
	m_extrems.MinY = m_extrems.MaxY = 0.0f;

	for (unsigned i = 0; i < _dimensions; ++i) {
		for (unsigned j = 0; j < _dimensions; ++j) {
			Vertex& v = m_vertices[i * _dimensions + j];
			v.Position.x = (float(j) - half) / float(m_devisor);
			v.Position.z = (float(i) - half) / float(m_devisor);
			v.Position.y = 0.0f;
			v.Normal = { 0,1,0 };

			if (_spread_texture) {
				v.TexCoords.x = j / float(_dimensions - 1);
				v.TexCoords.y = i / float(_dimensions - 1);
			}
			else {
				v.TexCoords.x = (j & 1) ? 0.f : 1.f;
				v.TexCoords.y = (i & 1) ? 0.f : 1.f;
			}

			// local AABB
			m_extrems.MinX = std::min(m_extrems.MinX, v.Position.x);
			m_extrems.MaxX = std::max(m_extrems.MaxX, v.Position.x);
			m_extrems.MinZ = std::min(m_extrems.MinZ, v.Position.z);
			m_extrems.MaxZ = std::max(m_extrems.MaxZ, v.Position.z);
		}
	}
	_UpdateWorldOffset(); // apply placement (cube or legacy grid) via world offset
}

//----------------------------------------------------------------------------
void TerrainMesh::MakePlaneVerts(unsigned int _rows, unsigned int _columns, bool _spread_texture)
{
	m_rows = _rows; m_columns = _columns;
	m_vertices.resize(_rows * _columns);

	const float half_r = float(_rows - 1) * 0.5f;
	const float half_c = float(_columns - 1) * 0.5f;

	m_extrems.MinX = m_extrems.MinZ = +std::numeric_limits<float>::max();
	m_extrems.MaxX = m_extrems.MaxZ = -std::numeric_limits<float>::max();
	m_extrems.MinY = m_extrems.MaxY = 0.0f;

	for (unsigned c = 0; c < _columns; ++c) {
		for (unsigned r = 0; r < _rows; ++r) {
			Vertex& v = m_vertices[c * _rows + r];
			v.Position.x = (float(r) - half_r) / float(m_devisor);
			v.Position.z = (float(c) - half_c) / float(m_devisor);
			v.Position.y = 0.0f;
			v.Normal = { 0,1,0 };

			if (_spread_texture) {
				v.TexCoords.x = r / float(_rows - 1);
				v.TexCoords.y = c / float(_columns - 1);
			}
			else {
				v.TexCoords.x = (r & 1) ? 0.f : 1.f;
				v.TexCoords.y = (c & 1) ? 1.f : 0.f;
			}

			m_extrems.MinX = std::min(m_extrems.MinX, v.Position.x);
			m_extrems.MaxX = std::max(m_extrems.MaxX, v.Position.x);
			m_extrems.MinZ = std::min(m_extrems.MinZ, v.Position.z);
			m_extrems.MaxZ = std::max(m_extrems.MaxZ, v.Position.z);
		}
	}
	_UpdateWorldOffset();
}

//---------------------------------------------------------------------------
void TerrainMesh::MakePlaneIndices(unsigned int _dimensions)
{
	const int quads = (_dimensions - 1) * (_dimensions - 1);
	this->indicesLods.resize(std::max<size_t>(1, indicesLods.size()));
	this->indicesLods[0].resize(quads * 6);

	int runner = 0;
	for (int row = 0; row < _dimensions - 1; ++row)
	{
		for (int col = 0; col < _dimensions - 1; ++col)
		{
			// tri 1
			this->indicesLods[0][runner++] = _dimensions * row + col;
			this->indicesLods[0][runner++] = _dimensions * (row + 1) + col;
			this->indicesLods[0][runner++] = _dimensions * (row + 1) + (col + 1);
			// tri 2
			this->indicesLods[0][runner++] = _dimensions * row + col;
			this->indicesLods[0][runner++] = _dimensions * (row + 1) + (col + 1);
			this->indicesLods[0][runner++] = _dimensions * row + (col + 1);
		}
	}
	// guard in case anything changes above
	this->indicesLods[0].resize(runner);
}

//-----------------------------------------------------------------------------------------------
void TerrainMesh::SetCamera(Camera* _camera)
{
	m_camera = _camera;
}

//----------------------------------------------------------------------------------------------- -
void TerrainMesh::SetTessellationRenderingInfo(const TessellationRenderingInfo& _info)
{
	glm::vec2 chunk_offset_xz = m_tessellation_info.chunk_offset_xz;
	glm::vec2 chunk_scale_xz = m_tessellation_info.chunk_scale_xz;

	m_tessellation_info = _info;
	m_tessellation_info.world_offset = m_world_offset;

	if (chunk_offset_xz != glm::vec2{ 0,0 }
		&& chunk_scale_xz != glm::vec2{ 1,1 }) // if set -> dont change
	{
		m_tessellation_info.chunk_offset_xz = chunk_offset_xz;
		m_tessellation_info.chunk_scale_xz = chunk_scale_xz;
	}
}

//-----------------------------------------------------------------------------------------------
std::vector<glm::mat3> TerrainMesh::GetBoundingTriangles() const
{
	std::vector<glm::mat3> ret; // Getting 12 triangles of the bouning cube
	ret.push_back(glm::mat3(glm::vec3(m_extrems.MaxX, m_extrems.MaxY, m_extrems.MinZ),
		glm::vec3(m_extrems.MinX, m_extrems.MaxY, m_extrems.MaxZ),
		glm::vec3(m_extrems.MaxX, m_extrems.MaxY, m_extrems.MaxZ)));

	ret.push_back(glm::mat3(glm::vec3(m_extrems.MaxX, m_extrems.MaxY, m_extrems.MinZ),
		glm::vec3(m_extrems.MinX, m_extrems.MaxY, m_extrems.MinZ),
		glm::vec3(m_extrems.MinX, m_extrems.MaxY, m_extrems.MaxZ)));

	ret.push_back(glm::mat3(glm::vec3(m_extrems.MaxX, m_extrems.MinY, m_extrems.MinZ),
		glm::vec3(m_extrems.MinX, m_extrems.MinY, m_extrems.MaxZ),
		glm::vec3(m_extrems.MaxX, m_extrems.MinY, m_extrems.MaxZ)));

	ret.push_back(glm::mat3(glm::vec3(m_extrems.MaxX, m_extrems.MinY, m_extrems.MinZ),
		glm::vec3(m_extrems.MinX, m_extrems.MinY, m_extrems.MinZ),
		glm::vec3(m_extrems.MinX, m_extrems.MinY, m_extrems.MaxZ)));

	ret.push_back(glm::mat3(glm::vec3(m_extrems.MinX, m_extrems.MaxY, m_extrems.MaxZ),
		glm::vec3(m_extrems.MinX, m_extrems.MinY, m_extrems.MaxZ),
		glm::vec3(m_extrems.MaxX, m_extrems.MinY, m_extrems.MaxZ)));

	ret.push_back(glm::mat3(glm::vec3(m_extrems.MinX, m_extrems.MaxY, m_extrems.MaxZ),
		glm::vec3(m_extrems.MaxX, m_extrems.MinY, m_extrems.MaxZ),
		glm::vec3(m_extrems.MaxX, m_extrems.MaxY, m_extrems.MaxZ)));

	ret.push_back(glm::mat3(glm::vec3(m_extrems.MinX, m_extrems.MaxY, m_extrems.MinZ),
		glm::vec3(m_extrems.MinX, m_extrems.MinY, m_extrems.MinZ),
		glm::vec3(m_extrems.MaxX, m_extrems.MinY, m_extrems.MinZ)));

	ret.push_back(glm::mat3(glm::vec3(m_extrems.MinX, m_extrems.MaxY, m_extrems.MinZ),
		glm::vec3(m_extrems.MaxX, m_extrems.MinY, m_extrems.MinZ),
		glm::vec3(m_extrems.MaxX, m_extrems.MaxY, m_extrems.MinZ)));

	ret.push_back(glm::mat3(glm::vec3(m_extrems.MaxX, m_extrems.MaxY, m_extrems.MinZ),
		glm::vec3(m_extrems.MaxX, m_extrems.MinY, m_extrems.MinZ),
		glm::vec3(m_extrems.MaxX, m_extrems.MinY, m_extrems.MaxZ)));

	ret.push_back(glm::mat3(glm::vec3(m_extrems.MaxX, m_extrems.MaxY, m_extrems.MinZ),
		glm::vec3(m_extrems.MaxX, m_extrems.MinY, m_extrems.MaxZ),
		glm::vec3(m_extrems.MaxX, m_extrems.MaxY, m_extrems.MaxZ)));

	ret.push_back(glm::mat3(glm::vec3(m_extrems.MinX, m_extrems.MaxY, m_extrems.MinZ),
		glm::vec3(m_extrems.MinX, m_extrems.MinY, m_extrems.MinZ),
		glm::vec3(m_extrems.MinX, m_extrems.MinY, m_extrems.MaxZ)));

	ret.push_back(glm::mat3(glm::vec3(m_extrems.MinX, m_extrems.MaxY, m_extrems.MinZ),
		glm::vec3(m_extrems.MinX, m_extrems.MinY, m_extrems.MaxZ),
		glm::vec3(m_extrems.MinX, m_extrems.MaxY, m_extrems.MaxZ)));
	return ret;
}

//-----------------------------------------------------------------------------------------------
std::vector<glm::vec3> TerrainMesh::GetExtrems() const
{
	std::vector<glm::vec3> ret;
	ret.push_back(glm::vec3(m_extrems.MaxX, m_extrems.MaxY, m_extrems.MaxZ));
	ret.push_back(glm::vec3(m_extrems.MaxX, m_extrems.MaxY, m_extrems.MinZ));
	ret.push_back(glm::vec3(m_extrems.MinX, m_extrems.MaxY, m_extrems.MinZ));
	ret.push_back(glm::vec3(m_extrems.MinX, m_extrems.MaxY, m_extrems.MaxZ));
	ret.push_back(glm::vec3(m_extrems.MaxX, m_extrems.MinY, m_extrems.MaxZ));
	ret.push_back(glm::vec3(m_extrems.MaxX, m_extrems.MinY, m_extrems.MinZ));
	ret.push_back(glm::vec3(m_extrems.MinX, m_extrems.MinY, m_extrems.MinZ));
	ret.push_back(glm::vec3(m_extrems.MinX, m_extrems.MinY, m_extrems.MaxZ));
	return ret;
}

//---------------------------------------------------------------------------------
void TerrainMesh::SetExtremDots(const extremDots& _extrems)
{
	m_extrems = _extrems;
}

//-----------------------------------------------------------------------------------------------
extremDots TerrainMesh::GetExtremDots() const
{
	return m_extrems;
}

//-----------------------------------------------------------------------------------------------
glm::vec3 TerrainMesh::GetCenter() const
{
	return glm::vec3(
		0.5f * (m_extrems.MinX + m_extrems.MaxX),
		0.5f * (m_extrems.MinY + m_extrems.MaxY),
		0.5f * (m_extrems.MinZ + m_extrems.MaxZ)
	);
}

//-----------------------------------------------------------------------------------------------
void TerrainMesh::AssignHeights(const Texture& _heightMap,
																float _height_scale,
																float _max_height,
																float _min_height,
																int32_t _normal_sharpness,
																bool _apply_normal_blur)
{
	if (!HasCurrentOpenGLContext())
		base::Log("TerrainMesh::AssignHeights is called in non GL thread");

	// Reset vertical bounds before assigning any vertex heights
	m_extrems.MinY = std::numeric_limits<float>::infinity();
	m_extrems.MaxY = -std::numeric_limits<float>::infinity();

	glBindTexture(GL_TEXTURE_2D, _heightMap.m_id);

	if (_heightMap.m_channels == 4)
	{
		GLfloat* buffer = new GLfloat[_heightMap.m_height * _heightMap.m_width * 4];
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, buffer);
		m_heightMap.TextureFromBuffer((GLfloat*)buffer,
			_heightMap.m_width,
			_heightMap.m_height,
			GL_RGBA, GL_REPEAT, GL_LINEAR);

		if (_heightMap.m_height == m_columns && _heightMap.m_width == m_rows)
		{
			for (int i = 0; i < _heightMap.m_height * _heightMap.m_width * 4; i += 4)
			{
				float h = buffer[i] * _height_scale;
				// clamp to [min, max]
				h = std::max(_min_height, std::min(h, _max_height));

				Vertex& v = this->m_vertices[i / 4];
				v.Position.y = h;

				m_extrems.MinY = std::min(m_extrems.MinY, h);
				m_extrems.MaxY = std::max(m_extrems.MaxY, h);
			}
		}
		else
		{
			// @todo: handle resampling for RGBA heightmaps if you need it
		}

		delete[] buffer;
	}
	else if (_heightMap.m_channels == 1)
	{
		GLfloat* buffer = new GLfloat[_heightMap.m_height * _heightMap.m_width];
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, buffer);
		m_heightMap.TextureFromBuffer((GLfloat*)buffer,
			_heightMap.m_width,
			_heightMap.m_height,
			GL_RED, GL_REPEAT, GL_LINEAR);

		if (_heightMap.m_height == m_rows && _heightMap.m_width == m_columns)
		{
			for (int i = 0; i < _heightMap.m_height * _heightMap.m_width; ++i)
			{
				float h = buffer[i] * _height_scale;
				h = std::max(_min_height, std::min(h, _max_height));

				this->m_vertices[i].Position.y = h;
				m_extrems.MinY = std::min(m_extrems.MinY, h);
				m_extrems.MaxY = std::max(m_extrems.MaxY, h);
			}
		}
		else
		{
			unsigned int height_map_res_ratio = _heightMap.m_height / m_rows;
			for (int col = 0; col < m_columns; ++col)
			{
				for (int row = 0; row < m_rows; ++row)
				{
					unsigned int index = col * _heightMap.m_width * height_map_res_ratio
						+ row * height_map_res_ratio;

					float h = buffer[index] * _height_scale;
					h = std::max(_min_height, std::min(h, _max_height));

					auto& v = this->m_vertices[col * m_columns + row];
					v.Position.y = h;

					m_extrems.MinY = std::min(m_extrems.MinY, h);
					m_extrems.MaxY = std::max(m_extrems.MaxY, h);
				}
			}
		}

		_GenerateNormalMap(buffer, _heightMap.m_width, _heightMap.m_height, _normal_sharpness, _apply_normal_blur);
		delete[] buffer;
	}

	glBindTexture(GL_TEXTURE_2D, 0);
}

//--------------------------------------------------------------------------------------------
void TerrainMesh::MakePlaneIndices(unsigned int _rows, unsigned int _columns, unsigned int _lod)
{
	if (this->indicesLods.size() < _lod)
		this->indicesLods.resize(_lod);

	auto& idx = this->indicesLods[_lod - 1];
	idx.clear();

	const unsigned int lod_scale = 1u << (_lod - 1);
	// optional pre-reserve: each quad -> 6 indices
	const int qRows = (_rows - lod_scale) / lod_scale;
	const int qCols = (_columns - lod_scale) / lod_scale;
	if (qRows > 0 && qCols > 0) idx.reserve(qRows * qCols * 6);

	int runner = 0;
	for (int col = 0; col < int(_columns) - int(lod_scale); col += lod_scale)
	{
		for (int row = 0; row < int(_rows) - int(lod_scale); row += lod_scale)
		{
			// tri 1
			idx.push_back(col * _rows + row);
			idx.push_back((col + lod_scale) * _rows + row);
			idx.push_back((col + lod_scale) * _rows + (row + lod_scale));

			// tri 2
			idx.push_back(col * _rows + row);
			idx.push_back((col + lod_scale) * _rows + (row + lod_scale));
			idx.push_back(col * _rows + (row + lod_scale));

			runner += 6;
		}
	}
}

//---------------------------------------------------------------------------
Texture* TerrainMesh::GenerateNormals(GLuint _size)
{
	if (indicesLods.empty() || indicesLods[0].empty()) return nullptr;
	RecomputeNormalsFromIndices(this->m_vertices, this->indicesLods[0]);
	/*if (bakeNormalsToTexture) {*/
		std::vector<GLfloat> buf(m_vertices.size() * 3);
		for (size_t i = 0; i < m_vertices.size(); ++i) {
			glm::vec3 n = glm::normalize(m_vertices[i].Normal);
			buf[i * 3 + 0] = n.x * 0.5f + 0.5f; // [-1,1] -> [0,1]
			buf[i * 3 + 1] = n.y * 0.5f + 0.5f;
			buf[i * 3 + 2] = n.z * 0.5f + 0.5f;
		}
		m_normalMap.TextureFromBuffer(buf.data(), m_rows, m_columns, GL_RGB, GL_REPEAT, GL_LINEAR);
	//}
	return &m_normalMap;
}

//---------------------------------------------------------------------------
Texture* TerrainMesh::GenerateNormals(GLuint rows, GLuint columns)
{
	if (indicesLods.empty() || indicesLods[0].empty()) return nullptr;

	RecomputeNormalsFromIndices(this->m_vertices, this->indicesLods[0]);

	std::vector<GLfloat> buf(m_vertices.size() * 3);
	for (size_t i = 0; i < m_vertices.size(); ++i)
	{
		glm::vec3 n = glm::normalize(m_vertices[i].Normal);
		buf[i * 3 + 0] = n.x * 0.5f + 0.5f;
		buf[i * 3 + 1] = n.y * 0.5f + 0.5f;
		buf[i * 3 + 2] = n.z * 0.5f + 0.5f;
	}

	m_normalMap.TextureFromBuffer(buf.data(), m_rows, m_columns, GL_RGB, GL_REPEAT, GL_LINEAR);
	return &m_normalMap;
}

//---------------------------------------------------------------------------
void TerrainMesh::GenerateTessellationData()
{
	m_tessellation_data.Clear();
	if (this->indicesLods.empty())
		return;

	m_tessellation_data.m_vertices.reserve(indicesLods.back().size() / 6 * 20);
	for (unsigned int i = 0; i < indicesLods.back().size(); i += 6) // take the lowest detailed mesh
	{
		// indices in patch quad -> 0, 1, 2, 5
		m_tessellation_data.m_vertices.push_back(this->m_vertices[this->indicesLods.back()[i]].Position.x); // v.x
		m_tessellation_data.m_vertices.push_back(0.0f); // v.y
		m_tessellation_data.m_vertices.push_back(this->m_vertices[this->indicesLods.back()[i]].Position.z); // v.z
		m_tessellation_data.m_vertices.push_back(this->m_vertices[this->indicesLods.back()[i]].TexCoords.x); // u
		m_tessellation_data.m_vertices.push_back(this->m_vertices[this->indicesLods.back()[i]].TexCoords.y); // v

		m_tessellation_data.m_vertices.push_back(this->m_vertices[this->indicesLods.back()[i + 5]].Position.x); // v.x
		m_tessellation_data.m_vertices.push_back(0.0f); // v.y
		m_tessellation_data.m_vertices.push_back(this->m_vertices[this->indicesLods.back()[i + 5]].Position.z); // v.z
		m_tessellation_data.m_vertices.push_back(this->m_vertices[this->indicesLods.back()[i + 5]].TexCoords.x); // u
		m_tessellation_data.m_vertices.push_back(this->m_vertices[this->indicesLods.back()[i + 5]].TexCoords.y); // v

		m_tessellation_data.m_vertices.push_back(this->m_vertices[this->indicesLods.back()[i + 1]].Position.x); // v.x
		m_tessellation_data.m_vertices.push_back(0.0f); // v.y
		m_tessellation_data.m_vertices.push_back(this->m_vertices[this->indicesLods.back()[i + 1]].Position.z); // v.z
		m_tessellation_data.m_vertices.push_back(this->m_vertices[this->indicesLods.back()[i + 1]].TexCoords.x); // u
		m_tessellation_data.m_vertices.push_back(this->m_vertices[this->indicesLods.back()[i + 1]].TexCoords.y); // v

		m_tessellation_data.m_vertices.push_back(this->m_vertices[this->indicesLods.back()[i + 2]].Position.x); // v.x
		m_tessellation_data.m_vertices.push_back(0.0f); // v.y
		m_tessellation_data.m_vertices.push_back(this->m_vertices[this->indicesLods.back()[i + 2]].Position.z); // v.z
		m_tessellation_data.m_vertices.push_back(this->m_vertices[this->indicesLods.back()[i + 2]].TexCoords.x); // u
		m_tessellation_data.m_vertices.push_back(this->m_vertices[this->indicesLods.back()[i + 2]].TexCoords.y); // v
	}

	glGenVertexArrays(1, &m_tessellation_data.m_terrainVAO);
	glBindVertexArray(m_tessellation_data.m_terrainVAO);

	glGenBuffers(1, &m_tessellation_data.m_terrainVBO);
	glBindBuffer(GL_ARRAY_BUFFER, m_tessellation_data.m_terrainVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * m_tessellation_data.m_vertices.size(), &m_tessellation_data.m_vertices[0], GL_STATIC_DRAW);

	// position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	// texCoord attribute
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(sizeof(float) * 3));
	glEnableVertexAttribArray(1);
}

//---------------------------------------------------------------------------
void TerrainMesh::DrawTessellated(std::function<void(const TessellationRenderingInfo&)> _info_updater)
{
	// Keep info in sync with the actual mesh state
	m_tessellation_info.world_offset = m_world_offset;
	m_tessellation_info.heightmap_resolution = glm::vec2(float(m_rows), float(m_columns));
	if (m_tessellation_info.color_count == 0)
		m_tessellation_info.color_count = int(std::min<size_t>(m_tessellation_info.base_start_heights.size(), 8));

	if (!m_tessellation_info.base_start_heights.empty() && !m_tessellation_info.texture_scale.empty())
		_info_updater(m_tessellation_info);

	glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, m_heightMap.m_id);
	glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, m_normalMap.m_id);

	glPatchParameteri(GL_PATCH_VERTICES, 4);
	glBindVertexArray(m_tessellation_data.m_terrainVAO);

	const GLsizei vertexCount = GLsizei(m_tessellation_data.m_vertices.size() / 5); // 5 floats per vertex
	eGlDrawContext::GetInstance().DrawArrays(GL_PATCHES, 0, vertexCount, "TerrainMesh");
}

//---------------------------------------------------------------------------
void TerrainMesh::_UpdateWorldOffset()
{
	if (m_placeMode == PlacementMode::CubeOffset)
	{
		m_world_offset = CubeToWorldXZ(m_cube, m_orientation, m_outerRadius);
	}
	else 
	{ // GridOffset (legacy), but keep mesh local; move offset to world
 // Reproduce the previous baked shift: ((dim-1) * pos) / divisor
		const float ox = (int(m_rows) > 0) ? (float(int(m_rows) - 1) * m_position.x) / float(m_devisor) : 0.0f;
		const float oz = (int(m_columns) > 0) ? (float(int(m_columns) - 1) * m_position.y) / float(m_devisor) : 0.0f;
		m_world_offset = { ox, oz };
	}
}

//---------------------------------------------------------------------------------
void TerrainMesh::SetCube(const glm::ivec3& c, math::Hex::Orientation o, float outerRadius)
{
	m_cube = c;
	m_orientation = o;
	m_outerRadius = outerRadius;
	m_placeMode = PlacementMode::CubeOffset;
	_UpdateWorldOffset();
}

//---------------------------------------------------------------------------
std::optional<Vertex> TerrainMesh::FindVertex(float _x, float _z)
{
	if (m_rows == 0 || m_columns == 0) return std::nullopt;

	// Convert world → local by removing our placement offset
	const float lx = _x - m_world_offset.x;
	const float lz = _z - m_world_offset.y;

	const int half_r = int(m_rows) / 2;
	const int half_c = int(m_columns) / 2;

	// Inverse of local placement: X = (j - half_r) / m_devisor
	const float j_f = lx * m_devisor + half_r;
	const float i_f = lz * m_devisor + half_c;

	const int j = int(std::round(j_f));
	const int i = int(std::round(i_f));

	if (j < 0 || j >= int(m_rows) || i < 0 || i >= int(m_columns)) return std::nullopt;

	const size_t idx = size_t(i) * size_t(m_rows) + size_t(j);
	if (idx >= m_vertices.size()) return std::nullopt;

	return m_vertices[idx];
}

//---------------------------------------------------------------------------
void TerrainMesh::_GenerateNormalMap(const GLfloat* _heightmap, unsigned int _width, unsigned int _height, int32_t _normal_sharpness, bool _apply_normal_blur)
{
	std::vector<GLfloat> normalMapBuffer(_width * _height * 3);
	for (int y = 0; y < _height; ++y)
	{
		for (int x = 0; x < _width; ++x)
		{
			float hL = _heightmap[y * _width + std::max(x - 1, 0)];
			float hR = _heightmap[y * _width + std::min((int)x + 1, (int)_width - 1)];
			float hU = _heightmap[std::max(y - 1, 0) * _width + x];
			float hD = _heightmap[std::min((int)y + 1, (int)_height - 1) * _width + x];

			glm::vec3 normal;
			normal.x = hL - hR;
			normal.z = hU - hD;
			normal.y = _normal_sharpness * 0.001f; // You can adjust this value as needed for the vertical scaling
			normal = glm::normalize(normal);
			normal.x = (normal.x + 1.0f) / 2.0f;
			normal.y = (normal.y + 1.0f) / 2.0f;
			normal.z = (normal.z + 1.0f) / 2.0f;
			normalMapBuffer[(y * _width + x) * 3] = normal.x;
			normalMapBuffer[(y * _width + x) * 3 + 1] = normal.y;
			normalMapBuffer[(y * _width + x) * 3 + 2] = normal.z;
		}
	}

	if(_apply_normal_blur)
		_SmoothNormals(normalMapBuffer, _width, _height);

	m_normalMap.TextureFromBuffer(&normalMapBuffer[0], _width, _height, GL_RGB, GL_REPEAT, GL_LINEAR);
}

// Function to apply Gaussian blur to a normal map
//---------------------------------------------------------------------------
void TerrainMesh::_SmoothNormals(std::vector<float>& normalMap, int width, int height)
{
	// Define Gaussian kernel size (e.g., 5x5)

	// Calculate half kernel size
	int halfKernel = m_kernel_size / 2;

	// Generate Gaussian kernel
	std::vector<float> kernel(m_kernel_size * m_kernel_size);
	float sum = 0.0f;
	for (int i = -halfKernel; i <= halfKernel; ++i) {
		for (int j = -halfKernel; j <= halfKernel; ++j) {
			int index = (i + halfKernel) * m_kernel_size + (j + halfKernel);
			float weight = exp(-(i * i + j * j) / (2 * m_normal_sigma * m_normal_sigma));
			kernel[index] = weight;
			sum += weight;
		}
	}

	// Normalize the kernel
	for (int i = 0; i < m_kernel_size * m_kernel_size; ++i) {
		kernel[i] /= sum;
	}

	// Apply the Gaussian blur separately to each component of the normal vectors
	std::vector<float> smoothedNormalMap(normalMap.size());
	for (int c = 0; c < 3; ++c) { // Iterate over x, y, z components
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				float sum = 0.0f;
				for (int ky = -halfKernel; ky <= halfKernel; ++ky) {
					for (int kx = -halfKernel; kx <= halfKernel; ++kx) {
						int offsetX = x + kx;
						int offsetY = y + ky;
						if (offsetX >= 0 && offsetX < width && offsetY >= 0 && offsetY < height) {
							int index = (offsetY * width + offsetX) * 3 + c; // Component index
							int kernelIndex = (ky + halfKernel) * m_kernel_size + (kx + halfKernel);
							sum += normalMap[index] * kernel[kernelIndex];
						}
					}
				}
				int index = (y * width + x) * 3 + c; // Component index
				smoothedNormalMap[index] = sum;
			}
		}
	}

	// Normalize the smoothed normal vectors
	for (size_t i = 0; i < smoothedNormalMap.size(); i += 3) {
		float x = smoothedNormalMap[i];
		float y = smoothedNormalMap[i + 1];
		float z = smoothedNormalMap[i + 2];
		float length = sqrt(x * x + y * y + z * z);
		if (length > 0.0f) {
			smoothedNormalMap[i] /= length;
			smoothedNormalMap[i + 1] /= length;
			smoothedNormalMap[i + 2] /= length;
		}
	}

	// Copy the smoothed normal map back to the original normal map
	normalMap = smoothedNormalMap;
}
