#pragma once

#include "storage/storage_common.hpp"

#include <array>
#include <cinttypes>
#include <cstdio>

namespace db7::storage {
class BufferMonitor {
private:
  std::array<u64, 3> pins{};

public:
  void Add(PinKind kind, u64 n = 1) { pins[static_cast<size_t>(kind)] += n; }

  void Print() const {
    static constexpr std::array<const char *, 3> NAMES = {"Hit", "Foreign", "Miss"};

    u64 total = 0;
    for (u64 count : pins) total += count;

    std::printf("Buffer pool pins: %" PRIu64 "\n", total);
    for (size_t i = 0; i < pins.size(); ++i) {
      const double pct =
          total == 0 ? 0.0 : 100.0 * static_cast<double>(pins[i]) / static_cast<double>(total);
      std::printf("  %-8s %12" PRIu64 " %6.1f%%\n", NAMES[i], pins[i], pct);
    }
  }
};
} // namespace db7::storage