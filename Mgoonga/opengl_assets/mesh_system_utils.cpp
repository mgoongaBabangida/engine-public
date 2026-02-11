//#include "stdafx.h"
#include "mesh_system_utils.h"

#include "Mesh.h"
#include "Texture.h"

namespace gl_utils
{
  //-------------------------------------------------------------------
  Brush* MeshToBrush(eMesh&& _mesh, std::vector<Texture*> _textures)
  {
    return new Brush(_mesh.Name(), std::move(_mesh), _textures);
  }

  //-------------------------------------------------------------------
  Brush* CreatePlane(size_t _dimensions, size_t _divisor, bool _spreadUV, float _uvScale)
  {
    if (_divisor < 2) return nullptr; // need at least a 2x2 vertex grid

    const size_t numVertices = _divisor * _divisor;
    const size_t numQuads = (_divisor - 1) * (_divisor - 1);

    auto* shapeData = new Brush("plane");
    shapeData->m_vertices.reserve(numVertices);
    shapeData->m_indices.reserve(numQuads * 6);

    const float halfSize = static_cast<float>(_dimensions) * 0.5f;
    const float step = static_cast<float>(_dimensions) / static_cast<float>(_divisor - 1);

    for (size_t z = 0; z < _divisor; ++z)
    {
      for (size_t x = 0; x < _divisor; ++x)
      {
        Vertex v{};
        v.Position = glm::vec3(
          -halfSize + static_cast<float>(x) * step, // X
          0.0f,                                     // Y
          -halfSize + static_cast<float>(z) * step  // Z
        );

        if (_spreadUV) {
          // 0..1 across the entire plane
          v.TexCoords = glm::vec2(
            static_cast<float>(x) / static_cast<float>(_divisor - 1),
            static_cast<float>(z) / static_cast<float>(_divisor - 1)
          );
        }
        else {
          // Tiled UVs: requires sampler wrap = GL_REPEAT
          v.TexCoords = glm::vec2(
            static_cast<float>(x) * _uvScale,
            static_cast<float>(z) * _uvScale
          );
        }

        v.Normal = glm::vec3(0.0f, 1.0f, 0.0f); // +Y up (plane lies in XZ)
        shapeData->m_vertices.push_back(v);
      }
    }

    for (size_t z = 0; z < _divisor - 1; ++z)
    {
      for (size_t x = 0; x < _divisor - 1; ++x)
      {
        const size_t i0 = z * _divisor + x;      // (x,   z)
        const size_t i1 = z * _divisor + (x + 1);  // (x+1, z)
        const size_t i2 = (z + 1) * _divisor + x;      // (x,   z+1)
        const size_t i3 = (z + 1) * _divisor + (x + 1);  // (x+1, z+1)

        // CCW triangles for +Y normals
        shapeData->m_indices.push_back(i2);
        shapeData->m_indices.push_back(i1);
        shapeData->m_indices.push_back(i0);

        shapeData->m_indices.push_back(i2);
        shapeData->m_indices.push_back(i3);
        shapeData->m_indices.push_back(i1);
      }
    }

    shapeData->setupMesh();
    shapeData->calculatedTangent();
    return shapeData;
  }

  //----------------------------------------------------------------------
  Brush* CreateHex(float _radius, bool _spreadUV, float _uvScale)
  {
    auto* shapeData = new Brush("hex");
    shapeData->m_vertices.reserve(7);     // center + 6 outer vertices
    shapeData->m_indices.reserve(18);     // 6 triangles * 3 indices

    Vertex center{};
    center.Position = glm::vec3(0.0f, 0.0f, 0.0f);
    center.Normal = glm::vec3(0.0f, 1.0f, 0.0f);

    if (_spreadUV)
      center.TexCoords = glm::vec2(0.5f, 0.5f);
    else
      center.TexCoords = glm::vec2(0.0f, 0.0f);

    shapeData->m_vertices.push_back(center);

    constexpr float PI = 3.14159265358979323846f;
    for (int i = 0; i < 6; ++i)
    {
      float angle = PI / 3.0f * static_cast<float>(i); // 60° steps
      float x = _radius * std::cos(angle);
      float z = _radius * std::sin(angle);

      Vertex v{};
      v.Position = glm::vec3(x, 0.0f, z);
      v.Normal = glm::vec3(0.0f, 1.0f, 0.0f);

      if (_spreadUV) {
        // Map UVs into 0..1 range assuming hex fits into unit circle
        v.TexCoords = glm::vec2(0.5f + 0.5f * std::cos(angle),
          0.5f + 0.5f * std::sin(angle));
      }
      else {
        v.TexCoords = glm::vec2(x * _uvScale, z * _uvScale);
      }

      shapeData->m_vertices.push_back(v);
    }

    for (int i = 0; i < 6; ++i)
    {
      int next = (i + 1) % 6;
      shapeData->m_indices.push_back(0);        // center
      shapeData->m_indices.push_back(next + 1); // next
      shapeData->m_indices.push_back(i + 1);    // current
    }

    shapeData->setupMesh();
    shapeData->calculatedTangent();

    return shapeData;
  }

  void DebugPrintTBN(const std::vector<Vertex>& vertices)
  {
    int errorCount = 0;

    for (size_t i = 0; i < vertices.size(); ++i)
    {
      const auto& v = vertices[i];
      glm::vec3 N = glm::normalize(v.Normal);
      glm::vec3 T = glm::normalize(glm::vec3(v.tangent));
      glm::vec3 B = glm::normalize(v.bitangent);

      float NT = glm::dot(N, T);
      float NB = glm::dot(N, B);
      float TB = glm::dot(T, B);

      float lenN = glm::length(N);
      float lenT = glm::length(T);
      float lenB = glm::length(B);

      float det = glm::dot(glm::cross(N, T), B); // Should be +1 or -1

      const float EPSILON = 1e-3f;

      bool isOrtho = std::abs(NT) < EPSILON &&
        std::abs(NB) < EPSILON &&
        std::abs(TB) < EPSILON;

      bool isUnit = std::abs(lenN - 1.f) < EPSILON &&
        std::abs(lenT - 1.f) < EPSILON &&
        std::abs(lenB - 1.f) < EPSILON;

      bool isRightHanded = det > 0.0f;

      if (!isOrtho || !isUnit || !isRightHanded)
      {
        std::cout << "TBN issue at vertex " << i << ":\n";
        if (!isOrtho)
          std::cout << "  Non-orthogonal: dot(N,T)=" << NT << ", dot(N,B)=" << NB << ", dot(T,B)=" << TB << "\n";
        if (!isUnit)
          std::cout << "  Not normalized: |N|=" << lenN << ", |T|=" << lenT << ", |B|=" << lenB << "\n";
        if (!isRightHanded)
          std::cout << "  Left-handed basis (det = " << det << ")\n";
        ++errorCount;
      }
    }

    if (errorCount == 0)
      std::cout << "All vertex TBN bases are orthonormal and right-handed.\n";
    else
      std::cout << "Found " << errorCount << " problematic TBN bases.\n";
  }
}