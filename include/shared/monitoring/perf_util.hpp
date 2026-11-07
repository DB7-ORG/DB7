#pragma once

#include <thread>

#include "storage/storage_common.hpp"

namespace db7 {
class PerfUtil {
public:
  static void SetName(std::thread &worker, const char *name) {
    if constexpr (storage::DB7_BUFFER_POOL_STATS) {
      pthread_setname_np(worker.native_handle(), name);
    }
  }
};
} // namespace db7