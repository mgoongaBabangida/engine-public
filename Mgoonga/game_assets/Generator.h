// WorldSnapshot.h
#pragma once
#include "game_assets.h"

#include <vector>
#include <array>
#include <cstdint>

#include "Tile.h"
#include "GridIndex.h"
#include "IWorld.h" // new header

namespace logic
{
  // simple bitmask for tile improvements (kept local for now)
  enum : std::uint8_t
  {
    IMP_NONE = 0,
    IMP_ROAD = 1 << 0,
    IMP_FARM = 1 << 1,
    IMP_LUMBERMILL = 1 << 2
  };

  //-------------------------------------------------------------------
  struct DLL_GAME_ASSETS WorldSnapshot : public IWorld
  {
    // --- core terrain data (from generator) ---
    std::vector<Tile> tiles;
    std::uint64_t seed = 0;
    int width = 0, height = 0;
    GridIndex index;

    // --- gameplay overlays ---
    std::vector<std::uint8_t> improvements; // size == tiles.size()
    std::vector<std::uint16_t> cityOwner;   // 0 = none, else CityId or RegionId

    // --- construction helper (called by generator) ---
    void InitCore(std::uint64_t s, int w, int h, const GridIndex& gi)
    {
      seed = s;
      width = w;
      height = h;
      index = gi;
      tiles.resize(std::size_t(w) * std::size_t(h));
      improvements.assign(tiles.size(), IMP_NONE);
      cityOwner.assign(tiles.size(), 0);
    }

    // ---------- internal helpers ----------
    inline TileId idFromCube(const glm::ivec3& c) const
    {
      return index.idFromCube(c);
    }

    inline const Tile* tryGetTileInternal(TileId id) const
    {
      if (id == INVALID_TILE) return nullptr;
      if (id >= tiles.size()) return nullptr;
      return &tiles[id];
    }

    inline Tile* tryGetTileInternal(TileId id)
    {
      if (id == INVALID_TILE) return nullptr;
      if (id >= tiles.size()) return nullptr;
      return &tiles[id];
    }

    // ---------- IWorldInfo implementation ----------

    glm::ivec2 GetSize() const override { return { width, height }; }

    bool IsValid(const Qrc& qrc) const override
    {
      return idFromCube(qrc) != INVALID_TILE;
    }

    const Tile* TryGetTile(const Qrc& qrc) const override
    {
      return tryGetTileInternal(idFromCube(qrc));
    }

    bool IsLand(const Qrc& qrc) const override
    {
      if (auto* t = TryGetTile(qrc)) return t->isLand();
      return false;
    }

    bool IsWater(const Qrc& qrc) const override
    {
      if (auto* t = TryGetTile(qrc)) return t->isWater();
      return false;
    }

    bool IsHill(const Qrc& qrc) const override
    {
      if (auto* t = TryGetTile(qrc))
        return t->vBiome == VerticalBiome::Hills;
      return false;
    }

    bool IsMountain(const Qrc& qrc) const override
    {
      if (auto* t = TryGetTile(qrc))
        return t->vBiome == VerticalBiome::Mountains;
      return false;
    }

    bool IsTundra(const Qrc& qrc) const override
    {
      if (auto* t = TryGetTile(qrc))
        return t->climate == Climate::Tundra;
      return false;
    }

    bool HasForest(const Qrc& qrc) const override
    {
      if (auto* t = TryGetTile(qrc)) return t->hasForest();
      return false;
    }

    bool HasRoad(const Qrc& qrc) const override
    {
      TileId id = idFromCube(qrc);
      if (id == INVALID_TILE || id >= improvements.size()) return false;
      return (improvements[id] & IMP_ROAD) != 0;
    }

    bool HasFarm(const Qrc& qrc) const override
    {
      TileId id = idFromCube(qrc);
      if (id == INVALID_TILE || id >= improvements.size()) return false;
      return (improvements[id] & IMP_FARM) != 0;
    }

