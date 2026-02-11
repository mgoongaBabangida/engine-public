#pragma once
#include "DrawList.h"

namespace UI_lib
{
  struct GlyphMSDF
  {
    Rect uv;            // px in atlas
    float l, b, r, t;      // plane bounds in ems
    float advance;      // ems
  };

  struct FontMSDF
  {
    TextureID texture = 0;
    int atlasW = 0, atlasH = 0;
    float emSizePx = 64.f;   // baked px per em (from msdf-atlas-gen -size)
    float lineHeight = 1.2f; // ems
    float ascender = 0.9f; // ems
    float descender = 0.2f;   // ems (store as positive magnitude)
    float pxRange = 6.0f; // baked distance range (px)
    std::unordered_map<uint32_t, GlyphMSDF> glyphs;
    // kerning pair key = (u1<<32)|u2
    std::unordered_map<uint64_t, float> kerning; // advance adjustment (ems)
    bool topOrigin = true; // DevIL loaded top-origin? true => flipV
  };
}