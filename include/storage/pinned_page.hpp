#pragma once

#include "storage/page.hpp"

#include <utility>

namespace db7::storage {

class [[nodiscard]] PinnedPage {
private:
  Page *page_ = nullptr;

public:
  PinnedPage() = default;
  PinnedPage(Page *page) : page_(page) {}
  ~PinnedPage() { reset(); }

  PinnedPage(const PinnedPage &) = delete;
  PinnedPage &operator=(const PinnedPage &) = delete;

  PinnedPage(PinnedPage &&o) noexcept : page_(std::exchange(o.page_, nullptr)) {}
  PinnedPage &operator=(PinnedPage &&o) noexcept {
    if (this != &o) {
      reset();
      page_ = std::exchange(o.page_, nullptr);
    }
    return *this;
  }

  void reset() noexcept {
    if (page_ != nullptr) { release()->Unpin(); }
  }
  Page *release() noexcept { return std::exchange(page_, nullptr); }

  Page *get() const noexcept { return page_; }
  Page *operator->() const noexcept { return page_; }
  explicit operator bool() const noexcept { return page_ != nullptr; }
  operator Page *() const & noexcept { return page_; }
  operator Page *() const && = delete;
};

} // namespace db7::storage