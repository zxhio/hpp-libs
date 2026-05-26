#pragma once

#include <atomic>
#include <cstddef>

namespace slog {

template <typename T, std::size_t Capacity>
class SpscQueue {
public:
  static_assert(Capacity >= 2, "capacity must be >= 2");
  static_assert((Capacity & (Capacity - 1)) == 0, "capacity must be power of 2");

  SpscQueue() noexcept : head_(0), tail_(0) {}

  SpscQueue(const SpscQueue &) = delete;
  SpscQueue &operator=(const SpscQueue &) = delete;

  bool try_push(const T &item) noexcept {
    const auto h = head_.load(std::memory_order_relaxed);
    const auto next = (h + 1) & kMask;
    if (next == tail_.load(std::memory_order_acquire)) {
      return false; // full
    }
    slots_[h] = item;
    head_.store(next, std::memory_order_release);
    return true;
  }

  bool try_pop(T &item) noexcept {
    const auto t = tail_.load(std::memory_order_relaxed);
    if (t == head_.load(std::memory_order_acquire)) {
      return false; // empty
    }
    item = slots_[t];
    tail_.store((t + 1) & kMask, std::memory_order_release);
    return true;
  }

  bool empty() const noexcept {
    return tail_.load(std::memory_order_acquire) ==
           head_.load(std::memory_order_acquire);
  }

  std::size_t size() const noexcept {
    auto h = head_.load(std::memory_order_acquire);
    auto t = tail_.load(std::memory_order_acquire);
    return (h - t) & kMask;
  }

  static constexpr std::size_t capacity() noexcept { return Capacity - 1; }

private:
  static constexpr std::size_t kMask = Capacity - 1;

  alignas(64) std::atomic<std::size_t> head_;
  alignas(64) std::atomic<std::size_t> tail_;

  T slots_[Capacity];
};

} // namespace slog
