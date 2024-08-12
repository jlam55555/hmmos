#pragma once

/// \file
/// \brief Circular intrusive list implementation, inspired by Linux's
/// LIST_HEAD

#include "util/assert.h"
#include <cstddef>
#include <iterator>

namespace util {

/// \brief A C++ version of the common linked list data structure (see
/// `man 3 list`).
///
/// In its well-known form, macros and `offsetof()` are used to for
/// initialization and iteration.
///
/// Like the macro version, this is an intrusive, circular,
/// doubly-linked list. Unlike the macro version, this only supports
/// homogeneous nodes -- you can't interpret the `IntrusiveListHead`
/// as being a member of different parent nodes. However, we
/// shouldn't really be doing that anyways.
///
/// `IntrusiveListHead`s **assume they are data members of the parent
/// node**. Constructing an `IntrusiveListHead` explicitly (i.e., not
/// as a base class of `Parent`) is allowed but only should be used to
/// construct sentinel nodes, and care should be taken to not downcast
/// these to objects of type `Parent`.
///
/// `IntrusiveListHead`s (just like the list macros, or ordinary
/// non-intrusive lists) allow multiple participation. That is, the
/// parent node can inherit from (contain) multiple
/// `IntrusiveListHead`s, which must be disambiguated using different
/// tags. For example:
///
///     using MLPN = struct MultiParticipantListNode;
///     using MLPNList1 = IntrusiveListHead<MLPN, struct Tag1>;
///     using MLPNList2 = IntrusiveListHead<MLPN, struct Tag2>;
///     struct MultiParticipantListNode
///         : public MLPNList1, public MLPNList2 {};
///
///     // Sentinel nodes; these are only the `IntrusiveListHead`
///     // subobject.
///     MLPNList1 list1;
///     MLPNList2 list2;
///
///     // Regular parent node. These can be inserted into a
///     // MLPNList1 and a MLPNList2 linked list.
///     MLPN node1, node2;
///
///     list1.push_front(node1);
///     node1.MLPNList1::push_front(node2);
///     list2.push_front(node2);
///
///     // (list1) -> node1 -> node2 -> (list1)
///     // (list2) -> node2 -> (list2)
///     assert(list1.size() == 2);
///     assert(list2.size() == 1);
///
///     assert(node1.MLPNList1::size() == 2);
///     assert(node1.MLPNList2::size() == 0);
///     assert(node2.MLPNList1::size() == 2);
///     assert(node2.MLPNList2::size() == 1);
///
///
/// For comparison, see folly/boost's IntrusiveListHook/IntrusiveList
/// implementation. This uses pointer-to-data-members and requires an
/// explicit declaration of the list type rather than this approach,
/// which uses inheritance. The benefit of that approach is it's not
/// much more verbose, and allows the "list hook" (similar to a
/// `struct list_head`) to be in an arbitrary location in the object
/// rather than in the header. It's possible we'll need this down the
/// road if we care about struct layouts, but this should be
/// sufficient for the basic lists we need.
///
template <typename Parent, typename Tag = void> class IntrusiveListHead {
  using ListHead = IntrusiveListHead<Parent, Tag>;

public:
  struct Iterator {
    using value_type = Parent;
    using difference_type = std::ptrdiff_t;

    Iterator() = default;

    Parent &operator*() const {
      // Not really sure why this has to be (or is allowed to be)
      // const, but that's required by std::indirectly_readable.
      return it->parent();
    }
    Parent *operator->() const { return &it->parent(); }

    Iterator &operator++() {
      it = &it->next();
      return *this;
    }
    Iterator operator++(int) {
      Iterator rval = this;
      it = &it->next();
      return rval;
    }
    Iterator &operator--() {
      it = &it->prev();
      return *this;
    }
    Iterator operator--(int) {
      Iterator rval = this;
      it = &it->prev();
      return rval;
    }

    bool operator<=>(const Iterator &) const = default;

  private:
    friend ListHead;
    Iterator(ListHead *_it) : it{_it} {}
    ListHead *it = nullptr;
  };
  static_assert(std::bidirectional_iterator<Iterator>);

  Iterator begin() { return &next(); }
  Iterator end() { return this; }

  /// \brief Construct an empty list.
  ///
  /// This is either for a sentinel node that doesn't contain any
  /// elements, or for a non-sentinel node that has not been added to
  /// any lists.
  IntrusiveListHead() = default;

  /// \brief Construct an IntrusiveListNode from an iterator range of
  /// `Parent` objects.
  ///
  /// See comment about stable addresses for `insert_back()`.
  ///
  /// \return the sentinel node
  template <std::forward_iterator It>
  IntrusiveListHead(const It &begin, const It &end) {
    insert_back(begin, end);
  }

  /// \brief Append elements from a different range of `Parent`
  /// objects.
  ///
  /// Addresses of iterator elements are assumed to be stable and
  /// outlive the lifetime of any elements in the intrusive list.
  template <std::forward_iterator It>
  void insert_back(const It &begin, const It &end) {
    for (auto it = begin; it != end; ++it) {
      push_back(*it);
    }
  }

  // Get the next or previous node. See note above `parent()` about
  // dereferencing a parent.
  Parent &next() { return *_next; }
  const Parent &next() const { return *_next; }
  Parent &prev() { return *_prev; }
  const Parent &prev() const { return *_prev; }

  /// \brief Get element at n-th index.
  ///
  /// Due to the nature of the sentinel node, indices are
  /// 1-indexed. Negative indices start from the back of the array. 0
  /// indicates the current node, although that's probably not if
  /// you're calling this from the sentinel node.
  ///
  /// Note that there is no check for the sentinel node, which is
  /// treated as any other node in the linked list.
  Parent &at(int n) {
    return const_cast<Parent &>(const_cast<const ListHead *>(this)->at(n));
  }
  const Parent &at(int n) const {
    auto *rval = this;
    while (n > 0) {
      // Note implicit upcast here and in the following loop.
      rval = &rval->next();
      --n;
    }
    while (n < 0) {
      rval = &rval->prev();
      ++n;
    }
    return rval->parent();
  }

  /// \brief Check if circular list has no elements other than the
  /// sentinel.
  ///
  /// \return true if `size() == 0`.
  bool empty() const {
    DEBUG_ASSERT(!((&next() == &parent()) ^ (&prev() == &parent())));
    return &next() == &parent();
  }

  /// \brief Remove the current node from the linked list.
  ///
  /// \return an iterator for the following element
  Iterator erase() {
    if (empty()) {
      return begin();
    }
    get_list_head(next())._prev = &prev();
    auto *rval = get_list_head(prev())._next = &next();
    _next = &parent();
    _prev = &parent();
    return rval;
  }

  /// Insert a node after the current node.
  void push_front(Parent &p) {
    get_list_head(p).erase();
    get_list_head(p)._prev = &parent();
    get_list_head(p)._next = &next();
    get_list_head(next())._prev = &p;
    _next = &p;
  }

  /// Insert a node before the current node.
  void push_back(Parent &p) {
    get_list_head(p).erase();
    get_list_head(p)._prev = &prev();
    get_list_head(p)._next = &parent();
    get_list_head(prev())._next = &p;
    _prev = &p;
  }

  /// Length of this list, not including the sentinel node.
  size_t size() const {
    size_t rval = 0;
    const auto *ptr = this, *start = ptr;
    // Note the implicit upcast from Parent* to IntrusiveListHead*.
    while ((ptr = &ptr->next()) != start) {
      ++rval;
    }
    return rval;
  }

  /// Delete all elements from a list.
  void clear() {
    for (auto it = begin(); !it->empty(); it = it->erase()) {
    }
  }

private:
  /// Get parent node via an explicit downcast. It's always safe to
  /// call `get_list_head()` on the result (upcasting it back to an
  /// `IntrusiveListHead`), but it's not safe to treat the result as a
  /// `Parent` object unless the downcast is valid. In particular,
  /// treating the result as a full parent node for a standalone
  /// IntrusiveListHead is an error since it isn't part of a `Parent`
  /// node.
  ///
  /// While it's safe to upcasts a `Parent&` object to an
  /// `IntrusiveListHead`, this is already skirting the line of UB and
  /// depends on the programmer to handle the result properly.
  Parent &parent() { return static_cast<Parent &>(*this); }
  const Parent &parent() const { return static_cast<const Parent &>(*this); }

  /// Used for disambiguation between different `IntrusiveListHead`s
  /// when the node participates in multiple intrusive lists.
  ///
  /// This is effectively a static cast, and we don't need it if the
  /// context already requires the correct type (i.e., assigning to an
  /// `IntrusiveListHead *` object of the correct type..
  ListHead &get_list_head(Parent &t) { return t; }
  const ListHead &get_list_head(const Parent &t) const { return t; }

  Parent *_next{&parent()};
  Parent *_prev{&parent()};
};

} // namespace util
