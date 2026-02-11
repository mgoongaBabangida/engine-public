#pragma once
#include <string_view>

namespace UI_lib
{
  static inline float uiScale(const UI_lib::UiContext& ctx)
  {
    const float sx = float(ctx.fbW) / float(ctx.virtualW);
    const float sy = float(ctx.fbH) / float(ctx.virtualH);
    return std::min(sx, sy);
  }

  static inline std::u32string utf8ToUtf32(std::string_view s)
  {
    std::u32string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
      uint8_t c = (uint8_t)s[i];
      if (c < 0x80) { out.push_back(c); ++i; continue; }
      if ((c >> 5) == 0x6 && i + 1 < s.size()) { // 110xxxxx
        uint32_t cp = ((c & 0x1F) << 6) | (uint8_t(s[i + 1]) & 0x3F);
        out.push_back(cp); i += 2; continue;
      }
      if ((c >> 4) == 0xE && i + 2 < s.size()) { // 1110xxxx
        uint32_t cp = ((c & 0x0F) << 12) |
          ((uint8_t(s[i + 1]) & 0x3F) << 6) |
          (uint8_t(s[i + 2]) & 0x3F);
        out.push_back(cp); i += 3; continue;
      }
      if ((c >> 3) == 0x1E && i + 3 < s.size()) { // 11110xxx
        uint32_t cp = ((c & 0x07) << 18) |
          ((uint8_t(s[i + 1]) & 0x3F) << 12) |
          ((uint8_t(s[i + 2]) & 0x3F) << 6) |
          (uint8_t(s[i + 3]) & 0x3F);
        out.push_back(cp); i += 4; continue;
      }
      // invalid byte → replacement char
      out.push_back(0xFFFD); ++i;
    }
    return out;
  }

  static inline Rect intersect(const Rect& a, const Rect& b) {
    float x0 = std::max(a.x, b.x);
    float y0 = std::max(a.y, b.y);
    float x1 = std::min(a.x + a.w, b.x + b.w);
    float y1 = std::min(a.y + a.h, b.y + b.h);
    return { x0, y0, std::max(0.f, x1 - x0), std::max(0.f, y1 - y0) };
  }

  static inline Rect intersectR(const Rect& a, const Rect& b) {
    float x0 = std::max(a.x, b.x), y0 = std::max(a.y, b.y);
    float x1 = std::min(a.x + a.w, b.x + b.w), y1 = std::min(a.y + a.h, b.y + b.h);
    return { x0,y0,std::max(0.f,x1 - x0),std::max(0.f,y1 - y0) };
  }

  // rowH = inner row height (after padding). Bleed defaults to 2 px each way.
    //------------------------------------------------------------------------------
  static inline void flexMeasureChild(
    UiContext& ctx, Widget* w, float rowH,
    float& outW, float& outH,
    float bleedW_px = 2.0f, float bleedH_px = 2.0f) // device pixels target
  {
    outW = w->local.w;
    outH = w->local.h;

    auto ceilp = [](float v) { return std::ceil(v); };
    const float s = uiScale(ctx);                   // device px per 1 virtual px
    const float dev2virtW = (s > 0.f) ? (bleedW_px / s) : bleedW_px;
    const float dev2virtH = (s > 0.f) ? (bleedH_px / s) : bleedH_px;

    if (auto* lbl = dynamic_cast<Label*>(w)) {
      const float px = (lbl->fontSize > 0.f) ? lbl->fontSize : ctx.theme.fontSizes[2];

      float lineHpx;
      if (ctx.font && ctx.font->texture) {
        lineHpx = ctx.font->lineHeight * px;
      }
      else {
        lineHpx = px * 1.40f;
      }

      float wpx = outW;
      if (wpx <= 0.f) {
        if (ctx.font && ctx.font->texture) {
          const auto s32 = utf8ToUtf32(lbl->text);
          wpx = measureRun(*ctx.font, s32, px).widthPx;
        }
        else {
          wpx = std::max(1.f, 0.60f * px * float(lbl->text.size()));
        }
      }

      // Add ~2 device px slack on each dimension (converted to virtual px)
      outW = std::max(outW, ceilp(wpx) + dev2virtW);
      outH = std::max(outH, ceilp(lineHpx) + dev2virtH);
      return;
    }

    if (auto* img = dynamic_cast<Image*>(w)) {
      if (outW <= 0.f || outH <= 0.f) {
        if (const Sprite* sp = ctx.findSprite(img->spriteName)) {
          if (outW <= 0.f) outW = sp->uv.w;
          if (outH <= 0.f) outH = sp->uv.h;
        }
        else {
          if (outW <= 0.f) outW = rowH;
          if (outH <= 0.f) outH = rowH;
        }
      }
      return;
    }

    if (auto* ib = dynamic_cast<IconButton*>(w)) {
      if (outW <= 0.f || outH <= 0.f) {
        if (const Sprite* sp = ctx.findSprite(ib->spriteName)) {
          if (outH <= 0.f) outH = std::min(rowH, sp->uv.h);
          if (outW <= 0.f) outW = std::min(rowH, sp->uv.w);
        }
        else {
          if (outH <= 0.f) outH = rowH;
          if (outW <= 0.f) outW = rowH;
        }
      }
      return;
    }

    if (outW <= 0.f) outW = rowH;
    if (outH <= 0.f) outH = rowH;
  }

}