#pragma once
#include "Shader.h"
#include "LeafMesh.h"
#include <math/Camera.h>
#include <map>

//------------------------------------------------------------
class eLeafRender
{
public:
  eLeafRender(const std::string&, const std::string&);

  void	Render(const Camera& _camera, std::vector<shObject> _objects);
  Shader& GetShader() { return m_shader; }
protected:
  Shader m_shader;
  std::map<std::string, std::unique_ptr<LeafMesh>> m_meshes;
};
