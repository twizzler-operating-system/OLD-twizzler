#pragma once

/** @file
 * @brief Shared mapping flags between VMM and OSPACE */

/** This mapping can not be removed unless explicitly requested */
#define MAP_WIRE 1

/** This mapping is shared across all page tables */
#define MAP_GLOBAL 2

/** This mapping is for kernel memory */
#define MAP_KERNEL 4

/** Mapping is readable */
#define MAP_READ 8

/** Mapping is writable */
#define MAP_WRITE 0x10

/** Mapping is executable */
#define MAP_EXEC 0x20

/** Instead of mapping, pre-allocate any page-table structures necessary to create the mapping */
#define MAP_TABLE_PREALLOC 0x40

/** Replace any existing mapping instead of panicing if there's an existing mapping */
#define MAP_REPLACE 0x80

/** Map zero here (optimization) */
#define MAP_ZERO 0x100

#define PAGEVEC_MAX_IDX 4096

#define PAGE_MAP_COW 1