    bool HasLumbermill(const Qrc& qrc) const override
    {
      TileId id = idFromCube(qrc);
      if (id == INVALID_TILE || id >= improvements.size()) return false;
      return (improvements[id] & IMP_LUMBERMILL) != 0;
    }

    bool DoesBelongToCity(const Qrc& qrc) const override
    {
      TileId id = idFromCube(qrc);
      if (id == INVALID_TILE || id >= cityOwner.size()) return false;
      return cityOwner[id] != 0;
    }

    std::vector<Resource> GetBaseYields(const Qrc& qrc) const override
    {
      std::vector<Resource> out;
      if (auto* t = TryGetTile(qrc)) {
        for (auto r : t->resources)
          if (r != Resource::None)
            out.push_back(r);
      }
      return out;
    }

    //----------------------------------------------------------------------------
    std::vector<Qrc> GetNeighbors(const Qrc& center, int radius = 1) const override
    {
      if (radius <= 0)
        return {};

      // Total slots for all rings 1..radius: 6 * radius * (radius + 1) / 2
      const int count = 3 * radius * (radius + 1);
      std::vector<Qrc> result(static_cast<std::size_t>(count), INVALID_QRC);

      // Direction order (cube coords):
      // 0: NW, 1: NE, 2: E, 3: SE, 4: SW, 5: W
      static const Qrc DIRS[6] = {
        Qrc{  0, +1, -1 }, // NW
        Qrc{ +1,  0, -1 }, // NE
        Qrc{ +1, -1,  0 }, // E
        Qrc{  0, -1, +1 }, // SE
        Qrc{ -1,  0, +1 }, // SW
        Qrc{ -1, +1,  0 }  // W
      };

      auto isValidQrc = [this](const Qrc& qrc) -> bool {
        return index.idFromCube(qrc) != INVALID_TILE;
        };

      // For each ring r:
      //   slots in this ring = 6 * r
      //   cumulative slots before this ring = 6 * sum_{k=1}^{r-1} k = 3*(r-1)*r
      // Within ring r:
      //   localIndex = s * r + t, s = segment [0..5], t = [0..r-1]
      for (int r = 1; r <= radius; ++r)
      {
        const int base = 3 * (r - 1) * r; // starting index for ring r

        for (int s = 0; s < 6; ++s)
        {
          const int next = (s + 1) % 6;

          for (int t = 0; t < r; ++t)
          {
            const int localIndex = s * r + t;   // 0..(6*r-1)
            const int globalIndex = base + localIndex; // 0..count-1

            // Safety guard; with correct math this should always be true
            if (globalIndex < 0 || globalIndex >= count)
              continue;

            Qrc pos = center + DIRS[s] * r + DIRS[next] * t;

            if (isValidQrc(pos))
              result[globalIndex] = pos; // otherwise stays INVALID_QRC
          }
        }
      }

      return result;
    }

    // ---------- IWorld (mutators) ----------

    void BuildRoad(const Qrc& qrc) override
    {
      TileId id = idFromCube(qrc);
      if (id == INVALID_TILE || id >= improvements.size()) return;
      improvements[id] |= IMP_ROAD;
      // TODO: emit TileDelta / event for renderer
    }

    void RemoveRoad(const Qrc& qrc) override
    {
      TileId id = idFromCube(qrc);
      if (id == INVALID_TILE || id >= improvements.size()) return;
      improvements[id] &= ~IMP_ROAD;
    }

    void BuildFarm(const Qrc& qrc) override
    {
      TileId id = idFromCube(qrc);
      if (id == INVALID_TILE || id >= improvements.size()) return;
      improvements[id] |= IMP_FARM;
    }

    void RemoveFarm(const Qrc& qrc) override
    {
      TileId id = idFromCube(qrc);
      if (id == INVALID_TILE || id >= improvements.size()) return;
      improvements[id] &= ~IMP_FARM;
    }

    void BuildLumbermill(const Qrc& qrc) override
    {
      TileId id = idFromCube(qrc);
      if (id == INVALID_TILE || id >= improvements.size()) return;
      improvements[id] |= IMP_LUMBERMILL;
    }

