#include "stdafx.h"

#include <algorithm>
#include <unordered_set>

#include "Widget.h"
#include "Context.h"
#include "Wrap.h"
#include "StyleResolver.h"
#include "Focus.h"
#include "Utils.h"

namespace UI_lib
{
  static inline std::string entityKeyFromId(const std::string& id) {
    constexpr const char* k = "ent:";
    if (id.rfind(k, 0) == 0 && id.size() > 4) return id.substr(4);
    return {};
  }

  //----------------------------------------------------------------------------
  void Widget::layout(UiContext& ctx, const Rect& parentRect)
  {
    // 1) start from local each frame
    Rect r = local; // <- local YAML space (relative to parent)

    // 2) anchors reinterpret local against parent size (optional)
    if (anchors.hasL || anchors.hasR || anchors.hasT || anchors.hasB) {
      if (anchors.hasL && anchors.hasR) {
        r.x = anchors.left;
        r.w = std::max(0.f, parentRect.w - anchors.left - anchors.right);
      }
      else if (anchors.hasL) {
        r.x = anchors.left;
      }
      else if (anchors.hasR) {
        r.x = std::max(0.f, parentRect.w - anchors.right - r.w);
      }
      if (anchors.hasT && anchors.hasB) {
        r.y = anchors.top;
        r.h = std::max(0.f, parentRect.h - anchors.top - anchors.bottom);
      }
      else if (anchors.hasT) {
        r.y = anchors.top;
      }
      else if (anchors.hasB) {
        r.y = std::max(0.f, parentRect.h - anchors.bottom - r.h);
      }
    }

    // 3) local -> absolute
    Rect abs{ parentRect.x + r.x, parentRect.y + r.y, r.w, r.h };

    // 4) clip against parent’s clip (or viewport)
    const Rect parentClip = (this->parent ? this->parent->clip : ctx.viewport);
    clip = intersect(parentClip, abs);

    // 5) store absolute for drawing/hit
    rect = abs;

    // 6) recurse
    layoutChildren(ctx);
  }

  //---------------------------------------------------------------------------
  void Widget::draw(UiContext& ctx) const
    {
     for (const auto& c : children)
       if (c->visible) c->draw(ctx);

     if (ctx.debugLayout) {
       const uint32_t CYAN = 0x00FFFFFF; // solid color should not be sampling texture, need separate shader path to see cyan @todo
       drawOutlineDevicePx(*ctx.prim, *ctx.dl, rect, CYAN, clip, ctx);
       }
    }

  //-------------------------------------------------------------------------
  bool Widget::onEvent(UiContext& ctx, const UIEvent& e)
  {
    if (e.kind == UIEvent::Kind::Pointer) {
      const auto& p = std::get<PointerEvent>(e.data);
      if (p.type == PointerEvent::Type::Enter) {
        if (hasRichTip) {
          ctx.armRich(rect, richTip);
        }
      }
      else if (p.type == PointerEvent::Type::Leave) {
        if (hasRichTip) {
          ctx.disarmRich();
        }
      }
    }
    return false;
  }

  //-------------------------------------------------------------------------
  void Widget::layoutAbsolute(UiContext& ctx, const Rect& absRect)
  {
    const Rect parentClip = (this->parent ? this->parent->clip : ctx.viewport);
    clip = intersect(parentClip, absRect);
    rect = absRect;
    layoutChildren(ctx);
  }

  //-----------------------------------------------------------------------------------
  void Label::draw(UiContext& ctx) const
  {
    if (!ctx.font) return;

    // Visual geometry
    const Rect vr = visualRect();
    const float opa = visualOpacity();

    // Device-pixel helpers
    const float s = uiScale(ctx);
    auto snap = [s](float v) { return (s > 0.f) ? std::round(v * s) / s : v; };
    auto snapR = [snap](Rect r) {
      float x0 = snap(r.x), y0 = snap(r.y);
      float x1 = snap(r.x + r.w), y1 = snap(r.y + r.h);
      return Rect{ x0, y0, x1 - x0, y1 - y0 };
    };
    auto inflate = [](Rect r, float dx, float dy) { return Rect{ r.x - dx, r.y - dy, r.w + 2 * dx, r.h + 2 * dy }; };

    const Rect vrs = snapR(vr);
    const float clipPad = (s > 0.f) ? (1.0f / s) : 1.0f;
    const Rect clipUse = inflate(clip, clipPad, clipPad);

    // Font metrics
    const float px = (fontSize > 0.f) ? fontSize : ctx.theme.fontSizes[2];
    const float lineH = ctx.font->lineHeight * px;
    const float ascPx = ctx.font->ascender * px;

    // Shape lines (use widthPx per line)
    std::u32string s32 = utf8ToUtf32(text);
    std::vector<ShapedLine> lines;
    if (wrap == Wrap::Wrap) {
      lines = wrapGreedy(*ctx.font, s32, px, std::max(0.f, vrs.w));
    }
    else {
      if (overflow == Overflow::Ellipsis) {
        s32 = elideWithEllipsis(*ctx.font, s32, px, std::max(0.f, vrs.w));
      }
      ShapedLine l;
      l.text = s32;
      l.widthPx = measureRun(*ctx.font, s32, px).widthPx;
      lines.push_back(std::move(l));
    }
    // Safety: ensure widthPx is filled (some wrappers might skip it)
    for (auto& L : lines) {
      if (L.widthPx <= 0.f) L.widthPx = measureRun(*ctx.font, L.text, px).widthPx;
    }

    // Vertical align (snap)
    float yStart = vrs.y;
    switch (valign) {
    case VAlign::Top:    yStart = vrs.y;                                  break;
    case VAlign::Center: yStart = vrs.y + (vrs.h - lineH * lines.size()) * 0.5f; break;
    case VAlign::Bottom: yStart = vrs.y + (vrs.h - lineH * lines.size()); break;
    }
    yStart = snap(yStart);

    // Compose color × opacity
    auto mulAlpha = [](uint32_t rgba, float a) {
      a = std::clamp(a, 0.f, 1.f);
      uint32_t A = (rgba) & 0xFFu;
      uint32_t R = (rgba >> 24) & 0xFFu;
      uint32_t G = (rgba >> 16) & 0xFFu;
      uint32_t B = (rgba >> 8) & 0xFFu;
      A = uint32_t(std::round(float(A) * a));
      return (R << 24) | (G << 16) | (B << 8) | A;
    };
    const uint32_t colA = mulAlpha(color, opa);

    ctx.prim->pushClip(clipUse);

    // Horizontal align per line (snap X and baseline)
    float y = yStart;
    for (const auto& L : lines) {
      float x = vrs.x; // left by default
      switch (halign) {
      case HAlign::Left:   x = vrs.x;                                   break;
      case HAlign::Center: x = vrs.x + 0.5f * (vrs.w - L.widthPx);      break;
      case HAlign::Right:  x = vrs.x + (vrs.w - L.widthPx);             break;
      }
      const float xSnap = snap(x);
      const float baselineY = snap(y + ascPx);

      ctx.prim->textMSDF(*ctx.dl, *ctx.font, L.text,
        { xSnap, baselineY - ascPx }, px, colA, clipUse);

      y += lineH;
    }

    ctx.prim->popClip();

    if (ctx.debugLayout) {
      const uint32_t BBOX = 0x00FF00FF;
      ctx.prim->solidRect(*ctx.dl, { vrs.x, vrs.y, vrs.w, 1 }, BBOX, clipUse);
      ctx.prim->solidRect(*ctx.dl, { vrs.x, vrs.y + vrs.h - 1, vrs.w, 1 }, BBOX, clipUse);
      ctx.prim->solidRect(*ctx.dl, { vrs.x, vrs.y, 1, vrs.h }, BBOX, clipUse);
      ctx.prim->solidRect(*ctx.dl, { vrs.x + vrs.w - 1, vrs.y, 1, vrs.h }, BBOX, clipUse);
    }
  }

  //----------------------------------------------------------------------------------------
  bool Button::onEvent(UiContext& ctx, const UIEvent& e)
  {
    if (e.kind == UIEvent::Kind::Key)
    {
      const auto ke = std::get<KeyEvent>(e.data);
      if (ke.type == KeyEvent::Type::KeyDown && (ke.key == Key::Enter || ke.key == Key::Space)) {
        if (onClick) onClick();
          return true;
      }
    }
    if (e.kind == UIEvent::Kind::Gamepad)
    {
      const auto gp = std::get<GamepadEvent>(e.data);
      if (gp.type == GamepadEvent::Type::Activate) 
        { if (onClick) onClick();
            return true; }
    }

    auto pe = std::get<PointerEvent>(e.data);
    switch (pe.type) {
    case PointerEvent::Type::Enter: hover = true; return false;
    case PointerEvent::Type::Leave: hover = false; pressed = false; return false;
    case PointerEvent::Type::Down:
      if (hitTest(pe.pos)) 
      {
        pressed = true;
        if (ctx.root)
          ctx.root->pointerCapture = this;
        return true;
      }
      return false;
    case PointerEvent::Type::Up:
      if (pressed)
      {
        pressed = false;
        if(ctx.root->pointerCapture == this)
          ctx.root->pointerCapture = nullptr;
        if (onClick)
          onClick();
          return true;
      }
      return false;
    default: break;
    }
    return false;
  }

