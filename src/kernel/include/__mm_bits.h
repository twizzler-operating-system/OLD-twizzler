#pragma once

#define MAP_WIRE 1
#define MAP_GLOBAL 2
#define MAP_KERNEL 4
#define MAP_READ 8
#define MAP_WRITE 0x10
#define MAP_EXEC 0x20
#define MAP_TABLE_PREALLOC 0x40
#define MAP_REPLACE 0x80
#define MAP_ZERO 0x100

#define PAGEVEC_MAX_IDX 4096
