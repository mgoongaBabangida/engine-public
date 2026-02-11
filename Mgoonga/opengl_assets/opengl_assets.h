#pragma once

#ifdef DLLDIR_EX
#define DLL_OPENGL_ASSETS __declspec(dllexport)
#else
#define DLL_OPENGL_ASSETS __declspec(dllimport)
#endif

#pragma warning( disable : 4251)
#pragma warning( disable : 4305) // trunc double to float

#include <glm\glm\glm.hpp>

#include <vector>
#include <thread>

#define CHECK_GL_ERROR()                                              \
    {                                                                 \
        GLenum err;                                                   \
        while ((err = glGetError()) != GL_NO_ERROR)                   \
        {                                                             \
            std::cerr << "OpenGL Error at " << __FILE__ << ":"        \
                      << __LINE__ << ": " << std::hex << err << "\n"; \
        }                                                             \
    }

struct bloomMip
{
  glm::vec2 size;
  glm::ivec2 intSize;
  unsigned int texture_id;
};

struct TessellationRenderingInfo
{
  // Layering / triplanar
  std::vector<float> base_start_heights;  // size = color_count (+ optional sentinel 1.0f)
  std::vector<float> texture_scale;       // per-layer texel world scale
  int   color_count = 0;           // number of terrain layers actually used
  int   snow_color = -1;          // index of snow layer, or -1 if none
  float snowness = 0.65f;       // threshold used in FS

  // Height / displacement
  float min_height = 0.0f;
  float max_height = 1.0f;
  float height_scale = 1.0f;

  // Normal mapping controls (PBR path)
  float normal_detail_strength = 0.6f;    // blend detail vs macro
  bool  normal_y_flip = false;   // flip green channel if needed

  // PBR feature toggles (you already have these uniforms)
  bool  pbr_renderer = false;
  bool  use_normal_texture_pbr = false;
  bool  use_roughness_texture_pbr = true;
  bool  use_metalic_texture_pbr = true;
  bool  use_ao_texture_pbr = true;
  bool  gamma_correction = true;
  float normal_mapping_strength = 0.5f;

  // Tessellation distance range (TCS)
  float tess_min_distance = 2.0f;
  float tess_max_distance = 16.0f;

  // Placement (TES/TCS need this)
  glm::vec2 chunk_scale_xz = glm::vec2(1.0f);
  glm::vec2 chunk_offset_xz = glm::vec2(0.0f);

  glm::vec2 world_offset = { 0.0f, 0.0f };

  // Heightmap resolution used in TES for correct derivatives
  // If your height map may be non-square, prefer vec2:
  glm::vec2 heightmap_resolution = { 1024.0f, 1024.0f };
};

void DLL_OPENGL_ASSETS              SetOpenGLContextThreadId(const std::thread::id&);
std::thread::id DLL_OPENGL_ASSETS   GetOpenGLContextThreadId();
