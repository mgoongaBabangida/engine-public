#include "stdafx.h"
#include "TerrainTessellatedRender.h"
#include <math/Camera.h>

//------------------------------------------------------------------------------------------------------------------------------------------------
eTerrainTessellatedRender::eTerrainTessellatedRender(const std::string& vS, const std::string& fS, const std::string& tS1, const std::string& tS2)
{
  m_tessellation_shader.installShaders(vS.c_str(), fS.c_str(), tS1.c_str(), tS2.c_str());
  glUseProgram(m_tessellation_shader.ID());
  m_tessellation_shader.GetUniformInfoFromShader();
}

//------------------------------------------------------------------------------------------------------------------------------------------------
eTerrainTessellatedRender::~eTerrainTessellatedRender()
{
}

//-----------------------------------------------------------------------------------------------------------------------
void eTerrainTessellatedRender::Render(const Camera& _camera, const Light& _light, const std::vector<shObject>& _objects)
{
  if (_objects.empty())
    return;

  glUseProgram(m_tessellation_shader.ID());

  m_tessellation_shader.SetUniformData("lights[0].ambient", _light.ambient);
  m_tessellation_shader.SetUniformData("lights[0].diffuse", _light.diffuse);
  m_tessellation_shader.SetUniformData("lights[0].specular", _light.specular);
  m_tessellation_shader.SetUniformData("lights[0].position", _light.light_position);
  m_tessellation_shader.SetUniformData("lights[0].direction", _light.light_direction);

  m_tessellation_shader.SetUniformData("lights[0].constant", _light.constant);
  m_tessellation_shader.SetUniformData("lights[0].linear", _light.linear);
  m_tessellation_shader.SetUniformData("lights[0].quadratic", _light.quadratic);
  m_tessellation_shader.SetUniformData("lights[0].cutOff", _light.cutOff);
  m_tessellation_shader.SetUniformData("lights[0].outerCutOff", _light.outerCutOff);

  m_tessellation_shader.SetUniformData("view", _camera.getWorldToViewMatrix());
  m_tessellation_shader.SetUniformData("projection", _camera.getProjectionMatrix());
  m_tessellation_shader.SetUniformData("eyePositionWorld", glm::vec4(_camera.getPosition(), 1.0f));

  for (auto& object : _objects)
  {
    m_tessellation_shader.SetUniformData("model", object->GetTransform()->getModelMatrix());
    object->GetModel()->Draw();
  }
}

// Assume these constants match GLSL
constexpr int kMaxLayers = 8;

//---------------------------------------------------
void eTerrainTessellatedRender::UpdateMeshUniforms(const TessellationRenderingInfo& _info)
{
  // --- Layer arrays ---
  const int layers = std::min(_info.color_count, kMaxLayers);
  // base_start_heights[0..layers] where [layers] is the sentinel 1.0
  for (int i = 0; i < layers; ++i)
    m_tessellation_shader.SetUniformData("base_start_heights[" + std::to_string(i) + "]",
      _info.base_start_heights[i]);

  // Sentinel closes the last range [layers-1, layers]
  m_tessellation_shader.SetUniformData("base_start_heights[" + std::to_string(layers) + "]", 1.0f);

  // textureScale per-layer
  for (int i = 0; i < layers; ++i) {
    float s = (i < int(_info.texture_scale.size())) ? _info.texture_scale[i] : 1.0f;
    m_tessellation_shader.SetUniformData("textureScale[" + std::to_string(i) + "]", s);
  }

  m_tessellation_shader.SetUniformData("min_height", _info.min_height);
  m_tessellation_shader.SetUniformData("max_height", _info.max_height);      // FIXED: was height_scale
  m_tessellation_shader.SetUniformData("height_scale", _info.height_scale);

  //// Tess distance & placement (TCS/TES)
  //m_tessellation_shader.SetUniformData("min_distance", _info.tess_min_distance);
  //m_tessellation_shader.SetUniformData("max_distance", _info.tess_max_distance);
  m_tessellation_shader.SetUniformData("worldOffset", _info.world_offset);

  m_tessellation_shader.SetUniformData("chunk_scale_xz", _info.chunk_scale_xz);
  m_tessellation_shader.SetUniformData("chunk_offset_xz", _info.chunk_offset_xz);

  //
  // Clear any unused slots if you want deterministic state(optional)
  //// for (int i = layers+1; i < kMaxLayers; ++i) { ... }
  //
  //// --- Scalar controls shared across TCS/TES/FS ---
  m_tessellation_shader.SetUniformData("color_count", layers);
  m_tessellation_shader.SetUniformData("snow_color", std::min(std::max(_info.snow_color, -1), layers - 1));
  m_tessellation_shader.SetUniformData("snowness", _info.snowness);
  m_tessellation_shader.SetUniformData("normal_detail_strength", _info.normal_mapping_strength);

  //
  //// PBR toggles
  /*m_tessellation_shader.SetUniformData("pbr_renderer", _info.pbr_renderer);
  m_tessellation_shader.SetUniformData("use_normal_texture_pbr", _info.use_normal_texture_pbr);
  m_tessellation_shader.SetUniformData("use_roughness_texture_pbr", _info.use_roughness_texture_pbr);
  m_tessellation_shader.SetUniformData("use_metalic_texture_pbr", _info.use_metalic_texture_pbr);
  m_tessellation_shader.SetUniformData("use_ao_texture_pbr", _info.use_ao_texture_pbr);
  m_tessellation_shader.SetUniformData("gamma_correction", _info.gamma_correction);*/
  //
  //// Normal-map controls
  m_tessellation_shader.SetUniformData("normal_detail_strength", _info.normal_detail_strength);
  m_tessellation_shader.SetUniformData("normal_y_flip", _info.normal_y_flip);
  //
  //// Heightmap resolution for TES derivatives (we used vec2 in the TES snippet)
  //m_tessellation_shader.SetUniformData("heightMapResolution", _info.heightmap_resolution.x);
}
