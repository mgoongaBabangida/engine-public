#pragma once
#include <atomic>
#include <array>
#include <cstdint>
#include <cassert>
#include "DrawList.h"

namespace UI_lib
{
  // SPSC triple buffer: Producer writes 'writeIdx', publishes to 'nextIdx' (release).
  // Consumer latches 'nextIdx' into 'frontIdx' (acquire) at frame start.
  // Producer never writes to the buffer equal to frontIdx or nextIdx.
  //-------------------------------------------------------------------------------------
  class DrawListBus
  {
  public:
    DrawListBus()
    {
      frontIdx.store(0, std::memory_order_relaxed);
      nextIdx.store(-1, std::memory_order_relaxed);
      writeIdx = 1; // leave 0 as initial front
    }

    // -------- Producer (UI thread) --------
    DrawList& beginWrite() noexcept
    {
      // back buffer is 'writeIdx'
      return bufs[writeIdx];
    }

    void publish() noexcept
    {
      // Make produced writes visible before publish
      nextIdx.store(writeIdx, std::memory_order_release);

      // Pick a new write index not equal to current front or current 'next'
      const int f = frontIdx.load(std::memory_order_relaxed);
      const int n = nextIdx.load(std::memory_order_relaxed);
      for (int i = 0; i < 3; ++i) {
        if (i != f && i != n) { writeIdx = i; break; }
      }
    }

    // -------- Consumer (Render thread) --------
    // Returns the current front snapshot (always valid).
    const DrawList& acquireFront() noexcept
    {
      // If producer published a newer one, latch it as front
      const int n = nextIdx.exchange(-1, std::memory_order_acquire);
      if (n != -1) {
        frontIdx.store(n, std::memory_order_relaxed);
      }
      const int f = frontIdx.load(std::memory_order_acquire);
      assert(f >= 0 && f < 3);
      return bufs[f];
    }

    // Optional: pre-reserve to avoid reallocs at runtime
    void reserve(size_t v, size_t i, size_t c)
    {
      for (auto& b : bufs) {
        b.verts.reserve(v);
        b.indices.reserve(i);
        b.cmds.reserve(c);
      }
    }

  private:
    alignas(64) std::atomic<int> frontIdx; // buffer currently visible to consumer
    alignas(64) std::atomic<int> nextIdx;  // last published by producer (or -1)
    int writeIdx;                          // buffer producer is filling next
    std::array<DrawList, 3> bufs;
  };
}

