#include "../test.h"
#include "boot_protocol.h"
#include "memdefs.h"
#include "mm/page_frame_allocator.h"
#include "mm/page_frame_table.h"
#include <array>
#include <span>

namespace {

/// Similar to PageFrameTable, but doesn't allocate page frame array.
class TestPageFrameTable : public mem::phys::PageFrameTable {
public:
  /// Expose protected overload that uses the provided PFT.
  /// Can't use `using` declaration for constructors.
  TestPageFrameTable(std::span<e820_mm_entry> mm,
                     std::span<mem::phys::PageFrameDescriptor> _pft)
      : PageFrameTable(mm, _pft) {}

  std::span<const e820_mm_entry> urs() { return get_usable_regions(); }
};

/// Test fixture with no allocators, only a page frame table with a
/// single 4-page contiguous usable region.
class PFTSimple : public test::TestFixture {
  void setup() final {}

  static constexpr e820_mm_entry usable{
      .base = 0, .len = 4 * PG_SZ, .type = E820_MM_TYPE_USABLE};

  std::array<e820_mm_entry, 1> mm{usable};
  std::array<mem::phys::PageFrameDescriptor, 32> pft_arr;

protected:
  TestPageFrameTable pft{mm, pft_arr};
};

/// Based on PFTSimple but with a page frame allocator.
class PFASimple : public PFTSimple {
protected:
  mem::phys::SimplePFA pfa{pft, 0, pft.mem_limit()};
};

/// Test fixture with 3 contiguous usable regions.
class PFAMemoryGap : public test::TestFixture {
  void setup() final {}

  static constexpr e820_mm_entry usable1{
      .base = 0, .len = PG_SZ, .type = E820_MM_TYPE_USABLE};
  static constexpr e820_mm_entry usable2{
      .base = 2 * PG_SZ, .len = PG_SZ, .type = E820_MM_TYPE_USABLE};
  static constexpr e820_mm_entry usable3{
      .base = 31 * PG_SZ, .len = PG_SZ, .type = E820_MM_TYPE_USABLE};

  std::array<e820_mm_entry, 3> mm{usable1, usable2, usable3};
  std::array<mem::phys::PageFrameDescriptor, 32> pft_arr;
  TestPageFrameTable pft{mm, pft_arr};

protected:
  mem::phys::SimplePFA pfa{pft, 0, pft.mem_limit()};
};

} // namespace

