// Generator.cpp
#include "Generator.h"
#include "Helpers.h"

#include <glm/glm/glm.hpp>
#include <queue>
#include <algorithm>
#include <numeric>
#include <limits>
#include <cmath>

#include <math/Hex.h>

namespace logic
{
  // axial -> cube (we store cube)
  inline glm::ivec3 axialToCube(int q, int r) {
    return { q, -q - r, r };
  }

  // Neighbor fetcher (hex, FLAT-TOP axial dirs)
  // Axis convention:
  //   q = columns (east-west), r = diagonal rows
  // Directions (from center):
  //   0: top-left     (0, -1)
  //   1: top-right    (+1, -1)
  //   2: right        (+1, 0)
  //   3: right-down   (0, +1)
  //   4: left-down    (-1, +1)
  //   5: left         (-1, 0)
  inline std::array<TileId, 6> neighbors(const WorldSnapshot& w, TileId id, const GeneratorParams& p)
  {
    std::array<TileId, 6> out{}; out.fill(TileId(-1));
    const int W = p.width, H = p.height;
    int r = int(id) / W;
    int q = int(id) % W;

    static const int AQ[6] = { 0, +1, +1,  0, -1, -1 };
    static const int AR[6] = { -1, -1,  0, +1, +1,  0 };

    auto wrapQ = [&](int qx) {
      return p.wrapX ? ((qx % W + W) % W) : qx;
      };

    for (int d = 0; d < 6; ++d) {
      int nq = q + AQ[d];
      int nr = r + AR[d];

      if (p.wrapX) {
        if (nr < 0 || nr >= H) continue;
        nq = wrapQ(nq);
      }
      else {
        if (nq < 0 || nq >= W || nr < 0 || nr >= H) continue;
      }

      out[d] = TileId(nq + nr * W);
    }

    return out;
  }

  struct XorShift64
  {
    std::uint64_t s;
    explicit XorShift64(std::uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ULL) {}
    std::uint64_t next() {
      std::uint64_t x = s;
      x ^= x << 13; x ^= x >> 7; x ^= x << 17;
      return s = x;
    }
    float uniform01() { return (next() >> 11) * (1.0 / 9007199254740992.0); } // [0,1)
    int   range(int lo, int hi) { return lo + int(next() % std::uint64_t(hi - lo + 1)); }
  };

  inline std::uint32_t hash32(std::uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x;
  }
  inline float randHash(std::uint32_t x) {
    return (hash32(x) & 0x00FFFFFF) / float(0x01000000);
  }
  inline float smoothstep01(float t) { return t * t * (3.f - 2.f * t); }

  inline float valueNoise2D(int xi, int yi, std::uint32_t seed) {
    return randHash(seed ^ std::uint32_t(xi * 374761393u) ^ std::uint32_t(yi * 668265263u));
  }
  inline float valueNoise2D(float x, float y, std::uint32_t seed) {
    int xi = int(std::floor(x)), yi = int(std::floor(y));
    float tx = x - xi, ty = y - yi;
    float v00 = valueNoise2D(xi, yi, seed);
    float v10 = valueNoise2D(xi + 1, yi, seed);
    float v01 = valueNoise2D(xi, yi + 1, seed);
    float v11 = valueNoise2D(xi + 1, yi + 1, seed);
    tx = smoothstep01(tx); ty = smoothstep01(ty);
    float vx0 = v00 * (1 - tx) + v10 * tx;
    float vx1 = v01 * (1 - tx) + v11 * tx;
    return vx0 * (1 - ty) + vx1 * ty;
  }

  // classic fBm in [0,1]
  inline float fBm2D(float x, float y, std::uint32_t seed, int octaves, float freq, float gain) {
    float amp = 0.5f, sum = 0.f, norm = 0.f;
    float fx = freq, fy = freq;
    for (int i = 0; i < octaves; ++i) {
      sum += amp * valueNoise2D(x * fx, y * fy, seed + i * 1337u);
      norm += amp;
      amp *= gain; fx *= 2.f; fy *= 2.f;
    }
    return (norm > 0.f) ? (sum / norm) : 0.f;
  }

  // ridged variant: 1 - |2*n-1|
  inline float ridged2D(float x, float y, std::uint32_t seed, int octaves, float freq, float gain) {
    float n = fBm2D(x, y, seed, octaves, freq, gain); // 0..1
    return 1.f - std::fabs(2.f * n - 1.f);            // 0..1
  }

  // ---------- build grid (unchanged except zeroing new fields if needed) ----------
//-------------------------------------------------------------------------
  static WorldSnapshot buildGrid(std::uint64_t seed, const GeneratorParams& p)
  {
    WorldSnapshot w;

    GridIndex gi;
    gi.width = p.width;
    gi.height = p.height;
    gi.wrapX = p.wrapX;
    gi.q0 = -(p.width / 2);
    gi.r0 = -(p.height / 2);

    w.InitCore(seed, p.width, p.height, gi);

    const int q0 = gi.q0;
    const int r0 = gi.r0;

    for (int r = 0; r < p.height; ++r) {
      for (int q = 0; q < p.width; ++q) {
        TileId id = TileId(q + r * p.width);
        Tile& t = w.tiles[id];
        t.id = id;
        auto c = axialToCube(q0 + q, r0 + r);
        t.cx = c.x; t.cy = c.y; t.cz = c.z;
      }
    }
    return w;
  }

  // ---------- elevation with pangaea-style shape + rectangular sea belt ----------
  static void genElevation(WorldSnapshot& w, const GeneratorParams& p)
  {
    const std::uint32_t sElevLow = std::uint32_t(w.seed ^ 0xA5A5A5A5u);
    const std::uint32_t sElevHi = std::uint32_t(w.seed ^ 0x12345678u);

    const int W = p.width;
    const int H = p.height;

    // 1) Find circular radius in cube space (to keep continent-ish shape)
    float maxR = 0.0f;
    for (const auto& t : w.tiles)
    {
      float r = std::sqrt(float(t.cx * t.cx + t.cz * t.cz));
      if (r > maxR) maxR = r;
    }
    if (maxR < 1e-3f) maxR = 1e-3f;

    // How fat the pangaea is, 0.0..1.0 relative to extents
    const float panR = std::clamp(p.pangaeaRadius, 0.3f, 0.98f);

    // 2) First pass: raw heights + mild circular “pangaea” bias (unchanged idea)
    std::vector<float> raw(w.tiles.size(), 0.0f);
    float minH = +1e9f;
    float maxH = -1e9f;

    const float lowFreqMul = 0.4f; // big continents
    const float hiFreqMul = 2.0f; // micro detail
    const float edgeCircleStrength = 0.25f; // circular sink (weakened; belt handled later)

    std::size_t idx = 0;
    for (auto& t : w.tiles)
    {
      float x = float(t.cx) * p.elevFreq;
      float y = float(t.cz) * p.elevFreq;

      float low = fBm2D(x * lowFreqMul, y * lowFreqMul,
        sElevLow, p.elevOctaves, 1.0f, p.elevGain); // 0..1
      float hi = fBm2D(x * hiFreqMul, y * hiFreqMul,
        sElevHi, p.elevOctaves, 1.0f, p.elevGain); // 0..1

      float base = 0.7f * low + 0.3f * hi;
      base = std::pow(base, 1.1f); // slight lowland bias

      // Circular radial from cube coords (keeps pangaea-ish blob)
      float radialCircle = std::sqrt(float(t.cx * t.cx + t.cz * t.cz)) / maxR;
      radialCircle = std::clamp(radialCircle, 0.0f, 1.0f);

      float edgeCircle = 0.0f;
      if (radialCircle > panR)
      {
        edgeCircle = (radialCircle - panR) / std::max(1e-3f, 1.0f - panR);
        edgeCircle = std::clamp(edgeCircle, 0.0f, 1.0f);
        edgeCircle = smoothstep01(edgeCircle);
      }

      float h = base - edgeCircleStrength * edgeCircle;

      raw[idx++] = h;
      if (h < minH) minH = h;
      if (h > maxH) maxH = h;
    }

    // 3) Normalize to [0,1]
    float denom = (maxH - minH);
    if (denom < 1e-5f) denom = 1e-5f;

    // 4) Rectangular “distance to edge” belt using (q,r) grid indices
    //
    //   centerQ, centerR = map center in grid space
    //   dx, dy in [0..1] measure how far we are from center along W,H
    //   radialRectGrid = max(dx,dy) in [0..1]: 0 center, 1 at ANY edge
    //
    // If radialRectGrid > panR ⇒ we start pushing down toward sea.
    float centerQ = (W - 1) * 0.5f;
    float centerR = (H - 1) * 0.5f;
    float halfW = std::max(centerQ, 1.0f);
    float halfH = std::max(centerR, 1.0f);

    const float postEdgeSeaPush = 0.60f; // stronger belt: raise/lower for more/less ocean

    idx = 0;
    for (auto& t : w.tiles)
    {
      // normalize raw height
      float hNorm = (raw[idx] - minH) / denom;
      hNorm = std::clamp(hNorm, 0.0f, 1.0f);

      // grid coordinates from index
      int q = int(idx % W);
      int r = int(idx / W);

      float dx = std::abs(float(q) - centerQ) / halfW; // 0 center, 1 left/right edge
      float dy = std::abs(float(r) - centerR) / halfH; // 0 center, 1 top/bottom edge

      float radialRectGrid = std::clamp(std::max(dx, dy), 0.0f, 1.0f);

      float edgeRect = 0.0f;
      if (radialRectGrid > panR)
      {
        edgeRect = (radialRectGrid - panR) / std::max(1e-3f, 1.0f - panR);
        edgeRect = std::clamp(edgeRect, 0.0f, 1.0f);
        edgeRect = smoothstep01(edgeRect);
      }

      // This guarantees a symmetric depression at ALL borders: left, right, top, bottom
      float hFinal = hNorm - postEdgeSeaPush * edgeRect;
      t.height = std::clamp(hFinal, 0.0f, 1.0f);

      ++idx;
    }
  }

