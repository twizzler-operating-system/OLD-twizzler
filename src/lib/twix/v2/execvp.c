/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <twix/twix.h>
#include <twz/obj.h>
#include <twz/sys/obj.h>
#include <twz/sys/thread.h>
#include <twz/sys/view.h>

#include "../syscalls.h"
#include "v2.h"

#include <elf.h>

struct elf64_header {
	uint8_t e_ident[16];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

extern int *__errno_location();
#include <twz/debug.h>
__attribute__((used)) static int __do_exec(uint64_t entry,
  uint64_t _flags,
  uint64_t vidlo,
  uint64_t vidhi,
  void *vector)
{
	(void)_flags;
	objid_t vid = MKID(vidhi, vidlo);

	struct sys_become_args ba = {
		.target_view = vid,
		.target_rip = entry,
		.rdi = (long)vector,
		.rsp = (long)SLOT_TO_VADDR(TWZSLOT_STACK) + 0x200000,
	};
	int r = sys_become(&ba, 0, 0);
	twz_thread_exit(r);
	return 0;
}

extern char **environ;
static int __internal_do_exec(twzobj *view,
  void *entry,
  char const *const *argv,
  char *const *env,
  void *auxbase,
  void *phdr,
  size_t phnum,
  size_t phentsz,
  void *auxentry,
  const char *exename,
  twzobj *exe)
{
	if(env == NULL)
		env = environ;

	twzobj stack;
	objid_t sid;
	int r;
	if((r = twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &sid))) {
		return r;
	}
	if((r = twz_object_init_guid(&stack, sid, FE_READ | FE_WRITE))) {
		return r;
	}
	if((r = twz_object_tie(view, &stack, 0))) {
		return r;
	}
	if((r = twz_object_delete_guid(sid, 0))) {
		return r;
	}

	uint64_t sp;

	/* calculate space */
	size_t str_space = 0;
	size_t argc = 0;
	size_t envc = 0;
	for(const char *const *s = &argv[0]; *s; s++) {
		str_space += strlen(*s) + 1;
		argc++;
	}

	for(char *const *s = &env[0]; *s; s++) {
		str_space += strlen(*s) + 1;
		envc++;
	}
	str_space += strlen(exename) + 1 + 1 + strlen("TWZEXENAME");

	sp = OBJ_TOPDATA;
	str_space = ((str_space - 1) & ~15) + 16;

	/* TODO: check if we have enough space... */

	/* 5 for: 1 for exename, 1 NULL each for argv and env, argc, and final null after env */
	long *vector_off = (void *)(sp
	                            - (str_space + (argc + envc + 5) * sizeof(char *)
	                               + sizeof(long) * 2 * 32 /* TODO: number of aux vectors */));
	long *vector = twz_object_lea(&stack, vector_off);

	size_t v = 0;
	vector[v++] = argc;
	char *str = (char *)twz_object_lea(&stack, (char *)sp);
	for(size_t i = 0; i < argc; i++) {
		const char *s = argv[i];
		str -= strlen(s) + 1;
		strcpy(str, s);
		vector[v++] = (long)twz_ptr_rebase(TWZSLOT_STACK, str);
	}
	vector[v++] = 0;

	for(size_t i = 0; i < envc; i++) {
		const char *s = env[i];
		str -= strlen(s) + 1;
		strcpy(str, s);
		vector[v++] = (long)twz_ptr_rebase(TWZSLOT_STACK, str);
	}
	str -= strlen(exename) + 1 + 1 + strlen("TWZEXENAME");
	strcpy(str, "TWZEXENAME=");
	strcpy(str + strlen("TWZEXENAME="), exename);
	char *copied_exename = str + strlen("TWZEXENAME=");
	vector[v++] = (long)twz_ptr_rebase(TWZSLOT_STACK, str);

	vector[v++] = 0;

	vector[v++] = AT_BASE;
	vector[v++] = (long)auxbase;

	vector[v++] = AT_PAGESZ;
	vector[v++] = 0x1000;

	vector[v++] = AT_ENTRY;
	vector[v++] = (long)auxentry;

	vector[v++] = AT_PHNUM;
	vector[v++] = (long)phnum;

	vector[v++] = AT_PHENT;
	vector[v++] = (long)phentsz;

	vector[v++] = AT_PHDR;
	vector[v++] = (long)phdr;