TEST_CLASS(mem::phys, PageFrameTable, load_memory_map) {
  // Correctly loads and normalizes the memory map.
  auto make_region = [](uint64_t base, uint64_t len = PG_SZ,
                        e820_mm_type type = E820_MM_TYPE_USABLE) {
    return e820_mm_entry{
        .base = base,
        .len = len,
        .type = type,
    };
  };

  // Simple case, make sure these make it to the usable region list.
  e820_mm_entry usable1 = make_region(0);
  e820_mm_entry usable2 = make_region(2 * PG_SZ);

  auto mm1 = std::to_array({usable1, usable2});
  std::array<PageFrameDescriptor, 32> pft_arr;

  TestPageFrameTable pft1(mm1, pft_arr);
  TEST_ASSERT(pft1.urs().size() == 2);
  TEST_ASSERT(pft1.urs()[0] == usable1);
  TEST_ASSERT(pft1.urs()[1] == usable2);
  TEST_ASSERT(pft1.total_mem_bytes == 2 * PG_SZ);
  TEST_ASSERT(pft1.usable_mem_bytes == 2 * PG_SZ);

  // Make sure usable regions are sorted.
  e820_mm_entry usable3 = make_region(4 * PG_SZ);
  auto mm2 = std::to_array({usable3, usable1, usable2});

  TestPageFrameTable pft2(mm2, pft_arr);
  TEST_ASSERT(pft2.urs().size() == 3);
  TEST_ASSERT(pft2.urs()[0] == usable1);
  TEST_ASSERT(pft2.urs()[1] == usable2);
  TEST_ASSERT(pft2.urs()[2] == usable3);
  TEST_ASSERT(pft2.total_mem_bytes == 3 * PG_SZ);
  TEST_ASSERT(pft2.usable_mem_bytes == 3 * PG_SZ);

  // Test overlapping regions. We don't currently have a way to
  // TEST_THROWS, although this shouldn't be too hard.
  //
  // e820_mm_entry overlapping = make_region(PG_SZ / 2);
  // auto mm3 = std::to_array({usable1, overlapping});
  // TestPageFrameTable pft3(mm3, pft);

  // Bootloader pages are subtracted from usable regions.
  // This looks something like this:
  //
  //                  | 0 | 1 | 2 | 3 | 4 | 5 | 6 |
  // usable pages     | X | X | X | X |   | X | X |
  // bootloader pages |   | X | X |   |   | X |   |
  //                  -----------------------------
  // result           | X |   |   | X |   |   | X |
  e820_mm_entry usable4 = make_region(0, 4 * PG_SZ);
  e820_mm_entry usable5 = make_region(5 * PG_SZ, 2 * PG_SZ);
  e820_mm_entry bootloader1 =
      make_region(PG_SZ, PG_SZ + 1, E820_MM_TYPE_BOOTLOADER);
  e820_mm_entry bootloader2 =
      make_region(5 * PG_SZ + 1, PG_SZ - 2, E820_MM_TYPE_BOOTLOADER);
  auto mm4 = std::to_array({usable4, usable5, bootloader1, bootloader2});

  TestPageFrameTable pft4(mm4, pft_arr);
  TEST_ASSERT(pft4.urs().size() == 3);
  TEST_ASSERT(pft4.urs()[0] == make_region(0));
  TEST_ASSERT(pft4.urs()[1] == make_region(3 * PG_SZ));
  TEST_ASSERT(pft4.urs()[2] == make_region(6 * PG_SZ));
  TEST_ASSERT(pft4.total_mem_bytes == 6 * PG_SZ);
  TEST_ASSERT(pft4.usable_mem_bytes == 3 * PG_SZ);

  // Regions should be aligned to full-pages. There isn't an attempt
  // to combine regions within the same page.
  e820_mm_entry usable6 = make_region(1, PG_SZ - 1);
  e820_mm_entry usable7 = make_region(PG_SZ, PG_SZ - 1);
  e820_mm_entry usable8 = make_region(2 * PG_SZ, 1);
  e820_mm_entry usable9 = make_region(2 * PG_SZ + 1, PG_SZ - 1);
  e820_mm_entry usable10 = make_region(3 * PG_SZ + PG_SZ / 2, PG_SZ);
  e820_mm_entry usable11 = make_region(5 * PG_SZ + PG_SZ / 2, 2 * PG_SZ);
  auto mm5 =
      std::to_array({usable6, usable7, usable8, usable9, usable10, usable11});
  TestPageFrameTable pft5(mm5, pft_arr);
  TEST_ASSERT(pft5.urs().size() == 1);
  TEST_ASSERT(pft5.urs()[0] == make_region(6 * PG_SZ));
  TEST_ASSERT(pft5.total_mem_bytes == usable6.len + usable7.len + usable8.len +
                                          usable9.len + usable10.len +
                                          usable11.len);
  TEST_ASSERT(pft5.usable_mem_bytes == PG_SZ);

  // Don't include other page types.
  e820_mm_entry reserved = make_region(PG_SZ, PG_SZ, E820_MM_TYPE_RESERVED);
  e820_mm_entry acpi_reclaimable =
      make_region(2 * PG_SZ, PG_SZ, E820_MM_TYPE_ACPI_RECLAIMABLE);
  e820_mm_entry acpi_nvs = make_region(3 * PG_SZ, PG_SZ, E820_MM_TYPE_ACPI_NVS);
  e820_mm_entry bad_mem = make_region(4 * PG_SZ, PG_SZ, E820_MM_TYPE_BAD_MEM);
  auto mm6 =
      std::to_array({usable1, reserved, acpi_reclaimable, acpi_nvs, bad_mem});
  TestPageFrameTable pft6(mm6, pft_arr);
  TEST_ASSERT(pft6.urs().size() == 1);
  TEST_ASSERT(pft6.urs()[0] == usable1);
  TEST_ASSERT(pft6.total_mem_bytes == 5 * PG_SZ);
  TEST_ASSERT(pft6.usable_mem_bytes == PG_SZ);
}

