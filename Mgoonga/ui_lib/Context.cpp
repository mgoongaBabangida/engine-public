#include "Context.h"
#include "Widget.h"
#include "Utils.h"

namespace UI_lib
{
  //-----------------------------------------------------------------------------------------
  void UiContext::capture(Widget* w)
  {
  }

  //-----------------------------------------------------------------------------------------
  void UiContext::releaseCapture(Widget* w)
  {
  }

  //-------------------------------------------------------------------------------------
  void UiContext::requestFocus(Widget* w)
  {
    if (!w || !w->wantsFocus()) return;
    if (focus.current == w) return;
    if (focus.current) focus.current->onBlur(*this);
    focus.current = w;
    focus.current->onFocus(*this);
    invalidate(); // redraw for focus ring, etc.
  }

  //----------------------------------------------------------------------------------------
  void UiContext::clearFocus()
  {
    if (focus.current) { focus.current->onBlur(*this); focus.current = nullptr; invalidate(); }
  }

  //-----------------------------------------------------------------------------------------
  bool UiContext::updateRichTooltip(float dtMs)
  {
    auto& tip = rich; const auto& cfg = richCfg;

    const bool wasVisible = tip.visible;
    const Rect prevBox = tip.box;
    const float prevAlpha = tip.alpha;

    // visibility state machine
    if (!tip.armed) {
      if (tip.visible || tip.fadingOut) { tip.visible = false; tip.fadingOut = true; }
    }
    else {
      tip.tHoverMs += dtMs;
      if (!tip.visible && tip.tHoverMs >= cfg.delayMs) {
        tip.visible = true; tip.fadingOut = false; tip.alpha = 0.f;
      }
    }

    // alpha progression
    if (tip.visible) {
      tip.alpha = std::min(1.f, tip.alpha + dtMs / std::max(1.f, cfg.fadeInMs));
    }
    else if (tip.fadingOut) {
      tip.alpha = std::max(0.f, tip.alpha - dtMs / std::max(1.f, cfg.fadeOutMs));
      if (tip.alpha <= 0.f) tip.fadingOut = false;
    }
    else {
      tip.alpha = 0.f;
    }

    // fully hidden
    if (!tip.visible && !tip.fadingOut && tip.alpha <= 0.f) {
      const bool changed = (prevAlpha != tip.alpha) || (wasVisible != tip.visible);
      if (changed) invalidate();
      return changed;
    }

    // ---- Measure content to compute box ----
    // Title width
    float titleW = 0.f, titleH = 0.f;
    if (!tip.content.title.empty() && font) {
      auto s32 = utf8ToUtf32(tip.content.title);
      auto m = measureRun(*font, s32, tip.content.titleSize);
      titleW = m.widthPx; titleH = font->lineHeight * tip.content.titleSize;
    }

    // Rows
    float rowsW = 0.f; float rowsH = 0.f;
    for (auto& r : tip.content.rows) {
      float iconW = 0.f;
      if (!r.icon.empty()) iconW = cfg.iconPx;
      float textW = 0.f;
      if (font && !r.text.empty()) {
        auto s32 = utf8ToUtf32(r.text);
        auto m = measureRun(*font, s32, r.fontSize);
        textW = m.widthPx;
      }
      rowsW = std::max(rowsW, iconW + (iconW > 0 ? cfg.textGap : 0.f) + textW);
      rowsH += std::max(cfg.iconPx, font ? font->lineHeight * r.fontSize : cfg.iconPx);
      rowsH += cfg.rowGap;
    }
    if (!tip.content.rows.empty()) rowsH -= cfg.rowGap; // no gap after last

    float contentW = std::max(titleW, rowsW);
    contentW = std::min(contentW, cfg.maxWidth);

    float contentH = 0.f;
    if (titleH > 0.f) { contentH += titleH; if (!tip.content.rows.empty()) contentH += cfg.gapY; }
    contentH += rowsH;

    float w = contentW + cfg.padL + cfg.padR;
    float h = contentH + cfg.padT + cfg.padB;

    // Place above anchor if possible, else below
    Rect r{};
    r.x = tip.anchorRect.x + (tip.anchorRect.w - w) * 0.5f;
    r.y = tip.anchorRect.y - h - 6.f;
    if (r.y < viewport.y) r.y = tip.anchorRect.y + tip.anchorRect.h + 6.f;

    r.w = w; r.h = h;
    tip.box = clampToViewport(r);

    const bool moved = (std::fabs(tip.box.x - prevBox.x) > 0.5f) || (std::fabs(tip.box.y - prevBox.y) > 0.5f);
    const bool resized = (std::fabs(tip.box.w - prevBox.w) > 0.5f) || (std::fabs(tip.box.h - prevBox.h) > 0.5f);
    const bool visChg = (wasVisible != tip.visible);
    const bool aChg = (prevAlpha != tip.alpha);

    const bool changed = moved || resized || visChg || aChg;
    if (changed) invalidate();
    return changed;
  }

  //----------------------------------------------------------------------------------------------------
  void UiContext::beginDrag(Widget* src, const DragPayload& p, const DragVisual& v, const Vec2& start)
  {
    drag = DragController{}; // reset
    drag.active = true;
    drag.thresholdMet = false;
    drag.startPos = start;
    drag.pos = start;
    drag.lastPos = start;
    drag.source = src;
    drag.payload = p;
    drag.visual = v;
    // While dragging, suppress normal hover tooltip
    tip.armed = false; tip.visible = false; tip.fadingOut = false; tip.alpha = 0.f;
  }

  //-----------------------------------------------------------------------------------------
  glm::vec2 UiContext::screenToVirtual(Vec2 s) const
  {
    // letterbox fit with uniform scale
    float sx = (float)fbW / virtualW, sy = (float)fbH / virtualH;
    float scale = std::min(sx, sy);
    float padX = (fbW - virtualW * scale) * 0.5f;
    float padY = (fbH - virtualH * scale) * 0.5f;
    return Vec2{ (s.x - padX) / scale, (s.y - padY) / scale };
  }

  //-----------------------------------------------------------------------------------------
  glm::vec4 UiContext::virtualToScissor(const Rect& r) const
  {
    const float sx = (float)fbW / virtualW;
    const float sy = (float)fbH / virtualH;
    const float scale = std::min(sx, sy);
    const float padX = (fbW - virtualW * scale) * 0.5f;
    const float padY = (fbH - virtualH * scale) * 0.5f;

    // float bounds (bottom-left origin for GL)
    const float x0 = padX + r.x * scale;
    const float y1 = padY + (r.y + r.h) * scale;         // top in screen space
    const float y0 = fbH - y1;                            // bottom for scissor
    const float x1 = padX + (r.x + r.w) * scale;

    // conservative integer scissor: include all pixels that intersect the float rect
    const int ix = (int)std::floor(x0);
    const int iy = (int)std::floor(y0);
    const int iw = (int)std::ceil(x1) - ix;
    const int ih = (int)std::ceil(y0 + (r.h * scale)) - iy; // or (int)std::ceil(fbH - (padY + r.y*scale)) - iy

    return glm::vec4(ix, iy, std::max(0, iw), std::max(0, ih));
  }
}