  //------------------------------------------------------------------
  void Button::draw(UiContext& ctx) const
  {
    UIState st = pressed ? UIState::Pressed : (hover ? UIState::Hover : UIState::Base);
    Style s = resolveStyle(ctx.theme, "Button", id, st, inlineStyle);

    // Fetch the nine-slice + its texture from any loaded atlas
    const auto nl = ctx.findNineWithTex("button_panel");
    if (!nl.nine || nl.tex == 0) return;

    auto mulAlpha = [](uint32_t rgba, float a) {
      a = std::clamp(a, 0.f, 1.f);
      uint32_t A = (rgba) & 0xFFu;
      uint32_t R = (rgba >> 24) & 0xFFu;
      uint32_t G = (rgba >> 16) & 0xFFu;
      uint32_t B = (rgba >> 8) & 0xFFu;
      A = uint32_t(std::round(float(A) * a));
      return (R << 24) | (G << 16) | (B << 8) | A;
    };

    // State color × animated opacity
    uint32_t col = colorBg;
    if (hover)   col = 0xA47B2AFF;
    if (pressed) col = 0x73551FFF;
    const uint32_t colA = mulAlpha(col, visualOpacity());

    // Draw the panel using visual rect (so transitions apply)
    const Rect vr = visualRect();

    ctx.prim->pushClip(clip);
    ctx.prim->nineSlice(*ctx.dl, nl.tex, vr, *nl.nine, colA, clip);

    // Optional text placeholder sprite (debug)
    if (const Sprite* spr = ctx.findSprite("_debug_text_bg"); spr && spr->tex) {
      Rect t = { vr.x + 8, vr.y + 4, vr.w - 16, vr.h - 8 };
      ctx.prim->texturedQuad(*ctx.dl,
        spr->tex, spr->texW, spr->texH,
        t, spr->uv,
        0xFFFFFFFF, clip);
    }
    else {
      static std::unordered_set<std::string> once;
      if (!once.count("_debug_text_bg")) {
        once.insert("_debug_text_bg");
        fprintf(stderr, "[UI] sprite '%s' not found\n", "_debug_text_bg");
      }
      return;
    }
    // Children
    Widget::draw(ctx);

    // Focus debug outline
    if (focused && ctx.debugLayout) {
      const uint32_t CYAN = 0x00FFFFFF;
      ctx.prim->solidRect(*ctx.dl, Rect{ vr.x, vr.y, vr.w, 1 }, CYAN, clip);
      ctx.prim->solidRect(*ctx.dl, Rect{ vr.x, vr.y + vr.h - 1, vr.w, 1 }, CYAN, clip);
      ctx.prim->solidRect(*ctx.dl, Rect{ vr.x, vr.y, 1, vr.h }, CYAN, clip);
      ctx.prim->solidRect(*ctx.dl, Rect{ vr.x + vr.w - 1, vr.y, 1, vr.h }, CYAN, clip);
    }

    ctx.prim->popClip();
  }

  //---------------------------------------------------------------------
  void Panel::draw(UiContext& ctx) const
  {
    UIState st = pressed ? UIState::Pressed : (hover ? UIState::Hover : UIState::Base);
    const auto rs = resolveStyle(ctx.theme, "Panel", id, st, inlineStyle);

    auto mulAlpha = [](uint32_t rgba, float a) {
      a = std::clamp(a, 0.f, 1.f);
      uint32_t A = (rgba) & 0xFFu;
      uint32_t R = (rgba >> 24) & 0xFFu;
      uint32_t G = (rgba >> 16) & 0xFFu;
      uint32_t B = (rgba >> 8) & 0xFFu;
      A = uint32_t(std::round(float(A) * a));
      return (R << 24) | (G << 16) | (B << 8) | A;
    };

    const Rect vr = visualRect();
    const uint32_t tintWithOpacity = mulAlpha(tint, visualOpacity());

    ctx.prim->pushClip(clip);

    // NEW: fetch nine-slice and its texture from any loaded atlas
    const auto nl = ctx.findNineWithTex(nineName);
    if (nl.nine && nl.tex != 0) {
      ctx.prim->nineSlice(*ctx.dl, nl.tex, vr, *nl.nine, tintWithOpacity, clip);
    }

    Widget::draw(ctx); // draw children after
    ctx.prim->popClip();
  }

  //------------------------------------------------------------------------------
  bool Panel::onEvent(UiContext& ctx, const UIEvent& e)
  {
    return Widget::onEvent(ctx, e);
  }

  //------------------------------------------------------------------------------
  bool Image::onEvent(UiContext& ctx, const UIEvent& e)
  {
    if (e.kind != UIEvent::Kind::Pointer) return Widget::onEvent(ctx, e);
    const auto p = std::get<PointerEvent>(e.data);

    if (p.type == PointerEvent::Type::Down && p.button == PointerButton::Left && draggable)
    {
      // Extract entityKey from this->id if it was mounted as "ent:<key>"
      const std::string ek = this->id;
      if (ek.empty()) {
        // Not an entity mount → let normal handling continue
        return Widget::onEvent(ctx, e);
      }

      // Build payload: use s0 for the string key; keep i0 free for numeric ids if needed
      DragPayload pay;
      pay.type = DragPayload::Type::Citizen;  // or Custom if you want it more generic
      pay.s0 = ek;                          // <--- entityKey lives here

      // Ghost visual: reuse our sprite
      DragVisual vis;
      vis.spriteName = this->spriteName;
      vis.tint = 0xFFFFFFB0;
      vis.hotspot = { 0.5f, 0.5f };

      ctx.beginDrag(this, pay, vis, p.pos);
      return true; // consumed
    }
    return Widget::onEvent(ctx, e);
  }

  //------------------------------------------------------------------------------
  void Image::draw(UiContext& ctx) const
  {
    const Sprite* spr = ctx.findSprite(spriteName);
    if (!spr || spr->tex == 0) { Widget::draw(ctx); return; }

    const Rect vr = visualRect();

    // Compose animated opacity into tint (RGBA = R<<24|G<<16|B<<8|A)
    auto mulAlpha = [](uint32_t rgba, float a) {
      a = std::clamp(a, 0.f, 1.f);
      uint32_t A = (rgba) & 0xFFu;
      uint32_t R = (rgba >> 24) & 0xFFu;
      uint32_t G = (rgba >> 16) & 0xFFu;
      uint32_t B = (rgba >> 8) & 0xFFu;
      A = uint32_t(std::round(float(A) * a));
      return (R << 24) | (G << 16) | (B << 8) | A;
    };
    const uint32_t tintA = mulAlpha(tint, visualOpacity());

    ctx.prim->pushClip(clip);
    ctx.prim->texturedQuad(*ctx.dl, spr->tex, spr->texW, spr->texH, vr, spr->uv, tintA, clip, spriteName == "city_rtt"); // ! invert_y for city strategic view backend buffer
    Widget::draw(ctx);
    ctx.prim->popClip();
  }

