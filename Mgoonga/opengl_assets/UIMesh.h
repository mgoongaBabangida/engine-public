#pragma once
#include "opengl_assets.h"

#include <base/interfaces.h>

#include "IUiSource.h"
#include "MeshBase.h"

//--------------------------------------------------------------
class DLL_OPENGL_ASSETS UIMesh : public IMesh
{
public:
  enum class UploadPolicy { Simple, Orphan, TripleBuffer };

  UIMesh();
  ~UIMesh() override;

  void SetUploadPolicy(UploadPolicy p, bool useGpuFences = true);
  void BindSource(IUiSource* src) { source = src; }
  void SetCallbacks(const UiCallbacks& c) { cbs = c; }

  // Optional: provide default programs if you want UIMesh to pick them
 /* void SetProgram(UiShaderKind kind, const Shader& s) { programs[(int)kind] = s; }
  void UseDefaultProgramMapping(bool v) { useDefaultPrograms = v; }*/

  // Plain IMesh draw (no interception)
  void Draw() override;

  const std::string& Name() const override { static std::string n = "UIMesh"; return n; }

private:
  IUiSource* source = nullptr;     // non-owning
  UiCallbacks  cbs;
 
  // Ring (1 or 3) — we keep arrays of 3, use ringCount=1 for Simple/Orphan
  static constexpr int kRing = 3;
  std::array<GLuint, kRing> vao_{};
  std::array<GLuint, kRing> vbo_{};
  std::array<GLuint, kRing> ibo_{};
  std::array<GLsync, kRing> fence_{};

  UploadPolicy policy_ = UploadPolicy::Simple;
  int          ringCount_ = 1;
  int          ringCur_ = 0;
  bool         useFences_ = true;

  void createSlot_(int i);
  void destroySlot_(int i);
  void bindAttribs_(int i);

  // Upload to the selected slot according to policy_
  void upload_(int slot, const UIGpuVertex* v, size_t nv, const uint32_t* i, size_t ni);

  // Fence helpers (only used when TripleBuffer + useFences_)
  void waitFence_(int slot);
  void setFence_(int slot);
};