    void RemoveLumbermill(const Qrc& qrc) override
    {
      TileId id = idFromCube(qrc);
      if (id == INVALID_TILE || id >= improvements.size()) return;
      improvements[id] &= ~IMP_LUMBERMILL;
    }

    void PillageTile(const Qrc& qrc) override
    {
      TileId id = idFromCube(qrc);
      if (id == INVALID_TILE || id >= improvements.size()) return;
      improvements[id] = IMP_NONE;
      // later: emit event, maybe spawn “ruined farm” visual, etc.
    }

    // Optional: keep your old TryGetTile(cx,cy,cz) signature if anything still uses it,
    // but internally route it through GridIndex instead of a linear search.
  };

  // Keep the free helpers, now thin wrappers on GridIndex:

  inline Tile* tryGetTileAtCube(WorldSnapshot& w, const glm::ivec3& c) {
    TileId id = w.index.idFromCube(c);
    if (id == INVALID_TILE || id >= w.tiles.size()) return nullptr;
    return &w.tiles[id];
  }

  inline const Tile* tryGetTileAtCube(const WorldSnapshot& w, const glm::ivec3& c) {
    TileId id = w.index.idFromCube(c);
    if (id == INVALID_TILE || id >= w.tiles.size()) return nullptr;
    return &w.tiles[id];
  }

  // GeneratorParams tuned for: pangaea, sea ring, Europe-like latitudinal climate, 3–4 rivers
  //---------------------------------------------------------------------------------------------
  struct GeneratorParams
  {
    // Grid
    int   width = 96;
    int   height = 64;
    bool  wrapX = true;

    // Land/sea
    float seaLevel = 0.45f; // lower sea -> more land
    float pangaeaRadius = 0.86f; // radial land mask fraction of half-diagonal

    // Elevation noise
    int   elevOctaves = 5;
    float elevFreq = 0.0065f;
    float elevGain = 0.53f;

    // Ridges/relief
    int   ridgeOctaves = 4;
    float ridgeFreq = 0.015f;
    float ridgeGain = 0.52f;
    float mountainElev = 0.70f; // height threshold before ridge gate
    float ridgeThresh = 0.60f; // ridgedness gate
    float hillSlopeThresh = 0.07f; // slope gate

    // Moisture propagation
    int   moistureBfsMax = 42;
    float moistureJitter = 0.08f;

    // Rivers (tile rivers; navigable)
    int   riverMinLen = 20;
    int   riverTargetCount = 6;   // 3–4
    int   riverMaxWalk = 220;
    float riverNorthBand = 0.35f; // sources from top ~35% rows
    float riverSouthBias = 0.02f; // tiny per-step southward bias

    // Climate bands (north→south)
    float bandTundraN = 0.12f; // top 12% -> Tundra
    float bandTaigaN = 0.30f; // next to 28% -> Taiga
    float bandMediterraneanS = 0.20f; // bottom 18% eligible for Med Scrub
    float bandDesertS = 0.15f; // bottom 12% eligible for Desert (with low moisture)

    // Climate moisture thresholds
    float steppeMoistureMax = 0.55f; // below => Steppe in mid-lats
    float swampMoistureMin = 0.65f; // above + lowland => Swamp
    float swampElevMax = seaLevel + 0.15f; // lowland cap for swamp

    // Slight minimum dryness so we don't get wet deserts
    const float minDryForDesert = 0.15f;

    // Initial forest cover by climate (0..255)
    std::uint8_t forestGrassland = 80;
    std::uint8_t forestTaiga = 170;
    std::uint8_t forestMedScrub = 40;
    std::uint8_t forestSteppe = 20;
    std::uint8_t forestSwamp = 120;
    std::uint8_t forestTundra = 15;
    std::uint8_t forestDesert = 5;

    // Resource spacing (kept for future; placement logic can be updated later)
    int   resMinDist = 3;
  };

  //---------------------------------------------------------------------------
  class DLL_GAME_ASSETS Generator
  {
  public:
    WorldSnapshot Generate(std::uint64_t seed, const GeneratorParams & p);
  };

} // namespace logic
