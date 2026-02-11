#pragma once
#include "DrawList.h"

namespace UI_lib
{
  enum class Ease : uint8_t { Linear, InQuad, OutQuad, InOutQuad, OutCubic };

  //-----------------------------------------------------------------------------
  inline float ease(Ease e, float t)
  {
    switch (e) {
    case Ease::InQuad: return t * t;
    case Ease::OutQuad: return t * (2.f - t);
    case Ease::InOutQuad: return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t;
    case Ease::OutCubic: { float u = t - 1.f; return u * u * u + 1.f; }
    default: return t;
    }
  }

  //------------------------------------------------------------------------------------------
  struct Tween
  {
    // Generic float tween; extend with unions for color/vec2 if needed
    float* target = nullptr;
    float from = 0, to = 1;
    float dur = 0.2f;
    float t = 0;
    Ease easeType = Ease::OutQuad;

    bool active = false;
    bool finished = false;
    bool clamp = true;
    std::function<void()> onComplete;

    void start(float* _target, float _to, float _dur, Ease e = Ease::OutQuad)
    {
      target = _target; from = *_target; to = _to; dur = _dur; t = 0; easeType = e; active = true; finished = false;
    }
    void stop() { active = false; }
    void update(float dt)
    {
      if (!active || !target) return; t += dt; float a = dur > 0 ? std::min(t / dur, 1.f) : 1.f; float v = from + (to - from) * ease(easeType, a); *target = v; if (a >= 1.f) { active = false; finished = true; if (onComplete) onComplete(); }
    }
  };

  //---------------------------------------------------------------------
  struct DLL_UI_LIB Animator
  { 
    std::vector<Tween> tweens;
    bool update(float dt) { for (auto& tw : tweens) tw.update(dt); return true; } //@todo return later
  };
}