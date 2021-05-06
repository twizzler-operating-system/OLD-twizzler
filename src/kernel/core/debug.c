#include <interrupt.h>
#include <processor.h>
#include <thread.h>
#include <trace.h>

int trace_indent_level = 0;

void kernel_debug_entry(void)
{
	arch_processor_reset();
}

struct section {
	void *addr;
	size_t len;
	size_t num;
	uint32_t type;
};

#define SECTION_SYMTAB 0
#define SECTION_STRTAB 1
#define SECTION_STRSHTAB 2
#define MAX_SECTIONS 3

static struct section sections[MAX_SECTIONS];

const char *debug_symbolize(void *addr)
{
	if(sections[SECTION_SYMTAB].addr == NULL || sections[SECTION_STRTAB].addr == NULL)
		return NULL;
	Elf64_Sym *sym = sections[SECTION_SYMTAB].addr;
	while((char *)sym < (char *)sections[SECTION_SYMTAB].addr + sections[SECTION_SYMTAB].len) {
		if(sym->st_value <= (uint64_t)addr && (uint64_t)addr < sym->st_value + sym->st_size) {
			return (char *)sections[SECTION_STRTAB].addr + sym->st_name;
		}
		sym++;
	}
	return NULL;
}

void debug_elf_register_sections(Elf64_Shdr *_sections, size_t num, size_t entsize, size_t stridx)
{
	printk("[debug] registering %ld ELF headers (stridx: %ld)\n", num, stridx);
	for(size_t i = 0; i < num; i++) {
		Elf64_Shdr *sh = (void *)((char *)_sections + entsize * i);

		size_t s;
		if(sh->sh_type == SHT_SYMTAB)
			s = SECTION_SYMTAB;
		else if(i == stridx)
			s = SECTION_STRSHTAB;
		else
			s = SECTION_STRTAB;
		mm_early_alloc(NULL, &sections[s].addr, sh->sh_size, 0);
		memcpy(sections[s].addr, (void *)sh->sh_addr, sh->sh_size);
		sections[s].len = sh->sh_size;
		sections[s].num = i;
		sections[s].type = sh->sh_type;
	}
}