 // ---------- force a few small inland lakes (post sea-tagging) ----------
  static void carveSmallLakes(WorldSnapshot& w, const GeneratorParams& p)
  {
    const int W = p.width;
    const int H = p.height;

    // Tunables – adjust to taste
    const int   targetLakeCount = 4;          // how many separate lakes we want
    const int   maxLakeSize = 10;          // max tiles per lake
    const int   edgeMargin = 2;          // stay away from hard edges

    const int   strictMinSeaDist = 8;          // "nice" inland condition
    const int   looseMinSeaDist = 3;          // fallback inland condition

    const float strictMaxLakeHeight = p.seaLevel + 0.20f;
    const float looseMaxLakeHeight = 1.0f;       // fallback: anywhere above sea

    // Min distance between lake *centers* in hex steps
    const int   minCenterHexDist = 20;

    XorShift64 rng(w.seed ^ 0x4C4B355u);

    auto inBounds = [&](int i) { return (i >= 0 && i < W * H); };

    // Hex distance using cube coords already stored in tiles
    auto hexDist = [&](int a, int b) {
      const Tile& A = w.tiles[a];
      const Tile& B = w.tiles[b];
      int dx = A.cx - B.cx;
      int dy = A.cy - B.cy;
      int dz = A.cz - B.cz;
      return (std::abs(dx) + std::abs(dy) + std::abs(dz)) / 2;
      };

    // 1) BFS distance from OPEN SEA (saltwater)
    std::vector<int> dist(w.tiles.size(), INT_MAX);
    std::queue<int> q;

    for (int id = 0; id < (int)w.tiles.size(); ++id)
    {
      const Tile& t = w.tiles[id];
      if (t.vBiome == VerticalBiome::Water && t.water == WaterBody::OpenSea)
      {
        dist[id] = 0;
        q.push(id);
      }
    }

    while (!q.empty())
    {
      int v = q.front(); q.pop();
      auto N = neighbors(w, TileId(v), p);
      for (TileId nid : N)
      {
        if (nid == TileId(-1)) continue;
        int idx = (int)nid;
        if (!inBounds(idx)) continue;
        if (dist[idx] > dist[v] + 1)
        {
          dist[idx] = dist[v] + 1;
          q.push(idx);
        }
      }
    }

    auto isNearOpenSea = [&](int id) {
      const auto& t = w.tiles[id];
      if (t.vBiome == VerticalBiome::Water && t.water == WaterBody::OpenSea)
        return true;
      auto N = neighbors(w, TileId(id), p);
      for (auto nid : N)
      {
        if (nid == TileId(-1)) continue;
        const auto& nt = w.tiles[(int)nid];
        if (nt.vBiome == VerticalBiome::Water && nt.water == WaterBody::OpenSea)
          return true;
      }
      return false;
      };

    // 2) Helper: build candidate list (strict vs loose mode)
    auto buildCandidates = [&](bool loose) {
      std::vector<int> candidates;
      candidates.reserve(w.tiles.size());

      const int   minSeaDist = loose ? looseMinSeaDist : strictMinSeaDist;
      const float maxLakeH = loose ? looseMaxLakeHeight : strictMaxLakeHeight;

      for (int r = 0; r < H; ++r)
      {
        for (int qx = 0; qx < W; ++qx)
        {
          int id = qx + r * W;
          const Tile& t = w.tiles[id];

          // only non-water tiles as lake *candidates*
          if (t.vBiome == VerticalBiome::Water) continue;

          // avoid very outer rows
          if (r < edgeMargin || r >= H - edgeMargin) continue;
          // avoid outer columns if not wrapping in X
          if (!p.wrapX && (qx < edgeMargin || qx >= W - edgeMargin)) continue;

          // must be above sea level (real land)
          if (t.height < p.seaLevel) continue;
          // not too high for a lake
          if (t.height > maxLakeH) continue;

          // must be somewhat inland
          if (dist[id] < minSeaDist) continue;

          // in loose mode also reject tiles that directly touch open sea
          if (loose && isNearOpenSea(id)) continue;

          // don't put lakes on top of vertical mountains (once classified later)
          if (t.vBiome == VerticalBiome::Mountains) continue;

          candidates.push_back(id);
        }
      }

      // prefer lower spots (basins)
      std::sort(candidates.begin(), candidates.end(),
        [&](int a, int b) { return w.tiles[a].height < w.tiles[b].height; });
      return candidates;
      };

    auto candidates = buildCandidates(false);
    if (candidates.empty())
      candidates = buildCandidates(true);

    if (candidates.empty())
      return; // nothing to do

    std::vector<std::uint8_t> used(w.tiles.size(), 0);
    std::vector<int>          lakeCenters;
    int                       lakesMade = 0;

    for (int ci = 0; ci < (int)candidates.size() && lakesMade < targetLakeCount; ++ci)
    {
      int centerId = candidates[ci];
      if (used[centerId]) continue;

      // Enforce min distance from previous lake centers
      bool tooClose = false;
      for (int c : lakeCenters)
      {
        if (hexDist(c, centerId) < minCenterHexDist)
        {
          tooClose = true;
          break;
        }
      }
      if (tooClose) continue;

      // Grow small patch from this center
      std::vector<int> patch;
      patch.reserve(maxLakeSize);
      patch.push_back(centerId);
      used[centerId] = 1;

      std::size_t head = 0;
      while (head < patch.size() && (int)patch.size() < maxLakeSize)
      {
        int cur = patch[head++];
        auto N = neighbors(w, TileId(cur), p);

        for (TileId nid : N)
        {
          if (nid == TileId(-1)) continue;
          int idx = (int)nid;
          if (used[idx]) continue;

          Tile& nt = w.tiles[idx];

          // don't grow into existing water
          if (nt.vBiome == VerticalBiome::Water) continue;
          if (nt.height < p.seaLevel) continue;

          // light randomness so shapes are irregular
          if (rng.uniform01() < 0.5f)
          {
            used[idx] = 1;
            patch.push_back(idx);
            if ((int)patch.size() >= maxLakeSize)
              break;
          }
        }
      }

      if (patch.empty())
        continue;

      for (int id : patch)
      {
        Tile& t = w.tiles[id];
        t.vBiome = VerticalBiome::Water;
        t.water = WaterBody::Lake;
        t.forestDensity = 0;
        t.riversMask = 0;
      }

      lakeCenters.push_back(centerId);
      ++lakesMade;
    }
  }

