#pragma once

namespace db7::access {

enum IndexType { BTREE };

class Index {
private:
  IndexType type_;

public:
  Index(IndexType type) : type_(type) {}
  virtual ~Index();

  IndexType GetType() const { return type_; }

  template <class TARGET>
  TARGET &Cast() {
    return reinterpret_cast<TARGET &>(*this);
  }

  template <class TARGET>
  const TARGET &Cast() const {
    return reinterpret_cast<const TARGET &>(*this);
  }
};
} // namespace db7::access