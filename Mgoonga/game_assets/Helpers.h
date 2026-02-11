#pragma once
#include <algorithm>
#include <cstdint>
#include <cmath>

#include "Tile.h"

namespace logic
{

  //-------------------------------
  // Small utilities
  //-------------------------------
  template <class T> inline T clamp(T v, T lo, T hi) { return std::max(lo, std::min(v, hi)); }

  // popcount fallback for 8-bit masks (no C++20 dependency)
  inline int popcount8(std::uint8_t x)
  {
    x = x - ((x >> 1) & 0x55u);
    x = (x & 0x33u) + ((x >> 2) & 0x33u);
    return int((((x + (x >> 4)) & 0x0Fu) * 0x01u));
  }

  //-------------------------------
  // Forest tiers (for quick thresholds)
  //-------------------------------
  enum class ForestTier : std::uint8_t { None, Sparse, Mature, OldGrowth };

  inline ForestTier forestTier(std::uint8_t density)
  {
    if (density == 0) return ForestTier::None;
    if (density <= 85)  return ForestTier::Sparse;     // ~0..0.33
    if (density <= 170) return ForestTier::Mature;     // ~0.33..0.66
    return ForestTier::OldGrowth;                       // ~0.66..1.0
  }

  inline bool hasForest(const Tile& t) { return t.forestDensity > 0; }

  //-------------------------------
  // Water & rivers
  //-------------------------------
  inline bool isCoastWater(const Tile& t) { return t.water == WaterBody::Coast; }
  inline bool isLake(const Tile& t) { return t.water == WaterBody::Lake; }
  inline bool isOpenSea(const Tile& t) { return t.water == WaterBody::OpenSea; }

  inline bool isCoastLand(const Tile& t) { return (static_cast<std::uint16_t>(t.tags) & static_cast<std::uint16_t>(TileTag::Coast)) != 0; }
  inline bool isCoast(const Tile& t) { return isCoastWater(t) || isCoastLand(t); }

  inline bool hasFreshwaterAccess(const Tile& t) {
    return t.riversMask != 0 || t.water == WaterBody::Lake || t.water == WaterBody::RiverEdge;
  }

  inline bool riverOnEdge(const Tile& t, int edge /*0..5*/) {
    const std::uint8_t bit = 1u << (edge & 5);
    return (t.riversMask & bit) != 0;
  }
  inline int riverDegree(const Tile& t) { return popcount8(t.riversMask & 0x3Fu); }

  //-------------------------------
  // Climate & elevation helpers
  //-------------------------------
  // Map normalized height (0..1) to an approximate physical altitude in meters.
  // Parameters let you tune sea level and land/sea scale without recompiling formulas.
  inline float altitudeMeters(const Tile& t,
    float seaLevel01 = 0.35f,
    float landRangeM = 3500.f,
    float seaRangeM = 2000.f) {
    const float h = clamp(t.height, 0.0f, 1.0f);
    if (h < seaLevel01) {
      const float x = (seaLevel01 - h) / std::max(1e-6f, seaLevel01);
      return -x * seaRangeM; // depth
    }
    const float x = (h - seaLevel01) / std::max(1e-6f, (1.0f - seaLevel01));
    return x * landRangeM;   // altitude
  }

  // Simple temperature “band” (0=cold .. 2=hot) from normalized temperature
  // You can swap this for a biome-consistent lapse-rate function later.
  inline int tempBand(const Tile& t) {
    if (t.temperature < 0.33f) return 0;
    if (t.temperature < 0.66f) return 1;
    return 2;
  }

  //-------------------------------
  // Potentials (normalized 0..1) — not yields, just site quality signals.
  // Keep these light; your real yield formulas can multiply/modify these.
  //-------------------------------