  //--------------------------------------------------------------------------------
  void IconButton::draw(UiContext& ctx) const
  {
    const Rect vr = visualRect();

    auto mulAlpha = [](uint32_t rgba, float a) {
      a = std::clamp(a, 0.f, 1.f);
      uint32_t A = (rgba) & 0xFFu;
      uint32_t R = (rgba >> 24) & 0xFFu;
      uint32_t G = (rgba >> 16) & 0xFFu;
      uint32_t B = (rgba >> 8) & 0xFFu;
      A = uint32_t(std::round(float(A) * a));
      return (R << 24) | (G << 16) | (B << 8) | A;
      };

    ctx.prim->pushClip(clip);

    // --- hover/pressed background, works even if spriteName is empty ---
    if (hover || pressed) {
      uint32_t bg = pressed ? 0x00000033 : 0x00000022;          // subtle dark overlay
      bg = mulAlpha(bg, visualOpacity());
      ctx.prim->solidRect(*ctx.dl, vr, bg, clip);
    }

    // --- Existing: sprite draw (only if we have one) ---
    if (!spriteName.empty()) {
      const Sprite* spr = ctx.findSprite(spriteName);
      if (spr && spr->tex != 0) {
        uint32_t t = tintNormal;
        if (pressed) t = tintDown;
        else if (hover) t = tintHover;

        const uint32_t tintA = mulAlpha(t, visualOpacity());

        const float iw = spr->uv.w, ih = spr->uv.h;
        if (iw > 0 && ih > 0 && vr.w > 0 && vr.h > 0) {
          const float s = std::min(vr.w / iw, vr.h / ih);
          const float dw = std::floor(iw * s + 0.5f);
          const float dh = std::floor(ih * s + 0.5f);
          const float dx = std::floor(vr.x + (vr.w - dw) * 0.5f + 0.5f);
          const float dy = std::floor(vr.y + (vr.h - dh) * 0.5f + 0.5f);
          const Rect dst{ dx, dy, dw, dh };

          ctx.prim->texturedQuad(*ctx.dl, spr->tex, spr->texW, spr->texH, dst, spr->uv, tintA, clip);
        }
      }
    }

    // Children (if any)
    Widget::draw(ctx);

    // focus outline (debug)
    if (focused && ctx.debugLayout) {
      const uint32_t CYAN = 0x00FFFFFF;
      ctx.prim->solidRect(*ctx.dl, Rect{ vr.x, vr.y, vr.w, 1 }, CYAN, clip);
      ctx.prim->solidRect(*ctx.dl, Rect{ vr.x, vr.y + vr.h - 1, vr.w, 1 }, CYAN, clip);
      ctx.prim->solidRect(*ctx.dl, Rect{ vr.x, vr.y, 1, vr.h }, CYAN, clip);
      ctx.prim->solidRect(*ctx.dl, Rect{ vr.x + vr.w - 1, vr.y, 1, vr.h }, CYAN, clip);
    }
    ctx.prim->popClip();
  }

  //-----------------------------------------------------------------------------
  void UiRoot::buildPath(Widget* root, const Vec2& p, std::vector<Widget*>& out)
  {
    out.clear();
    if (!root || !root->visible) return;

    // Internal helper returns true if it found a valid target in this subtree.
    std::function<bool(Widget*)> walk = [&](Widget* w) -> bool
      {
        if (!w || !w->visible) return false;

        // Bounds/clip gate for the whole subtree
        if (!w->hitTest(p)) return false;

        // Try children first (reverse = topmost)
        if (w->hitTestChildren) {
          for (auto it = w->children.rbegin(); it != w->children.rend(); ++it) {
            Widget* c = it->get();
            if (walk(c)) {
              out.insert(out.begin(), w);  // prepend ancestor
              return true;
            }
          }
        }

        // If no child was hit, this widget can be the target only if self is hittable
        if (!w->hitTestSelf) return false;

        out.push_back(w);
        return true;
      };

    walk(root);
  }

  //-----------------------------------------------------------------------------
  static void buildPathToLeaf(Widget* leaf, std::vector<Widget*>& out)
  {
    out.clear();
    for (Widget* p = leaf; p; p = p->parent) out.push_back(p);
    std::reverse(out.begin(), out.end());
  }

  //--------------------------------------------------------------------
  bool UiRoot::isDescendant(const Widget* root, const Widget* w)
  {
    if (!root || !w) return false;
    if (root == w) return true;
    for (auto const& c : root->children)
      if (isDescendant(c.get(), w)) return true;
    return false;
  }

  //--------------------------------------------------------------------
  bool UiRoot::isInAnyRoot(const Widget* w) const
  {
    if (!w) return false;
    for (auto* r : contents)
      if (r && isDescendant(r, w)) return true;
    return false;
  }

  //--------------------------------------------------------------------
  void UiRoot::sanitizePointers(UiContext& ctx)
  {
    if (pointerCapture && !isInAnyRoot(pointerCapture))
      pointerCapture = nullptr;

    if (hoverWidget && !isInAnyRoot(hoverWidget))
      hoverWidget = nullptr;

    if (focusWidget && !isInAnyRoot(focusWidget)) {
      // avoid leaving a focused-but-dead widget around
      focusWidget = nullptr;
    }
  }

