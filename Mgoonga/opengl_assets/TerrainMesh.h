#pragma once

#include "MyMesh.h"
#include <math/Hex.h>

class Camera;

//--------------------------------------------------------------------------------------------------------------
class TerrainMesh : public Brush
{
public:
  explicit TerrainMesh(const std::string& _name);

	virtual void						Draw()			override;

	void										DrawTessellated(std::function<void(const TessellationRenderingInfo&)> _info_updater);
	void										GenerateTessellationData();

	glm::ivec2							GetPosition() const { return m_position; }
	void										SetPosition(glm::ivec2 _pos) { m_position = _pos; }

	glm::vec2								GetWorlOffset() const { return m_world_offset; }
	/*void										SetWorldOffset(glm::vec2 _pos) { m_world_offset = _pos; }*/

	void										SetCamera(Camera* _camera);
	void										SetTessellationRenderingInfo(const TessellationRenderingInfo&);

	// world bounding box
	void										SetExtremDots(const extremDots&);
	extremDots							GetExtremDots() const;
	std::vector<glm::mat3>	GetBoundingTriangles() const;
	std::vector<glm::vec3>	GetExtrems() const;
	glm::vec3								GetCenter() const;

	void										AssignHeights(const Texture& heightMap, float _height_scale = 1.0f, float _max_height = 1.0f, float _min_height = 0.0f, int32_t _normal_sharpness = 10, bool _apply_normal_blur = false);

	void										MakePlaneIndices(unsigned int rows, unsigned int columns, unsigned int _lod = 1);
	void										MakePlaneIndices(unsigned int dimensions);

	void										MakePlaneVerts(unsigned int dimensions, bool spreed_texture = true);
	void										MakePlaneVerts(unsigned int rows, unsigned int columns, bool spreed_texture = true);

	Texture*								GenerateNormals(GLuint rows, GLuint columns);
	Texture*								GenerateNormals(GLuint size);

	GLuint									GetNormalMapId() const { return m_normalMap.m_id; }

	float										GetNormalSigma() const { return m_normal_sigma; }
	int32_t									GetKernelSize() const { return m_kernel_size; }

	void										SetNormalSigma(float _sigma)  { m_normal_sigma = _sigma; }
	void										SetKernelSize(int32_t _kernel) { m_kernel_size = _kernel; }

  std::optional<Vertex>		FindVertex(float x, float z);

	GLuint		Size() const { return m_size; }
	GLuint		Rows() const { return m_rows; }
	GLuint		Columns() const { return m_columns; }

	// TerrainMesh.h (public)
	enum class PlacementMode { GridOffset, CubeOffset };

	void SetPlacementMode(PlacementMode m) { m_placeMode = m; }
	PlacementMode GetPlacementMode() const { return m_placeMode; }

	void SetCube(const glm::ivec3& c,
							math::Hex::Orientation o,
							float outerRadius);
							const glm::ivec3& GetCube() const
	{ return m_cube; }

	void SetHexLayout(math::Hex::Orientation o, float outerRadius)
	{
		m_orientation = o; m_outerRadius = outerRadius; if (m_placeMode == PlacementMode::CubeOffset) _UpdateWorldOffset();
	}

protected:
	PlacementMode          m_placeMode = PlacementMode::GridOffset; // default = legacy
	glm::ivec3             m_cube{ 0,0,0 };
	math::Hex::Orientation m_orientation = math::Hex::Orientation::Flat;
	float                  m_outerRadius = 1.0f;

	// helper
	void _UpdateWorldOffset();  // recompute m_world_offset from current mode
	void _GenerateNormalMap(const GLfloat* _heightmap, unsigned int _width, unsigned int _height, int32_t normal_sharpness, bool _apply_normal_blur = false);
	void _SmoothNormals(std::vector<float>& normalMap, int width, int height);

	Texture				m_heightMap;
	Texture				m_normalMap;

	GLuint		m_size = 0;
	GLuint		m_rows = 0;
	GLuint		m_columns = 0;

	float					m_normal_sigma = 2.0f;
	int32_t				m_kernel_size = 5;

  unsigned int	m_devisor = 10;
	glm::ivec2		m_position; // index inside model
	glm::vec2			m_world_offset;
	float					m_LOD_Step = 1.5f;

	extremDots m_extrems;

	Camera* m_camera = nullptr;

	struct TessellationData
	{
		unsigned int m_terrainVAO = 0;
		unsigned int m_terrainVBO = 0;
		std::vector<float> m_vertices;

		void Clear()
		{
			m_vertices.clear();
			if (m_terrainVAO) { glDeleteVertexArrays(1, &m_terrainVAO); m_terrainVAO = 0; }
			if (m_terrainVBO) { glDeleteBuffers(1, &m_terrainVBO);     m_terrainVBO = 0; }
		}

		~TessellationData() { Clear(); }
	};
	TessellationData m_tessellation_data;

	TessellationRenderingInfo m_tessellation_info;
};