TEST_CLASS_WITH_FIXTURE(mem::phys, SimplePFA, alloc_simple, PFASimple) {
  std::array<std::optional<uint64_t>, 4> pgs{};

  // Simple OOM check.
  for (unsigned i = 0; i < 4; ++i) {
    TEST_ASSERT(pgs[i] = pfa.alloc(1));
    for (int j = 0; j < i; ++j) {
      TEST_ASSERT(*pgs[i] != *pgs[j]);
    }
  }
  TEST_ASSERT(!pfa.alloc(1));

  pfa.free(*pgs[0], 1);
  // Double-free should fail.
  // pfa.free(*pgs[0], 1);

  std::optional<uint64_t> new_alloc;
  TEST_ASSERT((new_alloc = pfa.alloc(1)));
  TEST_ASSERT(!pfa.alloc(1));
  TEST_ASSERT(pgs[0] == new_alloc);

  // Note that we can free pages from multiple allocs at once.
  // This probably isn't a good idea in general.
  pfa.free(0, 4);
}

TEST_CLASS_WITH_FIXTURE(mem::phys, SimplePFA, alloc_memory_gap, PFAMemoryGap) {
  std::optional<uint64_t> pg1, pg2, pg3, pg4;

  // Simple OOM check.
  TEST_ASSERT((pg1 = pfa.alloc(1)));
  TEST_ASSERT((pg2 = pfa.alloc(1)));
  TEST_ASSERT((pg3 = pfa.alloc(1)));
  TEST_ASSERT(!pfa.alloc(1));

  TEST_ASSERT(*pg1 != *pg2);
  TEST_ASSERT(*pg1 != *pg3);
  TEST_ASSERT(*pg2 != *pg3);

  pfa.free(*pg1, 1);
  // Double-free should fail.
  // pfa.free(*pg1, 1);

  TEST_ASSERT((pg4 = pfa.alloc(1)));
  TEST_ASSERT(!pfa.alloc(1));
  TEST_ASSERT(pg1 == pg4);

  TEST_ASSERT(pfa.get_total_pages() == 3);
  TEST_ASSERT(pfa.get_free_pages() == 0);
}

TEST_CLASS_WITH_FIXTURE(mem::phys, SimplePFA, bookkeeping, PFASimple) {
  // Check that bookkeeping is correct.
  TEST_ASSERT(pfa.get_total_pages() == 4);
  TEST_ASSERT(pfa.get_free_pages() == 4);

  TEST_ASSERT(pfa.alloc(1));
  TEST_ASSERT(pfa.get_total_pages() == 4);
  TEST_ASSERT(pfa.get_free_pages() == 3);

  TEST_ASSERT(pfa.alloc(1));
  TEST_ASSERT(pfa.get_total_pages() == 4);
  TEST_ASSERT(pfa.get_free_pages() == 2);
}

TEST_CLASS_WITH_FIXTURE(mem::phys, SimplePFA, multi_page_allocs, PFASimple) {
  // Allocation pattern:
  //
  // ---- initial state
  // x--- alloc 1
  // xxx- alloc 2
  // x-x- free 1 @ offset 1
  // (can't alloc 2 at this point)
  // x-xx alloc 1
  // x--x free 1 @ offset 2
  // xxxx alloc 2
  TEST_ASSERT(pfa.alloc(1));
  TEST_ASSERT(pfa.alloc(2));
  pfa.free(PG_SZ, 1);

  // Can't allocate 2 even though there are enough (total) pages
  // available.
  TEST_ASSERT(!pfa.alloc(2));
  TEST_ASSERT(pfa.get_free_pages() == 2);

  TEST_ASSERT(pfa.alloc(1));
  pfa.free(2 * PG_SZ, 1);
  TEST_ASSERT(pfa.alloc(2));
}

