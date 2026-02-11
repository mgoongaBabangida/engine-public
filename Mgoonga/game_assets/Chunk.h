#pragma once
#include "game_assets.h"

#include <math/Hex.h>

namespace terrain
{
  struct ChunkKey { int x, z; bool operator<(const ChunkKey& o) const { return x == o.x ? z < o.z : x < o.x; } };

  struct ChunkConfig
  {
    int   texRes = 1024;        // height/weight textures per chunk
    int   tilesPerChunkX = 12;         // how many hexes feed one chunk (X)
    int   tilesPerChunkZ = 12;         // how many hexes feed one chunk (Z)
    float outerRadius = 0.340540558f;       // hex outer radius in world units
    math::Hex::Orientation orient = math::Hex::Orientation::Flat;
    float worldScaleXZ = 1.6384;       // (optional) scale for local plane to world
  };

  constexpr int HEXS_PER_CHUNK_X = 12;
  constexpr int HEXS_PER_CHUNK_Y = 12;

  inline int floor_div(int a, int b) {
    int q = a / b; int r = a % b;
    return (r != 0 && ((r > 0) != (b > 0))) ? (q - 1) : q; // floor toward -?
  }

  inline glm::ivec2 TileToChunk(int q, int r) {
    return { floor_div(q, HEXS_PER_CHUNK_X),
             floor_div(r, HEXS_PER_CHUNK_Y) };
  }

  std::vector<glm::ivec3> DLL_GAME_ASSETS ChunkCubes(int Nx = 12, int Ny = 12, glm::ivec2 chunkIdx = { 0,0 });

  // helpers flat top
  static inline float HexOuterRadiusFromS0x(int Nx, float S0x)
  {
    return S0x / (1.5f * Nx + 0.5f);
  }

  // Center of chunk (cx,cy) that owns q in [q0..q0+Nx-1], r in [r0..r0+Ny-1]
  static inline glm::vec2 ChunkCenter_Flat(int Nx, int Ny, float R, glm::ivec2 c)
  {
    const int q0 = c.x * Nx;
    const int r0 = c.y * Ny;
    const float Xc = 0.75f * R * (2 * q0 + Nx - 1);
    const float Zc = 0.5f * sqrtf(3.0f) * R * (2 * r0 + Ny - 1)
      + 0.25f * sqrtf(3.0f) * R * (2 * q0 + Nx - 1);
    return { Xc, Zc };
  }

  static inline glm::vec2 ChunkScaleFromS0(int Nx, int Ny, float R, float S0x, float S0z)
  {
    const float W_req = (1.5f * (Nx - 1) + 2.0f) * R;
    const float H_req = sqrtf(3.0f) * R * (Ny + 0.5f * (Nx - 1));
    return { W_req / S0x, H_req / S0z };
  }
}