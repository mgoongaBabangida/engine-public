#pragma once
#include "stdafx.h"

#include <base/base.h>
#include <base/interfaces.h>
#include <fstream>

//----------------------------------
struct MeshBase : public I3DMesh
{
public:
  std::vector<Vertex> m_vertices;
  std::vector<GLuint> m_indices;

  GLuint VAO = -1, VBO = -1, EBO = -1;   // @todo 0 = no object

  MeshBase() = default;
  virtual ~MeshBase() { destroy(); }

  // No copying
  MeshBase(const MeshBase&) = delete;
  MeshBase& operator=(const MeshBase&) = delete;

  // Move ctor: steal and invalidate source
  MeshBase(MeshBase && other) noexcept
    : m_vertices(std::move(other.m_vertices))
    , m_indices(std::move(other.m_indices))
    , VAO(std::exchange(other.VAO, -1))
    , VBO(std::exchange(other.VBO, -1))
    , EBO(std::exchange(other.EBO, -1))
  {}

  // Move assign: free ours, then steal theirs
  MeshBase& operator=(MeshBase && other) noexcept
  {
    if (this != &other)
    {
      destroy();
      m_vertices = std::move(other.m_vertices);
      m_indices = std::move(other.m_indices);
      VAO = std::exchange(other.VAO, -1);
      VBO = std::exchange(other.VBO, -1);
      EBO = std::exchange(other.EBO, -1);
    }
    return *this;
  }

private:
  void destroy() noexcept //@todo cleared by derived for now
  {
    /*if (EBO) { glDeleteBuffers(1, &EBO); EBO = -1; }
    if (VBO) { glDeleteBuffers(1, &VBO); VBO = -1; }
    if (VAO) { glDeleteVertexArrays(1, &VAO); VAO = -1; }*/
  }
};
