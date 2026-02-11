#pragma once

#include <opengl_assets/GUI.h>
#include <yaml-cpp/yaml.h>

#include "Widget.h"

namespace UI_lib
{
  //---------------------------------------------------------
  struct DLL_UI_LIB LegacyGuiAdapter : public Widget
  {
    // Strong ref so lifetime is owned by the tree
    std::shared_ptr<GUI> legacy;

    // Optional name of an atlas sprite to feed into legacy->SetTexture()
    std::string spriteName;

    // When true, we forward events to legacy OnMouse* and mark handled
    bool forwardPointer = true;

    // For clipping integration
    Rect clipRect{};

    LegacyGuiAdapter(std::shared_ptr<GUI> g) : legacy(std::move(g)) {
      focusable = false;
    }

    void layout(UiContext& ctx, const Rect& parentRect) override;

    void draw(UiContext& ctx) const override
    {
      // We want the new system to own batching. For now, call a textured quad
      // that renders the legacy's texture rectangle. Later you can remove
      // LegacyGuiAdapter entirely as widgets port over.

      //@ todo need to luckup atlas!
      //auto tex = ctx.atlas.texture; // use UI atlas by default
      //if (auto spr = ctx.findSprite("_debug_text_bg")) {
      //  ctx.prim->texturedQuad(*ctx.dl, tex, ctx.atlas.texW, ctx.atlas.texH, rect, spr->uv, 0xFFFFFFFF, clip);
      //}
      // If you have an existing render for GUI, you can also bypass and call it here.
      Widget::draw(ctx);
    }

    bool onEvent(UiContext& ctx, const UIEvent& e) override
    {
      if (!legacy || !forwardPointer) return false;
      if (e.kind != UIEvent::Kind::Pointer) return false;
      const auto& p = std::get<PointerEvent>(e.data);
      const int px = (int)p.pos.x, py = (int)p.pos.y;
      switch (p.type) {
      case PointerEvent::Type::Down: return legacy->OnMousePress(px, py, p.button == PointerButton::Left, (KeyModifiers)p.mods);
      case PointerEvent::Type::Up: return legacy->OnMouseRelease((KeyModifiers)p.mods);
      case PointerEvent::Type::Move: return legacy->OnMouseMove(px, py, (KeyModifiers)p.mods);
      default: return false;
      }
    }
  };

  // ------------------------------
  // YAML factory for Legacy GUI
  // ------------------------------
  // Example YAML:
  // type: LegacyGUI
  // id: law_button
  // rect: { x: 56, y: 120, w: 160, h: 48 }
  // sprite: button_panel # optional: name from atlas.yaml
  // takeMouse: true # maps to legacy->SetTakeMouseEvents(true)
  // executeOnRelease: true # maps to legacy->SetExecuteOnRelease(true)
}