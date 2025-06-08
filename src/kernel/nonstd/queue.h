#pragma once

#include "nonstd/deque.h"

namespace nonstd {

template <typename T, typename Container = deque<T>> class queue {
public:
  explicit queue() = default;
  explicit queue(const Container &_cont) : cont{_cont} {}
  explicit queue(Container &&_cont) : cont{std::move(_cont)} {}
  queue(const queue &other) = default;
  queue(queue &&other) = default;
  template <std::input_iterator InputIt>
  queue(InputIt first, InputIt last) : cont(first, last) {}

  ~queue() = default;

  queue &operator=(const queue &other) = default;
  queue &operator=(queue &&other) = default;

  // element access
  T &front() { return cont.front(); }
  const T &front() const { return cont.front(); }
  T &back() { return cont.back(); }
  const T &back() const { return cont.back(); }

  // capacity
  bool empty() const { return cont.empty(); }
  size_t size() const { return cont.size(); }

  // modifiers
  void push(const T &value) { cont.push_back(value); }
  void push(T &&value) { cont.push_back(std::move(value)); }
  template <typename... Args> void emplace(Args &&...args) {
    cont.emplace_back(std::forward<Args>(args)...);
  }
  void pop() { cont.pop_front(); }

  // non-member functions
  friend bool operator==(const queue &lhs, const queue &rhs) {
    return lhs.cont == rhs.cont;
  }

private:
  Container cont;
};

} // namespace nonstd
