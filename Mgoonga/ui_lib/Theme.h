#pragma once
#include <string>
#include <unordered_map>
#include <optional>
#include "base.h" // Rect, Vec2, etc.
#include "Tooltip.h"

namespace UI_lib
{
  struct Style
  {
    std::optional<float>   fontSize;
    std::optional<uint32_t> color;   // text color
    std::optional<uint32_t> tint;    // bg tint
    std::optional<float>   opacity;
    // (pad fields if you want, omitted here)
  };


  struct StyleSet  // per widget-type or per id
  {
    Style base;
    Style hover;
    Style pressed;
  };

  struct Theme
  {
    float spacing[6]{};
    float radii[4]{};
    float fontSizes[5]{};
    uint32_t color_parchment_bg = 0, color_ink = 0, color_gold = 0;
    TooltipConfig tipCfg;

    // changed types:
    std::unordered_map<std::string, StyleSet> byType;
    std::unordered_map<std::string, StyleSet> byId;
  };

  inline void mergeInto(Style& dst, const Style& src)
  {
    if (src.fontSize) dst.fontSize = src.fontSize;
    if (src.color)    dst.color = src.color;
    if (src.tint)     dst.tint = src.tint;
    if (src.opacity)  dst.opacity = src.opacity;
  }

} // namespace