  // Agriculture potential (how good for food/grain/grapes/etc. *before* laws/buildings).
  inline float agriPotential(const Tile& t) {
    if (t.vBiome == VerticalBiome::Water) return 0.0f;

    // Base by climate
    float base = 0.0f;
    switch (t.climate) {
    case Climate::Grassland:          base = 0.75f; break;
    case Climate::Steppe:             base = 0.55f; break;
    case Climate::MediterraneanScrub: base = 0.60f; break;
    case Climate::Taiga:              base = 0.50f; break;
    case Climate::Swamp:              base = 0.65f; break;
    case Climate::Tundra:             base = 0.30f; break;
    case Climate::Desert:             base = 0.10f; break;
    }

    // Vertical penalty
    float v = 1.0f;
    switch (t.vBiome) {
    case VerticalBiome::Plains:    v = 1.00f; break;
    case VerticalBiome::Hills:     v = 0.85f; break;
    case VerticalBiome::Mountains: v = 0.50f; break;
    case VerticalBiome::Water:     v = 0.00f; break;
    }

    // Freshwater bump
    float fw = hasFreshwaterAccess(t) ? 0.15f : 0.0f;

    // Forest penalty for agriculture (until cleared)
    float fpen = 1.0f;
    switch (forestTier(t.forestDensity)) {
    case ForestTier::None:      fpen = 1.00f; break;
    case ForestTier::Sparse:    fpen = 0.90f; break;
    case ForestTier::Mature:    fpen = 0.80f; break;
    case ForestTier::OldGrowth: fpen = 0.70f; break;
    }

    return clamp((base * v * fpen) + fw, 0.0f, 1.0f);
  }

  // Timber potential (how good for sustained wood production).
  inline float timberPotential(const Tile& t) {
    if (t.vBiome == VerticalBiome::Water) return 0.0f;

    float base = 0.0f;
    switch (t.climate) {
    case Climate::Taiga:              base = 0.90f; break;
    case Climate::Grassland:          base = 0.60f; break;
    case Climate::MediterraneanScrub: base = 0.55f; break;
    case Climate::Swamp:              base = 0.65f; break;
    case Climate::Steppe:             base = 0.40f; break;
    case Climate::Tundra:             base = 0.35f; break;
    case Climate::Desert:             base = 0.05f; break;
    }

    float v = 1.0f;
    switch (t.vBiome) {
    case VerticalBiome::Plains:    v = 1.00f; break;
    case VerticalBiome::Hills:     v = 0.95f; break;
    case VerticalBiome::Mountains: v = 0.80f; break;
    case VerticalBiome::Water:     v = 0.00f; break;
    }

    // Forest *boost* scaled by density (0..1)
    const float fd = t.forestDensity / 255.0f;
    const float fboost = 0.25f * fd; // gentle; laws/buildings can amplify later

    return clamp(base * v + fboost, 0.0f, 1.0f);
  }

  // Pasture potential (horses/cattle/wool) — rough graze quality signal.
  inline float pasturePotential(const Tile& t) {
    if (t.vBiome == VerticalBiome::Water) return 0.0f;

    float base = 0.0f;
    switch (t.climate) {
    case Climate::Grassland:          base = 0.80f; break;
    case Climate::Steppe:             base = 0.75f; break;
    case Climate::MediterraneanScrub: base = 0.65f; break;
    case Climate::Taiga:              base = 0.45f; break;
    case Climate::Swamp:              base = 0.35f; break;
    case Climate::Tundra:             base = 0.30f; break;
    case Climate::Desert:             base = 0.15f; break;
    }

    float v = 1.0f;
    switch (t.vBiome) {
    case VerticalBiome::Plains:    v = 1.00f; break;
    case VerticalBiome::Hills:     v = 0.90f; break;
    case VerticalBiome::Mountains: v = 0.60f; break;
    case VerticalBiome::Water:     v = 0.00f; break;
    }

    // Forest slightly hurts grazing unless very sparse
    float fpen = 1.0f;
    switch (forestTier(t.forestDensity)) {
    case ForestTier::None:      fpen = 1.00f; break;
    case ForestTier::Sparse:    fpen = 0.95f; break;
    case ForestTier::Mature:    fpen = 0.85f; break;
    case ForestTier::OldGrowth: fpen = 0.75f; break;
    }

    // Freshwater helps herds a bit
    const float fw = hasFreshwaterAccess(t) ? 0.05f : 0.0f;

    return clamp(base * v * fpen + fw, 0.0f, 1.0f);
  }

