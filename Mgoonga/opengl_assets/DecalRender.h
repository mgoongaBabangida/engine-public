#pragma once
#include "opengl_assets.h"

#include "Shader.h"
#include <base/base.h>
#include <math/Camera.h>
#include <math/Decal.h>
#include "ScreenMesh.h"

//---------------------------------------------------------------------------
class eDecalRender
{
public:
  eDecalRender(const std::string& vS, const std::string& fS);
  ~eDecalRender();

  void Render(const Camera& _camera, const std::vector<Decal>&);
  Shader& GetShader() { return mShader; }
protected:
  Shader mShader;
  std::unique_ptr<eScreenMesh>	screenMesh;
};