  // ---------- classify seas vs lakes (water tiles), set WaterBody + vBiome ----------
  static void tagSeasAndLakes(WorldSnapshot& w, const GeneratorParams& p)
  {
    const int W = p.width, H = p.height;
    auto inBounds = [&](int i) { return (i >= 0 && i < W* H); };

    std::vector<std::uint8_t> isWater(w.tiles.size(), 0);
    for (auto& t : w.tiles) isWater[t.id] = (t.height < p.seaLevel) ? 1 : 0;

    // flood-fill from borders to mark "sea"; unmarked water becomes "lake"
    std::vector<std::uint8_t> sea(w.tiles.size(), 0);
    std::queue<int> q;

    // enqueue border water
    for (int r = 0; r < H; ++r) {
      for (int qx = 0; qx < W; ++qx) {
        int id = qx + r * W;
        bool border = (r == 0 || r == H - 1 || (!p.wrapX && (qx == 0 || qx == W - 1)));
        if (border && isWater[id]) { sea[id] = 1; q.push(id); }
      }
    }

    auto neigh = [&](int id) {
      std::array<int, 6> out; out.fill(-1);
      auto nbs = neighbors(w, TileId(id), p);
      for (int i = 0; i < 6; ++i) {
        out[i] = (nbs[i] == TileId(-1)) ? -1 : int(nbs[i]);
      }
      return out;
      };

    while (!q.empty()) {
      int v = q.front(); q.pop();
      for (int nb : neigh(v)) if (inBounds(nb) && !sea[nb] && isWater[nb]) { sea[nb] = 1; q.push(nb); }
    }

    // assign water bodies
    for (auto& t : w.tiles) {
      if (!isWater[t.id]) continue;
      t.vBiome = VerticalBiome::Water;
      t.water = sea[t.id] ? WaterBody::OpenSea : WaterBody::Lake;
    }
  }

  // ---------- relief classes for land (Plains / Hills / Mountains) ----------
  static void classifyRelief(WorldSnapshot& w, const GeneratorParams& p)
  {
    const int N = (int)w.tiles.size();

    // 0) Rectangular extents for radial mask (same idea as in genElevation)
    int maxAbsCx = 0;
    int maxAbsCz = 0;
    for (const auto& t : w.tiles)
    {
      maxAbsCx = std::max(maxAbsCx, std::abs(t.cx));
      maxAbsCz = std::max(maxAbsCz, std::abs(t.cz));
    }
    if (maxAbsCx < 1) maxAbsCx = 1;
    if (maxAbsCz < 1) maxAbsCz = 1;

    auto radialRect01 = [&](const Tile& t) {
      float rx = std::abs(t.cx) / float(maxAbsCx);
      float rz = std::abs(t.cz) / float(maxAbsCz);
      return std::clamp(std::max(rx, rz), 0.0f, 1.0f);
      };

    // 1) Local slope proxy
    std::vector<float> maxSlope(N, 0.0f);
    for (auto& t : w.tiles)
    {
      if (t.vBiome == VerticalBiome::Water) continue;

      auto Nbs = neighbors(w, t.id, p);
      float m = 0.0f;
      for (auto nid : Nbs)
      {
        if (nid == TileId(-1)) continue;
        m = std::max(m, std::abs(w.tiles[nid].height - t.height));
      }
      maxSlope[t.id] = m;
    }

    float slopeMax = 0.0f;
    for (int i = 0; i < N; ++i) slopeMax = std::max(slopeMax, maxSlope[i]);
    if (slopeMax < 1e-6f) slopeMax = 1e-6f;

    // 2) Land height stats (for "relative height" hRel)
    float sumH = 0.0f;
    float sumH2 = 0.0f;
    int countLand = 0;
    for (const auto& t : w.tiles)
    {
      if (t.vBiome == VerticalBiome::Water) continue;
      sumH += t.height;
      sumH2 += t.height * t.height;
      ++countLand;
    }

    float meanH = 0.5f;
    float invStdH = 1.0f;
    if (countLand > 0)
    {
      meanH = sumH / float(countLand);
      float var = (sumH2 / float(countLand)) - meanH * meanH;
      if (var < 1e-6f) var = 1e-6f;
      invStdH = 1.0f / std::sqrt(var);
    }

    const std::uint32_t sRidge = std::uint32_t(w.seed ^ 0xC0FFEEu);
    XorShift64 rng(w.seed ^ 0xFACEFEEDu);

    // 3) Classify land tiles
    for (auto& t : w.tiles)
    {
      if (t.vBiome == VerticalBiome::Water) continue;

      const int id = (int)t.id;

      // Height relative to typical land
      float hRel = (t.height - meanH) * invStdH;
      hRel = std::clamp(0.25f * hRel + 0.5f, 0.0f, 1.0f);

      float sNorm = maxSlope[id] / slopeMax; // 0..1

      float nx = float(t.cx) * p.ridgeFreq;
      float ny = float(t.cz) * p.ridgeFreq;
      float ridge = ridged2D(nx, ny, sRidge, p.ridgeOctaves, p.ridgeFreq, p.ridgeGain); // 0..1

      float jitter = (rng.uniform01() - 0.5f) * 0.05f; // [-0.025, +0.025]

      // ---- center-favoring mask for mountains ----
      // radialRect ∈ [0,1], 0 center, 1 edge.
      float rr = radialRect01(t);

      // We want mountains most likely in a "ring" roughly away from center but not at edges.
      // Think: low near 0, peak around 0.4–0.6, then fall toward 1.
      float centerMid = 0.45f;
      float width = 0.35f; // controls how wide that "mountain ring" is

      float dist = std::abs(rr - centerMid) / std::max(width, 1e-3f); // 0 near centerMid, >1 far
      float centerBonus = 1.0f - dist;   // 1 at centerMid, 0 at distance = width
      centerBonus = std::clamp(centerBonus, 0.0f, 1.0f);
      centerBonus = centerBonus * centerBonus; // sharpen peak

      // Mix scores: hRel + ridge + slope + centerBonus
      float mountainScore = 0.45f * hRel + 0.30f * ridge + 0.15f * sNorm
        + 0.20f * centerBonus + jitter;

      float hillScore = 0.40f * hRel + 0.40f * sNorm + 0.20f * ridge
        + 0.05f * centerBonus + jitter;

      // Hard "peak" condition: anything very high + steep is a mountain, regardless of mask.
      bool highPeak = (t.height > 0.82f && sNorm > 0.32f);

      // Thresholds: tune for "few but visible" mountains
      bool isMountain = highPeak || (mountainScore > 0.70f);
      bool isHill = (!isMountain && hillScore > 0.42f);

      t.vBiome = isMountain ? VerticalBiome::Mountains
        : isHill ? VerticalBiome::Hills
        : VerticalBiome::Plains;
    }
  }

