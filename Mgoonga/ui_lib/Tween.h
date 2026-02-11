#pragma once
#include<iostream>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>
#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <cassert>

#include "base.h"

namespace UI_lib
{
  namespace tween {

    // ---- Easing ---------------------------------------------------------------
    enum class Ease {
      Linear,
      QuadIn, QuadOut, QuadInOut,
      CubicIn, CubicOut, CubicInOut,
      QuartIn, QuartOut, QuartInOut,
      QuintIn, QuintOut, QuintInOut,
      ExpoIn, ExpoOut, ExpoInOut,
      SineIn, SineOut, SineInOut,
      BackIn, BackOut, BackInOut,
      ElasticOut, ElasticIn,
      BounceOut, BounceIn, BounceInOut
    };

    static inline float _bounceOut(float t) {
      const float n1 = 7.5625f; const float d1 = 2.75f;
      if (t < 1.0f / d1) return n1 * t * t;
      if (t < 2.0f / d1) { t -= 1.5f / d1; return n1 * t * t + 0.75f; }
      if (t < 2.5f / d1) { t -= 2.25f / d1; return n1 * t * t + 0.9375f; }
      t -= 2.625f / d1; return n1 * t * t + 0.984375f;
    }

    static inline float ease(Ease e, float t) {
      t = std::clamp(t, 0.0f, 1.0f);
      switch (e) {
      case Ease::Linear: return t;
      case Ease::QuadIn: return t * t;
      case Ease::QuadOut: return 1 - (1 - t) * (1 - t);
      case Ease::QuadInOut: return t < 0.5f ? 2 * t * t : 1 - std::pow(-2 * t + 2, 2) / 2;
      case Ease::CubicIn: return t * t * t;
      case Ease::CubicOut: { float u = 1 - t; return 1 - u * u * u; }
      case Ease::CubicInOut: return t < 0.5f ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3) / 2;
      case Ease::QuartIn: return t * t * t * t;
      case Ease::QuartOut: { float u = 1 - t; return 1 - u * u * u * u; }
      case Ease::QuartInOut: return t < 0.5f ? 8 * t * t * t * t : 1 - std::pow(-2 * t + 2, 4) / 2;
      case Ease::QuintIn: return t * t * t * t * t;
      case Ease::QuintOut: { float u = 1 - t; return 1 - u * u * u * u * u; }
      case Ease::QuintInOut:return t < 0.5f ? 16 * t * t * t * t * t : 1 - std::pow(-2 * t + 2, 5) / 2;
      case Ease::ExpoIn: return (t == 0) ? 0 : std::pow(2, 10 * t - 10);
      case Ease::ExpoOut: return (t == 1) ? 1 : 1 - std::pow(2, -10 * t);
      case Ease::ExpoInOut:
        if (t == 0) return 0; if (t == 1) return 1;
        return t < 0.5f ? std::pow(2, 20 * t - 10) / 2 : (2 - std::pow(2, -20 * t + 10)) / 2;
      case Ease::SineIn: return 1 - std::cos((t * 3.14159265f) / 2);
      case Ease::SineOut: return std::sin((t * 3.14159265f) / 2);
      case Ease::SineInOut: return -(std::cos(3.14159265f * t) - 1) / 2;
      case Ease::BackIn: { const float c1 = 1.70158f; const float c3 = c1 + 1; return c3 * t * t * t - c1 * t * t; }
      case Ease::BackOut: { const float c1 = 1.70158f; const float c3 = c1 + 1; float u = t - 1; return 1 + c3 * u * u * u + c1 * u * u; }
      case Ease::BackInOut: {
        const float c1 = 1.70158f; const float c2 = c1 * 1.525f;
        return t < 0.5f ? (std::pow(2 * t, 2) * ((c2 + 1) * 2 * t - c2)) / 2
          : (std::pow(2 * t - 2, 2) * ((c2 + 1) * (t * 2 - 2) + c2) + 2) / 2;
      }
      case Ease::ElasticOut: {
        if (t == 0 || t == 1) return t;
        const float c4 = (2 * 3.14159265f) / 3;
        return std::pow(2, -10 * t) * std::sin((t * 10 - 0.75f) * c4) + 1;
      }
      case Ease::ElasticIn: {
        if (t == 0 || t == 1) return t;
        const float c4 = (2 * 3.14159265f) / 3;
        return -std::pow(2, 10 * t - 10) * std::sin((t * 10 - 10.75f) * c4);
      }
      case Ease::BounceOut: return _bounceOut(t);
      case Ease::BounceIn: return 1 - _bounceOut(1 - t);
      case Ease::BounceInOut: return t < 0.5f ? (1 - _bounceOut(1 - 2 * t)) / 2 : (1 + _bounceOut(2 * t - 1)) / 2;
      }
      return t;
    }