	vector[v++] = AT_EXECFN;
	vector[v++] = (long)twz_ptr_rebase(TWZSLOT_STACK, copied_exename);

	vector[v++] = AT_UID;
	vector[v++] = 0;
	vector[v++] = AT_GID;
	vector[v++] = 0;
	vector[v++] = AT_EUID;
	vector[v++] = 0;
	vector[v++] = AT_EGID;
	vector[v++] = 0;

	vector[v++] = AT_NULL;
	vector[v++] = 0;

	/* TODO: we should really do this in assembly */
	twz_view_set(view, TWZSLOT_STACK, sid, VE_READ | VE_WRITE);

	// memset(repr->faults, 0, sizeof(repr->faults));
	objid_t vid = twz_object_guid(view);
	objid_t eid = twz_object_guid(exe);

	struct twix_queue_entry tqe =
	  build_tqe(TWIX_CMD_EXEC, 0, 0, 4, ID_LO(vid), ID_HI(vid), ID_LO(eid), ID_HI(eid));
	twix_sync_command(&tqe);

	/* TODO: copy-in everything for the vector */
	int ret;
	uint64_t p = (uint64_t)SLOT_TO_VADDR(TWZSLOT_STACK) + (OBJ_NULLPAGE_SIZE + 0x200000);
	register long r8 asm("r8") = (long)vector_off + (long)SLOT_TO_VADDR(TWZSLOT_STACK);
	__asm__ __volatile__("movq %%rax, %%rsp\n"
	                     "call __do_exec\n"
	                     : "=a"(ret)
	                     : "a"(p),
	                     "D"((uint64_t)entry),
	                     "S"((uint64_t)(0)),
	                     "d"((uint64_t)vid),
	                     "c"((uint64_t)(vid >> 64)),
	                     "r"(r8));
	twz_thread_exit(ret);
	return ret;
}

static int __internal_load_elf_object(twzobj *view,
  twzobj *elfobj,
  void **base,
  void **phdrs,
  void **entry,
  bool interp)
{
	Elf64_Ehdr *hdr = twz_object_base(elfobj);

	twzobj new_text, new_data;
	int r;
	if((r = twz_object_new(&new_text,
	      NULL,
	      NULL,
	      OBJ_VOLATILE,
	      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_EXEC))) {
		return r;
	}
	if((r = twz_object_new(
	      &new_data, NULL, NULL, OBJ_VOLATILE, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE))) {
		return r;
	}

	char *phdr_start = (char *)hdr + hdr->e_phoff;
	for(unsigned i = 0; i < hdr->e_phnum; i++) {
		Elf64_Phdr *phdr = (void *)(phdr_start + i * hdr->e_phentsize);
		if(phdr->p_type == PT_LOAD) {
			//		twix_log("load: off=%lx, vaddr=%lx, paddr=%lx, fsz=%lx, msz=%lx\n",
			//		  phdr->p_offset,
			//		  phdr->p_vaddr,
			//		  phdr->p_paddr,
			//		  phdr->p_filesz,
			//		  phdr->p_memsz);
			//	twix_log("  -> %lx %lx\n",
			//	  phdr->p_vaddr & ~(phdr->p_align - 1),
			//	  phdr->p_offset & ~(phdr->p_align - 1));
			twzobj *to;
			if(phdr->p_flags & PF_X) {
				to = &new_text;
			} else {
				to = &new_data;
			}

			char *memstart = twz_object_base(to);
			char *filestart = twz_object_base(elfobj);
			memstart += ((phdr->p_vaddr & ~(phdr->p_align - 1)) % OBJ_MAXSIZE)
			            - (interp ? 0 : OBJ_NULLPAGE_SIZE);
			filestart += phdr->p_offset & ~(phdr->p_align - 1);
			size_t len = phdr->p_filesz;
			len += (phdr->p_offset & (phdr->p_align - 1));
			size_t zerolen = phdr->p_memsz - phdr->p_filesz;
			//		twix_log("  ==> %p %p %lx\n", filestart, memstart, len);
			if((r = sys_ocopy(twz_object_guid(to),
			      twz_object_guid(elfobj),
			      (long)memstart % OBJ_MAXSIZE,
			      (long)filestart % OBJ_MAXSIZE,
			      (len + 0xfff) & ~0xfff,
			      0))) {
				twix_log("oc: %d\n", r);
				return r;
			}
			memset(memstart + phdr->p_filesz, 0, zerolen);

			struct metainfo *mi = twz_object_meta(to);
			mi->flags |= MIF_SZ;
			mi->sz = len + zerolen;

			//		memcpy(memstart, filestart, len);
		}
	}

	if(twz_object_tie(view, &new_text, 0) < 0)
		abort();
	if(twz_object_tie(view, &new_data, 0) < 0)
		abort();
	/* TODO: delete these too */

	size_t base_slot = interp ? 0x10003 : 0;
	twz_view_set(view, base_slot, twz_object_guid(&new_text), VE_READ | VE_EXEC);
	twz_view_set(view, base_slot + 1, twz_object_guid(&new_data), VE_READ | VE_WRITE);

	if(base) {
		*base = (void *)(SLOT_TO_VADDR(base_slot) + OBJ_NULLPAGE_SIZE);
	}
	if(phdrs) {
		/* we don't care about the phdrs for the interpreter, so this only has to be right for the
		 * executable. */
		*phdrs = (void *)(OBJ_NULLPAGE_SIZE + hdr->e_phoff);
	}
	*entry = (base ? (char *)*base : (char *)0) + hdr->e_entry;

	return 0;
}

