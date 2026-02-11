#pragma once
#include "ui_lib.h"

#include <any>

#include "DrawList.h"
#include "FontMSDF.h"
#include "Theme.h"
#include "PrimitiveRenderer.h"
#include "AnimationSystem.h"
#include "SignalHub.h"
#include "Tooltip.h"
#include "Tween.h"
#include "RichTip.h"
#include "Drag.h"

namespace UI_lib
{
  struct UiRoot;
  struct Widget;

  enum class Layer : uint8_t { Normal = 0, Overlay = 1 };

  struct CursorSkin
  {
    std::string spriteName = "";  // e.g. "cursor_arrow"
    Vec2        hotspot = { 0,0 };         // px offset within the sprite
    uint32_t    tint = 0xFFFFFFFF;   // optional
    float       scale = 1.0f;  // user multiplier (1.0 = 100%)
    bool        autoScaleDPI = true; // multiply by uiScale(ctx) if true
  };

  //----------------------------------------------------------------------------
  struct DLL_UI_LIB UiContext
  {
    // in UiContext
    int virtualW = 1920;
    int virtualH = 1080;

    int fbW = 1920;    // actual framebuffer size
    int fbH = 1080;

    // In UiContext (additions)
    std::unordered_map<std::string, Atlas> atlases;      // "topbar", "panels", etc.

    // Optional global name → sprite/nine indices (so YAML can omit atlas names)
    std::unordered_map<std::string, Sprite>    spriteIndex;
    std::unordered_map<std::string, NineSlice> nineIndex;
    // (Optional) owners, used to refresh the indices after texture assignment
    std::unordered_map<std::string, std::string> spriteOwner; // spriteName -> atlasName
    std::unordered_map<std::string, std::string> nineOwner;   // nineName   -> atlasName

    Theme theme;
    SignalHub hub;
    TooltipConfig tipCfg;
    TooltipState  tip;

    PrimitiveRenderer* prim = nullptr;
    DrawList* dl = nullptr;
    Animator* animator = nullptr;

    float dpiScale = 1.f;

    FontMSDF* font = nullptr; // default UI font

    UiRoot*   root = nullptr; // set this once in UiSystem after constructing root
    Vec2      lastPointerPos{};          // remember the most recent virtual pointer
    bool      hoverRefreshRequested = false; // ask UiRoot to recompute hover after capture changes

    Vec2 cursorPx{};              // last-pointer in *virtual* px
    CursorSkin cursorSkin{};

    RichTipTheme richCfg;
    RichTipState rich;

    DragController drag;

    void capture(Widget* w);
    void releaseCapture(Widget* w);
    void requestHoverRefresh() { hoverRefreshRequested = true; }

    Rect viewport{ 0,0, (float)virtualW, (float)virtualH }; // virtual canvas

    struct FocusState {
      Widget* current = nullptr;
    } focus;

    void requestFocus(Widget* w);
    void clearFocus();

    void setCursorSprite(std::string name,
                         Vec2 hotspotPx = { 0,0 },
                         float scale = 1.0f,
                         bool autoScaleDPI = true,
                         uint32_t tint = 0xFFFFFFFF)
    {
      cursorSkin.spriteName = std::move(name);
      cursorSkin.hotspot = hotspotPx;
      cursorSkin.scale = scale;
      cursorSkin.autoScaleDPI = autoScaleDPI;
      cursorSkin.tint = tint;
    }

    // helpers
    inline void showRichNow(const Rect& anchor, const RichTipContent& c) {
      rich.anchorRect = anchor; rich.content = c; rich.visible = true; rich.fadingOut = false; rich.alpha = 0.f; rich.tHoverMs = 0.f;
    }
    inline void armRich(const Rect& anchor, const RichTipContent& c) {
      rich.anchorRect = anchor; rich.content = c; rich.armed = true; rich.tHoverMs = 0.f; // will show after delay
    }
    inline void disarmRich() {
      rich.armed = false;
    }

    bool updateRichTooltip(float dtMs);

    void beginDrag(Widget* src, const DragPayload& p, const DragVisual& v, const Vec2& start);
    void endDrag() { drag = DragController{}; }

    // Small wrapper so callers can get both the NineSlice and its texture
    struct NineLookup {
      const NineSlice* nine = nullptr;
      TextureID tex = 0;
    };

    inline const NineSlice* findNine(const std::string& name) const {
      return findNineWithTex(name).nine;
    }
    const Sprite* findSprite(const std::string& name) const {
      auto it = spriteIndex.find(name);
      return it == spriteIndex.end() ? nullptr : &it->second;
    }

    glm::vec2 screenToVirtual(Vec2 s) const;
    glm::vec4 virtualToScissor(const Rect& r) const;

    Rect clampToViewport(const Rect& r) const {
      Rect v = viewport;
      Rect out = r;
      if (out.x + out.w > v.x + v.w) out.x = std::max(v.x, v.x + v.w - out.w);
      if (out.y + out.h > v.y + v.h) out.y = std::max(v.y, v.y + v.h - out.h);
      if (out.x < v.x) out.x = v.x;
      if (out.y < v.y) out.y = v.y;
      return out;
    }

    // Return the nine-slice and the owning texture.
    // Policy: we search all atlases for a nine with this name.
    inline NineLookup findNineWithTex(const std::string& name) const {
      // Fast path: if you kept a global index, prefer that and also remember which atlas it came from.
      // If you DON'T track atlas names in the index, just scan atlases:
      for (const auto& [atlasName, A] : atlases) {
        auto it = A.nine.find(name);
        if (it != A.nine.end()) {
          return NineLookup{ &it->second, A.texture };
        }
      }
      return {};
    }

    bool invalidated = true;          // start true for first frame
    void invalidate() { invalidated = true; }
    bool consumeInvalidated() { bool v = invalidated; invalidated = false; return v; }

    bool debugLayout = false;

    inline void assignAtlasTexture(const std::string& atlasName, TextureID tex, int texW, int texH)
    {
      auto it = atlases.find(atlasName);
      if (it == atlases.end()) {
        fprintf(stderr, "[UI] assignAtlasTexture: atlas '%s' not found\n", atlasName.c_str());
        return;
      }
      Atlas& A = it->second;
      A.texture = tex;
      A.texW = texW;
      A.texH = texH;

      // Propagate to atlas-local sprites
      for (auto& [name, s] : A.sprites) {
        s.tex = tex;
        s.texW = texW;
        s.texH = texH;
        // refresh global index copy
        spriteIndex[name] = s;
      }

      // Keep nine-slice tex dims consistent (NineSlice carries texW/H)
      for (auto& [name, n] : A.nine) {
        n.texW = texW; n.texH = texH;
        nineIndex[name] = n;
      }

      printf("[UI] atlas '%s' assigned texture=%u (%dx%d)\n",
        atlasName.c_str(), (unsigned)tex, texW, texH);
    }
  };
}
