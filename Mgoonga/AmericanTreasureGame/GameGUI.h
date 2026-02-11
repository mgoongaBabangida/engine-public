#pragma once

#include <opengl_assets/GUI.h>

//------------------------------------------------
struct GameGUI
{
  //icons and buttons
  std::vector<std::shared_ptr<GUI>>  m_ship_icons;
  std::vector<std::shared_ptr<GUI>>  m_ship_icons_pirate;
  std::vector<std::shared_ptr<GUI>>  m_status_icons;
  std::vector<std::shared_ptr<GUI>>  m_status_icons_pirate;

  std::shared_ptr<GUI>               m_dice_gui;

  //Text
  std::shared_ptr<Text>              m_warning;
  std::shared_ptr<Text>              m_destination_text;
  std::vector<std::shared_ptr<Text>> m_base_labels;
};