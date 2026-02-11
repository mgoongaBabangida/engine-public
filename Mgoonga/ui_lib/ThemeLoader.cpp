#include "stdafx.h"
#include "ThemeLoader.h"

namespace UI_lib
{
  void loadThemeYaml(const std::string& path, Theme& t)
  {
    try {
      YAML::Node y = YAML::LoadFile(path);

      if (auto n = y["spacing"]) for (size_t i = 0; i < std::min<size_t>(6, n.size()); ++i) t.spacing[i] = n[i].as<float>();
      if (auto n = y["radii"])   for (size_t i = 0; i < std::min<size_t>(4, n.size()); ++i) t.radii[i] = n[i].as<float>();
      if (auto n = y["fontSizes"]) for (size_t i = 0; i < std::min<size_t>(5, n.size()); ++i) t.fontSizes[i] = n[i].as<float>();

      if (auto c = y["colors"]) {
        if (c["parchment_bg"]) t.color_parchment_bg = parseColorNode(c["parchment_bg"]);
        if (c["ink"])          t.color_ink = parseColorNode(c["ink"]);
        if (c["gold"])         t.color_gold = parseColorNode(c["gold"]);
      }

      if (auto bt = y["byType"]) {
        for (auto it : bt) {
          t.byType[it.first.as<std::string>()] = parseStyleSetNode(it.second);
        }
      }
      if (auto bi = y["byId"]) {
        for (auto it : bi) {
          t.byId[it.first.as<std::string>()] = parseStyleSetNode(it.second);
        }
      }

      if (auto tt = y["tooltip"])
      {
        if (tt["delayMs"])      t.tipCfg.delayMs = tt["delayMs"].as<float>();
        if (tt["maxWidth"])     t.tipCfg.maxWidth = tt["maxWidth"].as<float>();

        //if (tt["fadeInMs"])  t.tipCfg.fadeInMs = tt["fadeInMs"].as<float>();
        /*if (tt["fadeOutMs"]) t.tipCfg.fadeOutMs = tt["fadeOutMs"].as<float>();
        if (tt["mode"]) {
          std::string m = tt["mode"].as<std::string>();
          if (m == "follow") t.tipCfg.mode = TooltipConfig::Mode::Follow;
          else if (m == "lock") t.tipCfg.mode = TooltipConfig::Mode::LockOnShow;
          else t.tipCfg.mode = TooltipConfig::Mode::AnchorToWidget;
        }*/
        /*if (tt["prefer"]) {
          std::string p = tt["prefer"].as<std::string>();
          t.tipCfg.prefer = (p == "above") ? TooltipConfig::Prefer::Above : TooltipConfig::Prefer::Below;
        }*/

        if (auto p = tt["pad"])
        {
          if (p["l"]) t.tipCfg.padL = p["l"].as<float>();
          if (p["r"]) t.tipCfg.padR = p["r"].as<float>();
          if (p["t"]) t.tipCfg.padT = p["t"].as<float>();
          if (p["b"]) t.tipCfg.padB = p["b"].as<float>();
        }
        if (tt["cursorOffsetX"]) t.tipCfg.cursorOffsetX = tt["cursorOffsetX"].as<float>();
        if (tt["cursorOffsetY"]) t.tipCfg.cursorOffsetY = tt["cursorOffsetY"].as<float>();
        if (tt["nineName"])      t.tipCfg.nineName = tt["nineName"].as<std::string>();
        if (tt["textColor"])     t.tipCfg.textColor = parseColorNode(tt["textColor"]);
        if (tt["bgTint"])        t.tipCfg.bgTint = parseColorNode(tt["bgTint"]);
        if (tt["opacity"])       t.tipCfg.opacity = std::clamp(tt["opacity"].as<float>(), 0.f, 1.f);
      }
    }
    catch (const YAML::ParserException& e) {
      printf("YAML ParserException in '%s': %s\n", path.c_str(), e.what());
    }
    catch (const YAML::BadConversion& e) {
      printf("YAML BadConversion in '%s': %s\n", path.c_str(), e.what());
    }
  }
}