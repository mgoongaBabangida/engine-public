#pragma once

#include <array>
#include <atomic>
#include <glm/glm/glm.hpp>

template <size_t N>
struct MatricesDB
{
  std::array<glm::mat4, N> buffers[2];
  std::atomic<int> front{ 0 }; // presented buffer

  // read from front
  const std::array<glm::mat4, N>& read() const noexcept {
    int idx = front.load(std::memory_order_acquire);
    return buffers[idx];
  }

  // write to back
  std::array<glm::mat4, N>& begin_write() noexcept {
    int cur = front.load(std::memory_order_relaxed);
    int back = 1 - cur;
    return buffers[back];
  }

  // publish
  void publish_written() noexcept {
    int cur = front.load(std::memory_order_relaxed);
    int back = 1 - cur;
    front.store(back, std::memory_order_release);
  }
};
