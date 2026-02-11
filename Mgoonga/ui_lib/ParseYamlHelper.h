#pragma once
#include <yaml-cpp/yaml.h>
#include <sstream>
#include <string>
#include <algorithm>
#include <cctype>
#include "base.h"

namespace UI_lib {

  // Normalize a scalar string: replace commas/semicolons with spaces
  inline std::string _normList(std::string s) {
    for (char& c : s) if (c == ',' || c == ';') c = ' ';
    return s;
  }

  inline bool _tryParseFloats(const std::string& s, float* out, int n) {
    std::istringstream iss(_normList(s));
    for (int i = 0; i < n; i++) {
      if (!(iss >> out[i])) return false;
    }
    return true;
  }

  inline float _asFloat(const YAML::Node& n, float def = 0.f) {
    try { return n.as<float>(); }
    catch (...) { return def; }
  }

  // Accepts: scalar → uniform; sequence; map; or "x y" / "x,y"
  inline Vec2 parseVec2(const YAML::Node& n, Vec2 def = { 0.f, 0.f }) {
    if (!n) return def;

    // Sequence: [x, y]
    if (n.IsSequence()) {
      const size_t len = n.size();
      if (len == 0) return def;
      if (len == 1) { float v = _asFloat(n[0], def.x); return { v, v }; }
      return { _asFloat(n[0], def.x), _asFloat(n[1], def.y) };
    }

    // Map: { x: .., y: .. } OR { r: .., g: .. }
    if (n.IsMap()) {
      float x = def.x, y = def.y;
      if (n["x"]) x = _asFloat(n["x"], x);
      if (n["y"]) y = _asFloat(n["y"], y);
      // fallbacks for color-style keys
      if (n["r"]) x = _asFloat(n["r"], x);
      if (n["g"]) y = _asFloat(n["g"], y);
      return { x, y };
    }

    // Scalar: number → uniform, or "x y" / "x,y"
    if (n.IsScalar()) {
      const std::string s = n.as<std::string>();
      // try two floats
      float vals[2];
      if (_tryParseFloats(s, vals, 2)) return { vals[0], vals[1] };
      // fallback: single float uniform
      try { float v = std::stof(s); return { v, v }; }
      catch (...) { return def; }
    }

    return def;
  }

  inline Vec4 parseVec4(const YAML::Node& n, Vec4 def = { 0.f,0.f,0.f,0.f }) {
    if (!n) return def;

    // Sequence: [x, y, z, w]
    if (n.IsSequence()) {
      const size_t len = n.size();
      if (len == 0) return def;
      if (len == 1) { float v = _asFloat(n[0], def.x); return { v, v, v, v }; }
      if (len == 2) { float x = _asFloat(n[0], def.x), y = _asFloat(n[1], def.y); return { x,y,def.z,def.w }; }
      if (len == 3) { float x = _asFloat(n[0], def.x), y = _asFloat(n[1], def.y), z = _asFloat(n[2], def.z); return { x,y,z,def.w }; }
      return { _asFloat(n[0],def.x), _asFloat(n[1],def.y), _asFloat(n[2],def.z), _asFloat(n[3],def.w) };
    }

    // Map: { x:.., y:.., z:.., w:.. } OR color-style { r,g,b,a }
    if (n.IsMap()) {
      float x = def.x, y = def.y, z = def.z, w = def.w;
      if (n["x"]) x = _asFloat(n["x"], x);
      if (n["y"]) y = _asFloat(n["y"], y);
      if (n["z"]) z = _asFloat(n["z"], z);
      if (n["w"]) w = _asFloat(n["w"], w);
      // color aliases
      if (n["r"]) x = _asFloat(n["r"], x);
      if (n["g"]) y = _asFloat(n["g"], y);
      if (n["b"]) z = _asFloat(n["b"], z);
      if (n["a"]) w = _asFloat(n["a"], w);
      return { x,y,z,w };
    }

    // Scalar string: "x y z w" or "x,y,z,w" or single uniform
    if (n.IsScalar()) {
      const std::string s = n.as<std::string>();
      float vals[4];
      if (_tryParseFloats(s, vals, 4)) return { vals[0], vals[1], vals[2], vals[3] };
      // try 3, fill w from def
      {
        std::istringstream iss(_normList(s));
        float x, y, z;
        if (iss >> x >> y >> z) return { x,y,z,def.w };
      }
      // try 2, fill z,w from def
      {
        std::istringstream iss(_normList(s));
        float x, y;
        if (iss >> x >> y) return { x,y,def.z,def.w };
      }
      // single uniform
      try { float v = std::stof(s); return { v,v,v,v }; }
      catch (...) { return def; }
    }

    return def;
  }

} // namespace UI_lib
#pragma once