  // Mining potential (ore/stone/marble/gold) — purely geomorph + climate friction.
  inline float miningPotential(const Tile& t) {
    if (t.vBiome == VerticalBiome::Water) return 0.0f;

    float v = 0.0f;
    switch (t.vBiome) {
    case VerticalBiome::Plains:    v = 0.35f; break;
    case VerticalBiome::Hills:     v = 0.70f; break;
    case VerticalBiome::Mountains: v = 1.00f; break;
    case VerticalBiome::Water:     v = 0.00f; break;
    }

    // Climate influences logistics friction (desert/tundra/swamp are harder)
    float c = 1.0f;
    switch (t.climate) {
    case Climate::Grassland:          c = 1.00f; break;
    case Climate::MediterraneanScrub: c = 0.95f; break;
    case Climate::Steppe:             c = 0.95f; break;
    case Climate::Taiga:              c = 0.90f; break;
    case Climate::Tundra:             c = 0.85f; break;
    case Climate::Swamp:              c = 0.80f; break;
    case Climate::Desert:             c = 0.80f; break;
    }

    return clamp(v * c, 0.0f, 1.0f);
  }

  //-------------------------------
  // Specials & resource helpers
  //-------------------------------
  inline bool hasSpecial(const Tile& t, SpecialIcon s) {
    for (auto sc : t.specials) if (sc == s) return true;
    return false;
  }
  inline bool hasResource(const Tile& t, Resource r) {
    for (auto rc : t.resources) if (rc == r) return true;
    return false;
  }
  inline int resourceCount(const Tile& t) {
    int n = 0; for (auto rc : t.resources) if (rc != Resource::None) ++n; return n;
  }
  inline int specialCount(const Tile& t) {
    int n = 0; for (auto sc : t.specials) if (sc != SpecialIcon::None) ++n; return n;
  }

  //-------------------------------
  // Movement & defense baselines (tunable)
  //-------------------------------
  // Return a cheap baseline move cost multiplier (1.0 = flat land roadless).
  // Your pathfinder can multiply by road/river/law/tech modifiers.
  inline float baseMoveCost(const Tile& t) {
    if (t.vBiome == VerticalBiome::Water) return 1.0f; // ship rules elsewhere
    float m = 1.0f;
    switch (t.vBiome) {
    case VerticalBiome::Plains:    m = 1.00f; break;
    case VerticalBiome::Hills:     m = 1.25f; break;
    case VerticalBiome::Mountains: m = 2.00f; break;
    case VerticalBiome::Water:     m = 1.00f; break;
    }
    // Swamps slow, deserts mildly slow, forests add friction
    switch (t.climate) {
    case Climate::Swamp:  m *= 1.35f; break;
    case Climate::Desert: m *= 1.15f; break;
    default: break;
    }
    switch (forestTier(t.forestDensity)) {
    case ForestTier::Sparse:    m *= 1.10f; break;
    case ForestTier::Mature:    m *= 1.20f; break;
    case ForestTier::OldGrowth: m *= 1.35f; break;
    default: break;
    }
    return m;
  }

  // Cheap baseline defense modifier (e.g., 1.0 = no bonus; >1 = defensive bonus).
  inline float baseDefenseMod(const Tile& t) {
    if (t.vBiome == VerticalBiome::Water) return 1.0f;
    float d = 1.0f;
    switch (t.vBiome) {
    case VerticalBiome::Plains:    d = 1.00f; break;
    case VerticalBiome::Hills:     d = 1.15f; break;
    case VerticalBiome::Mountains: d = 1.30f; break;
    case VerticalBiome::Water:     d = 1.00f; break;
    }
    // Forest grants cover; swamp modestly helps; desert no cover
    switch (forestTier(t.forestDensity)) {
    case ForestTier::Sparse:    d *= 1.05f; break;
    case ForestTier::Mature:    d *= 1.10f; break;
    case ForestTier::OldGrowth: d *= 1.15f; break;
    default: break;
    }
    if (t.climate == Climate::Swamp) d *= 1.05f;
    return d;
  }