  // ---------- generate navigable rivers as WATER TILES, flowing generally N→S ----------
  static void generateRivers(WorldSnapshot& w, const GeneratorParams& p)
  {
    const int W = p.width;
    const int H = p.height;
    const int N = (int)w.tiles.size();

    XorShift64 rng(w.seed ^ 0xBADC0FFEEULL);

    // --- 1) Precompute downhill pointers for non-water tiles ---
    std::vector<int> downhill(N, -1);

    for (auto& t : w.tiles)
    {
      const int id = (int)t.id;

      // Do not compute flow for any water tile (sea, lake, already-river)
      if (t.vBiome == VerticalBiome::Water)
      {
        downhill[id] = -1;
        continue;
      }

      auto Nbs = neighbors(w, t.id, p);

      float bestH = t.height;
      int   bestId = -1;
      float southBias = p.riverSouthBias; // encourages southward flow (toward larger row index)

      // First pass: strict downhill with bias
      for (int k = 0; k < 6; ++k)
      {
        TileId nid = Nbs[k];
        if (nid == TileId(-1)) continue;
        const auto& nt = w.tiles[nid];

        // Water neighbors are potential outlets, but not used for strict downhill here.
        if (nt.vBiome == VerticalBiome::Water) continue;

        float dy = (int(nid) / W) - (id / W); // +1 => neighbor is further south
        float score = nt.height - southBias * dy;

        if (score < bestH)
        {
          bestH = score;
          bestId = (int)nid;
        }
      }

      // Second pass: break plateaus / flat areas with a bit of randomness
      if (bestId < 0)
      {
        float bestScore = std::numeric_limits<float>::max();
        for (int k = 0; k < 6; ++k)
        {
          TileId nid = Nbs[k];
          if (nid == TileId(-1)) continue;
          const auto& nt = w.tiles[nid];

          // Water neighbors still not used here for flow; they will be treated as outlets
          if (nt.vBiome == VerticalBiome::Water) continue;

          float dy = (int(nid) / W) - (id / W);
          float s = nt.height - southBias * dy + rng.uniform01() * 0.03f;
          if (s < bestScore)
          {
            bestScore = s;
            bestId = (int)nid;
          }
        }
      }

      downhill[id] = bestId;
    }

    // --- 2) Choose source candidates in northern band, sorted by height ---
    std::vector<int> cand;
    cand.reserve(N);

    int northCut = std::max(1, int(std::floor(p.riverNorthBand * H)));
    for (int r = 0; r < northCut; ++r)
    {
      for (int qx = 0; qx < W; ++qx)
      {
        int id = qx + r * W;
        const auto& t = w.tiles[id];
        if (t.vBiome == VerticalBiome::Water) continue; // no sources in water
        cand.push_back(id);
      }
    }

    std::sort(cand.begin(), cand.end(), [&](int a, int b) {
      return w.tiles[a].height > w.tiles[b].height; // highest first
      });

    // --- 3) Carve rivers with spacing & no thickening ---
    int made = 0;
    std::vector<std::uint8_t> used(N, 0); // marks tiles already used by any river

    auto nearUsedRiver = [&](int id) -> bool
      {
        // Check immediate neighbors for any tile already used by a river
        auto Nbs = neighbors(w, TileId(id), p);
        for (auto nid : Nbs)
        {
          if (nid == TileId(-1)) continue;
          if (used[(int)nid]) return true;
        }
        return false;
      };

    for (int c : cand)
    {
      if (made >= p.riverTargetCount) break;
      if (used[c]) continue;            // don't start on an existing river cell
      if (nearUsedRiver(c)) continue;   // 3.1: don't start next to an existing river

      // --- Walk downhill until we hit any water or exceed max steps ---
      int cur = c;
      int len = 0;
      std::vector<int> path;
      path.reserve(256);

      while (cur >= 0 && len < p.riverMaxWalk)
      {
        auto& t = w.tiles[cur];

        // 3.3: stop the river when we reach ANY water (sea, lake, previous river)
        if (t.vBiome == VerticalBiome::Water)
          break;

        path.push_back(cur);
        cur = downhill[cur];
        if (cur < 0) break;
        ++len;
      }

      if (len < p.riverMinLen)
        continue; // too short, discard

      // --- Paint the river path as 1-tile wide WATER tiles ---
      for (int id : path)
      {
        if (used[id]) continue; // 3.2: do not thicken existing river corridors

        auto& t = w.tiles[id];
        t.vBiome = VerticalBiome::Water;
        t.water = WaterBody::River;
        t.forestDensity = 0;
        t.riversMask = 0; // not using edge-rivers for navigable rivers in this pass

        used[id] = 1;
      }

      ++made;
    }
  }

  // ---------- moisture from distance to any water (sea, lake, river) ----------
  static void computeMoisture(WorldSnapshot& w, const GeneratorParams& p) {
    std::vector<int> dist(w.tiles.size(), INT_MAX);
    std::queue<int> qq;
    const int W = p.width, H = p.height;

    auto pushIf = [&](int id) {
      const auto& t = w.tiles[id];
      if (t.vBiome == VerticalBiome::Water) { dist[id] = 0; qq.push(id); }
    };

    for (int r = 0; r < H; ++r) for (int qx = 0; qx < W; ++qx) {
      int id = qx + r * W; pushIf(id);
    }

    while (!qq.empty()) {
      int v = qq.front(); qq.pop();
      for (auto nid : neighbors(w, TileId(v), p)) {
        if (nid == TileId(-1)) continue;
        if (dist[nid] > dist[v] + 1) {
          dist[nid] = dist[v] + 1;
          if (dist[nid] <= p.moistureBfsMax) qq.push((int)nid);
        }
      }
    }

    XorShift64 rng(w.seed ^ 0xDEADBEEFu);
    for (auto& t : w.tiles) {
      if (t.vBiome == VerticalBiome::Water) { t.moisture = 1.f; continue; }
      int d = std::min(dist[t.id], p.moistureBfsMax);
      float m = 1.f - float(d) / float(std::max(1, p.moistureBfsMax));
      m = std::clamp(m + (rng.uniform01() * 2.f - 1.f) * p.moistureJitter, 0.f, 1.f);
      t.moisture = m;
    }
  }

