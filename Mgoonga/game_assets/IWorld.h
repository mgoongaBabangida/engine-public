// IWorld.h
#pragma once
#include <vector>
#include <glm/glm/glm.hpp>
#include "Tile.h"
#include "GridIndex.h"

namespace logic
{
  using Qrc = glm::ivec3; // readability

  // Sentinel for "no tile here"
  inline constexpr Qrc INVALID_QRC{
    std::numeric_limits<int>::min(),
    std::numeric_limits<int>::min(),
    std::numeric_limits<int>::min()
  };

  //---------------------------------------------------------
  struct IWorldInfo
  {
    virtual ~IWorldInfo() = default;

    // Basic meta
    virtual glm::ivec2 GetSize() const = 0;          // (width, height) in axial grid
    virtual bool       IsValid(const Qrc& qrc) const = 0;

    // Core access; clients can still poke Tile directly if they really need detail.
    virtual const Tile* TryGetTile(const Qrc& qrc) const = 0;

    // Convenience “semantic” queries
    virtual bool IsLand(const Qrc& qrc) const = 0;
    virtual bool IsWater(const Qrc& qrc) const = 0;
    virtual bool IsHill(const Qrc& qrc) const = 0;
    virtual bool IsMountain(const Qrc& qrc) const = 0;
    virtual bool IsTundra(const Qrc& qrc) const = 0;
    virtual bool HasForest(const Qrc& qrc) const = 0;

    // Improvements / ownership (terrain + economy/ownership layer)
    virtual bool HasRoad(const Qrc& qrc) const = 0;
    virtual bool HasFarm(const Qrc& qrc) const = 0;
    virtual bool HasLumbermill(const Qrc& qrc) const = 0;
    virtual bool DoesBelongToCity(const Qrc& qrc) const = 0;

    // Yields & resources from terrain alone (no laws/buildings modifiers)
    virtual std::vector<Resource> GetBaseYields(const Qrc& qrc) const = 0;

    // Neighbours: radius 1 and 2 are the main use-cases
    virtual std::vector<Qrc> GetNeighbors(const Qrc& center, int radius = 1) const = 0;
  };

  //-----------------------------------------------------
  struct IWorld : public IWorldInfo
  {
    // Mutators. Only EconomySystem (and maybe some future WorldSystem) should get this.
    virtual void BuildRoad(const Qrc& qrc) = 0;
    virtual void RemoveRoad(const Qrc& qrc) = 0;
    virtual void BuildFarm(const Qrc& qrc) = 0;
    virtual void RemoveFarm(const Qrc& qrc) = 0;
    virtual void BuildLumbermill(const Qrc& qrc) = 0;
    virtual void RemoveLumbermill(const Qrc& qrc) = 0;

    virtual void PillageTile(const Qrc& qrc) = 0;   // destroy all improvements
    // Later: set city ownership, fortifications, etc.
  };

} // namespace logic