  //--------------------------------------------------------------------
  bool UiRoot::route(UiContext& ctx, const UIEvent& ev)
  {
    if (contents.empty()) return false;

    sanitizePointers(ctx);

    // Helpers to iterate visible roots in z-order
    auto topToBottom = [&](auto&& fn)->bool {
      for (int i = (int)contents.size() - 1; i >= 0; --i) {
        Widget* root = contents[i];
        if (!root || !root->visible) continue;
        if (fn(root)) return true;
      }
      return false;
    };
    auto firstRootHit = [&](const Vec2& p)->Widget* {
      for (int i = (int)contents.size() - 1; i >= 0; --i) {
        Widget* root = contents[i];
        if (!root || !root->visible) continue;
        std::vector<Widget*> path; buildPath(root, p, path);
        if (!path.empty()) return root;
      }
      return nullptr;
    };
    auto topVisibleRoot = [&]()->Widget* {
      for (int i = (int)contents.size() - 1; i >= 0; --i) {
        Widget* root = contents[i];
        if (root && root->visible) return root;
      }
      return nullptr;
    };

    // ===========================
    // KEYBOARD (incl. Esc cancel)
    // ===========================
    if (ev.kind == UIEvent::Kind::Key)
    {
      const auto ke = std::get<KeyEvent>(ev.data);

      // Esc cancels drag
      if (ke.type == KeyEvent::Type::KeyDown && ke.key == Key::Escape) {
        if (ctx.drag.active) {
          DragEvent dCancel{ DragEvent::Type::Cancel, ctx.drag.pos, {0,0}, 0 };
          if (ctx.drag.source)
            { UIEvent e; e.kind = UIEvent::Kind::Drag; e.data = dCancel; ctx.drag.source->onDrag(ctx, dCancel, ctx.drag.payload); }
          DragEvent dEnd{ DragEvent::Type::End, ctx.drag.pos, {0,0}, 0 };
          if (ctx.drag.source)
            { UIEvent e; e.kind = UIEvent::Kind::Drag; e.data = dEnd;    ctx.drag.source->onDrag(ctx, dEnd, ctx.drag.payload); }
          ctx.endDrag(); ctx.invalidate();
          return true;
        }
      }

      if (ke.type != KeyEvent::Type::KeyDown) return false;

      const bool shift = (int(ke.mods) & int(ModShift)) != 0;
      Widget* navRoot = topVisibleRoot(); // navigation scope = topmost visible root
      switch (ke.key)
      {
      case Key::Tab: {
        Widget* next = shift ? prevByTab(navRoot, ctx.focus.current)
          : nextByTab(navRoot, ctx.focus.current);
        if (next) ctx.requestFocus(next);
        return true;
      }
      case Key::Left: { if (auto* n = bestInDirection(navRoot, ctx.focus.current, NavDir::Left))  ctx.requestFocus(n); return true; }
      case Key::Right: { if (auto* n = bestInDirection(navRoot, ctx.focus.current, NavDir::Right)) ctx.requestFocus(n); return true; }
      case Key::Up: { if (auto* n = bestInDirection(navRoot, ctx.focus.current, NavDir::Up))    ctx.requestFocus(n); return true; }
      case Key::Down: { if (auto* n = bestInDirection(navRoot, ctx.focus.current, NavDir::Down))  ctx.requestFocus(n); return true; }
      case Key::Enter:
      case Key::Space: {
        if (auto* w = ctx.focus.current) {
          if (auto* b = dynamic_cast<Button*>(w); b && b->onClick) { b->onClick(); return true; }
        }
        return true;
      }
      default: break;
      }
      return false;
    }

    // ===========================
    // GAMEPAD
    // ===========================
    if (ev.kind == UIEvent::Kind::Gamepad) {
      const auto gp = std::get<GamepadEvent>(ev.data);
      Widget* navRoot = topVisibleRoot();
      switch (gp.type)
      {
      case GamepadEvent::Type::DPad: {
        if (auto* n = bestInDirection(navRoot, ctx.focus.current, gp.dir)) ctx.requestFocus(n);
        return true;
      }
      case GamepadEvent::Type::Activate: {
        if (auto* w = ctx.focus.current) {
          if (auto* b = dynamic_cast<Button*>(w); b && b->onClick) { b->onClick(); return true; }
        }
        return true;
      }
      case GamepadEvent::Type::Back: { ctx.clearFocus(); return true; }
      }
      return false;
    }

    // ===========================
    // NON-POINTER → route top→down
    // ===========================
    if (ev.kind != UIEvent::Kind::Pointer) {
      return topToBottom([&](Widget* root) { return root->onEvent(ctx, ev); });
    }

    const PointerEvent pe = std::get<PointerEvent>(ev.data);

    // Track cursor (tooltips/drag)
    if (pe.type == PointerEvent::Type::Move ||
      pe.type == PointerEvent::Type::Down ||
      pe.type == PointerEvent::Type::Up) {
      ctx.tip.cursor = pe.pos;
      ctx.cursorPx = pe.pos;
    }

    // ===========================
    // DRAG ACTIVE PATH
    // ===========================
    if (ctx.drag.active)
    {
      // Suppress thin tooltip while dragging
      ctx.tip.armed = false; ctx.tip.visible = false; ctx.tip.fadingOut = false; ctx.tip.alpha = 0.f;

      // Route drag over the topmost root under cursor (if any)
      Widget* dragRoot = firstRootHit(pe.pos);

      if (pe.type == PointerEvent::Type::Move)
      {
        ctx.drag.lastPos = ctx.drag.pos;
        ctx.drag.pos = pe.pos;

        // Threshold to "Begin"
        if (!ctx.drag.thresholdMet) {
          const float dx = ctx.drag.pos.x - ctx.drag.startPos.x;
          const float dy = ctx.drag.pos.y - ctx.drag.startPos.y;
          if (dx * dx + dy * dy >= 4.0f * 4.0f) {
            ctx.drag.thresholdMet = true;
            if (ctx.drag.source) {
              DragEvent dBeg{ DragEvent::Type::Begin, ctx.drag.pos, {0,0}, pe.time };
              UIEvent e; e.kind = UIEvent::Kind::Drag; e.data = dBeg;
              ctx.drag.source->onDrag(ctx, dBeg, ctx.drag.payload);
            }
          }
        }

        // Once begun, synth enter/leave/over on current root
        if (ctx.drag.thresholdMet && dragRoot) {
          std::vector<Widget*> path; buildPath(dragRoot, pe.pos, path);
          Widget* target = path.empty() ? nullptr : path.back();

          if (target != ctx.drag.hoverTarget) {
            if (ctx.drag.hoverTarget) {
              DragEvent dLeave{ DragEvent::Type::Leave, pe.pos, {0,0}, pe.time };
              UIEvent e; e.kind = UIEvent::Kind::Drag; e.data = dLeave;
              ctx.drag.hoverTarget->onDrag(ctx, dLeave, ctx.drag.payload);
            }
            ctx.drag.hoverTarget = target;
            if (target) {
              DragEvent dEnter{ DragEvent::Type::Enter, pe.pos, {0,0}, pe.time };
              UIEvent e; e.kind = UIEvent::Kind::Drag; e.data = dEnter;
              target->onDrag(ctx, dEnter, ctx.drag.payload);
            }
          }

          if (target) {
            DragEvent dOver{ DragEvent::Type::Over, pe.pos,
                             { ctx.drag.pos.x - ctx.drag.lastPos.x, ctx.drag.pos.y - ctx.drag.lastPos.y },
                             pe.time };
            ctx.drag.anyAccepting = target->canAccept(ctx.drag.payload) &&
              target->onDrag(ctx, dOver, ctx.drag.payload);
            ctx.drag.dropTarget = ctx.drag.anyAccepting ? target : nullptr;
          }
          else {
            ctx.drag.anyAccepting = false;
            ctx.drag.dropTarget = nullptr;
          }
          ctx.invalidate();
        }
      }

      if (pe.type == PointerEvent::Type::Up)
      {
        if (ctx.drag.thresholdMet && ctx.drag.dropTarget) {
          DragEvent dDrop{ DragEvent::Type::Drop, pe.pos, {0,0}, pe.time };
          ctx.drag.dropTarget->onDrag(ctx, dDrop, ctx.drag.payload);
        }
        else {
          if (ctx.drag.source) {
            DragEvent dCancel{ DragEvent::Type::Cancel, pe.pos, {0,0}, pe.time };
            UIEvent e; e.kind = UIEvent::Kind::Drag; e.data = dCancel;
            ctx.drag.source->onDrag(ctx, dCancel, ctx.drag.payload);
          }
        }
        if (ctx.drag.source) {
          DragEvent dEnd{ DragEvent::Type::End, pe.pos, {0,0}, pe.time };
          UIEvent e; e.kind = UIEvent::Kind::Drag; e.data = dEnd;
          ctx.drag.source->onDrag(ctx, dEnd, ctx.drag.payload);
        }
        ctx.endDrag();
        ctx.invalidate();
      }

      // Swallow all pointer events during active drag
      return true;
    }

    // ===========================
    // POINTER CAPTURE SHORT-CIRCUIT
    // ===========================
    if (pointerCapture) {
      ctx.tip.armed = false; ctx.tip.visible = false; ctx.tip.fadingOut = false; ctx.tip.alpha = 0.f;
      return pointerCapture->onEvent(ctx, ev);
    }

    // ===========================
    // SCROLL SPECIAL-CASE
    // ===========================
    if (pe.type == PointerEvent::Type::Scroll) {
      Widget* target = hoverWidget;
      if (!target) {
        Widget* r = firstRootHit(ctx.tip.cursor);
        if (!r) return topToBottom([&](Widget* root) { return root->onEvent(ctx, ev); }); // fallback
        // Bubble within that root
        std::vector<Widget*> path; buildPath(r, ctx.tip.cursor, path);
        if (path.empty()) return false;
        for (size_t i = 0; i + 1 < path.size(); ++i) { UIEvent e = ev; if (path[i]->onEvent(ctx, e)) return true; }
        if (path.back()->onEvent(ctx, ev)) return true;
        for (size_t i = path.size(); i-- > 0; ) { UIEvent e = ev; if (path[i]->onEvent(ctx, e)) return true; }
        return false;
      }
      // We have a hover target; bubble within its tree
      std::vector<Widget*> path; buildPathToLeaf(target, path);
      for (size_t i = 0; i + 1 < path.size(); ++i) { UIEvent e = ev; if (path[i]->onEvent(ctx, e)) return true; }
      if (target->onEvent(ctx, ev)) return true;
      for (size_t i = path.size(); i-- > 0; ) { UIEvent e = ev; if (path[i]->onEvent(ctx, e)) return true; }
      return false;
    }

    // ===========================
    // NORMAL POINTER: hover synth + routing
    // ===========================
    return topToBottom([&](Widget* root) {
      std::vector<Widget*> path; buildPath(root, pe.pos, path);
      Widget* target = path.empty() ? nullptr : path.back();
      if (!target) return false; // try next lower root

      if (target != hoverWidget) {
        // LEAVE previous
        if (hoverWidget) {
          ctx.tip.armed = false; ctx.tip.visible = false; ctx.tip.text.clear();
          ctx.tip.anchorRect = Rect{};
          UIEvent leaveEv; leaveEv.kind = UIEvent::Kind::Pointer;
          PointerEvent pev{}; pev.type = PointerEvent::Type::Leave; pev.pos = pe.pos; pev.delta = { 0,0 }; pev.wheel = 0.f;
          pev.button = pe.button; pev.clicks = pe.clicks; pev.mods = pe.mods; pev.frame = pe.frame; pev.time = pe.time;
          leaveEv.data = pev;
          hoverWidget->onEvent(ctx, leaveEv);
        }

        // ENTER new
        hoverWidget = target;
        if (hoverWidget) {
          if (!hoverWidget->tooltip.empty()) {
            ctx.tip.armed = true; ctx.tip.tHoverMs = 0.f; ctx.tip.text = hoverWidget->tooltip;
            ctx.tip.armPos = ctx.tip.cursor; ctx.tip.anchorRect = hoverWidget->visualRect();
          }
          UIEvent enterEv; enterEv.kind = UIEvent::Kind::Pointer;
          PointerEvent pev{}; pev.type = PointerEvent::Type::Enter; pev.pos = pe.pos; pev.delta = { 0,0 }; pev.wheel = 0.f;
          pev.button = pe.button; pev.clicks = pe.clicks; pev.mods = pe.mods; pev.frame = pe.frame; pev.time = pe.time;
          enterEv.data = pev;
          hoverWidget->onEvent(ctx, enterEv);
        }
      }

      // Capture phase
      for (size_t i = 0; i + 1 < path.size(); ++i) { UIEvent e = ev; if (path[i]->onEvent(ctx, e)) return true; }
      // Target
      if (target->onEvent(ctx, ev))
      {
        if(pe.type == PointerEvent::Type::Up)
        return true;
      }
      // Bubble phase
      for (size_t i = path.size(); i-- > 0; ) { UIEvent e = ev; if (path[i]->onEvent(ctx, e)) return true; }
      return true; // handled within this root; don't fall through
      });
  }

