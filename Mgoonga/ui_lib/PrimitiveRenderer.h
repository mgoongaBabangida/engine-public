#pragma once
#include "ui_lib.h"
#include "DrawList.h"
#include "FontMSDF.h"

namespace UI_lib
{
  //---------------------------------------------------------------------------------------
  struct PrimitiveRenderer
  {
  public:
    // Non-owning; valid only while you’re building a frame.
    void setTarget(DrawList* dl) noexcept { m_target = dl; }
    DrawList* target() const noexcept { return m_target; }

    // Clip stack
    void pushClip(const Rect& r) { m_clip.push_back(m_clip.empty() ? r : intersect(m_clip.back(), r)); }
    void popClip() { if (!m_clip.empty()) m_clip.pop_back(); }
    Rect currentClip() const { return m_clip.empty() ? Rect{ 0,0,1920,1080 } : m_clip.back(); }

    // ----- existing API (kept) -----
    void texturedQuad(DrawList& out, TextureID tex, uint32_t atlasW, uint32_t atlasH,
                      const Rect& dst, const Rect& uv,
                      uint32_t color, const Rect& clip, bool invert_y = false);

    void nineSlice(DrawList& out, TextureID tex, const Rect& dst, const NineSlice& n,
      uint32_t color, const Rect& clip);

    // ----- convenience overloads that use the bound target (optional but handy) -----
    void texturedQuad(uint32_t tex, uint32_t atlasW, uint32_t atlasH, const Rect& dst, const Rect& uv,
      uint32_t color, const Rect& clip);
    void nineSlice(uint32_t tex, const Rect& dst, const NineSlice& n,
        uint32_t color, const Rect& clip);

    void textMSDF(DrawList& out, const FontMSDF& f, std::u32string_view text,
      Vec2 origin, float pxSize, uint32_t rgba, const Rect& clip);

    static inline PipelineKey PipeMSDF() { PipelineKey k; k.shader = ShaderKind::MSDF; k.blendPremult = 0; return k; }
    static inline PipelineKey PipeSolid() { PipelineKey k; k.shader = ShaderKind::Solid; k.blendPremult = 1; return k; }

    void solidRect(DrawList& dl, const Rect& dst, uint32_t color, const Rect& explicitClip);

    // optional convenience that uses the bound target
    void solidRect(const Rect& dst, uint32_t color, const Rect& explicitClip) {
      if (!m_target) return;
      solidRect(*m_target, dst, color, explicitClip);
    }

  private:
    DrawList* m_target = nullptr; // not owned
    std::vector<Rect> m_clip;

    static inline Rect intersect(const Rect& a, const Rect& b)
    {
      const float x = std::max(a.x, b.x);
      const float y = std::max(a.y, b.y);
      const float r = std::min(a.x + a.w, b.x + b.w);
      const float btm = std::min(a.y + a.h, b.y + b.h);
      return { x, y, std::max(0.f, r - x), std::max(0.f, btm - y) };
    }

    static inline PipelineKey PipeSprite() { PipelineKey k; k.shader = ShaderKind::Sprite; k.blendPremult = 1; return k; }
    static inline PipelineKey PipeNine() { PipelineKey k; k.shader = ShaderKind::NineSlice; k.blendPremult = 1; return k; }
    static inline PipelineKey PipeSpriteInvertedY() { PipelineKey k; k.shader = ShaderKind::Sprite; k.blendPremult = 1; k.invert_y = 1; return k; }
  };
}