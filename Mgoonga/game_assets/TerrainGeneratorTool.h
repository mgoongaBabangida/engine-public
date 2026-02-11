#pragma once

#include <base/interfaces.h>
#include <math/Bezier.h>
#include <glm/glm/gtc/noise.hpp>
#include <opengl_assets/Texture.h>
#include <opengl_assets/TerrainModel.h>

#include <vector>
#include <set>
#include <future>

#include "Generator.h"
#include "Chunk.h"
#include "Params.h"

class IWindowImGui;
class eMainContextBase;
class eModelManager;
class eObject;
class eTextureManager;
class eOpenGlRenderPipeline;

//---------------------------------------------------------------------
class DLL_GAME_ASSETS TerrainGeneratorTool : public IScript
{
public:
  TerrainGeneratorTool(eMainContextBase* _game,
                       eModelManager*  _modelManager,
                       eTextureManager* _texManager,
                       eOpenGlRenderPipeline& _pipeline,
                       IWindowImGui* _imgui);
  virtual ~TerrainGeneratorTool();

  virtual void Update(float _tick) override;
  virtual void Initialize() override;

  void _InitDebugMountain();
  void _InitDebugHill();

  TerrainModel* BuildFromSnapshot(const logic::WorldSnapshot& ws, const terrain::ChunkConfig& cfg, bool path);

 void EnableTesselation();
 void DisableTesselation();
 void SetBPRRenderer(bool);
 void SetNormalMapping(bool);

protected:
  void _GenerateNoiseMap(GLuint _width, GLuint _height, float _scale, GLuint octaves,
                         float persistance, float lacunarity, glm::vec2 offfset, GLuint _seed);

  void _GenerateNoiseMap_WorldAligned(int width, int height, float scale, int octaves, 
                         float persistence, float lacunarity, glm::vec2 chunk_center, glm::vec2 chunk_scale_xz,
                                      float S0x, float S0z, glm::vec2 world_noise_offset, uint32_t seed);

  void _GenerateNoiseMap_WorldAlignedJob(int width, int height, float scale, int octaves, float persistence, 
    float lacunarity, glm::vec2 chunk_center, glm::vec2 chunk_scale_xz, float S0x, float S0z, 
    glm::vec2 world_noise_offset, uint32_t seed, std::vector<float>& out_noise, float& out_local_min, float& out_local_max);

  void _GeneratePlaneNoiseMap();
  void _UpdateRiverNoiseMap();

  void _GenerateColorMap();

  void _UpdateCurrentMesh();
  void _AddCurrentMesh();

  void _GenerateFallOffMap();
  void _GenerateFallOffMapWithFunction();
  void _UpdateShaderUniforms();

  void _ApplyGaussianBlur();

  TessellationRenderingInfo _BuildInfo() const;

  void _CacheChunkStride();

  void _ApplySnapshotBiomesToHeightmap(const logic::WorldSnapshot& ws, int Nx, int Ny, glm::ivec2 chunkIdx, float R, float S0x, float S0z, glm::vec2 chunk_scale_xz, glm::vec2 chunk_offset_xz, float, std::vector<float>& heightmap);

  std::future<bool>        m_generat_noise_task;
  std::vector<GLfloat>     m_noise_map;
  std::vector<glm::vec4>   m_color_map;
  std::vector<GLfloat>     m_falloff_map;
  Texture                  m_noise_texture;
  Texture                  m_color_texture;
  std::set<TerrainType>    m_terrain_types;
  std::shared_ptr<eObject> m_terrain;
  TerrainModel*            m_terrain_pointer = nullptr;

  using eOctavesHeightsBuffer = std::vector<GLfloat>;
  std::vector<eOctavesHeightsBuffer> m_octaves_buffer;

  int         m_cur_pos_X = 0;
  int         m_cur_pos_Y = 0;
  GLuint      m_width = 1024;
  GLuint      m_height = 1024;
  int         m_scale = 500;
  GLuint      m_octaves = 6;
  float       m_persistance = 0.5f;
  float       m_lacunarity = 2.0f;
  glm::ivec2  m_noise_offset = {0,0};
  GLuint      m_seed = 1;
  float       m_height_scale = 1.0f;
  float       m_min_height = 0.f;
  float       m_terrain_height = 0.1f;
  float       m_texture_scale[8];
  float       m_min_tessellation_distance = 5.0f;
  float       m_max_tessellation_distance = 30.0f;
  int         m_snowness = 0;
  
  float             m_max_height_coef = 1.0f;
  float             m_min_height_coef = 0.0f;
  dbb::Bezier       m_interpolation_curve;
  bool              m_use_curve = false;
  bool              m_initialized = false;
  bool              m_auto_update = false;
  std::atomic<bool> m_update_textures = false;
  bool              m_generate_plane = false;
  bool              m_apply_falloff = false;
  bool              m_use_normal_texture_pbr = false;
  bool              m_use_roughness_texture_pbr = true;
  bool              m_use_metalic_texture_pbr = true;
  bool              m_use_ao_texture_pbr = true;
  float             m_normal_mapping_strength = 0.5f;

  float             m_fall_off_a = 3.f;
  float             m_fall_off_b = 2.2f;
  float             m_fall_off_T = 50.f; //for 2nd type
  float             m_fall_off_k = 0.1f; //for 2nd type

  bool              m_apply_blur = false;
  float             m_blur_sigma = 1.0f;
  int32_t           m_blur_kernel_size = 5;
  int32_t           m_normal_sharpness = 10;

  //golbal ranges to avoid seams
  bool  m_has_global_range = false;
  float m_global_min_raw = 0.0f;   // in *raw fBm sum* domain
  float m_global_max_raw = 1.0f;   // in *raw fBm sum* domain
  float m_global_margin_frac = 0.80f;  // ±10% margin

  struct RiverInfo
  {
    float m_radius = 0.1f;
    float m_depth = 0.1f;
    bool m_update = false;
  };
  RiverInfo         m_river_info;
  dbb::Bezier       m_river_curve;

  float m_chunkStrideX = 0;
  float m_chunkStrideZ = 0;

  eMainContextBase*                             m_game = nullptr;
  eModelManager*                                m_modelManager = nullptr;
  eTextureManager*                              m_texture_manager = nullptr;
  std::reference_wrapper<eOpenGlRenderPipeline> m_pipeline;
  IWindowImGui*                                 m_imgui = nullptr;

  bool m_patch = true;

 public:
  struct TerrainTunables
  {
    MountainParams mountain;
    HillParams hill;
    PlainParams plain;
    WaterParams water;
  };
private:
  TerrainTunables m_tunables;   // default-constructed

public:
  void SetTunables(const TerrainTunables& t) { m_tunables = t; }
  const TerrainTunables& Tunables() const { return m_tunables; }
};
