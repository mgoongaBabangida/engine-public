#pragma once

#include "ui_lib.h"
#include "Theme.h"
#include "FontMSDF.h"
#include <yaml-cpp/yaml.h>

namespace UI_lib {
  DLL_UI_LIB void loadTheme(const std::string& path, Theme& t);
  DLL_UI_LIB void loadAtlas(const std::string& path, Atlas& a);
  DLL_UI_LIB bool loadAtlasYaml(UiContext& ctx, const std::string& atlasName, const std::string& yamlPath);
  DLL_UI_LIB bool loadFontMSDF_YAML(const std::string& path, FontMSDF& out, TextureID(*loadTexture)(const std::string& imagePath));
}

  /*
  # theme.yaml
  spacing: [2,4,8,12,16,24]
  radii: [0,4,8,12]
  fontSizes: [12,14,16,20,24]
  colors:
  parchment_bg: 0xFFF5E6FF
  ink: 0x1A1A1AFF
  gold: 0xC99A00FF
  
  
  # atlas.yaml
  texture: ui_atlas.png
  sprites:
  _debug_text_bg: { uv: { x:0, y:0, w:64, h:16 }, texW: 2048, texH: 2048 }
  icon_law: { uv: { x:80, y:0, w:32, h:32 }, texW: 2048, texH: 2048 }
  
  
  nine:
  parchment_panel:
  texW: 2048
  texH: 2048
  uvFull: { x:256, y:0, w:256, h:256 }
  left: 24; right: 24; top: 24; bottom: 24
  button_panel:
  texW: 2048
  texH: 2048
  uvFull: { x:544, y:0, w:128, h:64 }
  left: 16; right: 16; top: 16; bottom: 16
  
  
  # main.yaml (layout)
  root: &panel
  type: Panel
  id: root
  rect: { x: 40, y: 40, w: 640, h: 240 }
  nineName: parchment_panel
  children:
  - type: Label
  id: title
  rect: { x: 56, y: 56, w: 400, h: 32 }
  text: "Laws & Decrees"
  - type: Button
  id: adopt
  rect: { x: 56, y: 120, w: 160, h: 48 }
  text: "Adopt"
  - type: Button
  id: refuse
  rect: { x: 240, y: 120, w: 160, h: 48 }
  text: "Refuse"
  */