    // ---- Lerp helpers ----------------------------------------------------------
    static inline float  lerp(const float& a, const float& b, float t) { return a + (b - a) * t; }
    static inline Vec2   lerp(const Vec2& a, const Vec2& b, float t) { return { lerp(a.x,b.x,t), lerp(a.y,b.y,t) }; }
    static inline Vec4   lerp(const Vec4& a, const Vec4& b, float t) { return { lerp(a.x,b.x,t), lerp(a.y,b.y,t), lerp(a.z,b.z,t), lerp(a.w,b.w,t) }; }

    // ---- Tween<T> --------------------------------------------------------------
    enum class TweenState { Idle, Playing, Paused, Done };

    template<typename T>
    struct Tween
    {
      T from{}, to{};
      float duration = 0.3f;    // seconds
      float t = 0.0f;           // normalized [0..1]
      Ease easer = Ease::CubicOut;
      TweenState state = TweenState::Idle;
      bool yoyo = false;        // if true, auto reverse once
      bool reversed = false;

      std::function<void(const T&)> onSample = nullptr; // optional sink
      std::function<void()> onEnd = nullptr;

      void play(const T& f, const T& to_, float dur, Ease e, bool yoyo_ = false) {
        from = f; to = to_; duration = std::max(0.0001f, dur);
        easer = e; t = 0; state = TweenState::Playing; yoyo = yoyo_; reversed = false;
      }
      void stop() { state = TweenState::Done; }
      bool isActive() const { return state == TweenState::Playing || state == TweenState::Paused; }

      // returns current sampled value
      T sample() const { return lerp(from, to, ease(easer, t)); }

      void update(float dt) {
        if (state != TweenState::Playing) return;
        t += dt / duration;
        if (t >= 1.0f) {
          t = 1.0f;
          if (yoyo && !reversed) {
            // reverse once
            reversed = true; std::swap(from, to); t = 0.0f; // keep same easer
          }
          else {
            state = TweenState::Done;
          }
        }
        if (onSample)
          onSample(sample());
        if (state == TweenState::Done && onEnd) onEnd();
      }
    };

    // ---- Multi-track Animator --------------------------------------------------
    enum class Channel : uint8_t {
      Opacity = 0,    // float 0..1
      Scale,          // Vec2
      Offset,         // Vec2 (pixels)
      Color,          // Vec4 (rgba)
      Custom0, Custom1, Custom2 // free channels if you need
    };

    struct AnyTween
    {
      Channel ch;
      enum Type { F, V2, V4 } type;
      Tween<float> f; Tween<Vec2> v2; Tween<Vec4> v4;
      bool sticky = false;

      bool active() const
      {
      switch (type) { case F: return f.isActive(); case V2: return v2.isActive(); case V4: return v4.isActive(); }
                            return false;
      }
      void update(float dt) {
      switch (type) { case F: f.update(dt); break; case V2: v2.update(dt); break; case V4: v4.update(dt); break; }
      }
    };

    //-------------------------------------------------------------------
    struct Animator
    {
      std::vector<AnyTween> tracks;

      // get or create a track of given type
      AnyTween* require(Channel ch, AnyTween::Type ty)
      {
        for (auto& t : tracks)
          if (t.ch == ch)
            return &t;
        AnyTween a; 
        a.ch = ch; 
        a.type = ty;
        tracks.push_back(a);
        return &tracks.back();
      }

      void play(Channel ch, float from, float to, float dur, Ease e, bool yoyo = false, std::function<void(float)> sink = nullptr) {
        auto* tr = require(ch, AnyTween::F);
        if (sink)
          tr->f.onSample = sink;           // keep existing binding if sink==nullptr
        tr->f.play(from, to, dur, e, yoyo);
      }
      void play(Channel ch, Vec2 from, Vec2 to, float dur, Ease e, bool yoyo = false, std::function<void(Vec2)> sink = nullptr) {
        auto* tr = require(ch, AnyTween::V2);
        if (sink)
          tr->v2.onSample = sink;
        tr->v2.play(from, to, dur, e, yoyo);
      }
      void play(Channel ch, Vec4 from, Vec4 to, float dur, Ease e, bool yoyo = false, std::function<void(Vec4)> sink = nullptr) {
        auto* tr = require(ch, AnyTween::V4);
        if (sink)
          tr->v4.onSample = sink;
        tr->v4.play(from, to, dur, e, yoyo);
      }

      // pull value (for polling render paths). Returns std::optional<T>-ish by out param.
      bool get(Channel ch, float& out) const {
        for (auto& t : tracks) if (t.ch == ch && t.type == AnyTween::F) { out = t.f.sample(); return true; }
        return false;
      }
      bool get(Channel ch, Vec2& out) const {
        for (auto& t : tracks) if (t.ch == ch && t.type == AnyTween::V2) { out = t.v2.sample(); return true; }
        return false;
      }
      bool get(Channel ch, Vec4& out) const {
        for (auto& t : tracks) if (t.ch == ch && t.type == AnyTween::V4) { out = t.v4.sample(); return true; }
        return false;
      }