#if 0
static int __internal_load_elf_exec(twzobj *view, twzobj *elfobj, void **phdr, void **entry)
{
	Elf64_Ehdr *hdr = twz_object_base(elfobj);

	twzobj new_text, new_data;
	int r;
	if((r = twz_object_new(&new_text,
	      NULL,
	      NULL,
	      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_EXEC | TWZ_OC_VOLATILE))) {
		return r;
	}
	if((r = twz_object_new(
	      &new_data, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_VOLATILE))) {
		return r;
	}

	char *phdr_start = (char *)hdr + hdr->e_phoff;
	for(unsigned i = 0; i < hdr->e_phnum; i++) {
		Elf64_Phdr *phdr = (void *)(phdr_start + i * hdr->e_phentsize);
		if(phdr->p_type == PT_LOAD) {
			twix_log("load: off=%lx, vaddr=%lx, paddr=%lx, fsz=%lx, msz=%lx\n",
			  phdr->p_offset,
			  phdr->p_vaddr,
			  phdr->p_paddr,
			  phdr->p_filesz,
			  phdr->p_memsz);
			twix_log("  -> %lx %lx\n",
			  phdr->p_vaddr & ~(phdr->p_align - 1),
			  phdr->p_offset & ~(phdr->p_align - 1));

			twzobj *to;
			if(phdr->p_flags & PF_X) {
				to = &new_text;
			} else {
				to = &new_data;
			}

			char *memstart = twz_object_base(to);
			char *filestart = twz_object_base(elfobj);
			memstart += ((phdr->p_vaddr & ~(phdr->p_align - 1)) % OBJ_MAXSIZE) - OBJ_NULLPAGE_SIZE;
			filestart += phdr->p_offset & ~(phdr->p_align - 1);
			size_t len = phdr->p_filesz;
			len += (phdr->p_offset & (phdr->p_align - 1));
			size_t zerolen = phdr->p_memsz - phdr->p_filesz;
			//	twix_log("  ==> %p %p %lx\n", filestart, memstart, len);
			if((r = sys_ocopy(twz_object_guid(to),
			      twz_object_guid(elfobj),
			      (long)memstart % OBJ_MAXSIZE,
			      (long)filestart % OBJ_MAXSIZE,
			      (len + 0xfff) & ~0xfff,
			      0))) {
				twix_log("oc: %d\n", r);
				return r;
			}
			twix_log("ZEROing %p for len = %lx\n", memstart + phdr->p_filesz, zerolen);
			memset(memstart + phdr->p_filesz, 0, zerolen);
			//			memcpy(memstart, filestart, len);
		}
	}

	/* TODO: actually do tying */
	twz_object_tie(view, &new_text, 0);
	twz_object_tie(view, &new_data, 0);

	twz_view_set(view, 0, twz_object_guid(&new_text), VE_READ | VE_EXEC);
	twz_view_set(view, 1, twz_object_guid(&new_data), VE_READ | VE_WRITE);

	*phdr = (void *)(OBJ_NULLPAGE_SIZE + hdr->e_phoff);
	*entry = (char *)hdr->e_entry;

	return 0;
}
#endif

