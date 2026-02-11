#pragma once
#include "Theme.h"
#include <string>

namespace UI_lib
{

  enum class UIState { Base, Hover, Pressed };

  inline Style resolveStyle(const Theme& t,
    std::string_view typeName,
    std::string_view id,
    UIState s,
    const Style& inlineStyle /* from widget YAML */)
  {
    Style out{};
    // byType
    if (auto it = t.byType.find(std::string(typeName)); it != t.byType.end()) {
      mergeInto(out, it->second.base);
      if (s == UIState::Hover)   mergeInto(out, it->second.hover);
      if (s == UIState::Pressed) mergeInto(out, it->second.pressed);
    }
    // byId
    if (auto it = t.byId.find(std::string(id)); it != t.byId.end()) {
      mergeInto(out, it->second.base);
      if (s == UIState::Hover)   mergeInto(out, it->second.hover);
      if (s == UIState::Pressed) mergeInto(out, it->second.pressed);
    }
    // inline
    mergeInto(out, inlineStyle);
    return out;
  }

} // namespace