  // ---------- temperature + Europe-like climate assignment (horizontal bands) ----------
  static void assignClimateAndForest(WorldSnapshot& w, const GeneratorParams& p)
  {
    XorShift64 rng(w.seed ^ 0xF00DF00DULL);
    const int H = p.height;

    auto lat01 = [&](int row) { // 0 top (north), 1 bottom (south)
      return float(row) / float(std::max(1, H - 1));
      };

    for (auto& t : w.tiles)
    {
      // per-row latitude
      int row = int(t.id) / p.width;
      float y = lat01(row); // 0 = north, 1 = south

      // simple lapse: higher = cooler
      float lapse = std::clamp(1.0f - 0.5f * t.height, 0.0f, 1.0f);
      t.temperature = std::clamp(0.18f + 0.64f * y, 0.0f, 1.0f) * lapse;

      // water tiles: we don't bother assigning land climates or forest
      if (t.vBiome == VerticalBiome::Water)
        continue;

      // ----- band membership -----
      const bool inSouthMedBand = (y >= 1.0f - p.bandMediterraneanS); // warm south
      const bool inSouthDesBand = (y >= 1.0f - p.bandDesertS);        // inner, hottest south

      // Mid-lat helper (same logic as before)
      auto classifyMidLat = [&](float moist, float height) -> Climate
        {
          if (moist <= p.steppeMoistureMax)
            return Climate::Steppe;
          if (moist >= p.swampMoistureMin && height <= p.swampElevMax)
            return Climate::Swamp;
          return Climate::Grassland;
        };

      // ---------- North bands ----------
      if (y <= p.bandTundraN)
      {
        t.climate = Climate::Tundra;
      }
      else if (y <= p.bandTaigaN)
      {
        t.climate = Climate::Taiga;
      }
      else
      {
        // ---------- Mid + South bands ----------
        if (inSouthMedBand)
        {
          // Southern belt: Mediterranean / Steppe / Desert mix

          // Dryness (0=very wet, 1=bone dry)
          float dry = 1.0f - t.moisture;

          // Per-tile noise to break perfect contours
          float noise = (randHash(
            std::uint32_t(t.id) ^ std::uint32_t(w.seed)
          ) * 2.0f) - 1.0f; // [-1, +1]

          // How deep into the *desert band* are we?
          // 0 = top of desert band, 1 = bottom edge of map.
          float latFracDes = 0.0f;
          if (inSouthDesBand)
          {
            float bandTop = 1.0f - p.bandDesertS; // y where desert band starts
            float bandH = std::max(p.bandDesertS, 1e-3f);
            latFracDes = (y - bandTop) / bandH;   // 0..1
            latFracDes = std::clamp(latFracDes, 0.0f, 1.0f);
          }

          // Desert score: emphasize dryness, desert-lat, add some noise
          float desertScore =
            0.65f * dry +        // mostly "how dry"
            0.35f * latFracDes + // plus "how deep in desert band"
            0.10f * noise;       // break edges
          desertScore = std::clamp(desertScore, 0.0f, 1.0f);

          if (inSouthDesBand && dry >= p.minDryForDesert && desertScore > 0.3f)
          {
            // True hot-dry core => Desert
            t.climate = Climate::Desert;
          }
          else if (dry > 0.45f)
          {
            // Dry but not extreme => Steppe fringe
            t.climate = Climate::Steppe;
          }
          else
          {
            // Wet/coastal => Mediterranean scrub
            t.climate = Climate::MediterraneanScrub;
          }
        }
        else
        {
          // Mid-lats (between Taiga and Med-band)
          t.climate = classifyMidLat(t.moisture, t.height);
        }
      }

      // --- seed initial forest by climate (stochastic) ---
     // Interpret p.forestXXX (0..255) as "coverage probability".
      auto forestProbForClimate = [&](Climate c) -> float {
        switch (c)
        {
        case Climate::Grassland:          return p.forestGrassland / 255.0f;
        case Climate::Steppe:             return p.forestSteppe / 255.0f;
        case Climate::MediterraneanScrub: return p.forestMedScrub / 255.0f;
        case Climate::Taiga:              return p.forestTaiga / 255.0f;
        case Climate::Swamp:              return p.forestSwamp / 255.0f;
        case Climate::Tundra:             return p.forestTundra / 255.0f;
        case Climate::Desert:             return p.forestDesert / 255.0f;
        default:                          return 0.0f;
        }
        };

      auto baseDensityForClimate = [&](Climate c) -> int {
        switch (c)
        {
        case Climate::Grassland:          return 90;   // mixed fields & woods
        case Climate::Steppe:             return 60;   // scattered groves
        case Climate::MediterraneanScrub: return 80;   // scrubby, not dense forest
        case Climate::Taiga:              return 160;  // dense conifer
        case Climate::Swamp:              return 140;  // wet, tangled
        case Climate::Tundra:             return 40;   // sparse
        case Climate::Desert:             return 15;   // almost none
        default:                          return 0;
        }
        };

      // no forest on water tiles
      if (t.vBiome == VerticalBiome::Water) {
        t.forestDensity = 0;
      }
      else
      {
        float prob = forestProbForClimate(t.climate);       // 0..1
        // make mountains less forested in general
        if (t.vBiome == VerticalBiome::Mountains) prob *= 0.5f;

        float roll = rng.uniform01();
        if (prob <= 0.0f || roll > prob)
        {
          // this tile starts without forest
          t.forestDensity = 0;
        }
        else
        {
          // this tile *gets* forest; density depends on climate + moisture
          float m = std::clamp(t.moisture, 0.0f, 1.0f);    // wetter → denser
          int baseD = baseDensityForClimate(t.climate);
          // moisture factor between 0.7x and 1.3x
          float moistureFactor = 0.7f + 0.6f * m;

          // small random jitter for variety
          int jitter = int((rng.uniform01() * 40.0f) - 20.0f); // [-20,+20)

          int dens = int(float(baseD) * moistureFactor) + jitter;
          dens = std::clamp(dens, 0, 255);
          t.forestDensity = static_cast<std::uint8_t>(dens);
        }
      }
      
    }
  }

  // ---------- tag coasts for land tiles adjacent to SEA ----------
  static void tagCoasts(WorldSnapshot& w, const GeneratorParams& p) {
    for (auto& t : w.tiles) {
      if (t.vBiome == VerticalBiome::Water) continue;
      auto N = neighbors(w, t.id, p);
      bool nearSea = false;
      for (auto nid : N) {
        if (nid == TileId(-1)) continue;
        const auto& nt = w.tiles[nid];
        if (nt.vBiome == VerticalBiome::Water && nt.water == WaterBody::OpenSea) { nearSea = true; break; }
      }
      if (nearSea) t.tags |= TileTag::Coast;
    }
  }

  static void debugCountWater(const WorldSnapshot& w)
  {
    int sea = 0, lake = 0, river = 0;

    for (const auto& t : w.tiles)
    {
      if (t.vBiome == VerticalBiome::Water)
      {
        switch (t.water)
        {
        case WaterBody::OpenSea: ++sea;   break;
        case WaterBody::Lake:    ++lake;  break;
        case WaterBody::River:   ++river; break;
        default: break;
        }
      }
    }

    printf("[Generator] water tiles: sea=%d lake=%d river=%d\n", sea, lake, river);
  }

  static const char* climateName(Climate c)
  {
    switch (c)
    {
    case Climate::Grassland:          return "Grassland";
    case Climate::Steppe:             return "Steppe";
    case Climate::MediterraneanScrub: return "MedScrub";
    case Climate::Taiga:              return "Taiga";
    case Climate::Swamp:              return "Swamp";
    case Climate::Tundra:             return "Tundra";
    case Climate::Desert:             return "Desert";
    default:                          return "Unknown";
    }
  }

  static void debugClimateStats(const WorldSnapshot& w, const GeneratorParams& p)
  {
    std::uint32_t counts[7] = {}; // one per Climate enum
    const int W = p.width;
    const int H = p.height;

    for (const auto& t : w.tiles)
    {
      // ignore water tiles for climate debugging
      if (t.vBiome == VerticalBiome::Water) continue;
      int idx = static_cast<int>(t.climate);
      if (idx >= 0 && idx < 7) counts[idx]++;
    }

    std::printf("[Climate] map %dx%d (land only):\n", W, H);
    for (int i = 0; i < 7; ++i)
    {
      Climate c = static_cast<Climate>(i);
      std::printf("  %-12s : %u tiles\n", climateName(c), counts[i]);
    }

    // optional: quick peek at north/south band composition
    int bandH = std::max(1, H / 6); // top/bottom ~1/6 of map
    std::uint32_t north[7] = {}, south[7] = {};

    for (int r = 0; r < H; ++r)
    {
      for (int q = 0; q < W; ++q)
      {
        int id = q + r * W;
        const auto& t = w.tiles[id];
        if (t.vBiome == VerticalBiome::Water) continue;

        int idx = static_cast<int>(t.climate);
        if (idx < 0 || idx >= 7) continue;

        if (r < bandH)           north[idx]++;
        if (r >= H - bandH)      south[idx]++;
      }
    }

    std::printf("[Climate] north band (~%d rows):\n", bandH);
    for (int i = 0; i < 7; ++i)
    {
      Climate c = static_cast<Climate>(i);
      std::printf("  %-12s : %u tiles\n", climateName(c), north[i]);
    }

    std::printf("[Climate] south band (~%d rows):\n", bandH);
    for (int i = 0; i < 7; ++i)
    {
      Climate c = static_cast<Climate>(i);
      std::printf("  %-12s : %u tiles\n", climateName(c), south[i]);
    }
  }

