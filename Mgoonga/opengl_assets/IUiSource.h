// ui_gpu_batch.hpp  (public, no UI_lib includes)
#pragma once
#include <cstdint>
#include <glm/glm/vec4.hpp>

// Vertex layout you want on GPU (position, uv, color, aux0/aux1)
struct UIGpuVertex
{
  float x, y;
  float u, v;
  uint32_t rgba;   // premultiplied ABGR/RGBA (match your shader)
  float aux0;      // e.g., msdf pxRange or flags (0 if unused)
  float aux1;      // e.g., msdf scale (0 if unused)
};

// Match your visual variants
enum class UiShaderKind : uint8_t { Sprite, NineSlice, MSDF, Solid, Gradient, Masked, CursorFollow, GreyKernel };

// Per-command pipeline setup
struct UiPipelineKey
{
  UiShaderKind shader = UiShaderKind::Sprite;
  int32_t      renderFunc = 0;      // your SetRenderingFunction() selector
  uint8_t      premultiplied = 1;   // 1 = ONE,ONE_MINUS_SRC_ALPHA
  uint8_t      invert_y = 0; // 1 - invert Y sampling
};

struct UIRect { float x, y, w, h; };

struct UiCmdWire
{
  uint32_t tex0 = 0;
  uint32_t tex1 = 0;
  uint32_t tex2 = 0;
  UIRect   clipVirtual{};
  UiPipelineKey pipe{};
  uint32_t indexOffset = 0;
  uint32_t indexCount = 0;
};

// Non-owning frame source
struct IUiSource
{
  virtual ~IUiSource() = default;
  virtual const UIGpuVertex*  vertices(size_t& outCount) const = 0;
  virtual const uint32_t*     indices(size_t& outCount) const = 0;
  virtual const UiCmdWire*    commands(size_t& outCount) const = 0;
  virtual glm::ivec4          toScissorPx(const UIRect& virtualClip) const = 0;
};

// Per-frame callbacks driven by your eScreenRender
struct UiCallbacks
{
  std::function<void()>                                        begin = nullptr;
  std::function<void(const UiPipelineKey&, const UiCmdWire&)>  before = nullptr; // set program/blend/variants
  std::function<void(const UiCmdWire&)>                        after = nullptr; // optional
  std::function<void()>                                        end = nullptr;
};