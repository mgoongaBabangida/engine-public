// Tooltip.h
#pragma once
#include "ui_lib.h"
#include <string>
#include "base.h"
#include "Wrap.h"

namespace UI_lib
{
  struct UiContext;

  //-----------------------------------------------------------------------------------------------
  struct TooltipConfig
  {
    float delayMs = 350.f;      // time to show after hover
    float maxWidth = 360.f;     // wrap text if wider
    float padL = 8, padR = 8, padT = 6, padB = 6;
    float cursorOffsetX = 14.f; // offset from cursor
    float cursorOffsetY = 18.f;
    std::string nineName = "tooltip_bg"; // atlas nine-slice
    uint32_t textColor = 0x1A1A1AFF;     // default ink
    uint32_t bgTint = 0xFFFFFFFF;      // multiplied into 9-slice
    float opacity = 1.0f;                // extra fade (we keep it simple for now)

    float fadeInMs = 1'000.f;
    float fadeOutMs = 90.f;
    bool enableFade = true;

    enum class Mode { Follow, LockOnShow, AnchorToWidget } mode = Mode::AnchorToWidget;
    enum class Prefer { Above, Below } prefer = Prefer::Below; // for AnchorToWidget
  };

  //-----------------------------------------------------------------------------------------------
  struct TooltipState
  {
    bool   visible = false;       // actually drawing this frame
    bool   armed = false;       // hovered and waiting for delay
    float  tHoverMs = 0.f;         // time since armed
    Vec2   cursor{ 0,0 };         // virtual px
    std::string text;               // current tooltip text
    Rect   box;                     // computed rect for draw
    Vec2 armPos{ 0,0 };     // set on Enter
    Rect anchorRect{};     // set to target->rect on Enter (absolute)

     // Fade/tween
    float  alpha = 0.f;       // 0..1, multiplied into bg/text
    bool   fadingOut = false; // true when leaving/disarmed and we’re animating out
  };

  // Minimal shaper for width—reuse your measureRun and wrapGreedy if available
  struct MeasureResult { float widthPx = 0, heightPx = 0; std::vector<ShapedLine> lines; };

  MeasureResult DLL_UI_LIB shapeTooltip(const FontMSDF& font,
                                    const std::string& utf8,
                                    float px,
                                    float maxWidth);

  bool DLL_UI_LIB updateTooltip(UiContext& ctx, float dtMs);

} // namespace