static long __internal_execve_view_interp(twzobj *view,
  twzobj *exe,
  const char *interp_path,
  const char *const *argv,
  char *const *env,
  const char *exename)
{
	twzobj interp;
	Elf64_Ehdr *hdr = twz_object_base(exe);
	int r;
	if((r = twz_object_init_name(&interp, interp_path, FE_READ))) {
		return r;
	}

	void *interp_base, *interp_entry;
	if((r = __internal_load_elf_object(view, &interp, &interp_base, NULL, &interp_entry, true))) {
		return r;
	}

	void *exe_entry, *phdr;
	if((r = __internal_load_elf_object(view, exe, NULL, &phdr, &exe_entry, false))) {
		return r;
	}

	// twix_log("GOT interp base=%p, entry=%p\n", interp_base, interp_entry);
	// twix_log("GOT phdr=%p, entry=%p\n", phdr, exe_entry);

	__internal_do_exec(view,
	  interp_entry,
	  argv,
	  env,
	  interp_base,
	  phdr,
	  hdr->e_phnum,
	  hdr->e_phentsize,
	  exe_entry,
	  exename,
	  exe);
	return -1;
}
static int __twz_exec_create_view(twzobj *view, objid_t id, objid_t *vid)
{
	int r;
	if((r = twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_USE, 0, 0, vid))) {
		return r;
	}
	if((r = twz_object_init_guid(view, *vid, FE_READ | FE_WRITE))) {
		return r;
	}

	twz_view_set(view, TWZSLOT_CVIEW, *vid, VE_READ | VE_WRITE);

	twz_view_set(view, 0, id, VE_READ | VE_EXEC);

	if((r = twz_object_wire(NULL, view)))
		return r;
	if((r = twz_object_delete(view, 0)))
		return r;

	struct twzview_repr *vr = twz_object_base(view);
	vr->exec_id = id;
	return 0;
}

#include <twz/name.h>
long __hook_execve(const char *path, const char *const *argv, char *const *env)
{
	objid_t id = 0;
	int r = twz_name_dfl_resolve(path, 0, &id);
	if(r) {
		return r;
	}

	objid_t vid;
	twzobj view;
	if((r = __twz_exec_create_view(&view, id, &vid)) < 0) {
		return r;
	}

	twix_copy_fds(&view);

	twzobj exe;
	twz_object_init_guid(&exe, id, FE_READ);

	char *shbang = twz_object_base(&exe);
	if(*shbang == '#' && *(shbang + 1) == '!') {
		char _cmd[256] = { 0 };
		strncpy(_cmd, shbang + 2, 255);
		char *cmd = _cmd;
		while(*cmd == ' ')
			cmd++;
		char *nl = strchr(cmd, '\n');
		if(nl)
			*nl = 0;
		char *eoc = strchr(cmd, ' ');
		if(eoc) {
			*eoc++ = 0;
			if(*eoc == '-' && (*(eoc + 1) == '-' || *(eoc + 1) == 0))
				eoc = NULL;
		}

		int argc = 0;
		while(argv[argc++])
			;
		const char **new_argv = calloc(argc + 3, sizeof(char *));
		new_argv[0] = argv[0];
		if(eoc) {
			new_argv[1] = eoc;
			new_argv[2] = path;
		} else {
			new_argv[1] = path;
		}
		for(int i = 1; i <= argc; i++) {
			new_argv[i + (eoc ? 2 : 1)] = argv[i];
		}
		return __hook_execve(cmd, new_argv, env);
	}

	struct elf64_header *hdr = twz_object_base(&exe);

	kso_set_name(NULL, "[instance] [unix] %s", path);

	char *phdr_start = (char *)hdr + hdr->e_phoff;
	for(unsigned i = 0; i < hdr->e_phnum; i++) {
		Elf64_Phdr *phdr = (void *)(phdr_start + i * hdr->e_phentsize);
		if(phdr->p_type == PT_INTERP) {
			char *interp = (char *)hdr + phdr->p_offset;
			return __internal_execve_view_interp(&view, &exe, interp, argv, env, path);
		}
	}

	return -ENOTSUP;
	// r = twz_exec_view(&view, vid, hdr->e_entry, argv, env);

	// return r;
}

long hook_execve(struct syscall_args *args)
{
	return __hook_execve(
	  (const char *)args->a0, (const char *const *)args->a1, (char *const *)args->a2);
}
