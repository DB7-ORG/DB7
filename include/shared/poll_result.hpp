#pragma once

#include "common.hpp"
namespace db7 {
enum class PollResult : u8 { HaveMore, Finished, Blocked };
}