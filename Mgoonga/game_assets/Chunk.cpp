#include "Chunk.h"

namespace terrain
{
  //---------------------------------------------------------------------
  std::vector<glm::ivec3> ChunkCubes(int Nx, int Ny, glm::ivec2 chunkIdx)
  {
    std::vector<glm::ivec3> out;
    out.reserve(Nx * Ny);

    const int q0 = chunkIdx.x * Nx;
    const int r0 = chunkIdx.y * Ny;

    for (int dq = 0; dq < Nx; ++dq)
      for (int dr = 0; dr < Ny; ++dr) {
        int q = q0 + dq;
        int r = r0 + dr;
        out.push_back({ q, -q - r, r }); // (x,y,z)
      }
    return out;
  }
}