  // ---------- DEBUG: forest coverage & density by climate ----------
  static void debugForestStats(const WorldSnapshot& w, const GeneratorParams& p)
  {
    struct ForestAccum
    {
      int landTiles = 0;   // non-water tiles
      int forestTiles = 0;   // tiles with forestDensity > 0
      int sumDensity = 0;   // sum of forestDensity for forest tiles
      int minDensity = 256; // min forestDensity among forest tiles
      int maxDensity = 0;   // max forestDensity among forest tiles
    };

    constexpr int kClimateCount = 7; // Grassland, Steppe, MedScrub, Taiga, Swamp, Tundra, Desert
    ForestAccum acc[kClimateCount]{};

    auto climateIndex = [](Climate c) -> int {
      return static_cast<int>(c); // relies on enum being 0..6
      };

    auto climateName = [](Climate c) -> const char* {
      switch (c)
      {
      case Climate::Grassland:          return "Grassland";
      case Climate::Steppe:             return "Steppe";
      case Climate::MediterraneanScrub: return "MedScrub";
      case Climate::Taiga:              return "Taiga";
      case Climate::Swamp:              return "Swamp";
      case Climate::Tundra:             return "Tundra";
      case Climate::Desert:             return "Desert";
      default:                          return "Unknown";
      }
      };

    // accumulate stats per climate
    for (const auto& t : w.tiles)
    {
      if (t.vBiome == VerticalBiome::Water)
        continue; // ignore water for forest stats

      int ci = climateIndex(t.climate);
      if (ci < 0 || ci >= kClimateCount)
        continue;

      auto& a = acc[ci];
      a.landTiles++;

      if (t.forestDensity > 0)
      {
        a.forestTiles++;
        a.sumDensity += int(t.forestDensity);
        a.minDensity = std::min(a.minDensity, int(t.forestDensity));
        a.maxDensity = std::max(a.maxDensity, int(t.forestDensity));
      }
    }

    // global totals
    int totalLand = 0;
    int totalForest = 0;
    for (int i = 0; i < kClimateCount; ++i)
    {
      totalLand += acc[i].landTiles;
      totalForest += acc[i].forestTiles;
    }

    printf("[Forest] map %dx%d (land only):", p.width, p.height);
    printf("  total land tiles   : %d", totalLand);
    printf("  tiles with forest  : %d (%.1f%%)",
      totalForest,
      totalLand > 0 ? 100.0 * double(totalForest) / double(totalLand) : 0.0);

    printf("[Forest] per climate (land only):");
    for (int i = 0; i < kClimateCount; ++i)
    {
      Climate c = static_cast<Climate>(i);
      const auto& a = acc[i];

      if (a.landTiles == 0)
      {
        printf("  %-12s : land=0", climateName(c));
        continue;
      }

      double coveragePct = a.landTiles > 0
        ? 100.0 * double(a.forestTiles) / double(a.landTiles)
        : 0.0;

      double avgDensity = a.forestTiles > 0
        ? double(a.sumDensity) / double(a.forestTiles)
        : 0.0;

      int minD = (a.forestTiles > 0) ? a.minDensity : 0;
      int maxD = (a.forestTiles > 0) ? a.maxDensity : 0;

      printf("  %-12s : land=%4d  forest=%4d (%.1f%%)  dens(avg=%.1f, min=%3d, max=%3d)",
        climateName(c),
        a.landTiles,
        a.forestTiles,
        coveragePct,
        avgDensity,
        minD,
        maxD);
    }
  }

  //------------Resource Pass-----------------------------

  // ---- Resource helpers for this file ----
  
  // Land tile has neighbor that is a river or lake tile
  static bool hasFreshwaterNeighbour(const WorldSnapshot& w, TileId id, const GeneratorParams& p)
  {
    auto ns = neighbors(w, id, p);
    for (auto nid : ns) {
      if (nid == TileId(-1)) continue;
      const auto& nt = w.tiles[nid];
      if (nt.vBiome == VerticalBiome::Water &&
        (nt.water == WaterBody::River || nt.water == WaterBody::Lake)) {
        return true;
      }
    }
    return false;
  }

  inline int specialIconCount(const Tile& t) {
    int n = 0; for (auto sc : t.specials) if (sc != SpecialIcon::None) ++n; return n;
  }

  inline bool addResourceIfSpace(Tile& t, Resource r) {
    if (r == Resource::None) return false;
    if (hasResource(t, r))   return false;
    for (auto& slot : t.resources) {
      if (slot == Resource::None) { slot = r; return true; }
    }
    return false;
  }

  inline bool addIconIfSpace(Tile& t, SpecialIcon s) {
    if (s == SpecialIcon::None) return false;
    for (auto& slot : t.specials) {
      if (slot == SpecialIcon::None) { slot = s; return true; }
      if (slot == s) return false; // already has it
    }
    return false;
  }

  // The 5 "special pass" resources must be mutually exclusive on a tile.
  inline bool hasAnySpecialResource(const Tile& t) {
    for (auto r : t.resources) {
      switch (r) {
      case Resource::Stone:
      case Resource::Marble:
      case Resource::Salt:
      case Resource::Ore:
      case Resource::Horses:
        return true;
      default: break;
      }
    }
    return false;
  }

  // ----------------- 1) BASE RESOURCE PASS -----------------

  static void assignBaseResources_GeneralClimate(WorldSnapshot& w)
  {
    for (auto& t : w.tiles) {
      if (!t.isLand()) continue;

      // Tundra & Desert handled separately in 1.2 / 1.3
      if (t.climate == Climate::Tundra || t.climate == Climate::Desert)
        continue;

      // Mountains: no base yields, only adjacency (Marble handled later).
      if (t.vBiome == VerticalBiome::Mountains)
        continue;

      // Reset base resources, we'll rebuild them from climate.
      t.resources.fill(Resource::None);

      // All non-tundra/desert land get Food as base.
      addResourceIfSpace(t, Resource::Food);

      const int ci = static_cast<int>(t.climate);
      static_assert(sizeof(kClimateBaseRes) / sizeof(kClimateBaseRes[0]) == 7,
        "Update kClimateBaseRes if Climate enum changes.");
      const ClimateResourcePair& pair = kClimateBaseRes[ci];

      const bool isPlains = (t.vBiome == VerticalBiome::Plains);
      const bool isHills = (t.vBiome == VerticalBiome::Hills);

      // We always *try* to give both luxes; yields (3 vs 2) are handled later
      // in your economy layer based on plains/hills, not here.
      if (isPlains) {
        if (pair.plains != Resource::None) addResourceIfSpace(t, pair.plains);
        if (pair.hills != Resource::None) addResourceIfSpace(t, pair.hills);
      }
      else if (isHills) {
        if (pair.hills != Resource::None) addResourceIfSpace(t, pair.hills);
        if (pair.plains != Resource::None) addResourceIfSpace(t, pair.plains);
      }
      // Other vertical forms (if any appear later) just keep Food.
    }
  }

  static void assignBaseResources_Tundra(WorldSnapshot& w)
  {
    for (auto& t : w.tiles) {
      if (!t.isLand()) continue;
      if (t.climate != Climate::Tundra) continue;

      // Clear whatever general pass put there (if any).
      t.resources.fill(Resource::None);

      // Tundra: always at least some Food (poor values handled in yield formula).
      addResourceIfSpace(t, Resource::Food);

      // Forested tundra → Fur
      if (t.forestDensity > 0) {
        addResourceIfSpace(t, Resource::Fur);
      }
    }
  }

  static void assignBaseResources_Desert(WorldSnapshot& w, const GeneratorParams& p)
  {
    for (auto& t : w.tiles) {
      if (!t.isLand()) continue;
      if (t.climate != Climate::Desert) continue;

      t.resources.fill(Resource::None);

      const bool isPlain = (t.vBiome == VerticalBiome::Plains);
      const bool isHill = (t.vBiome == VerticalBiome::Hills);

      // Desert plains: only fertile along water (oases, floodplains).
      if (isPlain && hasFreshwaterNeighbour(w, t.id, p)) {
        addResourceIfSpace(t, Resource::Food);
      }

      // Desert hills: Soda Ash
      if (isHill) {
        addResourceIfSpace(t, Resource::SodaAsh);
      }

      // Desert mountains: nothing by default; Marble handled in special pass.
    }
  }

  static void baseResourcePass(WorldSnapshot& w, const GeneratorParams& p)
  {
    assignBaseResources_GeneralClimate(w);
    assignBaseResources_Tundra(w);
    assignBaseResources_Desert(w, p);
  }

  // ----------------- 2) SPECIAL RESOURCE PASS -----------------

  struct SpecialResStats {
    int marble = 0;
    int salt = 0;
    int ore = 0;
    int stone = 0;
    int horses = 0;
  };

  static bool isNearMountains(const WorldSnapshot& w, TileId id, const GeneratorParams& p)
  {
    auto ns = neighbors(w, id, p);
    for (auto nid : ns) {
      if (nid == TileId(-1)) continue;
      const auto& nt = w.tiles[nid];
      if (nt.vBiome == VerticalBiome::Mountains) return true;
    }
    return false;
  }

  static int countAdjacentMountains(const WorldSnapshot& w, TileId id, const GeneratorParams& p)
  {
    int c = 0;
    auto ns = neighbors(w, id, p);
    for (auto nid : ns) {
      if (nid == TileId(-1)) continue;
      const auto& nt = w.tiles[nid];
      if (nt.vBiome == VerticalBiome::Mountains) ++c;
    }
    return c;
  }

