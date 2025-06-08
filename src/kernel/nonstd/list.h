#pragma once

#include "nonstd/allocator.h"
#include "util/assert.h"
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <utility>

namespace nonstd {

/// \brief std::list implementation
///
/// I thought I was clever with the end iterators but it turns out I
/// re-re-discovered circular linked lists.
template <typename T> class list {
  struct Node;
  using Allocator = Mallocator<Node>;
  template <typename U> class IterImpl;

public:
  using iterator = IterImpl<T>;
  using const_iterator = IterImpl<const T>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  list() = default;
  explicit list(size_t count) { insert(end(), count, T{}); }
  list(size_t count, const T &value) { insert(end(), count, value); }
  template <std::input_iterator InputIt> list(InputIt first, InputIt last) {
    insert(end(), first, last);
  }

  list(const list &other) : list(other.begin(), other.end()) {}
  list(list &&other) { *this = std::move(other); }
  list(std::initializer_list<T> init) : list(init.begin(), init.end()) {}

  ~list() { clear(); }

  list &operator=(const list &other) {
    *this = list(other);
    return *this;
  }
  list &operator=(list &&other) {
    std::swap(_size, other._size);
    // This is tricky: save these since we'll overwrite _back on the
    // next swap if the list is empty.
    auto *b1 = _back, *b2 = other._back;
    std::swap(_front->prev, other._front->prev);
    std::swap(b1->next, b2->next);
    std::swap(_front, other._front);
    std::swap(_back, other._back);
    return *this;
  }

  // element access
  T &front() { return _front->val; }
  const T &front() const { return _front->val; }
  T &back() { return _back->val; }
  const T &back() const { return _back->val; }

  // iterators
  iterator begin() { return empty() ? end() : iterator{_front}; }
  const_iterator begin() const {
    return empty() ? end() : const_iterator{_front};
  }
  iterator end() {
    // This is the biggest-brain hack: the end iterator is the "Node"
    // with address &_back. This is not actually a valid node (no \a
    // val nor \a next so can't be dereferenced or incremented), but
    // it can be dereferenced and compared for equality. This depends
    // on the fact that Node's prefix is a backpointer, and avoids the
    // need for a sentinel node.
    //
    // In other words, iterator{_back} is the iterator for the last
    // element, iterator{&_back} is the end iterator.
    return iterator{reinterpret_cast<Node *>(&_back)};
  }
  const_iterator end() const {
    return const_iterator{
        reinterpret_cast<Node *>(const_cast<Node **>(&_back))};
  }
  reverse_iterator rbegin() { return std::make_reverse_iterator(end()); }
  const_reverse_iterator rbegin() const {
    return std::make_reverse_iterator(end());
  }
  reverse_iterator rend() { return std::make_reverse_iterator(begin()); }
  const_reverse_iterator rend() const {
    return std::make_reverse_iterator(begin());
  }

  // capacity
  bool empty() const { return _size == 0; }
  size_t size() const { return _size; }

  // modifiers
  void clear() {
    for (auto it = begin(); it != end(); it = erase(it)) {
    }
  }
  iterator insert(const_iterator pos, const T &value) {
    return emplace(pos, value);
  }
  iterator insert(const_iterator pos, T &&value) {
    return emplace(pos, std::move(value));
  }
  iterator insert(const_iterator pos, size_t count, const T &value) {
    iterator rv{pos.node};
    for (size_t i = 0; i < count; ++i) {
      if (auto tmp = insert(pos, value); rv == pos) {
        rv = tmp;
      }
    }
    return rv;
  }
  template <std::input_iterator InputIt>
  iterator insert(const_iterator pos, InputIt first, InputIt last) {
    iterator rv{pos.node};
    while (first != last) {
      if (auto tmp = insert(pos, *first++); rv == pos) {
        rv = tmp;
      }
    }
    return rv;
  }
  iterator insert(const_iterator pos, std::initializer_list<T> ilist) {
    return insert(pos, ilist.begin(), ilist.end());
  }
  template <typename... Args>
  iterator emplace(const_iterator pos, Args &&...args) {
    Node &next = *pos.node;
    Node *prev = next.prev;
    auto *new_node = Allocator{}.allocate(1);
    ::new (new_node) Node{std::forward<Args>(args)...};
    new_node->prev = prev;
    new_node->next = &next;
    next.prev = new_node;
    prev->next = new_node;
    ++_size;
    return iterator{new_node};
  }
  iterator erase(const_iterator pos) {
    Node &cur = *pos.node;
    Node &next = *pos.node->next;
    // NB: This still works if next is end() or prev is rend().
    next.prev = cur.prev;
    cur.prev->next = &next;
    cur.~Node();
    Allocator{}.deallocate(&cur, 1);
    --_size;
    return iterator{&next};
  }
  void push_back(const T &value) { emplace_back(value); }
  void push_back(T &&value) { emplace_back(std::move(value)); }
  template <typename... Args> void emplace_back(Args &&...args) {
    emplace(end(), std::forward<Args>(args)...);
  }
  void pop_back() { erase(std::prev(end())); }
  void push_front(const T &value) { emplace_front(value); }
  void push_front(T &&value) { emplace_front(std::move(value)); }
  template <typename... Args> void emplace_front(Args &&...args) {
    emplace(begin(), std::forward<Args>(args)...);
  }
  void pop_front() { erase(begin()); }

  // operations
  void splice(const_iterator pos, list &other) {
    splice_impl(pos, other, other.begin(), other.end(), other.size());
  }
  void splice(const_iterator pos, list &&other) { splice(pos, other); }
  void splice(const_iterator pos, list &other, const_iterator it) {
    splice_impl(pos, other, it, std::next(it), 1);
  }
  void splice(const_iterator pos, list &&other, const_iterator it) {
    splice(pos, other, it);
  }
  void splice(const_iterator pos, list &other, const_iterator first,
              const_iterator last) {
    splice_impl(pos, other, first, last, std::distance(first, last));
  }
  void splice(const_iterator pos, list &&other, const_iterator first,
              const_iterator last) {
    splice(pos, other, first, last);
  }

private:
  struct Node {
    // NB: specifically initialize val with () not {} so we don't
    // force its std::initializer_list constructor.
    Node(auto &&...args) : val(args...) {}
    Node(const Node &other) = delete;
    Node(Node &&other) = delete;
    Node &operator=(const Node &other) = delete;
    Node &operator=(Node &&other) = delete;

    Node *prev = nullptr;
    Node *next = nullptr;
    T val;
  };

  template <typename U> class IterImpl {
  public:
    using difference_type = std::ptrdiff_t;
    using value_type = U;
    using pointer = value_type *;
    using reference = value_type &;
    using iterator_category = std::bidirectional_iterator_tag;

    // std::input_or_output_iterator
    value_type &operator*() const { return node->val; }
    value_type *operator->() const { return &node->val; }

    // std::forward_iterator
    IterImpl() = default;
    bool operator==(IterImpl other) const { return node == other.node; }
    IterImpl &operator++() {
      node = node->next;
      return *this;
    }
    IterImpl operator++(int) {
      IterImpl rv = *this;
      ++*this;
      return rv;
    }

    // std::bidirectional_iterator
    IterImpl &operator--() {
      node = node->prev;
      return *this;
    }
    IterImpl operator--(int) {
      IterImpl rv = *this;
      --*this;
      return rv;
    }

    // implicit const_iterator conversion
    operator auto() const { return IterImpl<const U>(node); }

  private:
    friend list;
    explicit IterImpl(Node *_node) : node{_node} {}
    Node *node = nullptr;
  };
  static_assert(std::bidirectional_iterator<iterator>);
  static_assert(std::bidirectional_iterator<const_iterator>);
  static_assert(std::bidirectional_iterator<reverse_iterator>);
  static_assert(std::bidirectional_iterator<const_reverse_iterator>);

  void splice_impl(const_iterator pos, list &other, const_iterator first,
                   const_iterator last,
                   typename const_iterator::difference_type n);

  // NB: These Node pointers are arranged in a deliberate order. This
  // looks like the prefix of a special Node, which is used to
  // implement both the end() and rend() iterators.
  //
  // Initially _back==end() and _front==rend(). This is similar to
  // Linux's circular intrusive linked list implementation
  // (LIST_HEAD).
  Node *_back = end().node;
  Node *_front = _back;
  size_t _size = 0;
};

// non-member functions
template <typename T> bool operator==(const list<T> &lhs, const list<T> &rhs) {
  return lhs.size() == rhs.size() &&
         std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

// template function definitions
template <typename T>
inline void list<T>::splice_impl(const_iterator pos, list &other,
                                 const_iterator first, const_iterator last,
                                 typename const_iterator::difference_type n) {
  // These nodes might be overwritten by swaps.
  auto *prev = pos.node->prev;
  auto *first_prev = first.node->prev;
  auto *last_prev = last.node->prev;
  // Fixup prev/next pointers for first/last.
  std::swap(first.node->prev, pos.node->prev);
  std::swap(last_prev->next, prev->next);
  // Fixup next/prev pointers for prev/pos,
  // and next/prev pointers for first_prev,last.
  std::swap(prev->next, first_prev->next);
  std::swap(pos.node->prev, last.node->prev);
  _size += n;
  other._size -= n;
}

} // namespace nonstd
