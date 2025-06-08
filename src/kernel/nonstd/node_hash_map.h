#pragma once

#include "libc_minimal.h"
#include "nonstd/allocator.h"
#include "nonstd/hash_bytes.h"
#include "nonstd/list.h"
#include "util/assert.h"
#include <algorithm>
#include <initializer_list>
#include <tuple>
#include <type_traits>

namespace nonstd {

/// Like absl::node_hash_map or std::unordered_map. This guarantees
/// pointer/reference stability unlike absl::flat_hash_map.
template <typename K, typename V> class node_hash_map {
  template <typename U> class IterImpl;
  struct Node;
  using _value_type = std::pair<const K, V>;

public:
  using iterator = IterImpl<Node>;
  using const_iterator = IterImpl<const Node>;
  using Allocator = Mallocator<typename list<Node>::iterator>;

  node_hash_map() : node_hash_map(0) {}
  node_hash_map(size_t bucket_count)
      : _bucket_count{next_bucket_count(bucket_count)},
        _buckets{Allocator{}.allocate(_bucket_count)} {
    for (size_t i = 0; i < _bucket_count; ++i) {
      ::new (&_buckets[i]) node_iterator;
    }
  }
  template <std::input_iterator InputIt>
  node_hash_map(InputIt first, InputIt last, size_t bucket_count = 0)
      : node_hash_map{bucket_count} {
    while (first != last) {
      insert(*first++);
    }
  }
  node_hash_map(std::initializer_list<_value_type> init,
                size_t bucket_count = 0)
      : node_hash_map{init.begin(), init.end(), bucket_count} {}

  node_hash_map(const node_hash_map &other)
      : node_hash_map{other.begin(), other.end()} {}
  node_hash_map(node_hash_map &&other) noexcept : node_hash_map{} {
    *this = std::move(other);
  }

  ~node_hash_map() {
    clear();
    Allocator{}.deallocate(_buckets, _bucket_count);
  }

  node_hash_map &operator=(const node_hash_map &other) {
    *this = node_hash_map{other};
    return *this;
  }
  node_hash_map &operator=(node_hash_map &&other) noexcept {
    using std::swap; // enable ADL for swapping _data
    swap(_bucket_count, other._bucket_count);
    swap(_buckets, other._buckets);
    swap(_data, other._data);
    return *this;
  }

  // iterators
  iterator begin() { return iterator{_data.begin()}; }
  const_iterator begin() const { return const_iterator{_data.begin()}; }
  iterator end() { return iterator{_data.end()}; }
  const_iterator end() const { return const_iterator{_data.end()}; }

  // capacity
  bool empty() const { return _data.empty(); }
  size_t size() const { return _data.size(); }

  // modifiers
  void clear() {
    for (auto it = begin(); it != end(); it = erase(it)) {
    }
  }
  std::pair<iterator, bool> insert(const _value_type &val) {
    return emplace(val);
  }
  std::pair<iterator, bool> insert(_value_type &&val) {
    return emplace(std::move(val));
  }
  template <typename... Args> std::pair<iterator, bool> emplace(Args &&...args);
  template <std::equality_comparable_with<K> _K, typename... Args>
  std::pair<iterator, bool> try_emplace(_K &&key, Args &&...args);
  iterator erase(const_iterator pos);
  template <std::equality_comparable_with<K> _K> iterator erase(_K &&key) {
    if (auto it = find(std::forward<_K>(key)); it != end()) {
      erase(it);
    }
  }

  // lookup
  template <std::equality_comparable_with<K> _K> V &at(_K &&key) {
    if (auto it = find(std::forward<_K>(key)); it != end()) {
      return it->second;
    }
    ASSERT(false);
  }
  template <std::equality_comparable_with<K> _K> const V &at(_K &&key) const {
    if (auto it = find(std::forward<_K>(key)); it != end()) {
      return it->second;
    }
    ASSERT(false);
  }
  template <std::equality_comparable_with<K> _K> V &operator[](_K &&key) {
    auto [it, _] = try_emplace(std::forward<_K>(key));
    return it->second;
  }
  template <std::equality_comparable_with<K> _K> size_t count(_K &&key) const {
    return size_t(contains(std::forward<_K>(key)));
  }
  template <std::equality_comparable_with<K> _K> iterator find(_K &&key);
  template <std::equality_comparable_with<K> _K>
  const_iterator find(_K &&key) const;
  template <std::equality_comparable_with<K> _K> bool contains(_K &&key) const {
    return find(std::forward<_K>(key)) != end();
  }

  // bucket interface
  size_t bucket_count() const { return _bucket_count; }

  // hash policy
  void rehash(size_t count);
  void reserve(size_t count) {
    // TODO: support load factors other than 1
    rehash(count);
  }

private:
  struct Node {
    template <typename... Args>
    Node(size_t _hash, Args &&...args)
        : hash{_hash}, val(std::forward<Args>(args)...) {}

    size_t hash;
    _value_type val;
  };

  // a wrapper around the nonstd::list iterator
  template <typename U> class IterImpl {
  public:
    using difference_type = std::ptrdiff_t;
    using value_type =
        std::conditional_t<std::is_const_v<U>, const _value_type, _value_type>;
    using pointer = value_type *;
    using reference = value_type &;
    using iterator_category = std::forward_iterator_tag;

    // std::input_or_output_iterator
    value_type &operator*() const { return it->val; }
    value_type *operator->() const { return &it->val; }

    // std::forward_iterator
    IterImpl() = default;
    bool operator==(IterImpl other) const { return it == other.it; }
    IterImpl &operator++() {
      ++it;
      return *this;
    }
    IterImpl operator++(int) {
      IterImpl rv = *this;
      ++*this;
      return rv;
    }

    // implicit const_iterator conversion
    operator auto() const { return IterImpl<const U>{it}; }

  private:
    using BaseIt = std::conditional_t<
        std::is_const_v<U>,
        typename list<std::remove_const_t<U>>::const_iterator,
        typename list<U>::iterator>;
    friend class IterImpl<std::remove_const_t<U>>;
    friend class node_hash_map;

    explicit IterImpl(const BaseIt &_node) : it{_node} {}
    BaseIt it;
  };
  static_assert(std::forward_iterator<iterator>);
  static_assert(std::forward_iterator<const_iterator>);

  using node_iterator = typename list<Node>::iterator;

  size_t get_bucket(size_t hash) const { return hash % _bucket_count; }
  bool same_bucket(node_iterator it, size_t bucket) const {
    return it != _data.end() && get_bucket(it->hash) == bucket;
  }

  constexpr size_t next_bucket_count(size_t min_buckets);

  size_t _bucket_count = 0;
  // TODO: use forward_list to save 4 bytes per element
  node_iterator *_buckets = nullptr;
  list<Node> _data;
};

// non-member functions
template <typename K, typename V>
bool operator==(const node_hash_map<K, V> &lhs,
                const node_hash_map<K, V> &rhs) {
  return lhs.size() == rhs.size() &&
         std::all_of(lhs.begin(), lhs.end(), [&rhs](auto &kv) {
           auto it = rhs.find(kv.first);
           return it != rhs.end() && it->second == kv.second;
         });
}

// template function definitions
template <typename K, typename V>
template <typename... Args>
auto node_hash_map<K, V>::emplace(Args &&...args) -> std::pair<iterator, bool> {
  // Note: this always constructs a node (even if it's not
  // inserted), AND it's always moved if it's inserted. It should be
  // constructed in-place to avoid the move (but we can't avoid the
  // immediate deletion if it's not inserted).
  Node node{/*hash=*/0, std::forward<Args>(args)...};
  node.hash = hash<K>{}(node.val.first);

  const size_t idx = get_bucket(node.hash);
  auto &bucket = _buckets[idx];
  if (bucket == node_iterator{}) {
    bucket = _data.begin();
  } else {
    for (auto it = bucket; same_bucket(it, idx); ++it) {
      auto &[k, v] = it->val;
      if (k == node.val.first) {
        v = std::move(node.val.second);
        return {iterator{it}, false};
      }
    }
  }
  bucket = _data.emplace(bucket, std::move(node));

  // Buckets may be altered during rehash (but iterators shouldn't
  // be invalidated), save retval here.
  const auto rv = bucket;
  rehash(size());
  return {iterator{rv}, true};
}

template <typename K, typename V>
template <std::equality_comparable_with<K> _K, typename... Args>
auto node_hash_map<K, V>::try_emplace(_K &&key, Args &&...args)
    -> std::pair<iterator, bool> {
  const size_t _hash = hash<std::remove_reference_t<_K>>{}(key);
  const size_t idx = get_bucket(_hash);
  auto &bucket = _buckets[idx];
  if (bucket == node_iterator{}) {
    bucket = _data.begin();
  } else {
    for (auto it = bucket; same_bucket(it, idx); ++it) {
      auto &[k, v] = it->val;
      if (k == key) {
        return {iterator{it}, false};
      }
    }
  }
  bucket =
      _data.emplace(bucket, _hash, std::piecewise_construct_t{},
                    std::forward_as_tuple(key), std::forward_as_tuple(args...));

  // Buckets may be altered during rehash (but iterators shouldn't
  // be invalidated), save retval here.
  const auto rv = bucket;
  rehash(size());
  return {iterator{rv}, true};
}

template <typename K, typename V>
auto node_hash_map<K, V>::erase(const_iterator pos) -> iterator {
  const size_t idx = get_bucket(pos.it->hash);
  auto &bucket = _buckets[idx];
  ASSERT(bucket != node_iterator{});
  if (iterator{bucket} == pos) {
    // This is the first element in this bucket. We need to point
    // the bucket to the next element if it exists, otherwise delete
    // the bucket.
    if (!same_bucket(++bucket, idx)) {
      bucket = node_iterator{};
    }
  }
  return iterator{_data.erase(pos.it)};
}

template <typename K, typename V>
template <std::equality_comparable_with<K> _K>
auto node_hash_map<K, V>::find(_K &&key) -> iterator {
  const size_t _hash =
      hash<std::remove_reference_t<_K>>{}(std::forward<_K>(key));
  const size_t idx = get_bucket(_hash);
  const auto bucket = _buckets[idx];
  if (bucket == node_iterator{}) {
    return end();
  }
  for (auto it = bucket; same_bucket(it, idx); ++it) {
    if (key == it->val.first) {
      return iterator{it};
    }
  }
  return end();
}

template <typename K, typename V>
template <std::equality_comparable_with<K> _K>
auto node_hash_map<K, V>::find(_K &&key) const -> const_iterator {
  const size_t _hash =
      hash<std::remove_reference_t<_K>>{}(std::forward<_K>(key));
  const size_t idx = get_bucket(_hash);
  const auto bucket = _buckets[idx];
  if (bucket == node_iterator{}) {
    return end();
  }
  for (auto it = bucket; same_bucket(it, idx); ++it) {
    if (key == it->val.first) {
      return iterator{it};
    }
  }
  return end();
}

template <typename K, typename V>
void node_hash_map<K, V>::rehash(size_t count) {
  if (count <= _bucket_count) {
    return;
  }

  node_hash_map tmp{count};
  auto it = _data.begin();
  while (it != _data.end()) {
    const size_t idx = tmp.get_bucket(it->hash);
    auto &bucket = tmp._buckets[idx];
    if (bucket == node_iterator{}) {
      bucket = tmp._data.begin();
    }
    const auto next = std::next(it);
    tmp._data.splice(bucket, _data, it);
    bucket = it;
    it = next;
  }
  // Nit: more intuitive as a swap() operation
  *this = std::move(tmp);
}

template <typename K, typename V>
constexpr size_t node_hash_map<K, V>::next_bucket_count(size_t min_buckets) {
  // i = 16
  // while i < 2**32-1:
  //   while any(i*n==0 for n in range(2, int(ceil(sqrt(i))))): i+=1
  //   rv.append(i)
  //   i *= 2
  for (size_t candidate :
       {17U,        37U,        79U,         163U,       331U,      673U,
        1361U,      2729U,      5471U,       10949U,     21911U,    43853U,
        87719U,     175447U,    350899U,     701819U,    1403641U,  2807303U,
        5614657U,   11229331U,  22458671U,   44917381U,  89834777U, 179669557U,
        359339171U, 718678369U, 1437356741U, 2874713497U}) {
    if (candidate >= min_buckets) {
      return candidate;
    }
  }
  // Too large.
  ASSERT(false);
}

} // namespace nonstd
