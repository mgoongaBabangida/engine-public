#pragma once
#include "stdafx.h"
#include "ui_lib.h"

#include <functional>
#include <unordered_map>
#include <variant>
#include <optional>
#include <cstdint>

#include <glm/glm/vec2.hpp>
#include <glm/glm/vec4.hpp>

namespace UI_lib
{
  using Vec2 = glm::vec2;
  using Vec4 = glm::vec4;

  // Keep a light Rect (top-left, size) since we use w/h a lot.
  struct DLL_UI_LIB Rect { float x = 0, y = 0, w = 0, h = 0; };
  inline bool contains(const Rect& r, const Vec2& p) {
    return p.x >= r.x && p.y >= r.y && p.x <= r.x + r.w && p.y <= r.y + r.h;
  }

  enum class PointerButton : uint8_t { None, Left, Right, Middle };

  enum KeyMod : uint8_t { ModNone = 0, ModCtrl = 1, ModShift = 2, ModAlt = 4, ModSuper = 8 };
  inline KeyMod operator|(KeyMod a, KeyMod b) { return static_cast<KeyMod>((int)a | (int)b); }

  //------------------------------------------------------------------
  struct PointerEvent
  {
    enum class Type : uint8_t { Move, Down, Up, Scroll, Enter, Leave };
    Type type{ Type::Move };
    Vec2 pos{}; // in UI virtual pixels
    Vec2 delta{}; // for Move
    float wheel = 0.f; // for Scroll
    PointerButton button{ PointerButton::None };
    int clicks = 0; // 1=single,2=double
    KeyMod mods{ ModNone };
    uint64_t frame = 0; double time = 0.0; // optional
  };

  //--------------------------------------------------------------
  enum class Key : uint16_t
  {
    Unknown = 0,

    // Navigation + control
    Tab, Enter, Escape, Space,
    Left, Right, Up, Down,
    Home, End, PageUp, PageDown,
    Backspace, Delete, F1, F5, F12,
    Minus, Plus,
    Numpad0, Numpad1, Numpad2, Numpad3, Numpad4,
    Numpad5, Numpad6, Numpad7, Numpad8, Numpad9
  };

  //----------------------------------------------------------
  inline bool isFocusNav(Key k)
  {
    switch (k)
    {
    case Key::Tab:
    case Key::Left: case Key::Right:
    case Key::Up:   case Key::Down:
      return true;
    default: return false;
    }
  }

  //----------------------------------------------------------------
  struct KeyEvent
  {
    enum class Type : uint8_t { KeyDown, KeyUp, Char }; // we’ll use Down for nav
    Type      type{ Type::KeyDown };
    Key       key{ Key::Unknown };   // for KeyDown/KeyUp
    char32_t  ch{ U'\0' };           // for Type::Char (printable text)
    bool      repeat = false;  // OS repeat
    KeyMod    mods{ ModNone };
  };

  //----------------------------------------------------------------
  enum class NavDir : uint8_t { Up, Down, Left, Right };

  //----------------------------------------------------------------
  struct GamepadEvent
  {
    enum class Type : uint8_t { DPad, Activate, Back };
    Type type{ Type::DPad };
    NavDir dir{};
  };

  //----------------------------------------------------------------
  struct FocusEvent { bool focused = false; };

  //----------------------------------------------------------------
  struct DragEvent
  {
    enum class Type { Begin, Enter, Over, Leave, Drop, Cancel, End } type;
    Vec2 pos{};          // in virtual px
    Vec2 delta{};        // Move delta (for Over)
    float time = 0.f;    // optional
  };

  //----------------------------------------------------------------
  struct DLL_UI_LIB UIEvent
  {
    enum class Kind : uint8_t { Pointer, Key, Focus, Gamepad, Drag, Custom };
    Kind kind{ Kind::Pointer };
    std::variant<PointerEvent, KeyEvent, FocusEvent, GamepadEvent, DragEvent> data;
  };

}