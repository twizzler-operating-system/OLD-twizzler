#pragma once

typedef struct {
	uint32_t sh_name;
	uint32_t sh_type;
	uint64_t sh_flags;
	uint64_t sh_addr;
	uint64_t sh_offset;
	uint64_t sh_size;
	uint32_t sh_link;
	uint32_t sh_info;
	uint64_t sh_addralign;
	uint64_t sh_entsize;
} Elf64_Shdr;

#define SHT_SYMTAB 2 /* Symbol table */
#define SHT_STRTAB 3 /* String table */

typedef struct {
	uint32_t st_name;       /* Symbol name (string tbl index) */
	unsigned char st_info;  /* Symbol type and binding */
	unsigned char st_other; /* Symbol visibility */
	uint16_t st_shndx;      /* Section index */
	uint64_t st_value;      /* Symbol value */
	uint64_t st_size;       /* Symbol size */
} Elf64_Sym;
