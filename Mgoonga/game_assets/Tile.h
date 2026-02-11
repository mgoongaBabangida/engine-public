#pragma once
#include <array>
#include <cstdint>

namespace logic
{
  //------------------ Core IDs ------------------
  using TileId = std::uint32_t;

  //------------------ Map Constants -------------
  constexpr int kHexEdges = 6;
  constexpr std::size_t kMaxTileResources = 4;   // Colonization-style choice set
  constexpr std::size_t kMaxSpecialIcons = 2;  // Usually 0–1, but future-proof

  //------------------ Vertical + Horizontal space ------------------
  // Vertical “form factor” (height band)
  enum class VerticalBiome : std::uint8_t { Plains, Hills, Mountains, Water };

  // Horizontal “climate/cover” (independent of vertical)
  enum class Climate : std::uint8_t
  {
    Grassland,
    Steppe,
    MediterraneanScrub,
    Taiga,
    Swamp,
    Tundra,
    Desert
  };

  // Water body classification (tiles or adjacency context)
  enum class WaterBody : std::uint8_t
  {
    None,        // Land tile not on a water body
    RiverEdge,   // Land tile with river on one or more edges (see riversMask)
    Lake,        // Freshwater tile
    Coast,       // Saltwater adjacent to land
    OpenSea,      // Deep saltwater (non-coast)
    River
  };

  //------------------ Game resources ------------------
  // “Yield resources” the player chooses to extract (2–4 per tile).
  // Note: Food is a general bucket; Wheat/Fish are bonuses via SpecialIcon.
  enum class Resource : std::uint8_t
  {
    None = 0,
    Horses,
    Food,
    Grapes,
    Wool,
    Cotton,
    Fur,
    Citrus,
    SodaAsh,
    Olives,
    Amber,
    Gold,
    Barley,
    Clay,
    Marble,
    Cattle,
    Ore,
    Timber,
    Stone,
    Salt
  };

  // Special/bonus icons on tiles. These are NOT yields by themselves;
  // they grant +1 to a matching yield (or enable a special improvement).
  enum class SpecialIcon : std::uint8_t
  {
    None = 0,

    // Mirrors of yield resources (for +1 / upgrade unlocks)
    Horses, Food, Grapes, Wool, Silk, Fur, Citrus, SodaAsh, Olives,
    Amber, Gold, Barley, Clay, Marble, Cattle, Ore, Timber, Stone,

    // Extra specials
    Wheat,      // farmland +1 / special farm
    Fish,       // water-only +1 / fishery
    Charcoal,   // charcoal clamp / kiln site
    Dyes        // dye plants or dyestuffs site
  };

  //------------------ Tags/flags ------------------
  enum class TileTag : std::uint16_t
  {
    None = 0,
    Border = 1 << 0,   // world border / map wrap seam aid
    Coast = 1 << 1,   // land tile touching saltwater
    CapitalRing = 1 << 2,   // in capital radius (for rules/UI)
    Pass = 1 << 3,   // mountain pass (pathfinding/yield rules)
    Floodplain = 1 << 4,   // fertile river-adjacent land
    Harborable = 1 << 5    // valid harbor/port placement
  };

  inline TileTag operator|(TileTag a, TileTag b) {
    return static_cast<TileTag>(static_cast<std::uint16_t>(a) | static_cast<std::uint16_t>(b));
  }
  inline TileTag& operator|=(TileTag& a, TileTag b) { a = a | b; return a; }
  inline bool any(TileTag t) { return static_cast<std::uint16_t>(t) != 0; }

  //------------------ Tile ------------------
  struct Tile
  {
    // Identity & location
    TileId id{};
    int cx{}, cy{}, cz{};         // cube coords, cx + cy + cz == 0

    // Continuous generators (0..1)
    float height = 0.f;          // post-erosion normalized elevation
    float moisture = 0.f;          // precipitation/soil water
    float temperature = 0.f;       // climatology driver (lat/altitude)

    // Discrete classification
    VerticalBiome vBiome = VerticalBiome::Plains;
    Climate       climate = Climate::Grassland;
    WaterBody     water = WaterBody::None;

    // Forest overlay (dynamic): 0 = no forest; 1..255 density (thin→old-growth)
    std::uint8_t forestDensity = 0;

    // Rivers on hex edges (bitmask, 6 bits: 0..5 edges in clockwise order from +q)
    // Non-zero => tile “is river-adjacent” (freshwater access).
    std::uint8_t riversMask = 0;

    // Yield-choice set for this tile (2–4; unused slots = Resource::None)
    std::array<Resource, kMaxTileResources> resources{ {
      Resource::None, Resource::None, Resource::None, Resource::None
    } };

    // Optional tile specials (usually 0 or 1; extra slot reserved)
    std::array<SpecialIcon, kMaxSpecialIcons> specials{ {
      SpecialIcon::None, SpecialIcon::None
    } };

    // Regions & tags
    std::uint16_t regionId = 0;  // Voronoi or political region
    TileTag       tags = TileTag::None;

    // Convenience helpers (inline, no ABI cost)
    bool isLand()  const { return vBiome != VerticalBiome::Water && water != WaterBody::Lake && water != WaterBody::Coast && water != WaterBody::OpenSea; }
    bool isWater() const { return vBiome == VerticalBiome::Water || water == WaterBody::Lake || water == WaterBody::Coast || water == WaterBody::OpenSea; }
    bool nearRiver() const { return riversMask != 0 || water == WaterBody::RiverEdge; }
    bool hasForest() const { return forestDensity > 0; }
  };

} // namespace logic
