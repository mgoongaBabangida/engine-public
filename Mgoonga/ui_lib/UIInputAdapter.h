
// UiInputAdapter.hpp
#pragma once
#include <base/interfaces.h>

#include "ui_lib.h"
#include "Widget.h"
#include "UISystem.h"

namespace UI_lib
{
  static inline Key asciiToKey(uint32_t asci)
  {
    // Map your legacy ASCII/scancodes to UI keys.
    // Keep this tiny—only what you need for nav MVP.
    switch (asci)
    {
      // Common controls (fill with your ASCII values or engine codes)
    case 9:   return Key::Tab;      // '\t' (if you get it)
    case 13:  return Key::Enter;    // '\r'
    case 27:  return Key::Escape;   // ESC
    case 32:  return Key::Space;    // ' '
    case 31:  return Key::Backspace;    // ' '

    // If your engine uses custom codes for arrows/home/end/etc.,
    // map them here (examples below assume >255 slots).
    case 256 + 1: return Key::Left;
    case 256 + 2: return Key::Right;
    case 256 + 3: return Key::Up;
    case 256 + 4: return Key::Down;
    case 256 + 5: return Key::Home;
    case 256 + 6: return Key::End;
    case 256 + 7: return Key::PageUp;
    case 256 + 8: return Key::PageDown;

    case 0x100 + 9:  return Key::Numpad0;
    case 0x100 + 10: return Key::Numpad1;
    case 0x100 + 11: return Key::Numpad2;
    case 0x100 + 12: return Key::Numpad3;
    case 0x100 + 13: return Key::Numpad4;
    case 0x100 + 14: return Key::Numpad5;
    case 0x100 + 15: return Key::Numpad6;
    case 0x100 + 16: return Key::Numpad7;
    case 0x100 + 17: return Key::Numpad8;
    case 0x100 + 18: return Key::Numpad9;

    default: return Key::Unknown;
    }
  }

  static inline bool isPrintable(uint32_t asci, KeyModifiers mods)
  {
    // Don’t emit text when Ctrl/Alt held (you can tweak this policy)
    //if (mods & (KeyModifiers::CTRL | KeyModifiers::ALT)) return false;
    // Basic printable ASCII range
    return asci >= 32 && asci < 127;
  }

  //--------------------------------------------------------------------------
  class DLL_UI_LIB UiInputAdapter : public IInputObserver
  {
  public:
    bool OnMouseMove(int32_t x, int32_t y, KeyModifiers m) override
    {
      auto& sys = UiSystem::I();
      Vec2 v = sys.ctx.screenToVirtual({ (float)x,(float)y });
      UIEvent ev; ev.kind = UIEvent::Kind::Pointer;
      PointerEvent pe; pe.type = PointerEvent::Type::Move; pe.pos = v; pe.mods = (KeyMod)m;
      ev.data = pe;
      sys.route(ev);
      return false; // let others also see; routing decides consumption
    }

    bool OnMousePress(int32_t x, int32_t y, bool left, KeyModifiers m) override
    {
      auto& sys = UiSystem::I(); Vec2 v = sys.ctx.screenToVirtual({ (float)x,(float)y });
      cachedPos = v;
      UIEvent ev; ev.kind = UIEvent::Kind::Pointer;
      PointerEvent pe; pe.type = PointerEvent::Type::Down; pe.pos = v;
      pe.button = left ? PointerButton::Left : PointerButton::Right; pe.mods = (KeyMod)m;
      pe.clicks = 1;
      ev.data = pe;
      bool used = sys.route(ev);
      return used;
    }

    bool OnMouseDoublePress(int32_t x, int32_t y, bool left, KeyModifiers m) override
    {
      auto& sys = UiSystem::I(); Vec2 v = sys.ctx.screenToVirtual({ (float)x,(float)y });
      cachedPos = v;
      UIEvent ev; ev.kind = UIEvent::Kind::Pointer;
      PointerEvent pe; pe.type = PointerEvent::Type::Down; pe.pos = v;
      pe.button = left ? PointerButton::Left : PointerButton::Right; pe.mods = (KeyMod)m;
      pe.clicks = 2;
      ev.data = pe;
      bool used = sys.route(ev);
      return used;
    }

    bool OnMouseRelease(KeyModifiers m) override
    {
      auto& sys = UiSystem::I();
      UIEvent ev; ev.kind = UIEvent::Kind::Pointer;
      PointerEvent pe; pe.type = PointerEvent::Type::Up; pe.mods = (KeyMod)m; pe.pos = cachedPos;
      ev.data = pe; bool used = sys.route(ev); return used;
    }

    bool OnMouseWheel(int32_t x, int32_t y, KeyModifiers m) override
    {
      auto& sys = UiSystem::I();
      UIEvent ev; ev.kind = UIEvent::Kind::Pointer;
      PointerEvent pe; pe.type = PointerEvent::Type::Scroll; pe.wheel = (float)y; pe.mods = (KeyMod)m;
      ev.data = pe; bool used = sys.route(ev); return used;
    }

    //---------------------------Key---------------------------------------------------------
    bool OnKeyJustPressed(uint32_t asci, KeyModifiers m) override {
      // 1) KeyDown (for navigation/commands)
      {
        UIEvent ev; ev.kind = UIEvent::Kind::Key;
        KeyEvent ke; ke.type = KeyEvent::Type::KeyDown;
        ke.key = asciiToKey(asci);
        ke.mods = (KeyMod)m;
        ev.data = ke;
        UiSystem::I().route(ev);
      }

      // 2) Optional text input (printable) for future typing
      if (isPrintable(asci, m)) {
        UIEvent tev; tev.kind = UIEvent::Kind::Key;
        KeyEvent tke; tke.type = KeyEvent::Type::Char;
        tke.ch = static_cast<char32_t>(asci);
        tke.mods = (KeyMod)m;
        tev.data = tke;
        UiSystem::I().route(tev);
      }

      return false; // let others see if needed
    }

    bool OnKeyRelease(ASCII asci, const std::vector<bool>, KeyModifiers m) override {
      UIEvent ev; ev.kind = UIEvent::Kind::Key;
      KeyEvent ke; ke.type = KeyEvent::Type::KeyUp;
      ke.key = asciiToKey(static_cast<uint32_t>(asci));
      ke.mods = (KeyMod)m;
      ev.data = ke;
      UiSystem::I().route(ev);
      return false;
    }

    bool OnKeyPress(const std::vector<bool>, KeyModifiers) override { return false; }

    bool OnUpdate() override
    {
      auto& sys = UiSystem::I();
      if (!m_clock.isActive())
        m_clock.start();
      if (m_clock.timeElapsedLastFrameMsc() > 0)
      {
        sys.update(static_cast<float>(m_clock.newFrame()));
        sys.build();
      }
      return false;
    }
  private:
    math::eClock m_clock;
    Vec2 cachedPos{};
  };
}
