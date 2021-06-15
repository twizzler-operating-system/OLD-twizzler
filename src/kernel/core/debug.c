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
	char __sections[num * entsize];
	memcpy(__sections, _sections, sizeof(__sections));
	printk("[debug] registering %ld ELF headers (stridx: %ld) from %p\n", num, stridx, _sections);
	for(size_t i = 0; i < num; i++) {
		Elf64_Shdr *sh = (void *)(__sections + entsize * i);

		size_t s;
		if(sh->sh_type == SHT_SYMTAB)
			s = SECTION_SYMTAB;
		else if(sh->sh_type == SHT_STRTAB) {
			if(i == stridx)
				s = SECTION_STRSHTAB;
			else
				s = SECTION_STRTAB;
		} else {
			continue;
		}
		mm_early_alloc(NULL, &sections[s].addr, sh->sh_size, 0);
		memcpy(sections[s].addr, (void *)sh->sh_addr, sh->sh_size);
		sections[s].len = sh->sh_size;
		sections[s].num = i;
		sections[s].type = sh->sh_type;
	}
}

#include <object.h>
#include <slab.h>
static bool debug_process_line(char *line)
{
	if(!strcmp(line, "info mem")) {
		mm_print_stats();
	} else if(!strcmp(line, "info slab")) {
		slabcache_all_print_stats();
	} else if(!strcmp(line, "info cpus")) {
		processor_print_all_stats();
	} else if(!strcmp(line, "info threads")) {
		thread_print_all_threads();
	} else if(!strcmp(line, "info objs")) {
		obj_print_stats();
	} else if(!strcmp(line, "c") || !strcmp(line, "cont") || !strcmp(line, "continue")) {
		return true;
	}

	return false;
}

bool debug_process_input(unsigned int c)
{
	static int state = 0;
	static char buffer[128];
	static unsigned bufpos = 0;
	if(state == 0) {
		bufpos = 0;
		memset(buffer, 0, sizeof(buffer));
		state = 1;
		printk("\n\nkdbg> ");
	}
	if(c == '\n' || c == '\r') {
		printk("\n");
		bool r = debug_process_line(buffer);
		if(!r) {
			bufpos = 0;
			memset(buffer, 0, sizeof(buffer));
			printk("kdbg> ");
		} else {
			state = 0;
			return true;
		}
	} else {
		if(c == '\b' || c == 0x7f) {
			if(bufpos) {
				buffer[--bufpos] = 0;
				printk("\b \b");
			}
		} else {
			if(bufpos < sizeof(buffer) - 1) {
				buffer[bufpos++] = c;
				printk("%c", c);
			}
		}
	}
	return false;
}