  // -----------------------------
// Convenience helpers
// -----------------------------

//inline int resourceCount(const Tile& t)
//{
//  int n = 0;
//  for (auto r : t.resources)
//    if (r != Resource::None) ++n;
//  return n;
//}

//inline int specialCount(const Tile& t)
//{
//  int n = 0;
//  for (auto s : t.specials)
//    if (s != SpecialIcon::None) ++n;
//  return n;
//}

//inline bool hasResource(const Tile& t, Resource r)
//{
//  for (auto rc : t.resources)
//    if (rc == r) return true;
//  return false;
//}

//inline bool addResourceIfFree(Tile& t, Resource r)
//{
//  if (r == Resource::None) return false;
//  if (hasResource(t, r)) return false;

//  for (auto& slot : t.resources)
//  {
//    if (slot == Resource::None)
//    {
//      slot = r;
//      return true;
//    }
//  }
//  return false; // no free slot
//}

//inline bool addIconIfFree(Tile& t, SpecialIcon s)
//{
//  if (s == SpecialIcon::None) return false;
//  for (auto& slot : t.specials)
//  {
//    if (slot == SpecialIcon::None)
//    {
//      slot = s;
//      return true;
//    }
//    if (slot == s) return false; // already has it
//  }
//  return false;
//}

//// "Special main resources" = those from pass 2 (Stone, Marble, Salt, Horses, Ore).
//inline bool hasSpecialMainResource(const Tile& t)
//{
//  for (auto r : t.resources)
//  {
//    switch (r)
//    {
//    case Resource::Stone:
//    case Resource::Gold:   // if you later treat Gold as a special, keep here
//    case Resource::Salt:
//    case Resource::Horses:
//    case Resource::Ore:
//    case Resource::Marble:
//      return true;
//    default:
//      break;
//    }
//  }
//  return false;
//}

//inline bool isMountain(const Tile& t)
//{
//  return t.vBiome == VerticalBiome::Mountains;
//}

//inline bool isHill(const Tile& t)
//{
//  return t.vBiome == VerticalBiome::Hills;
//}

//inline bool isPlains(const Tile& t)
//{
//  return t.vBiome == VerticalBiome::Plains;
//}

//inline bool isDesert(const Tile& t)
//{
//  return t.climate == Climate::Desert;
//}

//inline bool isTundra(const Tile& t)
//{
//  return t.climate == Climate::Tundra;
//}

//inline bool isMed(const Tile& t)
//{
//  return t.climate == Climate::MediterraneanScrub;
//}

//inline bool isSteppe(const Tile& t)
//{
//  return t.climate == Climate::Steppe;
//}

//inline bool isGrassland(const Tile& t)
//{
//  return t.climate == Climate::Grassland;
//}

//inline bool isTaiga(const Tile& t)
//{
//  return t.climate == Climate::Taiga;
//}

//inline bool isSwamp(const Tile& t)
//{
//  return t.climate == Climate::Swamp;
//}

//inline bool nearMountains(const WorldSnapshot& w, const GeneratorParams& p, TileId id)
//{
//  auto N = neighbors(w, id, p);
//  for (auto nid : N)
//  {
//    if (nid == TileId(-1)) continue;
//    if (isMountain(w.tiles[nid])) return true;
//  }
//  return false;
//}

//inline bool nearSeaOrLake(const WorldSnapshot& w, const GeneratorParams& p, TileId id)
//{
//  auto N = neighbors(w, id, p);
//  for (auto nid : N)
//  {
//    if (nid == TileId(-1)) continue;
//    const auto& n = w.tiles[nid];
//    if (n.vBiome == VerticalBiome::Water &&
//      (n.water == WaterBody::OpenSea || n.water == WaterBody::Lake || n.water == WaterBody::Coast))
//      return true;
//  }
//  return false;
//}

//// Timber yield helper you’ll use on the Sim side.
//inline int timberYieldFromForest(const Tile& t)
//{
//  if (t.forestDensity == 0) return 0;
//  float d = t.forestDensity / 255.0f;
//  if (d < 0.33f)  return 2;
//  if (d < 0.66f)  return 3;
//  return 4;
//}

// -----------------------------
// Climate ? base lux mapping
// (You can tweak this table to match the exact mapping you locked)
// -----------------------------
  struct ClimateResourcePair
  {
    Resource plains;
    Resource hills;
  };

  // Index by static_cast<int>(Climate)
  static const ClimateResourcePair kClimateBaseRes[] =
  {
    // Grassland
    { Resource::Barley, Resource::Grapes },

    // Steppe
    { Resource::Wool, Resource::Cotton },

    // MediterraneanScrub (locked: Citrus / Olives)
    { Resource::Citrus, Resource::Olives },

    // Taiga
    { Resource::Cattle, Resource::Gold },

    // Swamp
    { Resource::Clay, Resource::Amber },

    // Tundra  (handled specially in 1.2 ? Fur)
    { Resource::None, Resource::None },

    // Desert  (hills ? SodaAsh, plains none)
    { Resource::None, Resource::SodaAsh }
  };

  static_assert(sizeof(kClimateBaseRes) / sizeof(kClimateBaseRes[0]) == 7,
    "Update kClimateBaseRes if Climate enum changes.");

} // namespace logic::tilemath

