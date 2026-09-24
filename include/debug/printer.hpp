#pragma once

#include "common.hpp"

namespace db7::storage {
struct PageIdentifier;
class Page;
class BufferPool;
} // namespace db7::storage

namespace db7::shared {
void Print(const storage::PageIdentifier &pid);
void Print(const storage::Page &page);
void Print(const storage::BufferPool &pool);
void PrintVarlenLayout(byte *data);
} // namespace db7::shared