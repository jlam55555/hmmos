#include "../test.h"
#include "util/intrusive_list.h"
#include <array>

namespace {

struct SimpleListNode;
using IntrusiveSimpleList = util::IntrusiveListHead<SimpleListNode>;
struct SimpleListNode : public IntrusiveSimpleList {
  SimpleListNode(int _val) : val{_val} {}
  int val;
};

using MLPN = struct MultiParticipantListNode;
using MLPNList1 = util::IntrusiveListHead<MLPN, struct Tag1>;
using MLPNList2 = util::IntrusiveListHead<MLPN, struct Tag2>;
struct MultiParticipantListNode : public MLPNList1, public MLPNList2 {
  MultiParticipantListNode(int _val) : val{_val} {}
  int val;
};

} // namespace

TEST_CLASS(util, IntrusiveList, empty) {
  IntrusiveSimpleList list;
  SimpleListNode node1{1}, node2{2};

  TEST_ASSERT(list.empty());
  TEST_ASSERT(node1.empty());
  TEST_ASSERT(node2.empty());

  list.push_back(node1);

  TEST_ASSERT(!list.empty());
  TEST_ASSERT(!node1.empty());
  TEST_ASSERT(node2.empty());
  TEST_ASSERT(list.size() == 1);

  // Note: this causes node2 to "steal" from the node1 linked list.
  node2.push_back(node1);

  TEST_ASSERT(list.empty());
  TEST_ASSERT(!node1.empty());
  TEST_ASSERT(!node2.empty());

  // At this point, the node1 <-> node2 <-> node1 linked list is
  // technically invalid since there's no sentinel node, so it'll
  // report as having size=1. There's not much distinction between
  // sentinel and real nodes, and it's up to the programmer to follow
  // the convention that there's always one sentinel node in the
  // linked list.
  TEST_ASSERT(node1.size() == 1);
  TEST_ASSERT(node2.size() == 1);
}

TEST_CLASS(util, IntrusiveList, push_back) {
  IntrusiveSimpleList list;
  SimpleListNode node1{1}, node2{2}, node3{3}, node4{4}, node5{5};

  TEST_ASSERT(list.empty());

  list.push_back(node1);
  list.push_back(node2);
  list.push_back(node3);
  list.push_back(node4);

  TEST_ASSERT(list.size() == 4);
  TEST_ASSERT(list.next().val == 1);
  TEST_ASSERT(list.next().next().val == 2);
  TEST_ASSERT(list.next().next().next().next().val == 4);
  TEST_ASSERT(list.next().prev().next().val == 1);

  // Same thing but using `at()`.
  TEST_ASSERT(list.at(1).val == 1);
  TEST_ASSERT(list.at(2).val == 2);
  TEST_ASSERT(list.at(3).val == 3);
  TEST_ASSERT(list.at(4).val == 4);

  // Push a node in the middle.
  node3.push_back(node5);
  TEST_ASSERT(list.size() == 5);
  TEST_ASSERT(node3.prev().val == 5);
  TEST_ASSERT(list.at(2).val == 2);
  TEST_ASSERT(list.at(3).val == 5);
  TEST_ASSERT(list.at(4).val == 3);
}

TEST_CLASS(util, IntrusiveList, push_front) {
  IntrusiveSimpleList list;
  SimpleListNode node1{1}, node2{2}, node3{3};

  TEST_ASSERT(list.empty());

  list.push_front(node1);
  list.push_front(node2);
  list.push_front(node3);

  TEST_ASSERT(list.at(1).val == 3);
  TEST_ASSERT(list.at(2).val == 2);
  TEST_ASSERT(list.at(3).val == 1);

  TEST_ASSERT(list.at(-1).val == 1);
  TEST_ASSERT(list.at(-2).val == 2);
  TEST_ASSERT(list.at(-3).val == 3);
}

TEST_CLASS(util, IntrusiveList, erase) {
  IntrusiveSimpleList list;
  SimpleListNode node1{1}, node2{2}, node3{3};

  TEST_ASSERT(list.empty() && node1.empty() && node2.empty() && node3.empty());

  list.push_back(node1);
  list.push_back(node2);
  list.push_back(node3);

  TEST_ASSERT(list.size() == 3);

  // Regular erase.
  node2.erase();

  TEST_ASSERT(list.size() == 2);
  TEST_ASSERT(list.at(1).val == 1);
  TEST_ASSERT(list.at(2).val == 3);

  // Erase a node that already doesn't belong to a list. (Should be a
  // no-op.)
  node2.erase();
  TEST_ASSERT(node2.empty());

  // Insert a node to itself. This should remove the node from any
  // lists it exists in and make it an empty node. (I mean this is
  // pretty useless behavior but it's good to be well-defined.)
  node2.push_back(node2);
  TEST_ASSERT(node2.empty());

  node1.push_back(node1);
  TEST_ASSERT(node1.empty());
  TEST_ASSERT(list.size() == 1);
  TEST_ASSERT(list.at(1).val == 3);
}

TEST_CLASS(util, IntrusiveList, push_many) {
  // Let's generate the following sequence of events.
  // (empty)
  // push_front:          1
  // push_back:           1 2
  // push_front:          3 1 2
  // push_back before 2:  3 1 4 2
  // erase 1:             3 4 2
  // erase 4:             3 2
  // push_front after 2:  3 2 5
  SimpleListNode nodes[] = {1, 2, 3, 4, 5};
  IntrusiveSimpleList list;

  list.push_front(nodes[0]);
  list.push_back(nodes[1]);
  list.push_front(nodes[2]);
  nodes[1].push_back(nodes[3]);
  nodes[0].erase();
  nodes[3].erase();
  nodes[1].push_front(nodes[4]);

  TEST_ASSERT(list.size() == 3);
  TEST_ASSERT(list.at(1).val == 3);
  TEST_ASSERT(list.at(2).val == 2);
  TEST_ASSERT(list.at(3).val == 5);
}

