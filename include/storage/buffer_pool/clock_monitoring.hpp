#pragma once

#include "storage/storage_common.hpp"

#include <array>
#include <atomic>
#include <cinttypes>
#include <cstdio>

namespace db7::storage {

enum EvictionType { HIT, WRITTEN, MISS };

class EvictionMonitor {
private:
  static constexpr size_t SLOT_COUNT = 64; // ideally >= number of threads
  static constexpr size_t KIND_COUNT = 3;

  struct alignas(64) Slot {
    std::array<std::atomic<u64>, KIND_COUNT> pins{};
  };

  std::array<Slot, SLOT_COUNT> slots{};

  static size_t ThreadSlot() {
    static std::atomic<size_t> next{0};
    thread_local const size_t slot = next.fetch_add(1, std::memory_order_relaxed) % SLOT_COUNT;
    return slot;
  }

  std::array<u64, KIND_COUNT> Totals() const {
    std::array<u64, KIND_COUNT> totals{};
    for (const Slot &slot : slots) {
      for (size_t i = 0; i < KIND_COUNT; ++i) {
        totals[i] += slot.pins[i].load(std::memory_order_relaxed);
      }
    }
    return totals;
  }

public:
  void Add(EvictionType kind, u64 n = 1) {
    if constexpr (DB7_BUFFER_POOL_STATS) {
      slots[ThreadSlot()].pins[static_cast<size_t>(kind)].fetch_add(n, std::memory_order_relaxed);
    }
  }

  void Print() const {
    static constexpr std::array<const char *, KIND_COUNT> NAMES = {"Hit", "Written", "Miss"};

    if constexpr (!DB7_BUFFER_POOL_STATS) {
      std::printf("MONITORING NOT ENABLED\n");
      return;
    }

    u64 total = 0;
    auto pins = Totals();
    for (u64 count : pins) total += count;

    std::printf("[CLOCK TICKS]: %" PRIu64 "\n", total);
    for (size_t i = 0; i < pins.size(); ++i) {
      const double pct =
          total == 0 ? 0.0 : 100.0 * static_cast<double>(pins[i]) / static_cast<double>(total);
      std::printf("  %-8s %12" PRIu64 " %6.1f%%\n", NAMES[i], pins[i], pct);
    }
  }
};
} // namespace db7::storage