/// Free then re-alloc should return same page.
TEST_CLASS_WITH_FIXTURE(mem::phys, SimplePFA, reuse_last_freed, PFASimple) {
  std::optional<uint64_t> pg1, pg2;

  TEST_ASSERT(pg1 = pfa.alloc(4));
  pfa.free(*pg1, 4);
  TEST_ASSERT(pg1 == pfa.alloc(2));

  TEST_ASSERT(pg2 = pfa.alloc(1));
  pfa.free(*pg2, 1);
  TEST_ASSERT(pg2 == pfa.alloc(1));

  // Cleanup.
  pfa.free(*pg1, 2);
  pfa.free(*pg2, 1);

  // Note that this won't (necessarily) work when allocating a larger
  // size. Let's set up the pattern:
  //
  // ---- initial state
  // xxxx alloc 4, needle at 0
  // -xxx free 1 @ offset 0 (needle doesn't move)
  // -x-- free 2 @ offset 2 (needle doesn't move)
  //
  // Now we're done with the setup. If we do:
  // 1. alloc 1, free 1 @ offset 0, alloc 1 (second alloc will be at
  //   same location)
  // 2. alloc 1, free 1 @ offset 0, alloc 2 (will not be at offset 0
  //   since it doesn't fit)
  TEST_ASSERT(pfa.alloc(4));
  pfa.free(0, 1);
  pfa.free(2 * PG_SZ, 2);

  // 1.
  TEST_ASSERT(pg1 = pfa.alloc(1));
  pfa.free(0, 1);
  TEST_ASSERT(pg1 == pfa.alloc(1));
  pfa.free(0, 1);

  // 2.
  TEST_ASSERT(pg1 = pfa.alloc(1));
  pfa.free(0, 1);
  TEST_ASSERT(pg1 != pfa.alloc(2));
}

TEST_CLASS_WITH_FIXTURE(mem::phys, PageFrameTable, sub_allocator, PFTSimple) {
  SimplePFA pfa(pft, 1 * PG_SZ, 3 * PG_SZ);

  TEST_ASSERT(pft.total_mem_bytes == 4 * PG_SZ);
  TEST_ASSERT(pft.usable_mem_bytes == 4 * PG_SZ);

  TEST_ASSERT(pfa.get_total_pages() == 2);
  TEST_ASSERT(pfa.get_free_pages() == 2);

  std::optional<uint64_t> pg1, pg2;
  auto in_range = [](uint64_t pg) { return pg == PG_SZ || pg == 2 * PG_SZ; };
  TEST_ASSERT((pg1 = pfa.alloc(1)) && in_range(*pg1));
  TEST_ASSERT((pg2 = pfa.alloc(1)) && in_range(*pg2));
  TEST_ASSERT(!pfa.alloc(1));
}

TEST_CLASS_WITH_FIXTURE(mem::phys, PageFrameTable, multiple_suballocators,
                        PFTSimple) {
  // Make sure we can create multiple page frame allocators in
  // non-overlapping subregions.
  SimplePFA pfa1(pft, 0, 2 * PG_SZ);
  TEST_ASSERT(pfa1.get_total_pages() == 2);

  std::optional<uint64_t> pg;

  {
    SimplePFA pfa2(pft, 2 * PG_SZ, 4 * PG_SZ);

    TEST_ASSERT(pg = pfa2.alloc(1));
    TEST_ASSERT(pfa2.get_total_pages() == 2);

    // This will fail with an assertion.
    // SimplePFA pfa3(pft, 3 * PG_SZ, 4 * PG_SZ);
  }

  // The alloc should succeed once the bad alloc goes out of scope.
  // Note that we can alloc the same pages as the destroyed allocator.
  SimplePFA pfa4(pft, 2 * PG_SZ, 3 * PG_SZ);
  TEST_ASSERT(pg == pfa4.alloc(1));
  TEST_ASSERT(pfa4.get_total_pages() == 1);
}
