#include "stdafx.h"

#include "Tooltip.h"
#include "PrimitiveRenderer.h"
#include "Theme.h"
#include "Widget.h" // for wrap/measure utils if you want to reuse
#include "Context.h"

#include <iostream>

namespace UI_lib
{

  static float clampf(float v, float a, float b) { return std::max(a, std::min(v, b)); }

  static MeasureResult shapeTooltip(const FontMSDF& font,
                                    const std::string& utf8,
                                    float px,
                                    float maxWidth)
  {
    MeasureResult m;
    std::u32string s32(utf8.begin(), utf8.end());
    if (maxWidth <= 0) {
      auto r = measureRun(font, s32, px);
      m.widthPx = r.widthPx;
      m.heightPx = font.lineHeight * px;
      ShapedLine L; L.text = s32; L.widthPx = r.widthPx; m.lines.push_back(std::move(L));
      return m;
    }
    auto lines = wrapGreedy(font, s32, px, maxWidth);
    m.lines = std::move(lines);
    m.widthPx = 0.f;
    for (auto& L : m.lines) m.widthPx = std::max(m.widthPx, L.widthPx);
    m.heightPx = font.lineHeight * px * m.lines.size();
    return m;
  }

  //------------------------------------------------------------------
  bool updateTooltip(UiContext& ctx, float dtMs)
  {
    auto& tip = ctx.tip; const auto& cfg = ctx.tipCfg;

    if (ctx.tipCfg.mode == TooltipConfig::Mode::AnchorToWidget) {
      if (ctx.root && ctx.root->hoverWidget) {
        ctx.tip.anchorRect = ctx.root->hoverWidget->visualRect(); // accounts for tweens
      }
    }

    // -------------------------
    // FAST PATH: no fade
    // -------------------------
    if (!cfg.enableFade) {
      const bool wasVisible = tip.visible;
      const Rect  prevBox = tip.box;

      if (!tip.armed) { tip.visible = false; return wasVisible; }

      tip.tHoverMs += dtMs;

      bool turnedVisible = false;
      if (!wasVisible && tip.tHoverMs >= cfg.delayMs) { tip.visible = true; turnedVisible = true; }

      if (!tip.visible) return turnedVisible; // still counting down

      // measure with max width
      const float px = ctx.theme.fontSizes[2];
      auto M = shapeTooltip(*ctx.font, tip.text, px, cfg.maxWidth);

      float w = M.widthPx + cfg.padL + cfg.padR;
      float h = M.heightPx + cfg.padT + cfg.padB;

      Rect r;
      switch (ctx.tipCfg.mode) {
      case TooltipConfig::Mode::Follow:
        r.x = tip.cursor.x + cfg.cursorOffsetX;
        r.y = tip.cursor.y + cfg.cursorOffsetY;
        break;
      case TooltipConfig::Mode::LockOnShow: {
        static bool pinned = false;
        if (turnedVisible) { tip.armPos = tip.cursor; pinned = true; }
        r.x = tip.armPos.x + cfg.cursorOffsetX;
        r.y = tip.armPos.y + cfg.cursorOffsetY;
        break;
      }
      case TooltipConfig::Mode::AnchorToWidget:
        if (cfg.prefer == TooltipConfig::Prefer::Above) {
          r.x = tip.anchorRect.x + (tip.anchorRect.w - w) * 0.5f;
          r.y = tip.anchorRect.y - h - 6.f;
          if (r.y < ctx.viewport.y) r.y = tip.anchorRect.y + tip.anchorRect.h + 6.f;
        }
        else {
          r.x = tip.anchorRect.x + (tip.anchorRect.w - w) * 0.5f;
          r.y = tip.anchorRect.y + tip.anchorRect.h + 6.f;
          if (r.y + h > ctx.viewport.y + ctx.viewport.h)
            r.y = tip.anchorRect.y - h - 6.f;
        }
        break;
      }
      r.w = w; r.h = h;
      Rect newBox = ctx.clampToViewport(r);
      tip.box = newBox;

      bool moved = (std::fabs(newBox.x - prevBox.x) > 0.5f) || (std::fabs(newBox.y - prevBox.y) > 0.5f);
      bool resized = (std::fabs(newBox.w - prevBox.w) > 0.5f) || (std::fabs(newBox.h - prevBox.h) > 0.5f);
      bool changed = turnedVisible || moved || resized;
      if (changed) ctx.invalidate();
      return changed;
    }

    // -------------------------
    // FADE PATH
    // -------------------------
    const bool wasVisible = tip.visible;
    const Rect  prevBox = tip.box;
    const float prevAlpha = tip.alpha;

    // Arm / unarm and flip visibility after delay
    if (!tip.armed) {
      // Disarmed → start/continue fade-out if needed
      if (tip.visible || tip.fadingOut) {
        tip.visible = false;
        tip.fadingOut = true;
      }
    }
    else {
      tip.tHoverMs += dtMs;
      if (!tip.visible && tip.tHoverMs >= cfg.delayMs) {
        tip.visible = true;       // show now
        tip.fadingOut = false;
        tip.alpha = 0.f;        // <<< crucial: start fade-in from 0
      }
    }

    // Progress alpha
    if (tip.visible) {
      const float denom = std::max(1.f, cfg.fadeInMs);
      tip.alpha = std::min(1.f, tip.alpha + dtMs / denom);
    }
    else if (tip.fadingOut) {
      const float denom = std::max(1.f, cfg.fadeOutMs);
      tip.alpha = std::max(0.f, tip.alpha - dtMs / denom);
      if (tip.alpha <= 0.f) tip.fadingOut = false;
    }
    else {
      tip.alpha = 0.f;
    }

    // Fully hidden and not fading — we can stop here (but report alpha/vis change)
    if (!tip.visible && !tip.fadingOut && tip.alpha <= 0.f) {
      const bool changed = (prevAlpha != tip.alpha) || (wasVisible != tip.visible);
      if (changed) ctx.invalidate();
      return changed;
    }

    // Measure & place while visible or fading (alpha > 0)
    if (!ctx.font || tip.text.empty()) {
      tip.visible = false;
      tip.fadingOut = false;
      tip.alpha = 0.f;
      const bool changed = (prevAlpha != 0.f) || (wasVisible != false);
      if (changed) ctx.invalidate();
      return changed;
    }

    const float px = ctx.theme.fontSizes[2];
    auto M = shapeTooltip(*ctx.font, tip.text, px, cfg.maxWidth);

    float w = M.widthPx + cfg.padL + cfg.padR;
    float h = M.heightPx + cfg.padT + cfg.padB;

    Rect r;
    switch (ctx.tipCfg.mode) {
    case TooltipConfig::Mode::Follow:
      r.x = tip.cursor.x + cfg.cursorOffsetX;
      r.y = tip.cursor.y + cfg.cursorOffsetY;
      break;
    case TooltipConfig::Mode::LockOnShow: {
      static bool pinned = false;
      if (!wasVisible && tip.visible) { tip.armPos = tip.cursor; pinned = true; }
      r.x = tip.armPos.x + cfg.cursorOffsetX;
      r.y = tip.armPos.y + cfg.cursorOffsetY;
      break;
    }
    case TooltipConfig::Mode::AnchorToWidget:
      if (cfg.prefer == TooltipConfig::Prefer::Above) {
        r.x = tip.anchorRect.x + (tip.anchorRect.w - w) * 0.5f;
        r.y = tip.anchorRect.y - h - 6.f;
        if (r.y < ctx.viewport.y) r.y = tip.anchorRect.y + tip.anchorRect.h + 6.f;
      }
      else {
        r.x = tip.anchorRect.x + (tip.anchorRect.w - w) * 0.5f;
        r.y = tip.anchorRect.y + tip.anchorRect.h + 6.f;
        if (r.y + h > ctx.viewport.y + ctx.viewport.h)
          r.y = tip.anchorRect.y - h - 6.f;
      }
      break;
    }
    r.w = w; r.h = h;
    tip.box = ctx.clampToViewport(r);

    const bool moved = (std::fabs(tip.box.x - prevBox.x) > 0.5f) || (std::fabs(tip.box.y - prevBox.y) > 0.5f);
    const bool resized = (std::fabs(tip.box.w - prevBox.w) > 0.5f) || (std::fabs(tip.box.h - prevBox.h) > 0.5f);
    const bool visChg = (wasVisible != tip.visible);
    const bool aChg = (prevAlpha != tip.alpha);

    const bool changed = moved || resized || visChg || aChg;
    if (changed) ctx.invalidate();
    return changed;
  }

} // namespace