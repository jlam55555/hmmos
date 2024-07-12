#pragma once
/// Useful definitions related to memory sizing.
///
/// TODO: Some of these are architecture-specific (and should probably
/// be moved to an arch/ subdir).

#if defined(__cplusplus) && !defined(__clang__)
// _Static_assert isn't defined in g++.
#define _Static_assert static_assert
#endif

#define KB 0x400U
#define MB (KB * KB)
#define GB (KB * MB)

#define PG_SZ (4 * KB)
#define PG_SZ_BITS 12
#define PG_PER_PT 0x1000U
#define HUGE_PG_SZ (PG_SZ * PG_PER_PT)

#define PG_ALIGNED(addr) !(addr & (PG_SZ - 1))
#define HUGEPG_ALIGNED(addr) !(addr & (HUGE_PG_SZ - 1))

_Static_assert((1U << PG_SZ_BITS) == PG_SZ,
               "bad PG_SZ_BITS or PG_SZ definition");
_Static_assert(PG_ALIGNED(PG_SZ) && !PG_ALIGNED(PG_SZ + 1),
               "bad PG_ALIGNED definition");
_Static_assert(HUGEPG_ALIGNED(HUGE_PG_SZ) && !HUGEPG_ALIGNED(PG_SZ),
               "bad HUGEPG_ALIGNED definition");

#define HM_START (3 * GB)
