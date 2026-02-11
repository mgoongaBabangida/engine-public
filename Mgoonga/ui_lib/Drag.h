#pragma once
#include "base.h"

namespace UI_lib
{
  class Widget;

  //------------------------------------------------------------------
  struct DragVisual
  {
    // Optional sprite to draw as ghost
    std::string spriteName;
    uint32_t    tint = 0xFFFFFFB0;   // semi-opaque by default
    Vec2        size{ 0,0 };           // 0 = use sprite px size
    Vec2        hotspot{ 0.5f, 0.5f }; // 0..1 anchor within visual
  };

  //------------------------------------------------------------------
  struct DragPayload
  {
    // Type tag + opaque data the game can interpret
    // Keep it small; use IDs, not big objects.
    enum class Type { None, Citizen, Item, Resource, Custom } type = Type::None;
    int         i0 = -1;             // e.g., entity id / index
    int         i1 = -1;             // optional secondary
    uint32_t    u0 = 0;              // optional flags
    std::string s0;                  // optional name/key
  };

  // Drag controller lives in UiContext for global state
  //------------------------------------------------------------------
  struct DragController
  {
    bool   active = false;
    bool   thresholdMet = false;
    Vec2   startPos{};
    Vec2   pos{};
    Vec2   lastPos{};
    Widget* source = nullptr;
    Widget* hoverTarget = nullptr;
    Widget* dropTarget = nullptr;  // best accepting target under cursor
    DragPayload payload{};
    DragVisual  visual{};
    bool   anyAccepting = false;   // last Over said "accept"
    bool   cancelOnEsc = true;
  };

}