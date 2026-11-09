#include "excecution/parallel/task_scheduler_pool.hpp"
#include "third_party/lightweightsemaphore.h"

namespace db7::excecution::parallel {
struct LightWeightSemaphoreWrapper {
  moodycamel::LightweightSemaphore semaphore_;
};

} // namespace db7::excecution::parallel