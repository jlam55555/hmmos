#pragma once

#include "nonstd/allocator.h"
#include "util/assert.h"
#include <algorithm>
#include <cstdlib>
#include <iterator>

namespace nonstd {

template <typename T> class deque {
  /// Block sizing policy: I did this on vibes looking at gcc and
  /// clang's default policies. https://stackoverflow.com/q/57031917
  static constexpr size_t min_block_count = 16;
  static constexpr size_t block_size = std::max((size_t)512, 16 * sizeof(T));
  static constexpr size_t elems_per_block = block_size / sizeof(T);

  using InternalAllocator = Mallocator<T *>;
  using BlockAllocator = Mallocator<std::byte[block_size]>;
  template <typename U> class IterImpl;

public:
  using iterator = IterImpl<T>;
  using const_iterator = IterImpl<const T>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  deque()
      : _block_count{min_block_count},
        _blocks(InternalAllocator{}.allocate(_block_count)) {
    memset(_blocks, 0, sizeof(std::uintptr_t) * _block_count);
  }
  explicit deque(size_t count) : deque{} {
    for (size_t i = 0; i < count; ++i) {
      emplace(end());
    }
  }
  deque(size_t count, const T &value) : deque{} { insert(end(), count, value); }
  template <std::input_iterator InputIt>
  deque(InputIt first, InputIt last) : deque{} {
    insert(end(), first, last);
  }

  deque(const deque &other) : deque(other.begin(), other.end()) {}
  deque(deque &&other) : deque{} { *this = std::move(other); }
  deque(std::initializer_list<T> init) : deque(init.begin(), init.end()) {}

  ~deque() {
    clear();
    // \a shrink_to_fit() necessary to reclaim the blocks, which
    // should all be empty now.
    shrink_to_fit();
    InternalAllocator{}.deallocate(_blocks, _block_count);
#ifdef DEBUG
    for (size_t i = 0; i < _block_count; ++i) {
      ASSERT(_blocks[i] == nullptr);
    }
#endif
  }

  deque &operator=(const deque &other) {
    *this = deque(other);
    return *this;
  }
  deque &operator=(deque &&other) {
    swap(other);
    return *this;
  }
  deque &operator=(std::initializer_list<T> init) {
    *this = deque(init.begin(), init.end());
    return *this;
  }

  // element access
  T &at(size_t pos) {
    ASSERT(pos < size());
    return operator[](pos);
  }
  const T &at(size_t pos) const {
    ASSERT(pos < size());
    return operator[](pos);
  }
  T &operator[](size_t pos) {
    const auto [block_idx, elem_idx] =
        get_block_elem_idx(start_idx + pos, start_block, _block_count);
    return _blocks[block_idx][elem_idx];
  }
  const T &operator[](size_t pos) const {
    const auto [block_idx, elem_idx] =
        get_block_elem_idx(start_idx + pos, start_block, _block_count);
    return _blocks[block_idx][elem_idx];
  }
  T &front() { return at(0); }
  const T &front() const { return at(0); }
  T &back() { return at(size() - 1); }
  const T &back() const { return at(size() - 1); }

  // iterators
  iterator begin() { return make_iterator(0); }
  const_iterator begin() const { return make_iterator(0); }
  iterator end() { return make_iterator(size()); }
  const_iterator end() const { return make_iterator(size()); }
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
  void shrink_to_fit();

  // modifiers
  void clear() { erase(begin(), end()); }
  iterator insert(const_iterator pos, const T &value) {
    return emplace(pos, value);
  }
  iterator insert(const_iterator pos, T &&value) {
    return emplace(pos, std::move(value));
  }
  iterator insert(const_iterator pos, size_t count, const T &value);
  template <std::input_iterator InputIt>
  iterator insert(const_iterator pos, InputIt first, InputIt last);
  iterator insert(const_iterator pos, std::initializer_list<T> ilist) {
    return insert(pos, ilist.begin(), ilist.end());
  }
  template <typename... Args>
  iterator emplace(const_iterator pos, Args &&...args);
  iterator erase(const_iterator pos);
  iterator erase(const_iterator first, const_iterator last);
  void push_back(const T &value) { emplace_back(value); }
  void push_back(T &&value) { emplace_back(std::move(value)); }
  template <typename... Args> void emplace_back(Args &&...args) {
    emplace(end(), args...);
  }
  void pop_back() { erase(std::prev(end())); }
  void push_front(const T &value) { emplace_front(value); }
  void push_front(T &&value) { emplace_front(std::move(value)); }
  template <typename... Args> void emplace_front(Args &&...args) {
    emplace(begin(), args...);
  }
  void pop_front() { erase(begin()); }
  void resize(size_t count);
  void resize(size_t count, const T &value);
  void swap(deque &other) {
    using std::swap;
    swap(_blocks, other._blocks);
    swap(_block_count, other._block_count);
    swap(_size, other._size);
    swap(start_block, other.start_block);
    swap(start_idx, other.start_idx);
  }

private:
  template <typename U> class IterImpl {
  public:
    using difference_type = std::ptrdiff_t;
    using value_type = U;
    using pointer = value_type *;
    using reference = value_type &;
    using iterator_category = std::random_access_iterator_tag;

    // std::input_or_output_iterator
    value_type &operator*() const {
      const auto [block_idx, elem_idx] =
          get_block_elem_idx(idx, start_block, block_count);
      return blocks[block_idx][elem_idx];
    }
    value_type *operator->() const { return std::addressof(**this); }

    // std::forward_iterator
    IterImpl() = default;
    bool operator==(IterImpl other) const { return idx == other.idx; }
    IterImpl &operator++() {
      ++idx;
      return *this;
    }
    IterImpl operator++(int) {
      IterImpl rv = *this;
      ++*this;
      return rv;
    }

    // std::bidirectional_iterator
    IterImpl &operator--() {
      --idx;
      return *this;
    }
    IterImpl operator--(int) {
      IterImpl rv = *this;
      --*this;
      return rv;
    }

    // std::random_access_iterator
    auto operator<=>(IterImpl other) const { return idx <=> other.idx; }
    IterImpl &operator+=(difference_type n) {
      idx += n;
      return *this;
    }
    friend IterImpl operator+(IterImpl i, difference_type n) { return i += n; }
    friend IterImpl operator+(difference_type n, IterImpl i) { return i += n; }
    IterImpl &operator-=(difference_type n) {
      idx -= n;
      return *this;
    }
    friend IterImpl operator-(IterImpl i, difference_type n) { return i -= n; }
    difference_type operator-(IterImpl other) const { return idx - other.idx; }
    value_type &operator[](difference_type n) const { return *(*this + n); }

    // implicit const_iterator conversion
    operator auto() const requires(!std::is_const_v<T>) {
      return *reinterpret_cast<const IterImpl<const T> *>(this);
    }

  private:
    friend deque;
    explicit IterImpl(U **_blocks, size_t _block_count, size_t _start_block,
                      size_t _idx)
        : blocks{_blocks}, block_count{_block_count},
          start_block{_start_block}, idx{_idx} {}

    // Note we store a lot of state in this iterator.
    //
    // - This can _not_ be invalidated on \a deque::erase() if erasing
    //   the first or last element, hence we store the index with \a
    //   start_idx factored in.
    // - This must support a wrapped block array, so we need to store
    //   \a blocks, \a block_count, and \a start_block. Just \a
    //   start_block won't suffice.
    // - This shouldn't be invalidated on deque::swap(), so we can't
    //   keep a reference to the deque that contains it.
    // - This can be invalidated on (any) insertion, notably if \a
    //   start_block, \a block_count, and/or \a blocks changes.
    //
    // Luckily, the prefix increment/decrement/comparison operators
    // are fairly lightweight, since they assume that the other
    // members (which are only used for dereferencing) are constant
    // for non-iterator-invalidating operations.
    //
    // I'd like for these fields to be const, but this iterator must
    // satisfy \a weakly_incrementable which requires movability. That
    // only happens on post-increment, which is always less
    // performant.
    U **blocks;
    size_t block_count;
    size_t start_block;
    size_t idx;
  };
  static_assert(std::random_access_iterator<iterator>);
  static_assert(std::random_access_iterator<const_iterator>);
  static_assert(std::random_access_iterator<reverse_iterator>);
  static_assert(std::random_access_iterator<const_reverse_iterator>);

  /// Helper functions to create an iterator.
  iterator make_iterator(size_t idx) {
    return iterator{_blocks, _block_count, start_block, start_idx + idx};
  }
  const_iterator make_iterator(size_t idx) const {
    return iterator{_blocks, _block_count, start_block, start_idx + idx};
  }

  /// Allocate a new block at the start (if \a start) or end
  /// (otherwise).
  ///
  /// This will update \a _blocks, \a _block_count, and \a start_block
  /// if the blocks array was full. This will update start_block if \a
  /// start (but will not update \a start_idx).
  void allocate_block(bool start);

  /// Returns (block, element) index.
  static std::pair<size_t, size_t>
  get_block_elem_idx(size_t idx, size_t _start_block, size_t _block_count) {
    const std::ldiv_t block_elem = ldiv(idx, (long)elems_per_block);
    return std::pair{(_start_block + block_elem.quot) % _block_count,
                     block_elem.rem};
  }

  size_t _block_count = 0;

  /// Internal array of \a _block_count blocks.
  T **_blocks = nullptr;

  /// Number of elements stored. Resize of the internal array happens
  /// when (start_idx + _size) * elems_per_block >= _block_count.
  size_t _size = 0;

  /// Index of leftmost block. May not contain element with index 0 if
  /// \a start_idx >= \a elems_per_block. This is only updated when
  /// inserting at the beginning of the array or shrinking to fit,
  /// both of which invalidate all iterators.
  size_t start_block = 0;

  /// Index of first element, relative to start of \a start_block. May
  /// be >= \a elems_per_block if elements were popped from the front.
  size_t start_idx = 0;
};

// non-member functions
template <typename T>
bool operator==(const deque<T> &lhs, const deque<T> &rhs) {
  return lhs.size() == rhs.size() &&
         std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

// template function definitions
template <typename T> void deque<T>::shrink_to_fit() {
  // Deallocate extra blocks at the end.
  for (size_t i =
           (start_block + ((start_idx + _size - 1) / elems_per_block + 1)) %
           _block_count;
       i != start_block && _blocks[i] != nullptr; i = (i + 1) % _block_count) {
    BlockAllocator{}.deallocate(
        (typename BlockAllocator::value_type *)_blocks[i], 1);
    _blocks[i] = nullptr;
  }
  // Deallocate extra blocks at start.
  for (; start_idx >= elems_per_block;
       start_idx -= elems_per_block,
       start_block = (start_block + 1) % _block_count) {
    BlockAllocator{}.deallocate(
        (typename BlockAllocator::value_type *)_blocks[start_block], 1);
    _blocks[start_block] = nullptr;
  }
  // The above cases don't account for an empty deque. There's
  // probably a cleaner way to do this but whatevs.
  if (empty() && _blocks[start_block] != nullptr) {
    start_idx = 0;
    BlockAllocator{}.deallocate(
        (typename BlockAllocator::value_type *)_blocks[start_block], 1);
    _blocks[start_block] = nullptr;
  }
}

template <typename T>
auto deque<T>::insert(const_iterator pos, size_t count, const T &value)
    -> iterator {
  const size_t idx = pos.idx - start_idx;
  size_t cur_idx = idx;
  // Note: this fails if inserting count>1 and we're not inserting
  // at the back, since we don't currently support inserting in the
  // middle.
  for (size_t i = 0; i < count; ++i) {
    // Iterator is invalidated after insert, so we have to keep
    // re-creating it.
    insert(pos, value);
    pos = make_iterator(++cur_idx);
  }
  return make_iterator(idx);
}

template <typename T>
template <std::input_iterator InputIt>
auto deque<T>::insert(const_iterator pos, InputIt first, InputIt last)
    -> iterator {
  const size_t idx = pos.idx - start_idx;
  size_t cur_idx = idx;
  // Note: this fails if inserting multiple elements and we're not
  // inserting at the back, since we don't currently support
  // inserting in the middle.
  while (first != last) {
    insert(pos, *first++);
    pos = make_iterator(++cur_idx);
  }
  return make_iterator(idx);
}

template <typename T>
template <typename... Args>
auto deque<T>::emplace(const_iterator pos, Args &&...args) -> iterator {
  const size_t idx = pos.idx - start_idx;
  size_t i = 0;
  if (pos == end()) {
    if ((start_idx + size()) % elems_per_block == 0) {
      allocate_block(/*start=*/false);
    }
    i = size();
  } else if (pos == begin()) {
    if (start_idx == 0) {
      allocate_block(/*start=*/true);
      start_idx = elems_per_block;
    }
    --start_idx;
    i = 0;
  } else {
    // TODO: support insertions in the middle. This makes my life
    // easier and insertions in the middle are inefficient anyways.
    ASSERT(false);
  }
  ::new (std::addressof((*this)[i])) T(std::forward<Args>(args)...);
  ++_size;
  return make_iterator(idx);
}

template <typename T> auto deque<T>::erase(const_iterator pos) -> iterator {
  const size_t idx = pos.idx - start_idx;
  (*this)[idx].~T();
  if (pos == begin()) {
    ++start_idx;
  } else if (pos == std::prev(end())) {
    // pass
  } else {
    // TODO: support erasing from the middle
    ASSERT(false);
  }
  --_size;
  return make_iterator(idx);
}

template <typename T>
auto deque<T>::erase(const_iterator first, const_iterator last) -> iterator {
  const size_t idx = first.idx - start_idx;
  auto it = make_iterator(idx);
  if (it == last) {
    return it;
  }
  while ((it = erase(it)) != last) {
  }
  return it /* (== last) */;
}

template <typename T> void deque<T>::resize(size_t count) {
  ssize_t diff = count - _size;
  if (diff < 0) {
    // We can't use erase() since the current implementation doesn't
    // allow erasing from the middle.
    auto it = rbegin();
    for (ssize_t i = 0; i < -diff; ++i) {
      erase((++it).base());
    }
  } else {
    for (size_t i = 0; i < diff; ++i) {
      emplace_back();
    }
  }
}

template <typename T> void deque<T>::resize(size_t count, const T &value) {
  ssize_t diff = count - size();
  if (diff < 0) {
    // We can't use erase() since the current implementation doesn't
    // allow erasing from the middle.
    auto it = rbegin();
    for (ssize_t i = 0; i < -diff; ++i) {
      erase((++it).base());
    }
  } else {
    for (size_t i = 0; i < diff; ++i) {
      push_back(value);
    }
  }
}

template <typename T> void deque<T>::allocate_block(bool start) {
  const auto realloc_blocks_array = [&] {
    size_t new_block_count = _block_count * 2;
    T **new_blocks = InternalAllocator{}.allocate(new_block_count);

    // Copy _blocks[start_block, start_block+_block_count)
    // (potentially wrapped) to new_blocks[0, _block_count) (not
    // wrapped).
    for (size_t from = start_block, to = 0; to < _block_count;
         ++to, from = (from + 1) % _block_count) {
      ASSERT(_blocks[from] != nullptr);
      new_blocks[to] = _blocks[from];
    }
    std::fill(new_blocks + _block_count, new_blocks + new_block_count, nullptr);

    InternalAllocator{}.deallocate(_blocks, _block_count);
    _block_count = new_block_count;
    _blocks = new_blocks;
    start_block = 0;
  };

  if (start) {
    if (!start_idx && size() > elems_per_block * (_block_count - 1)) {
      // All blocks full, reallocate blocks array.
      realloc_blocks_array();
    }

    if (!start_idx) {
      // Allocate a new block.
      start_block = (start_block + _block_count - 1) % _block_count;
      _blocks[start_block] = (T *)BlockAllocator{}.allocate(1);
    }
  } else {
    const size_t end_idx = start_idx + size();
    if (end_idx >= elems_per_block * _block_count) {
      // All blocks full, reallocate blocks array.
      realloc_blocks_array();
    }

    const auto [block_idx, elem_idx] =
        get_block_elem_idx(end_idx, start_block, _block_count);
    T *&block = _blocks[block_idx];
    if (elem_idx == 0 && block == nullptr) {
      // Allocate a new block.
      block = (T *)BlockAllocator{}.allocate(1);
    }
  }
}

} // namespace nonstd
