#include "stdafx.h"
#include "LegacyGUIAdapter.h"
#include "Context.h"

void UI_lib::LegacyGuiAdapter::layout(UiContext& ctx, const Rect& parentRect)
{
  // Use our rect as absolute (Sprint 1). Anchors/flex come later.
  (void)parentRect;
  if (legacy) legacy->Move({ (int)rect.x, (int)rect.y });
  clip = rect; clipRect = clip;


  // If spriteName is set and atlas has it, push UVs to legacy once
  if (!spriteName.empty())
  {
    if (auto s = ctx.findSprite(spriteName)) {
      // Convert atlas UV (pixel rect) to legacy SetTexture parameters
      // NOTE: You'll need to adapt this to your Texture loading.
      // Here we assume the atlas texture is already bound to legacy.
      legacy->SetTexture(/*Texture*/ *legacy->GetTexture(),
        { (int)s->uv.x, (int)s->uv.y },
        { (int)(s->uv.x + s->uv.w), (int)(s->uv.y + s->uv.h) });
    }
    spriteName.clear(); // only once
  }
}