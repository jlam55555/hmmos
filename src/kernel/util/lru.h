#pragma once

/// \file lru.h
/// \brief LRU cache
///
/// A simple LRU cache implementation intended for the page
/// cache. Cached elements are non-evictable (locked) until all leases
/// to that element are destroyed.
///
/// This should generally be treated as a transparent cache, i.e.,
/// clients should not need to manually check the existence of items
/// in the cache. However, there are two exceptions: (1) if the cache
/// is ever bypassed and cache coherency is required (O_DIRECT I/O
/// requires flushing/invalidating active page cache items), and (2)
/// to avoid unnecessary cache population when not necessary,
/// e.g. flush the page if and only if the page exists in the cache
/// and is dirty. This scenario can use the \a evict_one(Lease&&) API.
///
/// The provided \a populate_cache(Key) callback will be invoked just
/// before a missing item is added to the cache in \a get(). This can
/// be used by the client to perform the necessary setup (e.g., for
/// the page cache this means allocating a page frame and mapping it
/// into the page table). Similarly, the \a on_evict(Key, Value) hook
/// will be invoked just before eviction.
///

#include "nonstd/node_hash_map.h"
#include "util/assert.h"
#include "util/intrusive_list.h"
#include "util/objutil.h"

#include <cstddef>
#include <functional>

namespace util {

template <typename K, typename V> class LRUCache {
private:
  struct Node : IntrusiveListHead<Node> {
    template <std::convertible_to<K> _K, std::convertible_to<V> _V>
    Node(LRUCache<K, V> &_cache, _K &&_key, _V &&_val)
        : cache{_cache}, key{std::forward<_K>(_key)}, val{std::forward<_V>(
                                                          _val)} {}

    // TODO mutex
    LRUCache<K, V> &cache;
    int refcount = 0;
    K key;
    V val;
  };

public:
  /// Client's handle to a cached item. Holding a lease will prevent
  /// the element from being evicted.
  ///
  /// If you need to lock an item without holding onto a lease
  /// (e.g. managing leases for every page in a cached file is
  /// inefficient), you can use \ref inc_leases()/\ref dec_leases()
  /// manually. This needs more careful attention to avoid memory
  /// leaks.
  class Lease {
  public:
    Lease() = default;
    ~Lease() {
      if (!empty()) {
        dec_leases();
      }
    }

    NON_COPYABLE(Lease);
    Lease(Lease &&l) { *this = std::move(l); }
    Lease &operator=(Lease &&l) {
      std::swap(node, l.node);
      return *this;
    }
    bool empty() const { return node == nullptr; }

    // \pre For the followin functions, assumes \a !empty()
    V &value() const { return node->val; }
    operator V &() const { return value(); }
    size_t active_leases() const { return node->refcount; }

    void inc_leases() {
      if (++node->refcount == 1) {
        // Remove from evictable list.
        node->erase();
      }
    }
    void dec_leases() {
      if (--node->refcount == 0) {
        // Add to evictable list.
        node->cache.lru.push_front(*node);
      }
    }

  private:
    friend LRUCache<K, V>;
    explicit Lease(Node &_node) : node{&_node} { inc_leases(); }
    Node *node = nullptr;
  };

  /// \param _populate_cache logic to populate the cache item, if not
  ///                        present. This is also used as a
  ///                        notification of cache population -- if it
  ///                        is invoked, the cache item will
  ///                        immediately be added to the cache (but
  ///                        will not be in the cache at the time of
  ///                        the invocation)
  /// \param _on_evict callback to notify the client of a cache
  ///                  eviction
  LRUCache(std::function<V(const K &)> _populate_cache,
           std::function<void(const K &, const V &)> _on_evict)
      : populate_cache{_populate_cache}, on_evict{_on_evict} {}

  /// Lookup an item in the cache. If it doesn't exist, atomically
  /// fetch the contents and insert it into the map.
  ///
  /// "Atomic" means that if one thread has started fetching/evicting
  /// an element, any later accesses to \ref get() will wait for the
  /// first to complete.
  ///
  /// \param insert_if_missing if false, returns an empty @ref Lease
  ///                          if the item is not present in the
  ///                          cache. This should only be used for
  ///                          O_DIRECT I/O.
  ///
  /// TODO: actually implement this atomicity. get() and Lease
  /// operations should hold a mutex on the Node object.
  template <std::equality_comparable_with<K> _K>
  Lease get(_K &&key, bool insert_if_missing = true) {
    auto it = lookup.find(key);
    if (it == lookup.end()) {
      if (insert_if_missing) {
        it = lookup.try_emplace(key, *this, key, populate_cache(key)).first;
      } else {
        return {};
      }
    }
    return Lease{it->second};
  }

  /// Convenience notation for \ref get(). Looks like a map lookup.
  template <std::equality_comparable_with<K> _K> Lease operator[](_K &&key) {
    return get(std::forward<_K>(key));
  }

  /// Returns the number of elements present in the cache.
  size_t size() const { return lookup.size(); }

  /// Returns the number of evictable items present in the cache.
  size_t evictable() const { return lru.size(); }

  /// Attempt to evict items until \a size_hint items are remaining.
  ///
  /// This interface doesn't allow you to choose which elements to
  /// evict (that would require revoking all active leases).
  ///
  /// \param size_hint number of items to keep in the cache. The
  ///                  actual remaining size may be higher if there
  ///                  are no more evictable items left.
  /// \return number of evicted items
  size_t evict(size_t size_hint) {
    size_t evicted = 0;
    while (lookup.size() > size_hint && !lru.empty()) {
      evict_one(lru.prev());
      ++evicted;
    }
    return evicted;
  }

  /// Evict a single item, not in LRU order. E.g., for use by O_DIRECT
  /// I/O flushing.
  bool evict_one(Lease &&lease) {
    if (lease.node->refcount != 1) {
      // Not evictable.
      return false;
    }
    Node &node = *lease.node;
    lease.~Lease();
    evict_one(node);
    return true;
  }

private:
  void evict_one(Node &node) {
    ASSERT(node.refcount == 0);
    on_evict(node.key, node.val);
    node.erase();
    lookup.erase(node.key);
  }

  /// All items in the cache, evictable or not.
  nonstd::node_hash_map<K, Node> lookup;

  /// Evictable items in the cache, arranged in access (LRU) order.
  /// Everything in this list should have refcount 0.
  IntrusiveListHead<Node> lru;

  std::function<V(const K &)> populate_cache;
  std::function<void(const K &, const V &)> on_evict;
};

} // namespace util
