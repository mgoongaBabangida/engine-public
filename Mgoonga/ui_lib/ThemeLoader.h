#pragma once
#include "Theme.h"
#include <yaml-cpp/yaml.h>

namespace UI_lib
{

  inline uint32_t parseColorNode(const YAML::Node& n) {
    if (!n) return 0xFFFFFFFF;

    // If node already parsed as an integer by yaml-cpp (e.g., 0xAABBCCDD unquoted),
    // prefer that path; this avoids string parsing when not needed.
    try {
      if (n.IsScalar()) {
        // Try numeric first (covers unquoted decimal or hex)
        // yaml-cpp will accept 0x... as an int on many builds.
        return n.as<uint32_t>();
      }
    }
    catch (...) {
      // fall through to string parsing
    }

    // Fallback: parse as string for hex forms "0xAARRGGBB" or "#RRGGBB[AA]"
    std::string t = n.as<std::string>();
    auto trim = [](std::string s) {
      size_t a = s.find_first_not_of(" \t\r\n");
      size_t b = s.find_last_not_of(" \t\r\n");
      return (a == std::string::npos) ? std::string() : s.substr(a, b - a + 1);
    };
    t = trim(t);
    if (t.empty()) return 0xFFFFFFFF;

    if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X'))
      return static_cast<uint32_t>(std::stoul(t.substr(2), nullptr, 16));

    if (!t.empty() && t[0] == '#') {
      std::string hex = t.substr(1);
      uint32_t v = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
      if (hex.size() == 6) return (0xFFu << 24) | v; // add alpha
      return v;
    }

    // Decimal string fallback
    return static_cast<uint32_t>(std::stoul(t, nullptr, 10));
  }

  static Style parseStyleNode(const YAML::Node& n) {
    Style s;
    if (n["fontSize"]) s.fontSize = n["fontSize"].as<float>();
    if (n["color"])    s.color = n["color"].as<uint32_t>();
    if (n["tint"])     s.tint = n["tint"].as<uint32_t>();
    if (n["opacity"])  s.opacity = std::clamp(n["opacity"].as<float>(), 0.f, 1.f);
    return s;
  }

  static StyleSet parseStyleSetNode(const YAML::Node& n) {
    StyleSet ss;
    if (n["base"] || n["hover"] || n["pressed"]) {
      if (auto b = n["base"])    ss.base = parseStyleNode(b);
      if (auto h = n["hover"])   ss.hover = parseStyleNode(h);
      if (auto p = n["pressed"]) ss.pressed = parseStyleNode(p);
    }
    else {
      // simple form → treat as base
      ss.base = parseStyleNode(n);
    }
    return ss;
  }

  void loadThemeYaml(const std::string& path, Theme& t);

} // namespace