TEST_CLASS(util, IntrusiveList, insert_back) {
  auto nodes = std::to_array<SimpleListNode>({1, 2, 3, 4});
  IntrusiveSimpleList list;

  TEST_ASSERT(list.empty());

  list.insert_back(nodes.begin(), nodes.end());

  TEST_ASSERT(list.size() == 4);
  TEST_ASSERT(list.at(1).val == 1);
  TEST_ASSERT(list.at(2).val == 2);
  TEST_ASSERT(list.at(3).val == 3);
  TEST_ASSERT(list.at(4).val == 4);

  // Using the constructor that takes an iterator range should behave
  // similarly. Note that this will steal all elements from the first
  // list.
  IntrusiveSimpleList list2(nodes.begin(), nodes.end());

  TEST_ASSERT(list2.size() == 4);
  TEST_ASSERT(list2.at(1).val == 1);
  TEST_ASSERT(list2.at(2).val == 2);
  TEST_ASSERT(list2.at(3).val == 3);
  TEST_ASSERT(list2.at(4).val == 4);

  TEST_ASSERT(list.empty());
}

TEST_CLASS(util, IntrusiveList, reinsert) {
  // Reinserting an element onto a list will remove it from the list
  // and reinsert in the new location.
  auto nodes = std::to_array<SimpleListNode>({1, 2, 3, 4});
  IntrusiveSimpleList list(nodes.begin(), nodes.end());

  TEST_ASSERT(list.size() == 4);
  TEST_ASSERT(list.at(3).val == 3);

  list.push_front(nodes[2]);

  TEST_ASSERT(list.size() == 4);
  TEST_ASSERT(list.at(3).val == 2);
  TEST_ASSERT(list.at(1).val == 3);
}

TEST_CLASS(util, IntrusiveList, invariants) {
  auto nodes = std::to_array<SimpleListNode>({1, 5, 3, -2, 4, 1, 4});
  IntrusiveSimpleList list(nodes.begin(), nodes.end());

  auto *it = &list;
  for (int i = 0, sz = list.size(); i < sz; ++i, it = &it->next()) {
    if (i) {
      // Check values and `at()` for non-sentinel nodes.
      TEST_ASSERT(it == &list.at(i));
      TEST_ASSERT(nodes[i - 1].val == list.at(i).val);
    }
    // Check prev/next pointers.
    TEST_ASSERT(&it->prev().next() == it);
    TEST_ASSERT(&it->next().prev() == it);
  }
}

TEST_CLASS(util, IntrusiveList, iteration) {
  // `for range` loop depends on iterators.
  auto nodes = std::to_array<SimpleListNode>({1, 5, -3, 4});
  IntrusiveSimpleList list(nodes.begin(), nodes.end());

  int i = 0;
  for (auto &parent : list) {
    TEST_ASSERT(parent.val == nodes[i++].val);
  }
}

TEST_CLASS(util, IntrusiveList, safe_iteration) {
  // Allow deletions while iterating.
  auto nodes = std::to_array<SimpleListNode>({1, 2, 3, 4});
  IntrusiveSimpleList list(nodes.begin(), nodes.end());

  // Delete odd nodes.
  for (auto it = list.begin(), end = list.end(); it != end;) {
    if (it->val & 1) {
      it = it->erase();
    } else {
      ++it;
    }
  }

  TEST_ASSERT(list.size() == 2);
  TEST_ASSERT(list.at(1).val == 2);
  TEST_ASSERT(list.at(2).val == 4);
}

TEST_CLASS(util, IntrusiveList, clear) {
  auto nodes = std::to_array<SimpleListNode>({1, 2, 3, 4});
  IntrusiveSimpleList list(nodes.begin(), nodes.end());

  TEST_ASSERT(list.size() == 4);
  TEST_ASSERT(!nodes[0].empty());
  TEST_ASSERT(!nodes[1].empty());
  TEST_ASSERT(!nodes[2].empty());
  TEST_ASSERT(!nodes[3].empty());

  list.clear();

  TEST_ASSERT(list.empty());
  TEST_ASSERT(nodes[0].empty());
  TEST_ASSERT(nodes[1].empty());
  TEST_ASSERT(nodes[2].empty());
  TEST_ASSERT(nodes[3].empty());
}

TEST_CLASS(util, IntrusiveList, multi_participant) {
  MLPNList1 list1;
  MLPNList2 list2;
  MLPN node1{1}, node2{2};

  list1.push_front(node1);
  node1.MLPNList1::push_front(node2);
  list2.push_front(node2);

  // list1 -> node1 -> node2 -> list1 (length 2)
  // list2 -> node2 -> list2 (length 1)
  TEST_ASSERT(list1.size() == 2);
  TEST_ASSERT(list2.size() == 1);

  TEST_ASSERT(node1.MLPNList1::size() == 2);
  TEST_ASSERT(node1.MLPNList2::size() == 0);
  TEST_ASSERT(node2.MLPNList1::size() == 2);
  TEST_ASSERT(node2.MLPNList2::size() == 1);

  // Test lists from sentinel node's POV.
  TEST_ASSERT(list1.at(1).val == 1);
  TEST_ASSERT(list1.at(2).val == 2);
  TEST_ASSERT(list2.at(1).val == 2);

  // Test lists from nodes' POVs. (This generally isn't safe since we
  // generally don't know which nodes are the sentinel nodes from the
  // nodes' POVs).
  TEST_ASSERT(node1.MLPNList1::next().val == 2);
  TEST_ASSERT(node2.MLPNList1::prev().val == 1);
}