      void update(float dt)
      {
        for (auto& t : tracks) t.update(dt);
        // drop fully finished tracks (keeps last value until next play)
        tracks.erase(std::remove_if(tracks.begin(), tracks.end(),
          [](const AnyTween& t) { return !t.active() && !t.sticky; }),
          tracks.end());
      }
    };

    struct AnimValues
    {
      float opacity = 1.0f;
      Vec2  offset = { 0,0 };
      Vec2  scale = { 1,1 };
      Vec4  color = { 1,1,1,1 }; // multiplicative (rgba)
    };

    // ---- Open/Close Transition State ------------------------------------------
    enum class TransState { Closed, Opening, Open, Closing };

    //-----------------------------------------------------------------------------
    struct Transition
    {
      TransState state = TransState::Closed;
      Animator anim;

      // Cached visual state (sampled each frame)
      float opacity = 0.0f;
      Vec2  scale = { 1.0f,1.0f };
      Vec2  offset = { 0.0f,0.0f };
      Vec4  color = { 1,1,1,1 };

      // Preset config
      float dur_open = 0.18f;
      float dur_close = 0.14f;
      Ease ease_open = Ease::CubicOut;
      Ease ease_close = Ease::CubicIn;

      void bind(Channel ch, float* p) {
        auto* tr = anim.require(ch, AnyTween::F);
        tr->f.onSample = [p, this](float v) { *p = v; this->opacity = v; };
        tr->sticky = true;
      }
      void bind(Channel ch, Vec2* p) {
        auto* tr = anim.require(ch, AnyTween::V2);
        tr->v2.onSample = [p, this](Vec2 v) { *p = v; this->offset = v; };
        tr->sticky = true;
      }
      void bind(Channel ch, Vec4* p) {
        auto* tr = anim.require(ch, AnyTween::V4);
        tr->v4.onSample = [p, this](Vec4 v) { *p = v; this->color = v; };
        tr->sticky = true;
      }

      // Convenience: bind to a whole bundle
      void bindAll(AnimValues& a) {
        bind(Channel::Opacity, &a.opacity);
        bind(Channel::Offset, &a.offset);
        bind(Channel::Scale, &a.scale);
        bind(Channel::Color, &a.color);
      }

      // Policy: reverse if mid-flight
      void open() {
        if (state == TransState::Open || state == TransState::Opening) return;
        state = TransState::Opening;
        anim.play(Channel::Opacity, opacity, 1.0f, dur_open, ease_open,  /*yoyo=*/false, /*sink=*/nullptr);
        anim.play(Channel::Scale, scale, Vec2{ 1.0f,1.0f }, dur_open, ease_open, false, nullptr);
        anim.play(Channel::Offset, offset, Vec2{ 0.0f,-4.0f }, dur_open, ease_open, false, nullptr);
      }
      void close() {
        if (state == TransState::Closed || state == TransState::Closing) return;
        state = TransState::Closing;
        anim.play(Channel::Opacity, opacity, 0.0f, dur_close, ease_close, false, nullptr);
        anim.play(Channel::Scale, scale, Vec2{ 0.98f,0.98f }, dur_close, ease_close, false, nullptr);
        anim.play(Channel::Offset, offset, Vec2{ 0.0f, +4.0f }, dur_close, ease_close, false, nullptr);
      }

      void jumpOpen() { state = TransState::Open;  opacity = 1.0f; scale = { 1,1 }; offset = { 0,-4 }; }
      void jumpClosed() { state = TransState::Closed; opacity = 0.0f; scale = { 0.98f,0.98f }; offset = { 0,4 }; }

      void update(float dt) {
        anim.update(dt);
        // settle state
        if (state == TransState::Opening && anim.tracks.empty()) { state = TransState::Open; }
        if (state == TransState::Closing && anim.tracks.empty()) { state = TransState::Closed; }
      }

      bool visible() const { return state != TransState::Closed || opacity > 0.001f; }
    };

    // ---- Presets ---------------------------------------------------------------
    // Quick stylings for different feels (you can extend as needed)
    struct TransitionPreset {
      float dur_open, dur_close;
      Ease  ease_open, ease_close;
      Vec2  openOffset, closeOffset;
      Vec2  openScale, closeScale;
      bool  elastic = false;
    };

    static inline Transition makeFadeSlidePreset(const TransitionPreset& p) {
      Transition t;
      t.dur_open = p.dur_open; t.dur_close = p.dur_close;
      t.ease_open = p.ease_open; t.ease_close = p.ease_close;
      t.offset = p.closeOffset; t.scale = p.closeScale; t.opacity = 0.0f;
      return t;
    }
  }
}
// Examples to use:
// auto t = makeFadeSlidePreset({0.18f,0.14f, Ease::CubicOut, Ease::CubicIn, {0,-4},{0,4},{1,1},{0.98f,0.98f}, false});
