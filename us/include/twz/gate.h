#pragma once
#define TWZ_GATE_SIZE 32
/*
#define __TWZ_GATE(fn, g)                                                                          \
    __asm__(".section .gates, \"ax\", @progbits\n"                                                 \
            ".global __twz_gate_" #fn "\n"                                                         \
            ".type __twz_gate_" #fn " STT_FUNC\n"                                                  \
            ".org " #g "*32, 0x90\n"                                                               \
            "__twz_gate_" #fn ":\n"                                                                \
            "leaq " #fn "(%rip), %rax\n"                                                           \
            "jmpq *%rax\n"                                                                         \
            "retq\n"                                                                               \
            ".balign 16, 0x90\n"                                                                   \
            ".previous");
__asm__(".section .gates, \"ax\", @progbits\n"
        ".global __twz_gate_bstream_write \n"
        ".type __twz_gate_bstream_write STT_FUNC\n"
        ".org 2*32, 0x90\n"
        "__twz_gate_bstream_write:\n"
        "movabs $bstream_write, %rax\n"
        "leaq -__twz_gate_bstream_write+2*32(%rip), %r15\n"
        "addq %r15, %rax\n"
        "jmp .\n"
        ".balign 32, 0x90\n"
        ".previous");
*/

extern void libtwz_gate_return(long);
#define __TWZ_GATE_SHARED(fn, g)                                                                   \
	__asm__(".section .gates, \"ax\", @progbits\n"                                                 \
	        ".global __twz_gate_" #fn "\n"                                                         \
	        ".type __twz_gate_" #fn " STT_FUNC\n"                                                  \
	        ".org " #g "*32, 0x90\n"                                                               \
	        "__twz_gate_" #fn ":\n"                                                                \
	        "movabs $" #fn ", %rax\n"                                                              \
	        "call *%rax\n"                                                                         \
	        "movq %rax, %rdi\n"                                                                    \
	        "jmp libtwz_gate_return\n"                                                             \
	        "retq\n"                                                                               \
	        ".balign 32, 0x90\n"                                                                   \
	        ".previous");

#define __TWZ_GATE(fn, g)                                                                          \
	__asm__(".section .gates, \"ax\", @progbits\n"                                                 \
	        ".global __twz_gate_" #fn "\n"                                                         \
	        ".type __twz_gate_" #fn " STT_FUNC\n"                                                  \
	        ".org " #g "*32, 0x90\n"                                                               \
	        "__twz_gate_" #fn ":\n"                                                                \
	        "movabs $" #fn ", %rax\n"                                                              \
	        "leaq -__twz_gate_" #fn "+" #g "*32(%rip), %r10\n"                                     \
	        "addq %r10, %rax\n"                                                                    \
	        "jmpq *%rax\n"                                                                         \
	        "retq\n"                                                                               \
	        ".balign 32, 0x90\n"                                                                   \
	        ".previous");

#define TWZ_GATE(fn, g) __TWZ_GATE(fn, g)
#define TWZ_GATE_SHARED(fn, g) __TWZ_GATE_SHARED(fn, g)
#define TWZ_GATE_OFFSET (OBJ_NULLPAGE_SIZE + 0x200)

#include <twz/obj.h>
#define TWZ_GATE_CALL(_obj, g)                                                                     \
	({                                                                                             \
		twzobj *obj = _obj;                                                                        \
		(void *)((obj ? (uintptr_t)twz_object_base(obj) - OBJ_NULLPAGE_SIZE : 0ull)                \
		         + g * TWZ_GATE_SIZE + TWZ_GATE_OFFSET);                                           \
	})

struct secure_api_header {
	objid_t sctx;
	objid_t view;
};

void twz_secure_api_setup_tmp_stack(void);
#define twz_secure_api_call1(hdr, gate, arg)                                                       \
	({                                                                                             \
		twz_secure_api_setup_tmp_stack();                                                          \
		struct sys_become_args args = {                                                            \
			.target_view = hdr->view,                                                              \
			.target_rip = TWZ_GATE_CALL(NULL, gate),                                               \
			.rsp = (void *)(TWZSLOT_TMPSTACK * OBJ_MAXSIZE + 0x200000),                            \
			.rdi = (long)arg,                                                                      \
		};                                                                                         \
		long r = sys_become(&args, 0, 0);                                                          \
		r;                                                                                         \
	})