  //-------------------------------------------------------------------------
  void UiRoot::draw(UiContext& ctx)
  {
    for (Widget* root : contents)
    {
      if (!root || !root->visible) continue;
      root->layout(ctx, ctx.viewport);
      root->draw(ctx);
    }
  }

  static float childNaturalWidth(const Widget* w) { return w->rect.w; }
  static float childNaturalHeight(const Widget* w) { return w->rect.h; }

  static void LogChildRects(const char* tag, const Widget& w) {
    printf("[FlexRow:%s] rect=(%.0f,%.0f,%.0f,%.0f)\n",
      tag, w.rect.x, w.rect.y, w.rect.w, w.rect.h);
    for (auto& c : w.children) {
      printf("  - %s : (x=%.0f y=%.0f w=%.0f h=%.0f)\n",
        c->id.c_str(), c->rect.x, c->rect.y, c->rect.w, c->rect.h);
    }
  }

  //--------------------------------------------------------------------------------------------------------------
  void FlexRow::layout(UiContext& ctx, const Rect& parentRect)
  {
    // --- Resolve this row's rect from local + anchors (same logic as Widget::layout) ---
    Rect r = local; // local is relative to parent

    if (anchors.hasL || anchors.hasR || anchors.hasT || anchors.hasB) {
      // X / W
      if (anchors.hasL && anchors.hasR) {
        r.x = anchors.left;
        r.w = std::max(0.f, parentRect.w - anchors.left - anchors.right);
      }
      else if (anchors.hasL) {
        r.x = anchors.left;
      }
      else if (anchors.hasR) {
        r.x = std::max(0.f, parentRect.w - anchors.right - r.w);
      }
      // Y / H
      if (anchors.hasT && anchors.hasB) {
        r.y = anchors.top;
        r.h = std::max(0.f, parentRect.h - anchors.top - anchors.bottom);
      }
      else if (anchors.hasT) {
        r.y = anchors.top;
      }
      else if (anchors.hasB) {
        r.y = std::max(0.f, parentRect.h - anchors.bottom - r.h);
      }
    }

    // Absolute rect and clip
    Rect abs{ parentRect.x + r.x, parentRect.y + r.y, r.w, r.h };
    const Rect parentClip = (this->parent ? this->parent->clip : ctx.viewport);
    clip = intersect(parentClip, abs);
    rect = abs;

    // --- Inner content box ---
    const float innerW = std::max(0.f, rect.w - (padL + padR));
    const float innerH = std::max(0.f, rect.h - (padT + padB));

    // --- Preferred width for children (labels with w=0 won't collapse) ---
    auto preferredWidth = [&](Widget* c) -> float {
      if (c->local.w > 0.f) return c->local.w;

      if (auto* lbl = dynamic_cast<Label*>(c)) {
        const float px = (lbl->fontSize > 0.f) ? lbl->fontSize : ctx.theme.fontSizes[2];
        std::u32string s32 = utf8ToUtf32(lbl->text);
        float w = measureRun(*ctx.font, s32, px).widthPx;
        return std::ceil(w + 0.5f);
      }
      if (auto* img = dynamic_cast<Image*>(c)) {
        return (img->local.w > 0.f) ? img->local.w
          : std::max((img->local.h > 0.f ? img->local.h : 32.f), 1.f);
      }
      if (auto* ib = dynamic_cast<IconButton*>(c)) {
        return std::max(ib->local.w, 36.f);
      }
      return std::max(1.f, c->local.w);
    };

    float totalFixedW = 0.f;
    for (auto& c : children) totalFixedW += preferredWidth(c.get());

    const int n = (int)children.size();
    const float gapsTotal = gap * std::max(0, n - 1);
    const float freeW = std::max(0.f, innerW - totalFixedW - gapsTotal);

    // --- Horizontal justification ---
    float startX = rect.x + padL;
    float perGap = gap;
    switch (justify) {
    case Justify::Start: break;
    case Justify::Center: startX += freeW * 0.5f; break;
    case Justify::End:    startX += freeW; break;
    case Justify::SpaceBetween:
      if (n > 1) { perGap = (innerW - totalFixedW) / float(n - 1); perGap = std::max(0.f, perGap); }
      break;
    case Justify::SpaceEvenly:
      if (n > 0) { perGap = (innerW - totalFixedW) / float(n + 1); startX += perGap; }
      break;
    }

    // --- Place children (ignore children's anchors inside flex) ---
    float curX = startX;
    for (auto& c : children)
    {
      const float cw = preferredWidth(c.get());
      const float ch = (c->local.h > 0.f) ? c->local.h : std::min(innerH, 32.f);

      float cy = rect.y + padT;  // top
      if (align == Align::Center)      cy = rect.y + padT + (innerH - ch) * 0.5f;
      else if (align == Align::Bottom) cy = rect.y + rect.h - padB - ch;

      c->layoutAbsolute(ctx, Rect{ curX, cy, cw, ch });
      curX += cw + perGap;
    }

    if (id == "TBLeft" || id == "TBCenter" || id == "TBRight") {
      printf("[TopBar] %s rect=(%.1f,%.1f,%.1f,%.1f) clip=(%.1f,%.1f,%.1f,%.1f)\n",
        id.c_str(), rect.x, rect.y, rect.w, rect.h, clip.x, clip.y, clip.w, clip.h);
    }
  }

  //---------------------------------------------------------------------------------------------
  void FlexRow::layoutChildren(UiContext& ctx)
  {
    // We are inside 'rect' (already absolute & clipped). Use pad/gap/justify/align here.

    const float availW = std::max(0.f, rect.w - (padL + padR));
    const float availH = std::max(0.f, rect.h - (padT + padB));

    // --- intrinsic sizing (prevents 0×0 labels/images/buttons) ---
    auto measureChild = [&](Widget* w, float& cw, float& ch) {
      cw = w->local.w; ch = w->local.h;

      if (auto* lbl = dynamic_cast<Label*>(w)) {
        const float px = (lbl->fontSize > 0.f) ? lbl->fontSize : ctx.theme.fontSizes[2];
        if (!ctx.font) { if (ch <= 0) ch = std::max(px * 1.4f, 1.f); if (cw <= 0) cw = 1.f; return; }
        if (ch <= 0) ch = std::max(ctx.font->lineHeight * px, 1.f);
        if (cw <= 0) {
          auto s32 = utf8ToUtf32(lbl->text);
          cw = std::max(measureRun(*ctx.font, s32, px).widthPx, 1.f);
        }
        return;
      }
      if (auto* img = dynamic_cast<Image*>(w)) {
        if (cw <= 0 || ch <= 0) {
          if (const Sprite* sp = ctx.findSprite(img->spriteName)) {
            if (cw <= 0) cw = sp->uv.w; if (ch <= 0) ch = sp->uv.h;
          }
          else { if (cw <= 0) cw = availH; if (ch <= 0) ch = availH; }
        }
        return;
      }
      if (auto* ib = dynamic_cast<IconButton*>(w)) {
        if (cw <= 0 || ch <= 0) {
          if (const Sprite* sp = ctx.findSprite(ib->spriteName)) {
            if (ch <= 0) ch = std::min(availH, sp->uv.h);
            if (cw <= 0) cw = std::min(availH, sp->uv.w);
          }
          else { if (ch <= 0) ch = availH; if (cw <= 0) cw = availH; }
        }
        return;
      }
      if (cw <= 0) cw = availH;
      if (ch <= 0) ch = availH;
    };

    struct Item { Widget* w; float cw; float ch; };
    std::vector<Item> items; items.reserve(children.size());
    float totalW = 0.f;
    for (auto& c : children) {
      if (!c->visible) continue;
      float cw = 0.f, ch = 0.f; measureChild(c.get(), cw, ch);
      ch = std::min(ch, availH);
      totalW += cw;
      items.push_back({ c.get(), cw, ch });
    }
    const int N = (int)items.size();
    const float baseGaps = gap * std::max(0, N - 1);
    const float freeW = std::max(0.f, availW - totalW - baseGaps);

    float x = rect.x + padL;
    float extraGap = 0.f;
    switch (justify) {
    case Justify::Start: break;
    case Justify::Center: x += freeW * 0.5f; break;
    case Justify::End:    x += freeW; break;
    case Justify::SpaceBetween: if (N > 1) extraGap = std::max(0.f, freeW / float(N - 1)); break;
    case Justify::SpaceEvenly:  /* optional */ break;
    }

    for (int i = 0; i < N; ++i) {
      auto* w = items[i].w;
      const float cw = items[i].cw, ch = items[i].ch;
      float y = rect.y + padT;
      if (align == Align::Center) y = rect.y + (rect.h - ch) * 0.5f;
      else if (align == Align::Bottom) y = rect.y + rect.h - padB - ch;

      // Place child ABSOLUTELY, then the child's layoutChildren() will run (via Widget::layoutAbsolute)
      w->layoutAbsolute(ctx, Rect{ x, y, cw, ch });

      x += cw + gap + extraGap;
    }
  }

