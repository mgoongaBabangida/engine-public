#pragma once
#include <cstdint>
#include <glm/glm/glm.hpp>

namespace logic {

  using TileId = uint32_t;
  constexpr TileId INVALID_TILE = TileId(-1);

  struct GridIndex {
    int width = 0, height = 0;   // axial rectangle size
    int q0 = 0, r0 = 0;          // axial origin used when you built the grid
    bool wrapX = true;           // Civ-style cylindrical wrap

    // axial -> id (O(1))
    inline TileId idFromAxial(int q, int r) const {
      // translate world axial (q,r) to [0..W-1],[0..H-1]
      int iq = q - q0;
      int ir = r - r0;

      if (wrapX) {
        if (ir < 0 || ir >= height) return INVALID_TILE;
        iq = ((iq % width) + width) % width;
      }
      else {
        if (iq < 0 || iq >= width || ir < 0 || ir >= height) return INVALID_TILE;
      }
      return TileId(iq + ir * width);
    }

    // cube -> id (O(1)) — axial (q,r) is (x,z) in your cube convention
    inline TileId idFromCube(const glm::ivec3& c) const { return idFromAxial(c.x, c.z); }

    // id -> cube (useful for distance etc.)
    inline glm::ivec3 cubeFromId(TileId id) const {
      int ir = int(id) / width;
      int iq = int(id) % width;
      int q = q0 + iq;
      int r = r0 + ir;
      // axial(q,r) -> cube(x,y,z) with y = -x - z
      return { q, -q - r, r };
    }
  };

} // namespace logic