  static bool isNearLakeOrSea(const WorldSnapshot& w, TileId id, const GeneratorParams& p)
  {
    auto ns = neighbors(w, id, p);
    for (auto nid : ns) {
      if (nid == TileId(-1)) continue;
      const auto& nt = w.tiles[nid];
      if (nt.vBiome == VerticalBiome::Water &&
        (nt.water == WaterBody::Lake || nt.water == WaterBody::OpenSea || nt.water == WaterBody::Coast))
        return true;
    }
    return false;
  }

  // Any of the 5 special/strategic resources we want mutually exclusive per tile
  inline bool hasSpecialResource(const Tile& t)
  {
    for (auto r : t.resources) {
      switch (r) {
      case Resource::Stone:
      case Resource::Marble:
      case Resource::Salt:
      case Resource::Ore:
      case Resource::Horses:
        return true;
      default:
        break;
      }
    }
    return false;
  }

  // -------------------------------------------
// MARBLE: rare, only on mountains OR hills
// that are adjacent to mountains.
// Target: ~5–10 deposits for a 96x64 map.
// -------------------------------------------
  static void marblePass(WorldSnapshot& w, const GeneratorParams& p, SpecialResStats& stats)
  {
    std::vector<int> cand;
    cand.reserve(w.tiles.size());

    for (auto& t : w.tiles) {
      if (!t.isLand()) continue;

      bool isValid = false;

      if (t.vBiome == VerticalBiome::Mountains) {
        // direct marble-on-peak (the only resource allowed on mountains)
        isValid = true;
      }
      else if (t.vBiome == VerticalBiome::Hills) {
        // hills adjacent to mountains also allowed
        auto ns = neighbors(w, t.id, p);
        bool nearMountains = false;
        for (auto nid : ns) {
          if (nid == TileId(-1)) continue;
          const auto& nt = w.tiles[nid];
          if (nt.vBiome == VerticalBiome::Mountains) {
            nearMountains = true;
            break;
          }
        }
        if (nearMountains) {
          isValid = true;
        }
      }

      if (!isValid) continue;

      // Do not stack with other special resources (Stone/Salt/Ore/Horses/etc.)
      if (hasSpecialResource(t)) continue;

      cand.push_back((int)t.id);
    }

    if (cand.empty()) return;

    XorShift64 rng(w.seed ^ 0xABCD1234u);

    // Simple Fisher–Yates shuffle
    for (int i = (int)cand.size() - 1; i > 0; --i) {
      int j = rng.range(0, i);
      std::swap(cand[i], cand[j]);
    }

    const int totalTiles = (int)w.tiles.size();
    const int targetMin = 6;
    const int targetMax = 12;
    int target = std::clamp(totalTiles / 1500, targetMin, targetMax); // 96x64 → ~6

    int placed = 0;
    int limit = std::min(target, (int)cand.size());

    for (int i = 0; i < limit; ++i) {
      Tile& t = w.tiles[cand[i]];
      if (addResourceIfSpace(t, Resource::Marble)) {
        ++stats.marble;
        ++placed;
      }
    }
  }

  // -------------------------------------------
  // SALT: land “refined” resource, not ultra-rare.
  // Priority:
  //  - Desert plains/hills
  //  - Then Steppe/MedScrub tiles near coast or lake/river.
  // Target: ~3–6 deposits on 96x64.
  // -------------------------------------------
  static void saltPass(WorldSnapshot& w, const GeneratorParams& p, SpecialResStats& stats)
  {
    std::vector<int> cand;
    cand.reserve(w.tiles.size());

    for (auto& t : w.tiles) {
      if (!t.isLand()) continue;
      if (t.vBiome == VerticalBiome::Mountains) continue; // no salt on peaks

      if (hasSpecialResource(t)) continue; // no stacking with Marble/Ore/Horses/Stone

      const bool isPlainOrHill =
        (t.vBiome == VerticalBiome::Plains || t.vBiome == VerticalBiome::Hills);

      if (!isPlainOrHill) continue;

      const bool isDesert = (t.climate == Climate::Desert);
      const bool isSemiArid =
        (t.climate == Climate::Steppe || t.climate == Climate::MediterraneanScrub);

      const bool nearWater = isCoastLand(t) || hasFreshwaterNeighbour(w, t.id, p);

      // 1) Desert anywhere (salt flats, basins)
      // 2) Steppe/MedScrub near water (coastal pans, marshy evaporation)
      if (!isDesert && !(isSemiArid && nearWater)) continue;

      // Some basic "geology" filter
      if (miningPotential(t) < 0.4f) continue;

      cand.push_back((int)t.id);
    }

    if (cand.empty()) return;

    XorShift64 rng(w.seed ^ 0xC0A1C0A1u);

    for (int i = (int)cand.size() - 1; i > 0; --i) {
      int j = rng.range(0, i);
      std::swap(cand[i], cand[j]);
    }

    const int totalTiles = (int)w.tiles.size();
    const int targetMin = 8;
    const int targetMax = 16;
    int target = std::clamp(totalTiles / 2500, targetMin, targetMax); // 96x64 → ~3–4

    int placed = 0;
    int limit = std::min(target, (int)cand.size());

    for (int i = 0; i < limit; ++i) {
      Tile& t = w.tiles[cand[i]];
      if (addResourceIfSpace(t, Resource::Salt)) {
        ++stats.salt;
        ++placed;
      }
    }
  }

  // 2.3 Ore: hills in mid-latitudes, near mountains if possible.
  static void orePass(WorldSnapshot& w, const GeneratorParams& p, SpecialResStats& stats)
  {
    XorShift64 rng(w.seed ^ 0x0DE0FACEu);

    const int H = p.height;
    int maxOre = 30; // can tweak

    int count = 0;
    for (auto& t : w.tiles) {
      if (count >= maxOre) break;
      if (!t.isLand()) continue;
      if (hasAnySpecialResource(t)) continue;
      if (t.vBiome != VerticalBiome::Hills) continue; // no ore *on* mountains

      // Mid-latitude preference (~25%-75% rows)
      int row = int(t.id) / p.width;
      float lat = float(row) / float(std::max(1, H - 1));
      if (lat < 0.25f || lat > 0.75f) continue;

      // Geology bias: use miningPotential as proxy
      float mp = miningPotential(t);
      if (mp < 0.6f) continue;

      // Prefer adjacency to mountains
      float bonus = isNearMountains(w, t.id, p) ? 0.25f : 0.0f;
      float roll = rng.uniform01();
      if (roll > 0.20f + bonus) continue;

      if (addResourceIfSpace(t, Resource::Ore)) {
        ++count;
        ++stats.ore;
      }
    }
  }

  // 2.4 Stone: hills adjacent to mountains, fairly generous.
  static void stonePass(WorldSnapshot& w, const GeneratorParams& p, SpecialResStats& stats)
  {
    for (auto& t : w.tiles) {
      if (!t.isLand()) continue;
      if (t.vBiome != VerticalBiome::Hills) continue;
      if (hasAnySpecialResource(t)) continue;

      int adjM = countAdjacentMountains(w, t.id, p);
      if (adjM <= 0) continue;

      if (addResourceIfSpace(t, Resource::Stone)) {
        ++stats.stone;
      }
    }
  }

  // 2.5 Horses: grassland/steppe plains, moderate density.
  static void horsesPass(WorldSnapshot& w, const GeneratorParams& p, SpecialResStats& stats)
  {
    XorShift64 rng(w.seed ^ 0x401235EULL);

    int maxHerdTiles = 30;
    int count = 0;

    for (auto& t : w.tiles) {
      if (count >= maxHerdTiles) break;
      if (!t.isLand()) continue;
      if (hasAnySpecialResource(t)) continue;

      if (t.vBiome != VerticalBiome::Plains) continue;
      if (!(t.climate == Climate::Grassland || t.climate == Climate::Steppe || t.climate == Climate::Taiga))
        continue;

      // Avoid swamps/desert obviously (already filtered by climate)
      // Small random factor
      if (rng.uniform01() > 0.08f) continue;

      if (addResourceIfSpace(t, Resource::Horses)) {
        ++count;
        ++stats.horses;
      }
    }
  }