  //------------------------------------------------------------------
  void ScrollView::layout(UiContext& ctx, const Rect& parentRect)
  {
    // Base rect + clip (honor anchors via parent)
    Widget::layout(ctx, parentRect);

    // Inner viewport (padding removed)
    const Rect inner = {
      rect.x + padL,
      rect.y + padT,
      std::max(0.f, rect.w - padL - padR),
      std::max(0.f, rect.h - padT - padB)
    };
    innerW = inner.w;
    innerH = inner.h;

    // 1) Lay out children in *content space* with origin at inner.x/inner.y (no scroll baked yet)
    for (auto& c : children) {
      c->layout(ctx, inner);
    }

    // 2) Measure total content height in that space
    float maxBottom = inner.y;
    for (auto& c : children) {
      maxBottom = std::max(maxBottom, c->rect.y + c->rect.h);
    }
    contentH = std::max(0.f, maxBottom - inner.y);

    // 3) Clamp scroll
    const float maxScroll = std::max(0.f, contentH - innerH);
    scrollY = std::clamp(scrollY, 0.f, maxScroll);

    // 4) Apply a *uniform translation* of -scrollY to the entire subtree,
    //    and clip everyone to the inner viewport.
    auto applyScrollToSubtree = [&](Widget* w, auto&& self) -> void {
      // Shift this node once (its rect is absolute already)
      w->rect.y -= scrollY;
      // Clip to the inner viewport
      w->clip = intersectR(inner, w->rect);
      // Recurse
      for (auto& ch : w->children) self(ch.get(), self);
    };
    for (auto& c : children) applyScrollToSubtree(c.get(), applyScrollToSubtree);

    // Our own clip = outer rect is fine; the content is clipped to `inner`
    clip = rect;
  }

  //-----------------------------------------------------------
  bool ScrollView::onEvent(UiContext& ctx, const UIEvent& e)
  {
    if (e.kind != UIEvent::Kind::Pointer) return false;
    const auto pe = std::get<PointerEvent>(e.data);

    // --- Wheel stays as the working path (no hitTest gate) ---
    if (pe.type == PointerEvent::Type::Scroll) {
      const float maxScroll = std::max(0.f, contentH - innerH);
      if (maxScroll <= 0.f) return false;

      const float before = scrollY;
      scrollY = std::clamp(scrollY - pe.wheel * wheelStep, 0.f, maxScroll);

      velY += pe.wheel * wheelStep * 30.f;
      velY = std::clamp(velY, -maxVel, maxVel);
      kinetic = true;

      if (scrollY != before) ctx.invalidate();
      return true;
    }

    // Pointer hover/drag only if inside OR dragging (capture case)
    const bool inside = hitTest(pe.pos);
    if (!inside && !dragging) return false;

    const Rect tr = trackRect();
    const Rect th = thumbRect();
    const bool overThumb = (th.w > 0 && contains(th, pe.pos));

    switch (pe.type)
    {
    case PointerEvent::Type::Enter:
      if (!dragging) {
        const bool nh = overThumb;
        if (nh != hoverThumb) { hoverThumb = nh; ctx.invalidate(); }
      }
      return false;

    case PointerEvent::Type::Leave:
      // If we were dragging and the mouse leaves (e.g. window edge or fast move),
      // cancel drag and release capture so other widgets can get hover again.
      if (dragging) {
        dragging = false;
        if (ctx.root && ctx.root->pointerCapture == this)
          ctx.root->pointerCapture = nullptr;
      }
      // Clear hover when leaving the scrollview
      if (hoverThumb) { hoverThumb = false; ctx.invalidate(); }
      return false;

    case PointerEvent::Type::Move:
      if (dragging) {
        const float maxScroll = std::max(0.f, contentH - innerH);
        if (maxScroll <= 0.f) { ctx.invalidate(); return true; }

        const float H = std::max(minThumbPx, tr.h * (innerH / contentH));
        const float travel = std::max(1.f, tr.h - H);
        float thumbY = pe.pos.y - grabDy;
        thumbY = std::clamp(thumbY, tr.y, tr.y + travel);

        const float t = (thumbY - tr.y) / travel;   // 0..1
        const float ns = t * maxScroll;

        // Update & ALWAYS invalidate during drag so a new frame is built
        scrollY = std::clamp(ns, 0.f, maxScroll);
        ctx.invalidate();
        return true;
      }
      else {
        const bool nh = overThumb;
        if (nh != hoverThumb) { hoverThumb = nh; ctx.invalidate(); }
        return false;
      }

    case PointerEvent::Type::Down:
      if (pe.button == PointerButton::Left && overThumb) {
        dragging = true;
        hoverThumb = true;
        grabDy = pe.pos.y - th.y;
        if (ctx.root)
          ctx.root->pointerCapture = this;
        ctx.invalidate();
        return true;
      }
      return false;

    case PointerEvent::Type::Up:
      if (/*pe.button == PointerButton::Left &&*/ dragging) //@todo mouse release does not transfer mouse button yet !!!
      {
        dragging = false;
        if (ctx.root && ctx.root->pointerCapture == this)
          ctx.root->pointerCapture = nullptr;
        // Recompute hover on release (cursor might still be over thumb)
        const bool nh = (th.w > 0 && contains(th, pe.pos));
        if (nh != hoverThumb) hoverThumb = nh;
        ctx.invalidate();
        return true;
      }
      return false;

    default:
      return false;
    }
  }

  //--------------------------------------------------------------------------
  void ScrollView::draw(UiContext& ctx) const
  {
    Widget::draw(ctx); // draws children if you left it to base; or keep your own flow

    if (!showScrollbar) return;

    // Snap helper like other widgets
    const float s = uiScale(ctx);
    auto snap = [s](float v) { return (s > 0.f) ? std::round(v * s) / s : v; };
    auto snapR = [snap](Rect r) {
      float x0 = snap(r.x), y0 = snap(r.y);
      float x1 = snap(r.x + r.w), y1 = snap(r.y + r.h);
      return Rect{ x0, y0, x1 - x0, y1 - y0 };
    };

    // Track and thumb rects
    Rect tr = snapR(trackRect());
    Rect th = snapR(thumbRect());

    // ---------- Track ----------
    if (tr.w > 0 && tr.h > 0) {
      bool drawn = false;

      // Prefer nine-slice if provided
      if (!trackNineName.empty()) {
        if (auto n = ctx.findNineWithTex(trackNineName); n.nine) {
          ctx.prim->nineSlice(*ctx.dl, n.tex, tr, *n.nine, trackTint, rect /*clip*/);
          drawn = true;
        }
      }

      // Else try plain sprite stretched to rect (not as pretty as nine, but OK)
      if (!drawn && !trackSpriteName.empty()) {
        if (auto spr = ctx.findSprite(trackSpriteName)) {
          ctx.prim->texturedQuad(*ctx.dl, spr->tex, spr->texW, spr->texH, tr, spr->uv, trackTint, rect /*clip*/);
          drawn = true;
        }
      }

      // Fallback to solid color bar
      if (!drawn) {
        ctx.prim->solidRect(*ctx.dl, tr, barColor, rect /*clip*/);
      }
    }

    // ---------- Thumb ----------
    if (th.w > 0 && th.h > 0) {
      // pick tint by state
      uint32_t tint = thumbTintNormal;
      if (dragging) tint = thumbTintActive;
      else if (hoverThumb) tint = thumbTintHover;

      bool drawn = false;

      if (!thumbNineName.empty()) {
        if (auto n = ctx.findNineWithTex(thumbNineName); n.nine) {
          ctx.prim->nineSlice(*ctx.dl, n.tex, th, *n.nine, tint, rect /*clip*/);
          drawn = true;
        }
      }
      if (!drawn && !thumbSpriteName.empty()) {
        if (auto spr = ctx.findSprite(thumbSpriteName)) {
          ctx.prim->texturedQuad(*ctx.dl, spr->tex, spr->texW, spr->texH, th, spr->uv, tint, rect /*clip*/);
          drawn = true;
        }
      }
      if (!drawn) {
        // Fallback to flat color states
        uint32_t flat = thumbColor;
        if (dragging) flat = thumbActive;
        else if (hoverThumb) flat = thumbHover;
        ctx.prim->solidRect(*ctx.dl, th, flat, rect /*clip*/);
      }
    }
  }

