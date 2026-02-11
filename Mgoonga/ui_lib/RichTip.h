#pragma once

#include "DrawList.h"
#include "Theme.h"

namespace UI_lib
{
  //-------------------------------------------------------------
  struct RichTipRow
  {
    std::string icon;      // sprite name, optional
    std::string text;      // utf8
    uint32_t    color = 0x3A2A12FF;
    float       fontSize = 20.f;
  };

  //---------------------------------------------------------------
  struct RichTipContent
  {
    std::string           title;     // optional
    uint32_t              titleColor = 0x3A2A12FF;
    float                 titleSize = 22.f;
    std::vector<RichTipRow> rows;
    bool empty() const { return title.empty() && rows.empty(); }
  };

  //------------------------------------------------------
  struct RichTipTheme
  {
    std::string nineName = "parchment_bar"; // background 9-slice name
    float padL = 12, padR = 12, padT = 12, padB = 12;
    float gapY = 8.f;        // gap between title and first row
    float rowGap = 6.f;      // gap between rows
    float iconPx = 24.f;     // default icon size
    float textGap = 8.f;     // gap between icon and text
    float maxWidth = 520.f;
    // behavior
    bool  enableFade = true;
    float fadeInMs = 120.f;
    float fadeOutMs = 120.f;
    float delayMs = 350.f;     // hover delay before show
  };

  //-------------------------------------------------------
  struct RichTipState
  {
    bool   visible = false;
    bool   fadingOut = false;
    float  alpha = 0.f;
    float  tHoverMs = 0.f;
    bool   armed = false;

    Rect   box{};
    Rect   anchorRect{};
    Vec2   armPos{};
    RichTipContent content;
  };
}