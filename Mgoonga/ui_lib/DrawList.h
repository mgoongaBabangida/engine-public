#pragma once
#include "base.h"
#include "ui_lib.h"

namespace UI_lib
{
  using TextureID = uint32_t;

  //--------------------------------------------------
  struct Vertex
  {
    float x, y; // position
    float u, v; // texcoord
    uint32_t rgba; // packed color
    float aux0, aux1; // for MSDF edge/px or 9-slice flags
  };

  enum class ShaderKind : uint8_t { Sprite, NineSlice, MSDF, Solid, Gradient, Masked, CursorFollow, GreyKernel };

  //--------------------------------------------------
  struct PipelineKey
  {
    ShaderKind shader = ShaderKind::Sprite;
    int        renderFunc = 0;   // maps to your SetRenderingFunction()
    uint8_t    blendPremult = 1; // 0=straight, 1=premult
    uint8_t    invert_y = 0; // 0 normal, 1 - invert y
  };

  //--------------------------------------------------
  struct DrawCmd
  {
    uint32_t   tex0 = 0;         // primary texture (atlas or font)
    uint32_t   tex1 = 0;         // optional (mask/second)
    uint32_t   tex2 = 0;         // optional (third)
    Rect clip{};
    uint32_t idxOffset = 0;
    uint32_t idxCount = 0;
    PipelineKey pipe{};
  };

  //--------------------------------------------------
  struct DLL_UI_LIB DrawList
  {
    std::vector<Vertex> verts;
    std::vector<uint32_t> indices;
    std::vector<DrawCmd> cmds;

    void clear() { verts.clear(); indices.clear(); cmds.clear(); }
  };

  // Minimal 9-slice sprite descriptor (atlas-space in pixels; scaled at draw time)
  //--------------------------------------------------
  struct DLL_UI_LIB NineSlice
  {
    int texW = 0, texH = 0; Rect uvFull{}; int left = 0, right = 0, top = 0, bottom = 0; /* TextureID tex = 0;*/
  };

  //--------------------------------------------------
  struct DLL_UI_LIB Sprite
  {
    Rect uv; // UVs in pixels 
    int texW = 0, texH = 0;
    TextureID tex = 0;
  };

  //--------------------------------------------------
  struct DLL_UI_LIB Atlas
  {
    TextureID texture = 0; // GL texture id
    int texW = 0, texH = 0;
    std::unordered_map<std::string, Sprite> sprites; // name -> sprite
    std::unordered_map<std::string, NineSlice> nine; // name -> nine-slice
  };

}