  //------------------------------------------------
  void ScrollView::update(UiContext& ctx, float dt)
  {
  }

  //---------------------------------------------------------------------------------------
  void drawOutlineDevicePx(UI_lib::PrimitiveRenderer& prim, UI_lib::DrawList& dl, const UI_lib::Rect& rect, uint32_t color, const UI_lib::Rect& clip, const UI_lib::UiContext& ctx)
  {
    const float s = uiScale(ctx);
    if (s <= 0.f) return;

    // Convert 1 device pixel into virtual pixels, round up to guarantee visibility
    const float t = std::ceil(1.0f / s);  // thickness in *virtual* px

    const float x = std::round(rect.x);
    const float y = std::round(rect.y);
    const float w = std::round(rect.w);
    const float h = std::round(rect.h);

    // inset the far edges by t to stay strictly inside the rect (avoids scissor edge-kill)
    prim.solidRect(dl, { x,       y,       w,  t }, color, clip);        // top
    prim.solidRect(dl, { x,       y + h - t,   w,  t }, color, clip);        // bottom
    prim.solidRect(dl, { x,       y,       t,  h }, color, clip);        // left
    prim.solidRect(dl, { x + w - t,   y,       t,  h }, color, clip);        // right
  }

  //------------------------------------------------------------------------------
  bool DropSlot::onDrag(UiContext& ctx, const DragEvent& d, const DragPayload& p)
  {
    switch (d.type)
    {
    case DragEvent::Type::Enter: {
      hover = canAccept(p);
      ctx.invalidate();
      return hover;              // tell router “I’m a valid target” if true
    }
    case DragEvent::Type::Over: {
      // still OK to drop? (e.g., slot capacity could change)
      return canAccept(p);
    }
    case DragEvent::Type::Leave: {
      hover = false;
      ctx.invalidate();
      return false;
    }
    case DragEvent::Type::Drop: {
      hover = false;
      ctx.invalidate();

      if (!canAccept(p)) return false;

      // Extract minimal intent
      const std::string& entityKey = p.s0;               // set by Image::onEvent (id "ent:<key>" -> s0)
      const std::string  target = /*!slotKey.empty() ? slotKey :*/ id;  // prefer explicit slotKey

      // ---- anchor rect in screen space ----
    // Prefer visualRect() if you have it (includes tween offset/scale).
      Rect a = this->visualRect();          // if not available, use: Rect a = this->rect;

      // Optional: also store pointer position at drop moment
      const float px = ctx.cursorPx.x;
      const float py = ctx.cursorPx.y;

      // Post intent; controller will attach active scope/city
      ctx.hub.post("ui.assign.request",
        Obj({ {"entityKey", entityKey},
             {"targetSlot", target},

          // fields for popup anchoring
        {"anchorWidgetId", id},
        {"anchorRect", Obj({
          {"x", (double)a.x},
          {"y", (double)a.y},
          {"w", (double)a.w},
          {"h", (double)a.h},
        })},

        // Optional (helps if you later want “arrow points to cursor” behavior)
        {"dropX", (double)px},
        {"dropY", (double)py}, 
          }));

    // compact, stringly-typed fallback (no maps in Payload):
    /*  std::string msg;
      msg.reserve(entityKey.size() + target.size() + 32);
      msg += "entityKey="; msg += entityKey;
      msg += "&targetSlot="; msg += target;
      ctx.hub.post("ui.assign.request", msg);*/

      // optional: kick a success tween here (e.g., scale bounce)
      return true;
    }
    default: break;
    }
    return false;
  }

  //----------------------------------------------------------------------------------
  void DropSlot::draw(UiContext& ctx) const
  {
    // draw highlight when hover during drag
    if (hover) {
      const uint32_t HL = 0xFFD54AFF; // amber-ish
      ctx.prim->solidRect(*ctx.dl, { rect.x, rect.y, rect.w, 2 }, HL, clip);
      ctx.prim->solidRect(*ctx.dl, { rect.x, rect.y + rect.h - 2, rect.w, 2 }, HL, clip);
      ctx.prim->solidRect(*ctx.dl, { rect.x, rect.y, 2, rect.h }, HL, clip);
      ctx.prim->solidRect(*ctx.dl, { rect.x + rect.w - 2, rect.y, 2, rect.h }, HL, clip);
    }
    Widget::draw(ctx);
  }

  //-------------------------------------------------------------
  void LineEdit::draw(UiContext& ctx) const
  {
    Rect r = visualRect();
    uint32_t bg = active ? 0xFFFFFFFF : 0xEEEEEEFF;
    uint32_t bd = 0x3A2A12FF;
    ctx.prim->solidRect(*ctx.dl, r, bg, clip);
    ctx.prim->solidRect(*ctx.dl, { r.x, r.y, r.w, 1 }, bd, clip);
    ctx.prim->solidRect(*ctx.dl, { r.x, r.y + r.h - 1, r.w, 1 }, bd, clip);
    ctx.prim->solidRect(*ctx.dl, { r.x, r.y, 1, r.h }, bd, clip);
    ctx.prim->solidRect(*ctx.dl, { r.x + r.w - 1, r.y, 1, r.h }, bd, clip);
    if (ctx.font) {
      const float px = 18.f;
      const float asc = ctx.font->ascender * px;
      ctx.prim->textMSDF(*ctx.dl, *ctx.font, utf8ToUtf32(text), { r.x + 6, r.y + asc + (r.h - px) * 0.5f - asc }, px, 0x1A1A1AFF, clip);
    }
  }

  //------------------------------------------------------------------------------
  bool LineEdit::onEvent(UiContext& ctx, const UIEvent& e)
  {
    if (e.kind == UIEvent::Kind::Pointer)
    {
      const auto pe = std::get<PointerEvent>(e.data);
      if (pe.type == PointerEvent::Type::Down && hitTest(pe.pos)) {
        if (ctx.root) ctx.root->setFocus(this);
        if (ctx.root) ctx.root->pointerCapture = this;
        return true;
      }
      if (pe.type == PointerEvent::Type::Up && ctx.root && ctx.root->pointerCapture == this) {
        // keep focus; don't drop it on mouse up
        if (ctx.root) ctx.root->setFocus(this);
        return true;
      }
    }

    if (e.kind == UIEvent::Kind::Key && ctx.root && ctx.root->focusWidget == this) {
      auto ke = std::get<KeyEvent>(e.data);
      if (ke.type == KeyEvent::Type::KeyUp) return true; // don't route to keep focus on this widget
      if (ke.type != KeyEvent::Type::KeyDown) return false;
      if (ke.key == Key::Enter) {
        if (onCommit) onCommit(text); active = false; ctx.invalidate(); return true;
      }
      if (ke.key == Key::Backspace)
      { if (!text.empty())
        text.pop_back(); ctx.invalidate();
        return true;
      }
      // digits and signs only (basic)
      if ((ke.key >= Key::Numpad0 && ke.key <= Key::Numpad9) || (ke.key >= Key::Numpad0 && ke.key <= Key::Numpad9)) {
        char ch = char('0' + int(ke.key) - int(Key::Numpad0));
        text.push_back(ch); ctx.invalidate(); return true;
      }
      if (ke.key == Key::Minus || ke.key == Key::Plus)
      { text.push_back(ke.key == Key::Minus ? '-' : '+'); ctx.invalidate(); return true; }
      return false;
    }
    return false;
  }

  //----------------------------------------------------------------
  void CheckBox::draw(UiContext& ctx) const
  {
    // simple square + check sprite from atlas if you like; placeholder:
    uint32_t c = checked ? 0x000000FF : 0x999999FF;
    ctx.prim->solidRect(*ctx.dl, visualRect(), c, clip);
  }

  //---------------------------------------------------------------------------------
  bool CheckBox::onEvent(UiContext& ctx, const UIEvent& e)
  {
    if (e.kind != UIEvent::Kind::Pointer) return false;
    const auto pe = std::get<PointerEvent>(e.data);
    switch (pe.type) {
    case PointerEvent::Type::Down:
      if (hitTest(pe.pos)) {
        if (ctx.root) ctx.root->setFocus(this);
        pressed = true;
        if (ctx.root) ctx.root->pointerCapture = this;
        return true;
      }
      return false;
    case PointerEvent::Type::Up:
      if (pressed) {
        pressed = false;
        if (ctx.root && ctx.root->pointerCapture == this)
          ctx.root->pointerCapture = nullptr;
        if (hitTest(pe.pos))
        {
          checked = !checked;
          if (onChanged) onChanged(checked);
        }
        return true;
      }
      return false;
    default: return false;
    }
  }

