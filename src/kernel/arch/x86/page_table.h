#pragma once

/// \file arch/x86/page_table.h
/// \brief x86 page table implementation
///
/// \see mm/virt.h for full documentation

#include <cstdint>

namespace arch::page_table {

struct PageDirectoryEntry;

void enumerate_page_tables();
bool map(uint64_t phys, void *virt, bool u_s, bool r_w, bool uncacheable);
bool unmap(void *virt);
bool mark_uncacheable(void *virt);

/// Returns the page table hierarchy.
PageDirectoryEntry *get_page_directory();

/// Copies the page table hierarchy.
PageDirectoryEntry *clone_kernel_page_directory(PageDirectoryEntry *orig);

/// Switch to a new virtual address space.
void set_page_directory(PageDirectoryEntry *pde);

} // namespace arch::page_table
