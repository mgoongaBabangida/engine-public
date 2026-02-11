#include "stdafx.h"
#include "PrimitiveRenderer.h"
#include <cstring>   // std::memcmp
#include <algorithm> // std::max/min

namespace UI_lib
{
  // ----------------- small helpers -----------------
  static inline bool sameRect(const Rect& a, const Rect& b) {
    return std::memcmp(&a, &b, sizeof(Rect)) == 0;
  }

  static inline bool samePipe(const PipelineKey& A, const PipelineKey& B) {
    return A.shader == B.shader && A.renderFunc == B.renderFunc && A.blendPremult == B.blendPremult;
  }

  // Centralized push that respects batching by tex/clip/pipe
  static inline void pushQuadBatched(
    DrawList& dl,
    TextureID tex0, TextureID tex1, TextureID tex2,
    const Rect& dst, const Rect& uvPx,
    uint32_t rgba, const Rect& clip,
    int texW, int texH,
    const PipelineKey& pipe,
    bool flipV = false)                    // <-- NEW (defaults off)
  {
    // Start/continue batch
    if (dl.cmds.empty()
      || dl.cmds.back().tex0 != tex0
      || dl.cmds.back().tex1 != tex1
      || dl.cmds.back().tex2 != tex2
      || !sameRect(dl.cmds.back().clip, clip)
      || !samePipe(dl.cmds.back().pipe, pipe))
    {
      DrawCmd dc;
      dc.tex0 = tex0; dc.tex1 = tex1; dc.tex2 = tex2;
      dc.clip = clip;
      dc.idxOffset = static_cast<uint32_t>(dl.indices.size());
      dc.idxCount = 0;
      dc.pipe = pipe;
      dl.cmds.emplace_back(dc);
    }
    DrawCmd& cmd = dl.cmds.back();

    // Normalize UVs (uvPx is in texels, msdf-atlas-gen uses bottom origin)
    float u0 = uvPx.x / texW;
    float u1 = (uvPx.x + uvPx.w) / texW;
    float v0 = uvPx.y / texH;
    float v1 = (uvPx.y + uvPx.h) / texH;

    if (flipV) {
      // GL texture is top-origin -> invert V range to match bottom-origin UVs
      v0 = 1.0f - v0;
      v1 = 1.0f - v1;
      if (v0 > v1) std::swap(v0, v1);   // keep v0 < v1
    }

    const uint32_t base = static_cast<uint32_t>(dl.verts.size());
    dl.verts.push_back({ dst.x,           dst.y,           u0, v0, rgba, 0,0 });
    dl.verts.push_back({ dst.x + dst.w,   dst.y,           u1, v0, rgba, 0,0 });
    dl.verts.push_back({ dst.x + dst.w,   dst.y + dst.h,   u1, v1, rgba, 0,0 });
    dl.verts.push_back({ dst.x,           dst.y + dst.h,   u0, v1, rgba, 0,0 });

    dl.indices.insert(dl.indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
    cmd.idxCount += 6;
  }

  // ----------------- API implementations -----------------
  //--------------------------------------------------------------------------------
  void PrimitiveRenderer::texturedQuad(DrawList& dl,
    TextureID tex,
    uint32_t atlasW,
    uint32_t atlasH,
    const Rect& dst,
    const Rect& uvPx,
    uint32_t color,
    const Rect& explicitClip,
    bool invert_y)
  {
    // Final clip = current stack top ∩ explicit clip
    const Rect finalClip = intersect(currentClip(), explicitClip);
    const int texW = atlasW;
    const int texH = atlasH;

    // For now, bind same atlas into all three slots (tex1/tex2), keeps batching simple
    pushQuadBatched(dl, tex, tex, tex, dst, uvPx, color, finalClip, texW, texH, invert_y ? PipeSpriteInvertedY() : PipeSprite());
  }

  //--------------------------------------------------------------------------------
  void PrimitiveRenderer::nineSlice(DrawList& dl,
    TextureID tex,
    const Rect& dst,
    const NineSlice& ns,
    uint32_t color,
    const Rect& explicitClip)
  {
    const Rect finalClip = intersect(currentClip(), explicitClip);

    const float sx = ns.uvFull.x, sy = ns.uvFull.y, sw = ns.uvFull.w, sh = ns.uvFull.h;
    const float l = static_cast<float>(ns.left);
    const float r = static_cast<float>(ns.right);
    const float t = static_cast<float>(ns.top);
    const float b = static_cast<float>(ns.bottom);

    // Destination tiles
    Rect d[3][3] = {
      { {dst.x,             dst.y,             l, t},
        {dst.x + l,         dst.y,             dst.w - l - r, t},
        {dst.x + dst.w - r, dst.y,             r, t} },

      { {dst.x,             dst.y + t,         l, dst.h - t - b},
        {dst.x + l,         dst.y + t,         dst.w - l - r, dst.h - t - b},
        {dst.x + dst.w - r, dst.y + t,         r, dst.h - t - b} },

      { {dst.x,             dst.y + dst.h - b, l, b},
        {dst.x + l,         dst.y + dst.h - b, dst.w - l - r, b},
        {dst.x + dst.w - r, dst.y + dst.h - b, r, b} }
    };

    // Source tiles (in pixels)
    Rect s[3][3] = {
      { {sx,           sy,           l, t},
        {sx + l,       sy,           sw - l - r, t},
        {sx + sw - r,  sy,           r, t} },

      { {sx,           sy + t,       l, sh - t - b},
        {sx + l,       sy + t,       sw - l - r, sh - t - b},
        {sx + sw - r,  sy + t,       r, sh - t - b} },

      { {sx,           sy + sh - b,  l, b},
        {sx + l,       sy + sh - b,  sw - l - r, b},
        {sx + sw - r,  sy + sh - b,  r, b} }
    };

    // Emit 9 quads; skip degenerate tiles
    for (int iy = 0; iy < 3; ++iy) {
      for (int ix = 0; ix < 3; ++ix) {
        if (d[iy][ix].w <= 0.f || d[iy][ix].h <= 0.f) continue;
        pushQuadBatched(dl, tex, tex, tex, d[iy][ix], s[iy][ix], color, finalClip, ns.texW, ns.texH, PipeNine());
      }
    }
  }

  // ----------------- convenience overloads (use bound target) -----------------
   //--------------------------------------------------------------------------------
  void PrimitiveRenderer::texturedQuad(uint32_t tex, uint32_t atlasW, uint32_t atlasH,
    const Rect& dst,
    const Rect& uv,
    uint32_t color,
    const Rect& clip)
  {
    if (!m_target) return;
      texturedQuad(*m_target, tex, atlasW, atlasH, dst, uv, color, clip);
  }

  //--------------------------------------------------------------------------------
  void PrimitiveRenderer::nineSlice(uint32_t tex,
    const Rect& dst,
    const NineSlice& n,
    uint32_t color,
    const Rect& clip)
  {
    if (!m_target) return;
      nineSlice(*m_target, tex, dst, n, color, clip);
  }

  //--------------------------------------------------------------------------------
  void PrimitiveRenderer::textMSDF(DrawList& dl, const FontMSDF& f,
    std::u32string_view text, Vec2 origin,
    float pxSize, uint32_t rgba, const Rect& explicitClip)
  {
    if (!f.texture || f.atlasW == 0 || f.atlasH == 0) return;

    static bool dbgOnce = true;     // flip to true for one frame when testing
    if (dbgOnce) {
      printf("[MSDF] atlas %dx%d  emSizePx=%.1f  ascender(ems)=%.3f  lineHeight(ems)=%.3f\n",
        f.atlasW, f.atlasH, f.emSizePx, f.ascender, f.lineHeight);
      printf("[MSDF] call pxSize=%.1f  origin=(%.1f,%.1f)\n", pxSize, origin.x, origin.y);
    }

    const Rect finalClip = intersect(currentClip(), explicitClip);
    const float scale = pxSize / f.emSizePx;
    float penX = origin.x;
    float baselineY = origin.y + f.ascender * pxSize; // place baseline inside rect

    if (dbgOnce) {
      printf("[MSDF] baselineY=%.1f  (asc*px = %.1f)\n",
        baselineY, f.ascender * pxSize);
    }

    uint32_t prev = 0; bool havePrev = false;
    for (char32_t cp : text)
    {
      if (havePrev)
      {
        auto itK = f.kerning.find((uint64_t(prev) << 32) | uint64_t(cp));
        if (itK != f.kerning.end())
          penX += itK->second * pxSize; // kerning in ems → px
      }
      auto it = f.glyphs.find(cp);
      if (it == f.glyphs.end()) { penX += 0.5f * pxSize; continue; } // fallback advance

      const GlyphMSDF& g = it->second;

      // Quad in virtual px
      float x0 = penX + g.l * pxSize;
      float y0 = baselineY - g.t * pxSize;
      float x1 = penX + g.r * pxSize;
      float y1 = baselineY - g.b * pxSize;
      Rect dst{ x0, y0, x1 - x0, y1 - y0 };

      // Emit
      pushQuadBatched(dl, f.texture, f.texture, f.texture,
        dst, g.uv, rgba, finalClip, f.atlasW, f.atlasH, PipeMSDF(), f.topOrigin);

      penX += g.advance * pxSize; // advance in px
      prev = uint32_t(cp); havePrev = true;

      if (dbgOnce) {
        printf("[MSDF] cp=U+%04X  lbt=(%.3f,%.3f,%.3f,%.3f) ems  adv=%.3f ems\n",
          unsigned(cp), g.l, g.b, g.t, g.r, g.advance);
        printf("[MSDF] quad dst: (%.1f,%.1f)-(%.1f,%.1f)  w=%.1f h=%.1f\n",
          x0, y0, x1, y1, x1 - x0, y1 - y0);
        dbgOnce = false; // print only once
      }
    }
  }
  void PrimitiveRenderer::solidRect(DrawList& dl, const Rect& dst, uint32_t color, const Rect& explicitClip)
  {
    const Rect finalClip = intersect(currentClip(), explicitClip);
    // UVs don’t matter for a solid shader, but pushQuadBatched needs something.
    // Give it a fake 1×1 “UV” and texW/H=1. Backend will pick the Solid program from the pipeline key.
    pushQuadBatched(dl,
      /*tex0=*/0, /*tex1=*/0, /*tex2=*/0,
      dst, Rect{ 0,0,1,1 },
      color, finalClip,
      /*texW=*/1, /*texH=*/1,
      /*pipe=*/PipeSolid(),
      /*flipV=*/false);
  }
}
