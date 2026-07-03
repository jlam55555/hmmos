#pragma once

/// \file arch/x86/page_table.h
/// \brief x86 page table implementation
///
/// \see mm/virt.h for full documentation

#include <cstdint>

namespace arch::page_table {

struct PageDirectoryEntry;
struct PageTableEntry;

void enumerate_page_tables();
bool map(uint64_t phys, void *virt, bool u_s, bool r_w, bool uncacheable);
bool unmap(void *virt);
bool mark_uncacheable(void *virt);

/// \see mem::virt::setup_canonical_pt().
void setup_canonical_pt();
PageDirectoryEntry *get_canonical_pt();

/// Returns the page table hierarchy.
PageDirectoryEntry *get_page_directory();

/// Copies the page table hierarchy for the sake of constructing a new
/// process. Page tables that are part of the canonical page table are
/// not cloned.
PageDirectoryEntry *clone_kernel_page_directory(PageDirectoryEntry *orig);

void delete_cloned_page_directory(PageDirectoryEntry *pd);

/// Switch to a new virtual address space.
void set_page_directory(PageDirectoryEntry *pde);

/// Iterate over present userspace pages in the page table.
void iter_page_table(int32_t start_pg, int32_t end_pg,
                     void (*)(PageTableEntry *pte, void *vaddr));

/// Callback to use with \ref iter_page_table.
void unmap_at(PageTableEntry *ent, void *vaddr);

} // namespace arch::page_table