  static SpecialResStats specialResourcePass(WorldSnapshot& w, const GeneratorParams& p)
  {
    SpecialResStats stats{};
    marblePass(w, p, stats);
    saltPass(w, p, stats);
    orePass(w, p, stats);
    horsesPass(w, p, stats);
    stonePass(w, p, stats);
    return stats;
  }

  // ----------------- 3) ICON PASS -----------------

  struct IconStats {
    int totalTilesWithAnyIcon = 0;
    int perIcon[(int)SpecialIcon::Dyes + 1] = { 0 };
  };

  static void wheatIconPass(WorldSnapshot& w, const GeneratorParams& p, IconStats& stats)
  {
    XorShift64 rng(w.seed ^ 0x1234ABCDu);

    for (auto& t : w.tiles) {
      if (!t.isLand()) continue;
      if (t.vBiome != VerticalBiome::Plains) continue;
      if (!(t.climate == Climate::Grassland || t.climate == Climate::Steppe)) continue;

      // Needs Food and good agri potential
      if (!hasResource(t, Resource::Food)) continue;
      if (agriPotential(t) < 0.7f) continue;

      // Prefer freshwater adjacency (rivers/lakes)
      const bool nearFresh = hasFreshwaterNeighbour(w, t.id, p);
      float baseP = nearFresh ? 0.08f : 0.02f;

      if (rng.uniform01() > baseP) continue;

      if (addIconIfSpace(t, SpecialIcon::Wheat)) {
        ++stats.perIcon[(int)SpecialIcon::Wheat];
      }
    }
  }

  static void fishIconPass(WorldSnapshot& w, const GeneratorParams& p, IconStats& stats)
  {
    XorShift64 rng(w.seed ^ 0xF15F0A5u);

    for (auto& t : w.tiles) {
      if (!t.isWater()) continue;

      // Prefer coasts and lakes over deep open sea
      bool goodWater = (t.water == WaterBody::Lake || t.water == WaterBody::Coast || t.water == WaterBody::River);
      float baseP = goodWater ? 0.12f : 0.02f;

      if (rng.uniform01() > baseP) continue;

      if (addIconIfSpace(t, SpecialIcon::Fish)) {
        ++stats.perIcon[(int)SpecialIcon::Fish];
      }
    }
  }

  static void charcoalIconPass(WorldSnapshot& w, const GeneratorParams& p, IconStats& stats)
  {
    XorShift64 rng(w.seed ^ 0xC0ALu);

    for (auto& t : w.tiles) {
      if (!t.isLand()) continue;
      if (t.forestDensity < 150) continue; // quite forested

      // Prefer places where timber + mining meet
      bool nearMount = isNearMountains(w, t.id, p);
      float baseP = nearMount ? 0.06f : 0.01f;

      if (rng.uniform01() > baseP) continue;

      if (addIconIfSpace(t, SpecialIcon::Charcoal)) {
        ++stats.perIcon[(int)SpecialIcon::Charcoal];
      }
    }
  }

  static void dyesIconPass(WorldSnapshot& w, const GeneratorParams& p, IconStats& stats)
  {
    XorShift64 rng(w.seed ^ 0xD1E50001u);

    for (auto& t : w.tiles) {
      if (!t.isLand()) continue;

      bool warmWet =
        (t.climate == Climate::MediterraneanScrub || t.climate == Climate::Grassland || t.climate == Climate::Swamp)
        && t.moisture > 0.7f;

      if (!warmWet) continue;
      if (t.forestDensity < 60) continue; // some vegetation

      if (rng.uniform01() > 0.04f) continue;

      if (addIconIfSpace(t, SpecialIcon::Dyes)) {
        ++stats.perIcon[(int)SpecialIcon::Dyes];
      }
    }
  }

  static IconStats iconPass(WorldSnapshot& w, const GeneratorParams& p)
  {
    IconStats stats{};
    wheatIconPass(w, p, stats);
    fishIconPass(w, p, stats);
    charcoalIconPass(w, p, stats);
    dyesIconPass(w, p, stats);

    for (auto& t : w.tiles) {
      if (specialIconCount(t) > 0) ++stats.totalTilesWithAnyIcon;
    }

    return stats;
  }

  // ----------------- 4) TIMBER PASS -----------------

  inline int timberYieldFromForest(const Tile& t)
  {
    if (t.forestDensity == 0) return 0;
    float d = t.forestDensity / 255.0f;
    if (d < 0.33f)  return 2; // sparse
    if (d < 0.66f)  return 3; // mature
    return 4;                 // old-growth
  }

  static void timberPass(WorldSnapshot& w)
  {
    XorShift64 rng(w.seed ^ 0x71AB3A7u);

    for (auto& t : w.tiles) {
      if (!t.isLand()) continue;
      if (t.forestDensity == 0) continue;

      // Build new resource array
      std::array<Resource, kMaxTileResources> newRes;
      newRes.fill(Resource::None);

      // Slot 0: Timber
      newRes[0] = Resource::Timber;

      // Copy *non-Food, non-Timber* resources into remaining slots.
      int outIdx = 1;
      for (auto r : t.resources) {
        if (r == Resource::None || r == Resource::Timber || r == Resource::Food) continue;
        if (outIdx >= (int)kMaxTileResources) break;
        newRes[outIdx++] = r;
      }

      // If we kept more than 2 non-Timber resources, trim one: "-1 of other resources".
      if (outIdx > 3) {
        // Simple trim: drop the last one
        newRes[3] = Resource::None;
      }

      t.resources = newRes;
    }
  }

  // ----------------- DEBUG PRINTS -----------------

  static void debugSpecialResources(const WorldSnapshot& w)
  {
    int marble = 0, salt = 0, ore = 0, stone = 0, horses = 0;
    for (const auto& t : w.tiles) {
      for (auto r : t.resources) {
        switch (r) {
        case Resource::Marble: ++marble; break;
        case Resource::Salt:   ++salt;   break;
        case Resource::Ore:    ++ore;    break;
        case Resource::Stone:  ++stone;  break;
        case Resource::Horses: ++horses; break;
        default: break;
        }
      }
    }

    printf("[SpecialResources] marble=%d salt=%d ore=%d stone=%d horses=%d\n",
      marble, salt, ore, stone, horses);
  }

  static void debugIcons(const WorldSnapshot& w)
  {
    int totalTiles = 0;
    int perIcon[(int)SpecialIcon::Dyes + 1] = { 0 };

    for (const auto& t : w.tiles) {
      bool hasAny = false;
      for (auto s : t.specials) {
        if (s == SpecialIcon::None) continue;
        hasAny = true;
        perIcon[(int)s]++;
      }
      if (hasAny) ++totalTiles;
    }

    printf("[Icons] tiles_with_icons=%d\n", totalTiles);
    for (int i = 0; i <= (int)SpecialIcon::Dyes; ++i) {
      if (perIcon[i] == 0) continue;
      printf("  icon %d : %d\n", i, perIcon[i]);
    }
  }

  // ---------- main ----------
  WorldSnapshot Generator::Generate(std::uint64_t seed, const GeneratorParams& p)
  {
    WorldSnapshot w = buildGrid(seed, p);

    genElevation(w, p);
    tagSeasAndLakes(w, p);
    carveSmallLakes(w, p);
    classifyRelief(w, p);
    generateRivers(w, p);        // rivers are WaterBody::River tiles (navigable)
    //debugCountWater(w);
    computeMoisture(w, p);
    assignClimateAndForest(w, p);
    debugForestStats(w, p);

    // 1) base climate resources (Food + climate lux, plus tundra/desert specials)
    baseResourcePass(w, p);

    // 2) special resources (Stone, Marble, Salt, Ore, Horses – mutually exclusive)
    SpecialResStats sStats = specialResourcePass(w, p);

    // 3) icon pass (Wheat, Fish, Charcoal, Dyes, etc.)
    IconStats iStats = iconPass(w, p);

    // 4) forest → Timber rewrite
    timberPass(w);

    tagCoasts(w, p);

    // Debug
    debugSpecialResources(w);
    debugIcons(w);

    return w;
  }
} // namespace logic