  //-------------------------------------------------
  void ExportImportPopup::onOpen(const std::string& resKey, Widget* _parent)
  {
    resourceKey = resKey; parent = _parent; buildChildren();
  }

  //--------------------------------------------------
  void ExportImportPopup::onClose(UiContext& ctx, const char* reason /*"close"|"ok"|"cancel_outside"*/)
  {
    // Read UI state safely
    const bool exp = cbExport ? cbExport->checked : false;
    const bool imp = cbImport ? cbImport->checked : false;
    const bool stop = cbStop ? cbStop->checked : false;

    int64_t keepNum = 0;
    bool    hasKeep = false;
    if (edKeep) {
      const std::string txt = edKeep->text;
      if (!txt.empty()) {
        try { keepNum = static_cast<int64_t>(std::stoll(txt)); hasKeep = true; }
        catch (...) { /* ignore parse errors; hasKeep stays false */ }
      }
    }

    // Build payload
    Value p = Value::object({
      {"type",         "export_import"},      // schema tag
      {"resourceKey",  resourceKey},          // pass the target
      {"export",       exp},
      {"import",       imp},
      {"stop_consume", stop},
      {"hasKeep",      hasKeep},
      {"keep",         hasKeep ? Value(keepNum) : Value()}, // null if absent
      {"reason",       reason},               // how we committed
      });

    ctx.hub.post("ui.export_import.apply", p);
  }

  //----------------------------------------------------------------------------
  void ExportImportPopup::buildChildren()
  {
    children.clear();

    auto mkLabel = [&](const char* id, const char* txt, float y, float w = 160.f, float x = 8.f, float fs = 18.f) {
      auto L = std::make_unique<Label>();
      L->id = id; L->text = txt; L->fontSize = fs;
      L->local = Rect{ x, y, w, 22 };
      L->color = 0x3A2A12FF;
      L->parent = this;
      children.emplace_back(std::move(L));
      };

    auto mkCB = [&](const char* id, float x, float y)->CheckBox* {
      auto C = std::make_unique<CheckBox>();
      C->id = id; C->local = Rect{ x, y, 18, 18 }; C->parent = this;
      auto* r = C.get(); children.emplace_back(std::move(C)); return r;
      };

    auto mkEdit = [&](const char* id, float x, float y, float w)->LineEdit* {
      auto E = std::make_unique<LineEdit>();
      E->id = id; E->local = Rect{ x, y, w, 24 }; E->parent = this;
      auto* r = E.get(); children.emplace_back(std::move(E)); return r;
      };

    // --- Title ----------------------------------------------------
    // Center-ish title within the popup (width 190, 8px side padding)
    mkLabel("lblTitle", "Resource Trade", /*y*/4.f, /*w*/174.f, /*x*/8.f, /*fs*/20.f);

    // --- Rows -----------------------------------------------------
    // Export
    mkLabel("lblExport", "Export", 28.f);
    cbExport = mkCB("cbExport", 150.f, 28.f);

    // Import
    mkLabel("lblImport", "Import", 52.f);
    cbImport = mkCB("cbImport", 150.f, 52.f);

    // Stop Consumption (new)
    mkLabel("lblStop", "Stop Consumption", 76.f);
    cbStop = mkCB("cbStop", 150.f, 76.f); // place checkbox to the right of label

    // Keep (with line edit)
    mkLabel("lblKeep", "Keep", 100.f);
    edKeep = mkEdit("edKeep", 110.f, 98.f, 64.f);

    // Default size; adjusted taller to fit the extra row
    local = rect = { 0, 0, 190, 130 };
  }

  //----------------------------------------------------------------------------------
  void ModalLayer::open(std::unique_ptr<PopupPanel>&& _popup, const Rect& anchorRect, const std::string& resKey)
  {
    active = true; anchor = anchorRect;
    popup = std::move(_popup);
    popup->onOpen(resKey, this);

    // position to the right of anchor, clamped into viewport later in layout
    float x = anchor.x + anchor.w + 6, y = anchor.y;
    popup->local = popup->rect = { x, y, popup->rect.w, popup->rect.h };
  }

  //----------------------------------------------------------------------------------
  void ModalLayer::close(UiContext& ctx, bool sendPayload)
  {
    if (sendPayload && popup)
    {
      // build payload → { key, export, import, keep }
      popup->onClose(ctx, "ok");
    }
    popup.reset(); active = false; ctx.invalidate();
  }

  //---------------------------------------------------------------------
  void ModalLayer::layout(UiContext& ctx, const Rect& parentRect)
  {
    rect = clip = parentRect;   // full-screen invisible catcher
    if (popup) {
      // clamp popup inside viewport
      Rect P = popup->rect;
      if (P.x + P.w > rect.x + rect.w) P.x = rect.x + rect.w - P.w - 6;
      if (P.y + P.h > rect.y + rect.h) P.y = rect.y + rect.h - P.h - 6;
      popup->rect = popup->local = P;
      popup->clip = intersectR(rect, popup->rect);
      popup->layoutChildren(ctx);
    }
  }

  //--------------------------------------------------------------------
  void ModalLayer::draw(UiContext& ctx) const
  {
    if (!active || !popup) return;
    // no dim backdrop; keep crisp like a rich tooltip
    popup->draw(ctx);
  }

  //--------------------------------------------------------------------
  bool ModalLayer::onEvent(UiContext& ctx, const UIEvent& e)
  {
    if (!visible) return false;
    if (e.kind == UIEvent::Kind::Pointer)
    {
      const auto pe = std::get<PointerEvent>(e.data);
      // We expect rect to cover the whole viewport when visible.
      // `box` is the popover/panel rect relative to this layer.
      const bool insideBox = popup ? contains(popup->rect, pe.pos) : false;

      switch (pe.type)
      {
      case PointerEvent::Type::Move:
        ctx.invalidate();
        if (insideBox && popup) { UIEvent ev = e; if (popup->onEvent(ctx, ev)) return true; }
        return true;

      case PointerEvent::Type::Down:
        if (!insideBox) { close(ctx, /*submitOnClose=*/true); ctx.invalidate(); return true; }
        if (popup)
        { UIEvent ev = e; if (popup->onEvent(ctx, ev)) return true; }
        return true;

      case PointerEvent::Type::Up:
      case PointerEvent::Type::Scroll:
      case PointerEvent::Type::Enter:
      case PointerEvent::Type::Leave:
        if (insideBox && popup)
        { UIEvent ev = e; if (popup->onEvent(ctx, ev)) return true; }
        return true;

      default: return true;
      }
    }
    if (e.kind == UIEvent::Kind::Key) {
      const auto ke = std::get<KeyEvent>(e.data);
      if (ke.type == KeyEvent::Type::KeyDown && ke.key == Key::Escape) { close(ctx, true); return true; }
      return popup->onEvent(ctx, e);
    }
    return false;
  }

  //------------------------------------------------------------
  static bool isDescendant(const UI_lib::Widget* root, const UI_lib::Widget* w) {
    for (auto* p = w ? w->parent : nullptr; p; p = p->parent) {
      if (p == root) return true;
    }
    return false;
  }

  //---------------------------------------------------------
  bool PopupPanel::onEvent(UiContext& ctx, const UIEvent& e)
  {
    if (!visible) return false;

    if (e.kind == UIEvent::Kind::Pointer) {
      const auto pe = std::get<PointerEvent>(e.data);

      // Use *visual* rect gate
      bool over = contains(this->visualRect(), pe.pos);
      // If over == false, we still allow captured delivery via UiSystem::route.
      // Here, we only do local leaf routing when `over` is true.
      if (!over) return false;

      // Leaf targeting
      Widget* cur = this;
      for (;;) {
        Widget* next = nullptr;
        for (auto it = cur->children.rbegin(); it != cur->children.rend(); ++it) {
          Widget* c = it->get();
          if (!c || !c->visible) continue;
          if (!c->hitTest(pe.pos)) continue;
          next = c; break;
        }
        if (!next) break;
        cur = next;
        if (cur->children.empty()) break;
      }

      if (cur != this) {
        // Let leaf handle pointer; it can call setFocus in its Down handler
        return cur->onEvent(ctx, e);
      }
      return Panel::onEvent(ctx, e);
    }

    // Non-pointer: prefer focused child if it belongs to this popup
    if (ctx.root && ctx.root->focusWidget && UiRoot::isDescendant(this, ctx.root->focusWidget)) {
      UIEvent ev = e;
      if (ctx.root->focusWidget->onEvent(ctx, ev)) return true;
    }

    // Then top-most children
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
      UIEvent ev = e;
      if ((*it)->onEvent(ctx, ev)) return true;
    }

    return Panel::onEvent(ctx, e);
  }

  }
