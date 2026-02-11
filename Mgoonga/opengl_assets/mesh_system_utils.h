#pragma once
#include "opengl_assets.h"
#include "MyMesh.h"

class eMesh;
struct Texture;

namespace gl_utils
{
  Brush* MeshToBrush(eMesh&& _mesh, std::vector<Texture*> _textures);
  void   DebugPrintTBN(const std::vector<Vertex>& vertices);

  Brush* CreatePlane(size_t _dimensions = 100, size_t _divisor = 1, bool _spreadUV = false, float _uvScale = 1);
  Brush* CreateHex(float _radius, bool _spreadUV = true, float _uvScale = 1.f);
}
